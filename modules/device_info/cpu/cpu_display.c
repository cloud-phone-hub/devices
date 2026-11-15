// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2025-2026 fei_cong(https://github.com/feicong/feicong-course)
 */
#define pr_fmt(fmt) "cdh_device_info_cpu: " fmt

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/seq_file.h>
#include <linux/version.h>
#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 6, 0)
#include <trace/hooks/cpuinfo.h>
#include <trace/hooks/topology.h>
#else
#include <linux/arch_topology.h>
#include <linux/kprobes.h>
#include <linux/percpu.h>
#include <linux/ptrace.h>
#endif

#include "cdh_device_info.h"

static const char cdh_cpu_features[] =
	"fp asimd evtstrm aes pmull sha1 sha2 crc32 atomics fphp asimdhp cpuid asimdrdm lrcpc dcpop asimddp";

static bool cdh_cpuinfo_registered;
static bool cdh_cpu_capacity_registered;

static void cdh_cpuinfo_show(void *data, struct seq_file *m)
{
	unsigned int cpu;

	m->count = 0;
	m->from = 0;
	for (cpu = 0; cpu < CDH_CPU_CORES_MAX; cpu++) {
		const struct cdh_cpu_core_spec *core = cdh_get_cpu_core_spec(cpu);

		if (!core)
			continue;
		seq_printf(m, "processor\t: %u\n", core->cpu);
		seq_printf(m, "BogoMIPS\t: %s\n", core->bogo_mips);
		seq_printf(m, "Features\t: %s\n", cdh_cpu_features);
		seq_printf(m, "CPU implementer\t: %s\n", core->implementer);
		seq_puts(m, "CPU architecture: 8\n");
		seq_printf(m, "CPU variant\t: 0x%x\n", core->variant);
		seq_printf(m, "CPU part\t: %s\n", core->part);
		seq_printf(m, "CPU revision\t: %u\n\n", core->revision);
	}
	/* 真机 /proc/cpuinfo 无 Hardware 行，保持与真机一致不输出。 */
}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 6, 0)
static bool cdh_cpu_display_pinned;

static void cdh_cpu_capacity_show(void *data, unsigned long *capacity, int cpu)
{
	const struct cdh_cpu_core_spec *core;

	if (!capacity || cpu < 0)
		return;
	core = cdh_get_cpu_core_spec((unsigned int)cpu);
	if (!core)
		return;
	*capacity = core->capacity;
}

int cdh_cpu_display_init(void)
{
	int ret;

	(void)cdh_cpufreq_load_once(NULL);

	ret = register_trace_android_rvh_cpuinfo_c_show(cdh_cpuinfo_show, NULL);
	if (ret)
		return ret;
	cdh_cpuinfo_registered = true;

	ret = register_trace_android_rvh_cpu_capacity_show(cdh_cpu_capacity_show, NULL);
	if (ret) {
		pr_err("registered /proc/cpuinfo hook but cpu_capacity hook failed:%d\n", ret);
		__module_get(THIS_MODULE);
		cdh_cpu_display_pinned = true;
		return 0;
	}
	cdh_cpu_capacity_registered = true;

	__module_get(THIS_MODULE);
	cdh_cpu_display_pinned = true;
	pr_info("registered permanent /proc/cpuinfo and cpu_capacity display hooks\n");
	return 0;
}

void cdh_cpu_display_exit(void)
{
	if (!cdh_cpuinfo_registered && !cdh_cpu_capacity_registered)
		return;

	pr_info("restricted CPU display hooks are permanent; pinned=%d\n",
		cdh_cpu_display_pinned);
}
#else /* LINUX_VERSION_CODE < KERNEL_VERSION(6, 6, 0) */
typedef unsigned long (*cdh_kallsyms_lookup_name_t)(const char *name);

struct cdh_cpuinfo_proc_data {
	struct seq_file *seq;
};

static unsigned long cdh_saved_cpu_scale[CDH_CPU_CORES_MAX];

