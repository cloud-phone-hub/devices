// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2025-2026 fei_cong(https://github.com/feicong/feicong-course)
 */
#define pr_fmt(fmt) "cdh_device_info_midr: " fmt

#include <asm/cpu.h>
#include <linux/compiler.h>
#include <linux/cpu.h>
#include <linux/errno.h>
#include <linux/kernel.h>
#include <linux/kprobes.h>
#include <linux/percpu.h>

#include "cdh_device_info.h"

typedef unsigned long (*cdh_kallsyms_lookup_name_t)(const char *name);

/*
 * cpu_data is the canonical source used by arm64 midr_el1/revidr_el1 sysfs, but
 * Android GKI does not export it as KMI. Resolve it explicitly and fail module
 * load if the private symbol path is unavailable; no auxiliary-node fallback is
 * accepted for AIDA64-visible MIDR alignment.
 */
static struct cpuinfo_arm64 __percpu *cdh_cpu_data;
static u64 cdh_saved_midr[CDH_CPU_CORES_MAX];
static u64 cdh_saved_revidr[CDH_CPU_CORES_MAX];
static bool cdh_midr_registered;

static unsigned long cdh_lookup_private_symbol(const char *name)
{
	cdh_kallsyms_lookup_name_t lookup;
	struct kprobe kp = {
		.symbol_name = "kallsyms_lookup_name",
	};
	int ret;

	/* kprobe is only used as a resolver for private GKI symbols; no runtime hook is installed. */
	ret = register_kprobe(&kp);
	if (ret)
		return 0;
	lookup = (cdh_kallsyms_lookup_name_t)kp.addr;
	unregister_kprobe(&kp);
	if (!lookup)
		return 0;
	return lookup(name);
}

static struct cpuinfo_arm64 *cdh_cpuinfo(unsigned int cpu)
{
	if (!cdh_cpu_data || cpu >= nr_cpu_ids)
		return NULL;
	return per_cpu_ptr(cdh_cpu_data, cpu);
}

static void cdh_restore_midr_values(size_t count)
{
	size_t cpu;

	for (cpu = 0; cpu < count; cpu++) {
		struct cpuinfo_arm64 *info = cdh_cpuinfo((unsigned int)cpu);

		if (info) {
			WRITE_ONCE(info->reg_midr,
				   (typeof(info->reg_midr))cdh_saved_midr[cpu]);
			WRITE_ONCE(info->reg_revidr,
				   (typeof(info->reg_revidr))cdh_saved_revidr[cpu]);
		}
	}
}

int cdh_cpu_midr_sysfs_init(void)
{
	unsigned long cpu_data_addr;
	unsigned int cpu;

	(void)cdh_cpufreq_load_once(NULL);

	cpu_data_addr = cdh_lookup_private_symbol("cpu_data");
	if (!cpu_data_addr) {
		pr_err("failed to resolve cpu_data private symbol\n");
		return -ENOENT;
	}
	cdh_cpu_data = (struct cpuinfo_arm64 __percpu *)cpu_data_addr;

	for (cpu = 0; cpu < CDH_CPU_CORES_MAX; cpu++) {
		const struct cdh_cpu_core_spec *spec = cdh_get_cpu_core_spec(cpu);
		struct cpuinfo_arm64 *info = cdh_cpuinfo(cpu);

		if (!info || !spec) {
			pr_err("missing cpuinfo for cpu%u\n", cpu);
			cdh_restore_midr_values(cpu);
			return -ENODEV;
		}
		/* midr_el1/revidr_el1 sysfs show reads reg_midr/reg_revidr live, so update in place. */
		cdh_saved_midr[cpu] = READ_ONCE(info->reg_midr);
		cdh_saved_revidr[cpu] = READ_ONCE(info->reg_revidr);
		WRITE_ONCE(info->reg_midr, (typeof(info->reg_midr))spec->midr);
		WRITE_ONCE(info->reg_revidr, (typeof(info->reg_revidr))spec->revidr);
	}
	cdh_midr_registered = true;
	pr_info("updated cpu_data reg_midr/reg_revidr for MIDR sysfs\n");
	return 0;
}

void cdh_cpu_midr_sysfs_exit(void)
{
	if (!cdh_midr_registered)
		return;
	/* Restore the original KVM/QEMU MIDR values when the module is unloaded or rolled back. */
	cdh_restore_midr_values(CDH_CPU_CORES_MAX);
	cdh_midr_registered = false;
	pr_info("restored cpu_data reg_midr/reg_revidr values\n");
}
