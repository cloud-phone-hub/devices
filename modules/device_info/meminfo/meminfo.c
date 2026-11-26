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

#define CDH_GOLD_8GB_MEMTOTAL_KB	7760724UL
#define CDH_SWAP_TOTAL_KB		6291452UL
#define CDH_GOLD_8GB_COMMIT_LIMIT_KB	10171812UL
#define CDH_CMA_TOTAL_KB		344064UL
#define CDH_VMALLOC_TOTAL_KB		257687552UL
#define CDH_MIN_USED_HEADROOM_KB	262144UL

struct cdh_meminfo_target {
	const char *name;
	unsigned long kb;
	bool si_meminfo;
};

static const struct cdh_meminfo_target cdh_meminfo_targets[] = {
	{ "MemTotal:", CDH_GOLD_8GB_MEMTOTAL_KB, true },
	{ "SwapTotal:", CDH_SWAP_TOTAL_KB, false },
	{ "CommitLimit:", CDH_GOLD_8GB_COMMIT_LIMIT_KB, false },
	{ "CmaTotal:", CDH_CMA_TOTAL_KB, false },
	{ "VmallocTotal:", CDH_VMALLOC_TOTAL_KB, false },
};

static bool cdh_meminfo_proc_registered;
static bool cdh_si_meminfo_registered;

static unsigned long cdh_kb_to_pages(unsigned long kb)
{
	return kb >> (PAGE_SHIFT - 10);
}

static unsigned long cdh_pages_to_kb(unsigned long pages)
{
	return pages << (PAGE_SHIFT - 10);
}

/*
 * 按 QEMU 实际分配的物理内存容量动态推导真机可见 MemTotal：
 *   - 8GB 档（原始可用页 6.5GiB ~ 9.0GiB）严格保持 gold 真机实测值 7760724 kB；
 *   - 其它档位（如 4GB / 6GB / 12GB / 16GB / 24GB）按同比例扣除 SoC 固件保留页
 *     （约 4.6% 额外扣减，使 16GB 实例落在 ~15640000 kB），避免将 16GB/24GB 实例
 *     错压成 8GB 并消除 MemAvailable > MemTotal 的容量倒挂异常。
 */
static unsigned long cdh_resolve_target_memtotal_kb(unsigned long raw_kb)
{
	if (raw_kb == 0)
		return CDH_GOLD_8GB_MEMTOTAL_KB;
	if (raw_kb >= 6815744UL && raw_kb <= 9437184UL)
		return CDH_GOLD_8GB_MEMTOTAL_KB;
	return (raw_kb * CDH_GOLD_8GB_MEMTOTAL_KB / 8130560UL) & ~3UL;
}

static unsigned long cdh_meminfo_read_line_kb(struct seq_file *m, const char *name)
{
	size_t name_len = strlen(name);
	size_t i;

	for (i = 0; i + name_len < m->count; i++) {
		unsigned long val = 0;
		bool found_digit = false;
		size_t p;

		if (i > 0 && m->buf[i - 1] != '\n')
			continue;
		if (memcmp(m->buf + i, name, name_len) != 0)
			continue;
		p = i + name_len;
		while (p < m->count && m->buf[p] == ' ')
			p++;
		while (p < m->count && m->buf[p] >= '0' && m->buf[p] <= '9') {
			val = val * 10 + (unsigned long)(m->buf[p] - '0');
			found_digit = true;
			p++;
		}
		return found_digit ? val : 0;
	}
	return 0;
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
	unsigned long raw_kb;
	unsigned long target_kb;
	unsigned long max_free_pages;

	if (!totalram)
		return;
	raw_kb = cdh_pages_to_kb(*totalram);
	target_kb = cdh_resolve_target_memtotal_kb(raw_kb);
	*totalram = cdh_kb_to_pages(target_kb);
	if (freeram) {
		max_free_pages = *totalram;
		if (target_kb > CDH_MIN_USED_HEADROOM_KB)
			max_free_pages = cdh_kb_to_pages(target_kb - CDH_MIN_USED_HEADROOM_KB);
		if (*freeram > max_free_pages)
			*freeram = max_free_pages;
	}
}

static void cdh_meminfo_proc_show(void *data, struct seq_file *m)
{
	unsigned long mem_total_kb = cdh_meminfo_read_line_kb(m, "MemTotal:");
	unsigned long mem_avail_kb = cdh_meminfo_read_line_kb(m, "MemAvailable:");
	unsigned long mem_free_kb = cdh_meminfo_read_line_kb(m, "MemFree:");
	unsigned long raw_swap_total_kb = cdh_meminfo_read_line_kb(m, "SwapTotal:");
	unsigned long raw_swap_free_kb = cdh_meminfo_read_line_kb(m, "SwapFree:");
	unsigned long swap_used_kb = 0;
	unsigned long target_swap_free_kb;
	size_t i;

	if (mem_total_kb == 0)
		mem_total_kb = CDH_GOLD_8GB_MEMTOTAL_KB;

	if (mem_total_kb > CDH_MIN_USED_HEADROOM_KB) {
		unsigned long cap_avail = mem_total_kb - CDH_MIN_USED_HEADROOM_KB;

		if (mem_avail_kb > cap_avail)
			cdh_meminfo_replace_line(m, "MemAvailable:", cap_avail);
		if (mem_free_kb > cap_avail)
			cdh_meminfo_replace_line(m, "MemFree:", cap_avail);
	}

	if (raw_swap_total_kb > raw_swap_free_kb)
		swap_used_kb = raw_swap_total_kb - raw_swap_free_kb;
	target_swap_free_kb = (CDH_SWAP_TOTAL_KB > swap_used_kb) ?
			      (CDH_SWAP_TOTAL_KB - swap_used_kb) : 0;

	for (i = 0; i < ARRAY_SIZE(cdh_meminfo_targets); i++) {
		const struct cdh_meminfo_target *target = &cdh_meminfo_targets[i];
		unsigned long kb = target->kb;

		if (target->si_meminfo)
			continue;
		if (!strcmp(target->name, "CommitLimit:") && mem_total_kb != CDH_GOLD_8GB_MEMTOTAL_KB)
			kb = (mem_total_kb / 2) + CDH_SWAP_TOTAL_KB;
		cdh_meminfo_replace_line(m, target->name, kb);
	}
	cdh_meminfo_replace_line(m, "SwapFree:", target_swap_free_kb);
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
