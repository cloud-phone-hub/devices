// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2025-2026 fei_cong(https://github.com/feicong/feicong-course)
 */
#define pr_fmt(fmt) "cdh_device_info_topology: " fmt

#include <linux/arch_topology.h>
#include <linux/cpumask.h>
#include <linux/kernel.h>
#include <linux/version.h>

#include "cdh_device_info.h"

struct cdh_topology_saved {
	int thread_id;
	int core_id;
#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 1, 0)
	int cluster_id;
#endif
	int package_id;
	cpumask_t thread_sibling;
	cpumask_t core_sibling;
#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 1, 0)
	cpumask_t cluster_sibling;
#endif
	cpumask_t llc_sibling;
};

/* 卸载时恢复原始 topology，避免调试 rmmod 后把 guest 内核拓扑永久留在  画像。 */
static struct cdh_topology_saved cdh_saved_topology[8];
static bool cdh_topology_registered;

static void cdh_save_cpu_topology(unsigned int cpu)
{
	struct cdh_topology_saved *saved = &cdh_saved_topology[cpu];

	saved->thread_id = cpu_topology[cpu].thread_id;
	saved->core_id = cpu_topology[cpu].core_id;
#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 1, 0)
	saved->cluster_id = cpu_topology[cpu].cluster_id;
#endif
	saved->package_id = cpu_topology[cpu].package_id;
	cpumask_copy(&saved->thread_sibling, &cpu_topology[cpu].thread_sibling);
	cpumask_copy(&saved->core_sibling, &cpu_topology[cpu].core_sibling);
#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 1, 0)
	cpumask_copy(&saved->cluster_sibling, &cpu_topology[cpu].cluster_sibling);
#endif
	cpumask_copy(&saved->llc_sibling, &cpu_topology[cpu].llc_sibling);
}

static void cdh_restore_cpu_topology(unsigned int cpu)
{
	const struct cdh_topology_saved *saved = &cdh_saved_topology[cpu];

	cpu_topology[cpu].thread_id = saved->thread_id;
	cpu_topology[cpu].core_id = saved->core_id;
#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 1, 0)
	cpu_topology[cpu].cluster_id = saved->cluster_id;
#endif
	cpu_topology[cpu].package_id = saved->package_id;
	cpumask_copy(&cpu_topology[cpu].thread_sibling, &saved->thread_sibling);
	cpumask_copy(&cpu_topology[cpu].core_sibling, &saved->core_sibling);
#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 1, 0)
	cpumask_copy(&cpu_topology[cpu].cluster_sibling, &saved->cluster_sibling);
#endif
	cpumask_copy(&cpu_topology[cpu].llc_sibling, &saved->llc_sibling);
}

static void cdh_set_cluster_mask(cpumask_t *mask, unsigned int first, unsigned int last)
{
	unsigned int cpu;

	cpumask_clear(mask);
	for (cpu = first; cpu <= last; cpu++)
		cpumask_set_cpu(cpu, mask);
}

static void cdh_apply_cpu_topology(unsigned int cpu)
{
	const struct cdh_cpu_core_spec *spec = cdh_get_cpu_core_spec(cpu);
	bool pkg_per_cluster = cdh_is_package_per_cluster();
	unsigned int first = spec ? spec->cluster_first_cpu : ((cpu < 6) ? 0 : 6);
	unsigned int last  = spec ? spec->cluster_last_cpu  : ((cpu < 6) ? 5 : 7);
	unsigned int cid   = spec ? spec->cluster_id        : ((cpu < 6) ? 0 : 1);

	/*
	 * topology sysfs 文件直接读 cpu_topology[]，因此修改这份 canonical
	 * source 能同时覆盖 physical_package_id、cluster_id 和 sibling mask。
	 *   - package_per_cluster=false（默认，如 MT6833 6.6）：单 package_id=0、
	 *     core_siblings=ff，各簇分属 cluster_id 0/1..；
	 *   - package_per_cluster=true（如 Tensor GS101 5.10）：每簇独立
	 *     physical_package_id=0/1/2，core_siblings/package_cpus=本簇掩码。
	 */
	cpu_topology[cpu].package_id = pkg_per_cluster ? (int)cid : 0;
#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 1, 0)
	cpu_topology[cpu].cluster_id = (int)cid;
#endif
	cpu_topology[cpu].core_id = (cpu >= first) ? (int)(cpu - first) : (int)cpu;
	cpu_topology[cpu].thread_id = -1;
	cpumask_clear(&cpu_topology[cpu].thread_sibling);
	cpumask_set_cpu(cpu, &cpu_topology[cpu].thread_sibling);
	if (pkg_per_cluster)
		cdh_set_cluster_mask(&cpu_topology[cpu].core_sibling, first, last);
	else
		cdh_set_cluster_mask(&cpu_topology[cpu].core_sibling, 0, 7);
#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 1, 0)
	cdh_set_cluster_mask(&cpu_topology[cpu].cluster_sibling, first, last);
#endif
	cdh_set_cluster_mask(&cpu_topology[cpu].llc_sibling, 0, 7);
}

int cdh_cpu_topology_init(void)
{
	unsigned int cpu;

	(void)cdh_cpufreq_load_once(NULL);

	for (cpu = 0; cpu < ARRAY_SIZE(cdh_saved_topology); cpu++) {
		cdh_save_cpu_topology(cpu);
		cdh_apply_cpu_topology(cpu);
	}
	cdh_topology_registered = true;
	pr_info("updated cpu_topology for  package and cluster ids\n");
	return 0;
}

void cdh_cpu_topology_exit(void)
{
	unsigned int cpu;

	if (!cdh_topology_registered)
		return;
	for (cpu = 0; cpu < ARRAY_SIZE(cdh_saved_topology); cpu++)
		cdh_restore_cpu_topology(cpu);
	cdh_topology_registered = false;
	pr_info("restored cpu_topology values\n");
}