static unsigned long cdh_lookup_cpuinfo_symbol(const char *name)
{
	cdh_kallsyms_lookup_name_t lookup;
	struct kprobe kp = {
		.symbol_name = "kallsyms_lookup_name",
	};
	int ret;

	ret = register_kprobe(&kp);
	if (ret)
		return 0;
	lookup = (cdh_kallsyms_lookup_name_t)kp.addr;
	unregister_kprobe(&kp);
	if (!lookup)
		return 0;
	return lookup(name);
}

static int cdh_cpuinfo_entry(struct kretprobe_instance *ri, struct pt_regs *regs)
{
	struct cdh_cpuinfo_proc_data *data = (struct cdh_cpuinfo_proc_data *)ri->data;

#ifdef CONFIG_ARM64
	data->seq = (struct seq_file *)regs->regs[0];
	return 0;
#else
	data->seq = NULL;
	return -EOPNOTSUPP;
#endif
}

static int cdh_cpuinfo_ret(struct kretprobe_instance *ri, struct pt_regs *regs)
{
	struct cdh_cpuinfo_proc_data *data = (struct cdh_cpuinfo_proc_data *)ri->data;

	if (regs_return_value(regs) != 0 || !data->seq)
		return 0;
	cdh_cpuinfo_show(NULL, data->seq);
	return 0;
}

static struct kretprobe cdh_cpuinfo_kretprobe = {
	.handler = cdh_cpuinfo_ret,
	.entry_handler = cdh_cpuinfo_entry,
	.data_size = sizeof(struct cdh_cpuinfo_proc_data),
	.maxactive = 32,
};

int cdh_cpu_display_init(void)
{
	const struct seq_operations *op;
	unsigned int cpu;
	int ret;

	(void)cdh_cpufreq_load_once(NULL);

	/*
	 * 内核里存在两个静态 c_show（arch/arm64/kernel/cpuinfo.c 与 crypto/proc.c），
	 * 通过唯一导出的 cpuinfo_op.show 精确挂钩 /proc/cpuinfo。
	 */
	op = (const struct seq_operations *)cdh_lookup_cpuinfo_symbol("cpuinfo_op");
	if (!op || !op->show) {
		pr_err("failed to resolve cpuinfo_op.show\n");
		return -ENOENT;
	}
	cdh_cpuinfo_kretprobe.kp.addr = (kprobe_opcode_t *)op->show;
	ret = register_kretprobe(&cdh_cpuinfo_kretprobe);
	if (ret) {
		pr_err("failed to register cpuinfo_op.show kretprobe:%d\n", ret);
		return ret;
	}
	cdh_cpuinfo_registered = true;

	for (cpu = 0; cpu < CDH_CPU_CORES_MAX; cpu++) {
		const struct cdh_cpu_core_spec *core = cdh_get_cpu_core_spec(cpu);

		if (!core || cpu >= nr_cpu_ids)
			continue;
		cdh_saved_cpu_scale[cpu] = READ_ONCE(per_cpu(cpu_scale, cpu));
		WRITE_ONCE(per_cpu(cpu_scale, cpu), core->capacity);
	}
	cdh_cpu_capacity_registered = true;
	pr_info("registered /proc/cpuinfo kretprobe and cpu_scale display values\n");
	return 0;
}

void cdh_cpu_display_exit(void)
{
	unsigned int cpu;

	if (cdh_cpu_capacity_registered) {
		for (cpu = 0; cpu < CDH_CPU_CORES_MAX; cpu++) {
			if (cpu >= nr_cpu_ids)
				continue;
			WRITE_ONCE(per_cpu(cpu_scale, cpu), cdh_saved_cpu_scale[cpu]);
		}
		cdh_cpu_capacity_registered = false;
	}
	if (cdh_cpuinfo_registered) {
		unregister_kretprobe(&cdh_cpuinfo_kretprobe);
		cdh_cpuinfo_registered = false;
	}
	pr_info("unregistered /proc/cpuinfo kretprobe and restored cpu_scale\n");
}
#endif
