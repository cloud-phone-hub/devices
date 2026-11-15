// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2025-2026 fei_cong(https://github.com/feicong/feicong-course)
 */
#define pr_fmt(fmt) "cdh_device_info_meminfo: " fmt

#include <linux/kernel.h>
#include <linux/mm.h>
#include <linux/seq_file.h>
#include <linux/string.h>
#include <linux/version.h>
#include <trace/hooks/mm.h>
#if LINUX_VERSION_CODE < KERNEL_VERSION(6, 1, 0)
#include <linux/kprobes.h>
#include <linux/ptrace.h>
#include <linux/sysinfo.h>
#endif

#include "cdh_device_info.h"

struct cdh_meminfo_target {
	const char *name;
	unsigned long kb;
	bool si_meminfo;
};

static const struct cdh_meminfo_target cdh_meminfo_targets[] = {
	{ "MemTotal:", 7760724, true },
	{ "SwapTotal:", 6291452, false },
	{ "CommitLimit:", 10171812, false },
	{ "CmaTotal:", 344064, false },
	{ "VmallocTotal:", 257687552, false },
};

static bool cdh_meminfo_proc_registered;
static bool cdh_si_meminfo_registered;

static unsigned long cdh_kb_to_pages(unsigned long kb)
{
	return kb >> (PAGE_SHIFT - 10);
}

static const struct cdh_meminfo_target *cdh_meminfo_target(const char *name)
{
	size_t i;

	for (i = 0; i < ARRAY_SIZE(cdh_meminfo_targets); i++) {
		if (!strcmp(cdh_meminfo_targets[i].name, name))
			return &cdh_meminfo_targets[i];
	}
	return NULL;
}

static bool cdh_meminfo_replace_line(struct seq_file *m, const char *name, unsigned long kb)
{
	char line[64];
	size_t line_len;
	size_t i;

	line_len = scnprintf(line, sizeof(line), "%-16s%8lu kB\n", name, kb);
	if (line_len >= sizeof(line))
		return false;

	for (i = 0; i + strlen(name) < m->count; i++) {
		size_t old_len = 0;

		if (i > 0 && m->buf[i - 1] != '\n')
			continue;
		if (memcmp(m->buf + i, name, strlen(name)) != 0)
			continue;
		while (i + old_len < m->count && m->buf[i + old_len] != '\n')
			old_len++;
		if (i + old_len < m->count)
			old_len++;
		if (old_len != line_len) {
			pr_warn_once("cannot replace %s old_len=%zu new_len=%zu\n",
				     name, old_len, line_len);
			return false;
		}
		memcpy(m->buf + i, line, line_len);
		return true;
	}
	pr_warn_once("cannot find %s line in /proc/meminfo\n", name);
	return false;
}

static void cdh_si_meminfo_adjust(void *data, unsigned long *totalram,
					 unsigned long *freeram)
{
	const struct cdh_meminfo_target *target;

	if (!totalram)
		return;
	target = cdh_meminfo_target("MemTotal:");
	if (!target)
		return;
	*totalram = cdh_kb_to_pages(target->kb);
	if (freeram && *freeram > *totalram)
		*freeram = *totalram;
}

static void cdh_meminfo_proc_show(void *data, struct seq_file *m)
{
	size_t i;

	for (i = 0; i < ARRAY_SIZE(cdh_meminfo_targets); i++) {
		const struct cdh_meminfo_target *target = &cdh_meminfo_targets[i];

		if (target->si_meminfo)
			continue;
		cdh_meminfo_replace_line(m, target->name, target->kb);
	}
}

#if LINUX_VERSION_CODE < KERNEL_VERSION(6, 1, 0)
struct cdh_si_meminfo_data {
	struct sysinfo *info;
};

static int cdh_si_meminfo_entry(struct kretprobe_instance *ri, struct pt_regs *regs)
{
	struct cdh_si_meminfo_data *data = (struct cdh_si_meminfo_data *)ri->data;

#ifdef CONFIG_ARM64
	data->info = (struct sysinfo *)regs->regs[0];
	return 0;
#else
	data->info = NULL;
	return -EOPNOTSUPP;
#endif
}

static int cdh_si_meminfo_ret(struct kretprobe_instance *ri, struct pt_regs *regs)
{
	struct cdh_si_meminfo_data *data = (struct cdh_si_meminfo_data *)ri->data;

	if (!data->info)
		return 0;
	cdh_si_meminfo_adjust(NULL, &data->info->totalram, &data->info->freeram);
	return 0;
}

static struct kretprobe cdh_si_meminfo_kretprobe = {
	.handler = cdh_si_meminfo_ret,
	.entry_handler = cdh_si_meminfo_entry,
	.data_size = sizeof(struct cdh_si_meminfo_data),
	.maxactive = 64,
	.kp = {
		.symbol_name = "si_meminfo",
	},
};
#endif

int cdh_meminfo_init(void)
{
	int ret;

#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 1, 0)
	ret = register_trace_android_vh_si_meminfo_adjust(cdh_si_meminfo_adjust, NULL);
#else
	ret = register_kretprobe(&cdh_si_meminfo_kretprobe);
#endif
	if (ret)
		return ret;
	cdh_si_meminfo_registered = true;

	ret = register_trace_android_vh_meminfo_proc_show(cdh_meminfo_proc_show, NULL);
	if (ret) {
#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 1, 0)
		unregister_trace_android_vh_si_meminfo_adjust(cdh_si_meminfo_adjust, NULL);
#else
		unregister_kretprobe(&cdh_si_meminfo_kretprobe);
#endif
		cdh_si_meminfo_registered = false;
		return ret;
	}

	cdh_meminfo_proc_registered = true;
	pr_info("registered si_meminfo and /proc/meminfo display hooks\n");
	return 0;
}

void cdh_meminfo_exit(void)
{
	if (cdh_meminfo_proc_registered) {
		unregister_trace_android_vh_meminfo_proc_show(cdh_meminfo_proc_show, NULL);
		cdh_meminfo_proc_registered = false;
	}
	if (cdh_si_meminfo_registered) {
#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 1, 0)
		unregister_trace_android_vh_si_meminfo_adjust(cdh_si_meminfo_adjust, NULL);
#else
		unregister_kretprobe(&cdh_si_meminfo_kretprobe);
#endif
		cdh_si_meminfo_registered = false;
	}
	pr_info("unregistered si_meminfo and /proc/meminfo display hooks\n");
}
