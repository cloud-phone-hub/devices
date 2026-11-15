// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2025-2026 fei_cong(https://github.com/feicong/feicong-course)
 *
 * cpufreq sysfs 展示层：
 *   1) 从伴随文件 /mnt/vendorpatch/vendor_dlkm/lib/modules/cdh_device_info.cpufreq
 *      （回退 /vendor_dlkm/lib/modules/cdh_device_info.cpufreq）或模块参数 cpufreq
 *      加载机型频点簇配置；若未提供伴随文件则回退内置默认双簇（policy0: cpu0-5, policy6: cpu6-7）。
 *   2) 解析内核私有符号 cpufreq_global_kobject（即 /sys/devices/system/cpu/cpufreq），
 *      在其下创建 policy<N> 目录（含 stats/ 子组），并通过 sysfs_create_link 在
 *      /sys/devices/system/cpu/cpu<M>/cpufreq 下建立指向 ../cpufreq/policy<N> 的符号链接，
 *      使普通 App（untrusted_app）既能 list /sys/devices/system/cpu/cpufreq 看到
 *      policy0/policy6，又能沿 cpuN/cpufreq 软链读取各簇频点。
 */
#define pr_fmt(fmt) "cdh_device_info_cpufreq: " fmt

#include <linux/cpu.h>
#include <linux/device.h>
#include <linux/errno.h>
#include <linux/fs.h>
#include <linux/jiffies.h>
#include <linux/kernel.h>
#include <linux/kernel_read_file.h>
#include <linux/kobject.h>
#include <linux/kprobes.h>
#include <linux/ktime.h>
#include <linux/limits.h>
#include <linux/moduleparam.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/sysfs.h>
#include <linux/vmalloc.h>

#include "cdh_device_info.h"

#define CDH_CPUFREQ_MAX_POLICIES 8
#define CDH_CPUFREQ_MAX_CPUS     32
#define CDH_CPUFREQ_MAX_FREQS    64
#define CDH_CPUFREQ_STR_MAX      64
#define CDH_CPUFREQ_GOVS_MAX     128
#define CDH_CPUFREQ_CPUS_STR_MAX 128
#define CDH_CPUFREQ_FILE_MAX     (64 * 1024)

static char *cdh_cpufreq_path;
module_param_named(cpufreq_path, cdh_cpufreq_path, charp, 0444);
MODULE_PARM_DESC(cpufreq_path, "cpufreq 伴随文件路径（缺省 " CDH_DEFAULT_CPUFREQ_PATH "）");

static char *cdh_cpufreq_inline;
module_param_named(cpufreq, cdh_cpufreq_inline, charp, 0444);
MODULE_PARM_DESC(cpufreq, "调试用内联 cpufreq 配置文本（非空时忽略 cpufreq_path）");

struct cdh_cpufreq_policy {
	unsigned int policy_id;
	unsigned int cpus[CDH_CPUFREQ_MAX_CPUS];
	unsigned int nr_cpus;
	u32 min_freq;
	u32 max_freq;
	u32 cur_freq;
	u32 transition_latency;
	char driver[CDH_CPUFREQ_STR_MAX];
	char governor[CDH_CPUFREQ_STR_MAX];
	char available_governors[CDH_CPUFREQ_GOVS_MAX];
	char cpus_str[CDH_CPUFREQ_CPUS_STR_MAX];
	u32 freqs[CDH_CPUFREQ_MAX_FREQS];
	unsigned int nr_freqs;
	char bogo_mips[16];
	char implementer[16];
	unsigned int variant;
	bool has_variant;
	char part[16];
	unsigned int revision;
	bool has_revision;
	unsigned long capacity;
	bool has_capacity;
	u64 midr;
	bool has_midr;
	u64 revidr;
	bool has_revidr;
	struct kobject *kobj;
	struct kobject *gov_kobj;
	u32 linked_cpus_mask;
	bool group_created;
	bool stats_created;
};

static struct cdh_cpufreq_policy cdh_policies[CDH_CPUFREQ_MAX_POLICIES];
static unsigned int cdh_nr_policies;
static struct cdh_cpu_core_spec cdh_core_specs[CDH_CPU_CORES_MAX];
static bool cdh_scaling_readable;
static bool cdh_package_per_cluster;
static bool cdh_cpufreq_loaded;
static int cdh_cpufreq_load_ret;
static const char *cdh_cpufreq_used_source = "builtin-default";

typedef unsigned long (*cdh_kallsyms_lookup_name_t)(const char *name);

static unsigned long cdh_cpufreq_lookup_symbol(const char *name)
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

static char *cdh_trim_left(char *s)
{
	while (*s == ' ' || *s == '\t' || *s == '\r')
		s++;
	return s;
}

static void cdh_trim_right(char *s)
{
	size_t len = strlen(s);

	while (len > 0 && (s[len - 1] == ' ' || s[len - 1] == '\t' ||
			   s[len - 1] == '\r' || s[len - 1] == '\n'))
		s[--len] = '\0';
}

static char *cdh_next_token(char **cursor)
{
	char *start, *walk;

	if (!cursor || !*cursor)
		return NULL;
	start = cdh_trim_left(*cursor);
	if (!*start) {
		*cursor = start;
		return NULL;
	}
	walk = start;
	while (*walk && *walk != ' ' && *walk != '\t' && *walk != '\r')
		walk++;
	if (*walk) {
		*walk++ = '\0';
		walk = cdh_trim_left(walk);
	}
	*cursor = walk;
	return start;
}

static void cdh_format_cpus_str(struct cdh_cpufreq_policy *p)
{
	size_t off = 0;
	unsigned int i;

	p->cpus_str[0] = '\0';
	for (i = 0; i < p->nr_cpus; i++) {
		int n = scnprintf(p->cpus_str + off,
				  sizeof(p->cpus_str) - off,
				  "%s%u", i ? " " : "", p->cpus[i]);
		if (n <= 0)
			break;
		off += n;
	}
}

static void cdh_init_core_specs_defaults(void)
{
	unsigned int cpu;

	cdh_scaling_readable = false;
	cdh_package_per_cluster = false;

	for (cpu = 0; cpu < CDH_CPU_CORES_MAX; cpu++) {
		struct cdh_cpu_core_spec *s = &cdh_core_specs[cpu];

		memset(s, 0, sizeof(*s));
		s->cpu = cpu;
		strscpy(s->bogo_mips, "26.00", sizeof(s->bogo_mips));
		strscpy(s->implementer, "0x41", sizeof(s->implementer));
		if (cpu < 6) {
			s->variant = 0x2;
			strscpy(s->part, "0xd05", sizeof(s->part));
			s->revision = 0;
			s->capacity = 356;
			s->midr = 0x00000000412fd050ULL;
			s->revidr = 0;
			s->cluster_id = 0;
			s->cluster_first_cpu = 0;
			s->cluster_last_cpu = 5;
		} else {
			s->variant = 0x4;
			strscpy(s->part, "0xd0b", sizeof(s->part));
			s->revision = 0;
			s->capacity = 1024;
			s->midr = 0x00000000414fd0b0ULL;
			s->revidr = 0;
			s->cluster_id = 1;
			s->cluster_first_cpu = 6;
			s->cluster_last_cpu = 7;
		}
	}
}

static void cdh_init_policy_defaults(struct cdh_cpufreq_policy *p,
				     unsigned int policy_id,
				     const char *driver,
				     const char *governor,
				     const char *avail_govs,
				     u32 latency,
				     const char *bogo_mips)
{
	memset(p, 0, sizeof(*p));
	p->policy_id = policy_id;
	p->transition_latency = latency;
	strscpy(p->driver, driver, sizeof(p->driver));
	strscpy(p->governor, governor, sizeof(p->governor));
	strscpy(p->available_governors, avail_govs, sizeof(p->available_governors));
	if (bogo_mips && *bogo_mips)
		strscpy(p->bogo_mips, bogo_mips, sizeof(p->bogo_mips));
}

static u64 cdh_synthesize_midr(const struct cdh_cpu_core_spec *s)
{
	u32 impl = 0x41, part_num = 0xd05;

	(void)kstrtou32(s->implementer, 0, &impl);
	(void)kstrtou32(s->part, 0, &part_num);
	return ((u64)(impl & 0xffU) << 24) |
	       ((u64)(s->variant & 0xfU) << 20) |
	       (0xfULL << 16) |
	       ((u64)(part_num & 0xfffU) << 4) |
	       (u64)(s->revision & 0xfU);
}

static int cdh_finalize_policies(void)
{
	unsigned int i, j;

	if (!cdh_nr_policies)
		return -EINVAL;

	for (i = 0; i < cdh_nr_policies; i++) {
		struct cdh_cpufreq_policy *p = &cdh_policies[i];
		unsigned int first_cpu = 0, last_cpu = 0;
		u32 dmin = 0, dmax = 0;

		if (!p->nr_cpus)
			return -EINVAL;
		cdh_format_cpus_str(p);

		first_cpu = p->cpus[0];
		last_cpu = p->cpus[0];
		for (j = 1; j < p->nr_cpus; j++) {
			if (p->cpus[j] < first_cpu)
				first_cpu = p->cpus[j];
			if (p->cpus[j] > last_cpu)
				last_cpu = p->cpus[j];
		}

		for (j = 0; j < p->nr_freqs; j++) {
			if (!dmin || p->freqs[j] < dmin)
				dmin = p->freqs[j];
			if (p->freqs[j] > dmax)
				dmax = p->freqs[j];
		}
		if (!p->min_freq)
			p->min_freq = dmin;
		if (!p->max_freq)
			p->max_freq = dmax;
		if (!p->min_freq || !p->max_freq || p->min_freq > p->max_freq)
			return -EINVAL;
		if (!p->cur_freq)
			p->cur_freq = p->max_freq;
		if (!p->nr_freqs) {
			p->freqs[0] = p->max_freq;
			p->nr_freqs = 1;
			if (p->min_freq != p->max_freq) {
				p->freqs[1] = p->min_freq;
				p->nr_freqs = 2;
			}
		}
		if (!p->available_governors[0])
			strscpy(p->available_governors, p->governor,
				sizeof(p->available_governors));

		for (j = 0; j < p->nr_cpus; j++) {
			unsigned int cpu = p->cpus[j];
			struct cdh_cpu_core_spec *s;
			bool arch_changed = false;

			if (cpu >= CDH_CPU_CORES_MAX)
				continue;
			s = &cdh_core_specs[cpu];
			s->cluster_id = i;
			s->cluster_first_cpu = first_cpu;
			s->cluster_last_cpu = last_cpu;
			if (p->bogo_mips[0])
				strscpy(s->bogo_mips, p->bogo_mips, sizeof(s->bogo_mips));
			if (p->implementer[0]) {
				strscpy(s->implementer, p->implementer, sizeof(s->implementer));
				arch_changed = true;
			}
			if (p->has_variant) {
				s->variant = p->variant;
				arch_changed = true;
			}
			if (p->part[0]) {
				strscpy(s->part, p->part, sizeof(s->part));
				arch_changed = true;
			}
			if (p->has_revision) {
				s->revision = p->revision;
				arch_changed = true;
			}
			if (p->has_capacity)
				s->capacity = p->capacity;
			if (p->has_midr)
				s->midr = p->midr;
			else if (arch_changed)
				s->midr = cdh_synthesize_midr(s);
			if (p->has_revidr)
				s->revidr = p->revidr;
		}
	}
	return 0;
}

static void cdh_load_builtin_defaults(void)
{
	static const u32 gold_p0_freqs[] = {
		2000000, 1916000, 1812000, 1750000, 1645000, 1500000, 1393000, 1287000,
		1128000, 1048000, 968000, 862000, 756000, 703000, 650000, 500000,
	};
	static const u32 gold_p6_freqs[] = {
		2400000, 2306000, 2203000, 2118000, 1993000, 1837000, 1650000, 1534000,
		1418000, 1274000, 1129000, 1042000, 985000, 898000, 840000, 725000,
	};
	const char *def_govs = "sugov_ext conservative powersave performance schedutil";
	struct cdh_cpufreq_policy *p0 = &cdh_policies[0];
	struct cdh_cpufreq_policy *p6 = &cdh_policies[1];
	unsigned int i;

	cdh_init_core_specs_defaults();
	cdh_nr_policies = 2;

	cdh_init_policy_defaults(p0, 0, "mt-cpufreq", "sugov_ext", def_govs, 1000, NULL);
	for (i = 0; i < 6; i++)
		p0->cpus[i] = i;
	p0->nr_cpus = 6;
	p0->min_freq = 500000;
	p0->max_freq = 2000000;
	p0->cur_freq = 650000;
	memcpy(p0->freqs, gold_p0_freqs, sizeof(gold_p0_freqs));
	p0->nr_freqs = ARRAY_SIZE(gold_p0_freqs);

	cdh_init_policy_defaults(p6, 6, "mt-cpufreq", "sugov_ext", def_govs, 1000, NULL);
	p6->cpus[0] = 6;
	p6->cpus[1] = 7;
	p6->nr_cpus = 2;
	p6->min_freq = 725000;
	p6->max_freq = 2400000;
	p6->cur_freq = 1274000;
	memcpy(p6->freqs, gold_p6_freqs, sizeof(gold_p6_freqs));
	p6->nr_freqs = ARRAY_SIZE(gold_p6_freqs);

	(void)cdh_finalize_policies();
}

static bool cdh_parse_bool(const char *val, bool *out)
{
	if (!strcmp(val, "1") || !strcmp(val, "true") || !strcmp(val, "yes")) {
		*out = true;
		return true;
	}
	if (!strcmp(val, "0") || !strcmp(val, "false") || !strcmp(val, "no")) {
		*out = false;
		return true;
	}
	return false;
}

static int cdh_cpufreq_parse_text(char *text, const char *origin)
{
	char def_driver[CDH_CPUFREQ_STR_MAX] = "cpufreq-dt";
	char def_gov[CDH_CPUFREQ_STR_MAX] = "schedutil";
	char def_avail_govs[CDH_CPUFREQ_GOVS_MAX] = "schedutil";
	char def_bogo_mips[16] = "";
	u32 def_latency = 1000;
	struct cdh_cpufreq_policy *cur = NULL;
	char *cursor = text;
	int line_no = 0;

	cdh_init_core_specs_defaults();
	cdh_nr_policies = 0;
	memset(cdh_policies, 0, sizeof(cdh_policies));

	while (cursor && *cursor) {
		char *line = cursor;
		char *newline = strchr(cursor, '\n');
		char *rest, *key, *val, *tok;
		u32 num;

		if (newline) {
			*newline = '\0';
			cursor = newline + 1;
		} else {
			cursor = NULL;
		}
		line_no++;

		line = cdh_trim_left(line);
		cdh_trim_right(line);
		if (!*line || *line == '#')
			continue;

		rest = line;
		key = cdh_next_token(&rest);
		val = cdh_trim_left(rest);
		if (!key || !*val) {
			pr_err("%s 第 %d 行：格式应为 \"<键> <值>\"\n", origin, line_no);
			return -EINVAL;
		}

		if (!strcmp(key, "policy")) {
			if (cdh_nr_policies >= CDH_CPUFREQ_MAX_POLICIES) {
				pr_err("%s 第 %d 行：policy 数量超过上限 %d\n",
				       origin, line_no, CDH_CPUFREQ_MAX_POLICIES);
				return -EINVAL;
			}
			if (kstrtou32(val, 10, &num) || num >= CDH_CPUFREQ_MAX_CPUS) {
				pr_err("%s 第 %d 行：非法 policy 编号 %s\n",
				       origin, line_no, val);
				return -EINVAL;
			}
			cur = &cdh_policies[cdh_nr_policies++];
			cdh_init_policy_defaults(cur, num, def_driver, def_gov,
						 def_avail_govs, def_latency,
						 def_bogo_mips);
			continue;
		}

		if (!strcmp(key, "driver")) {
			if (cur)
				strscpy(cur->driver, val, sizeof(cur->driver));
			else
				strscpy(def_driver, val, sizeof(def_driver));
			continue;
		}
		if (!strcmp(key, "governor")) {
			if (cur)
				strscpy(cur->governor, val, sizeof(cur->governor));
			else
				strscpy(def_gov, val, sizeof(def_gov));
			continue;
		}
		if (!strcmp(key, "available_governors")) {
			if (cur)
				strscpy(cur->available_governors, val,
					sizeof(cur->available_governors));
			else
				strscpy(def_avail_govs, val, sizeof(def_avail_govs));
			continue;
		}
		if (!strcmp(key, "transition_latency")) {
			if (kstrtou32(val, 10, &num))
				return -EINVAL;
			if (cur)
				cur->transition_latency = num;
			else
				def_latency = num;
			continue;
		}
		if (!strcmp(key, "scaling_readable")) {
			if (!cdh_parse_bool(val, &cdh_scaling_readable))
				return -EINVAL;
			continue;
		}
		if (!strcmp(key, "package_per_cluster")) {
			if (!cdh_parse_bool(val, &cdh_package_per_cluster))
				return -EINVAL;
			continue;
		}
		if (!strcmp(key, "bogo_mips")) {
			if (cur) {
				strscpy(cur->bogo_mips, val, sizeof(cur->bogo_mips));
			} else {
				unsigned int cpu;

				strscpy(def_bogo_mips, val, sizeof(def_bogo_mips));
				for (cpu = 0; cpu < CDH_CPU_CORES_MAX; cpu++)
					strscpy(cdh_core_specs[cpu].bogo_mips, val,
						sizeof(cdh_core_specs[cpu].bogo_mips));
			}
			continue;
		}

		if (!cur) {
			pr_err("%s 第 %d 行：字段 %s 必须在 policy 段内声明\n",
			       origin, line_no, key);
			return -EINVAL;
		}

		if (!strcmp(key, "cpus")) {
			cur->nr_cpus = 0;
			while ((tok = cdh_next_token(&val)) != NULL) {
				if (cur->nr_cpus >= CDH_CPUFREQ_MAX_CPUS ||
				    kstrtou32(tok, 10, &num) ||
				    num >= CDH_CPUFREQ_MAX_CPUS)
					return -EINVAL;
				cur->cpus[cur->nr_cpus++] = num;
			}
		} else if (!strcmp(key, "min_freq")) {
			if (kstrtou32(val, 10, &cur->min_freq))
				return -EINVAL;
		} else if (!strcmp(key, "max_freq")) {
			if (kstrtou32(val, 10, &cur->max_freq))
				return -EINVAL;
		} else if (!strcmp(key, "cur_freq")) {
			if (kstrtou32(val, 10, &cur->cur_freq))
				return -EINVAL;
		} else if (!strcmp(key, "available_frequencies")) {
			cur->nr_freqs = 0;
			while ((tok = cdh_next_token(&val)) != NULL) {
				if (cur->nr_freqs >= CDH_CPUFREQ_MAX_FREQS ||
				    kstrtou32(tok, 10, &num) || !num)
					return -EINVAL;
				cur->freqs[cur->nr_freqs++] = num;
			}
		} else if (!strcmp(key, "implementer")) {
			strscpy(cur->implementer, val, sizeof(cur->implementer));
		} else if (!strcmp(key, "variant")) {
			if (kstrtouint(val, 0, &cur->variant))
				return -EINVAL;
			cur->has_variant = true;
		} else if (!strcmp(key, "part")) {
			strscpy(cur->part, val, sizeof(cur->part));
		} else if (!strcmp(key, "revision")) {
			if (kstrtouint(val, 0, &cur->revision))
				return -EINVAL;
			cur->has_revision = true;
		} else if (!strcmp(key, "capacity")) {
			if (kstrtoul(val, 10, &cur->capacity) || !cur->capacity)
				return -EINVAL;
			cur->has_capacity = true;
		} else if (!strcmp(key, "midr")) {
			if (kstrtou64(val, 0, &cur->midr))
				return -EINVAL;
			cur->has_midr = true;
		} else if (!strcmp(key, "revidr")) {
			if (kstrtou64(val, 0, &cur->revidr))
				return -EINVAL;
			cur->has_revidr = true;
		} else {
			pr_err("%s 第 %d 行：未知字段 %s\n", origin, line_no, key);
			return -EINVAL;
		}
	}

	return cdh_finalize_policies();
}

static int cdh_cpufreq_load_from_file(const char *path)
{
	void *data = NULL;
	size_t size = 0;
	char *text;
	ssize_t read;
	int ret;

	read = kernel_read_file_from_path(path, 0, &data, INT_MAX, &size,
					  READING_UNKNOWN);
	if (read < 0)
		return (int)read;
	if (!size || size > CDH_CPUFREQ_FILE_MAX) {
		vfree(data);
		return -EINVAL;
	}
	text = kmalloc(size + 1, GFP_KERNEL);
	if (!text) {
		vfree(data);
		return -ENOMEM;
	}
	memcpy(text, data, size);
	text[size] = '\0';
	vfree(data);

	ret = cdh_cpufreq_parse_text(text, path);
	kfree(text);
	return ret;
}

static int cdh_cpufreq_load(const char **used_source)
{
	int ret;

	if (cdh_cpufreq_inline && *cdh_cpufreq_inline) {
		char *copy = kstrdup(cdh_cpufreq_inline, GFP_KERNEL);

		if (!copy)
			return -ENOMEM;
		*used_source = "模块参数 cpufreq";
		ret = cdh_cpufreq_parse_text(copy, *used_source);
		kfree(copy);
		return ret;
	}

	if (cdh_cpufreq_path && *cdh_cpufreq_path) {
		*used_source = cdh_cpufreq_path;
		return cdh_cpufreq_load_from_file(cdh_cpufreq_path);
	}

	*used_source = CDH_PATCH_CPUFREQ_PATH;
	ret = cdh_cpufreq_load_from_file(CDH_PATCH_CPUFREQ_PATH);
	if (ret == -ENOENT) {
		*used_source = CDH_DEFAULT_CPUFREQ_PATH;
		ret = cdh_cpufreq_load_from_file(CDH_DEFAULT_CPUFREQ_PATH);
	}
	if (ret == -ENOENT) {
		*used_source = "builtin-default";
		cdh_load_builtin_defaults();
		return 0;
	}
	return ret;
}

int cdh_cpufreq_load_once(const char **used_source)
{
	if (!cdh_cpufreq_loaded) {
		cdh_cpufreq_load_ret = cdh_cpufreq_load(&cdh_cpufreq_used_source);
		cdh_cpufreq_loaded = true;
	}
	if (used_source)
		*used_source = cdh_cpufreq_used_source;
	return cdh_cpufreq_load_ret;
}

const struct cdh_cpu_core_spec *cdh_get_cpu_core_spec(unsigned int cpu)
{
	if (cpu >= CDH_CPU_CORES_MAX)
		return NULL;
	return &cdh_core_specs[cpu];
}

bool cdh_is_package_per_cluster(void)
{
	return cdh_package_per_cluster;
}

static struct cdh_cpufreq_policy *cdh_policy_for_kobj(struct kobject *kobj)
{
	unsigned int i;

	for (i = 0; i < cdh_nr_policies; i++) {
		if (cdh_policies[i].kobj == kobj)
			return &cdh_policies[i];
	}
	return NULL;
}

static ssize_t cdh_cpufreq_show(struct kobject *kobj,
				struct kobj_attribute *attr, char *buf)
{
	const struct cdh_cpufreq_policy *p = cdh_policy_for_kobj(kobj);
	const char *name = attr->attr.name;
	unsigned int i;
	ssize_t len = 0;

	if (!p)
		return -ENODEV;

	if (!strcmp(name, "affected_cpus") || !strcmp(name, "related_cpus"))
		return sysfs_emit(buf, "%s\n", p->cpus_str);
	if (!strcmp(name, "cpuinfo_min_freq") || !strcmp(name, "scaling_min_freq"))
		return sysfs_emit(buf, "%u\n", p->min_freq);
	if (!strcmp(name, "cpuinfo_max_freq") || !strcmp(name, "scaling_max_freq"))
		return sysfs_emit(buf, "%u\n", p->max_freq);
	if (!strcmp(name, "cpuinfo_cur_freq") || !strcmp(name, "scaling_cur_freq"))
		return sysfs_emit(buf, "%u\n", p->cur_freq);
	if (!strcmp(name, "cpuinfo_transition_latency"))
		return sysfs_emit(buf, "%u\n", p->transition_latency);
	if (!strcmp(name, "scaling_driver"))
		return sysfs_emit(buf, "%s\n", p->driver);
	if (!strcmp(name, "scaling_governor"))
		return sysfs_emit(buf, "%s\n", p->governor);
	if (!strcmp(name, "scaling_available_governors"))
		return sysfs_emit(buf, "%s \n", p->available_governors);
	if (!strcmp(name, "scaling_setspeed"))
		return sysfs_emit(buf, "<unsupported>\n");
	if (!strcmp(name, "scaling_available_frequencies")) {
		for (i = 0; i < p->nr_freqs; i++)
			len += sysfs_emit_at(buf, len, "%u ", p->freqs[i]);
		len += sysfs_emit_at(buf, len, "\n");
		return len;
	}
	return sysfs_emit(buf, "0\n");
}

static ssize_t cdh_cpufreq_stats_show(struct kobject *kobj,
				      struct kobj_attribute *attr, char *buf)
{
	const struct cdh_cpufreq_policy *p = cdh_policy_for_kobj(kobj);
	const char *name = attr->attr.name;
	unsigned int i, j;
	ssize_t len = 0;
	u64 total_cs;

	if (!p)
		return -ENODEV;

	if (!strcmp(name, "total_trans"))
		return sysfs_emit(buf, "%u\n", 128U + p->policy_id * 17U);

	if (!strcmp(name, "time_in_state")) {
		/* 将开机至今的 USER_HZ tick 按权重分摊到各频点，使统计值随运行时间自然增长。 */
		total_cs = jiffies_64_to_clock_t(get_jiffies_64() - INITIAL_JIFFIES);
		if (total_cs < (u64)p->nr_freqs * 10ULL)
			total_cs = (u64)p->nr_freqs * 10ULL;
		for (i = 0; i < p->nr_freqs; i++) {
			u64 ticks;

			if (p->freqs[i] == p->min_freq)
				ticks = (total_cs * 45ULL) / 100ULL;
			else if (p->freqs[i] == p->cur_freq)
				ticks = (total_cs * 25ULL) / 100ULL;
			else
				ticks = (total_cs * 30ULL) /
					(100ULL * max(1U, p->nr_freqs - 1));
			len += sysfs_emit_at(buf, len, "%u %llu\n",
					     p->freqs[i], ticks + i + 1);
		}
		return len;
	}

	if (!strcmp(name, "trans_table")) {
		len += sysfs_emit_at(buf, len, "   From  :   To\n");
		len += sysfs_emit_at(buf, len, "         : ");
		for (i = 0; i < p->nr_freqs; i++)
			len += sysfs_emit_at(buf, len, "%9u ", p->freqs[i]);
		len += sysfs_emit_at(buf, len, "\n");
		for (i = 0; i < p->nr_freqs; i++) {
			len += sysfs_emit_at(buf, len, "%9u: ", p->freqs[i]);
			for (j = 0; j < p->nr_freqs; j++) {
				unsigned int cnt = 0;

				if (i != j && ((i + 1 == j) || (j + 1 == i)))
					cnt = 4U + (i + j) % 9U;
				len += sysfs_emit_at(buf, len, "%9u ", cnt);
			}
			len += sysfs_emit_at(buf, len, "\n");
		}
		return len;
	}
	return -ENOENT;
}

static ssize_t cdh_cpufreq_noop_store(struct kobject *kobj,
				      struct kobj_attribute *attr,
				      const char *buf, size_t count)
{
	return count;
}

static ssize_t cdh_cpufreq_stats_reset_store(struct kobject *kobj,
					     struct kobj_attribute *attr,
					     const char *buf, size_t count)
{
	return count;
}

/*
 * 权限对齐真机内核（drivers/cpufreq/cpufreq.c 与 Android init 赋权）：
 *   - 0444（App 可读）：affected_cpus, related_cpus, cpuinfo_min_freq, cpuinfo_max_freq,
 *     cpuinfo_transition_latency, scaling_available_frequencies, scaling_available_governors,
 *     scaling_cur_freq, scaling_driver
 *   - 0644（App 可读）：scaling_setspeed
 *   - 0400/0600（仅特权进程可访问，untrusted_app 读返回 EACCES / permission-denied）：
 *     cpuinfo_cur_freq；以及当 scaling_readable=false 时的 scaling_min_freq,
 *     scaling_max_freq, scaling_governor（当 scaling_readable=true 时通过 .is_visible
 *     分别暴露为 0664 / 0664 / 0644，对齐真机 init chmod 后的权限）。
 */
static struct kobj_attribute affected_cpus_attr =
	__ATTR(affected_cpus, 0444, cdh_cpufreq_show, NULL);
static struct kobj_attribute related_cpus_attr =
	__ATTR(related_cpus, 0444, cdh_cpufreq_show, NULL);
static struct kobj_attribute cpuinfo_min_freq_attr =
	__ATTR(cpuinfo_min_freq, 0444, cdh_cpufreq_show, NULL);
static struct kobj_attribute cpuinfo_max_freq_attr =
	__ATTR(cpuinfo_max_freq, 0444, cdh_cpufreq_show, NULL);
static struct kobj_attribute cpuinfo_cur_freq_attr =
	__ATTR(cpuinfo_cur_freq, 0400, cdh_cpufreq_show, NULL);
static struct kobj_attribute cpuinfo_transition_latency_attr =
	__ATTR(cpuinfo_transition_latency, 0444, cdh_cpufreq_show, NULL);
static struct kobj_attribute scaling_available_frequencies_attr =
	__ATTR(scaling_available_frequencies, 0444, cdh_cpufreq_show, NULL);
static struct kobj_attribute scaling_available_governors_attr =
	__ATTR(scaling_available_governors, 0444, cdh_cpufreq_show, NULL);
static struct kobj_attribute scaling_cur_freq_attr =
	__ATTR(scaling_cur_freq, 0444, cdh_cpufreq_show, NULL);
static struct kobj_attribute scaling_driver_attr =
	__ATTR(scaling_driver, 0444, cdh_cpufreq_show, NULL);
static struct kobj_attribute scaling_governor_attr =
	__ATTR(scaling_governor, 0600, cdh_cpufreq_show, cdh_cpufreq_noop_store);
static struct kobj_attribute scaling_max_freq_attr =
	__ATTR(scaling_max_freq, 0600, cdh_cpufreq_show, cdh_cpufreq_noop_store);
static struct kobj_attribute scaling_min_freq_attr =
	__ATTR(scaling_min_freq, 0600, cdh_cpufreq_show, cdh_cpufreq_noop_store);
static struct kobj_attribute scaling_setspeed_attr =
	__ATTR(scaling_setspeed, 0644, cdh_cpufreq_show, cdh_cpufreq_noop_store);

static struct attribute *cdh_cpufreq_attrs[] = {
	&affected_cpus_attr.attr,
	&related_cpus_attr.attr,
	&cpuinfo_min_freq_attr.attr,
	&cpuinfo_max_freq_attr.attr,
	&cpuinfo_cur_freq_attr.attr,
	&cpuinfo_transition_latency_attr.attr,
	&scaling_available_frequencies_attr.attr,
	&scaling_available_governors_attr.attr,
	&scaling_cur_freq_attr.attr,
	&scaling_driver_attr.attr,
	&scaling_governor_attr.attr,
	&scaling_max_freq_attr.attr,
	&scaling_min_freq_attr.attr,
	&scaling_setspeed_attr.attr,
	NULL,
};

static umode_t cdh_cpufreq_attr_is_visible(struct kobject *kobj,
					   struct attribute *attr, int index)
{
	if (cdh_scaling_readable) {
		if (attr == &scaling_governor_attr.attr)
			return 0644;
		if (attr == &scaling_max_freq_attr.attr ||
		    attr == &scaling_min_freq_attr.attr)
			return 0664;
	}
	return attr->mode;
}

static const struct attribute_group cdh_cpufreq_attr_group = {
	.attrs = cdh_cpufreq_attrs,
	.is_visible = cdh_cpufreq_attr_is_visible,
};

static struct kobj_attribute stats_total_trans_attr =
	__ATTR(total_trans, 0444, cdh_cpufreq_stats_show, NULL);
static struct kobj_attribute stats_time_in_state_attr =
	__ATTR(time_in_state, 0444, cdh_cpufreq_stats_show, NULL);
static struct kobj_attribute stats_trans_table_attr =
	__ATTR(trans_table, 0444, cdh_cpufreq_stats_show, NULL);
static struct kobj_attribute stats_reset_attr =
	__ATTR(reset, 0200, NULL, cdh_cpufreq_stats_reset_store);

static struct attribute *cdh_cpufreq_stats_attrs[] = {
	&stats_total_trans_attr.attr,
	&stats_time_in_state_attr.attr,
	&stats_trans_table_attr.attr,
	&stats_reset_attr.attr,
	NULL,
};

static const struct attribute_group cdh_cpufreq_stats_group = {
	.name = "stats",
	.attrs = cdh_cpufreq_stats_attrs,
};

void cdh_cpufreq_sysfs_exit(void)
{
	unsigned int i, cpu;

	for (i = 0; i < cdh_nr_policies; i++) {
		struct cdh_cpufreq_policy *p = &cdh_policies[i];

		for (cpu = 0; cpu < CDH_CPUFREQ_MAX_CPUS; cpu++) {
			struct device *cpu_dev;

			if (!(p->linked_cpus_mask & (1U << cpu)))
				continue;
			cpu_dev = get_cpu_device(cpu);
			if (cpu_dev)
				sysfs_remove_link(&cpu_dev->kobj, "cpufreq");
		}
		p->linked_cpus_mask = 0;

		if (p->gov_kobj) {
			kobject_put(p->gov_kobj);
			p->gov_kobj = NULL;
		}
		if (!p->kobj)
			continue;
		if (p->stats_created) {
			sysfs_remove_group(p->kobj, &cdh_cpufreq_stats_group);
			p->stats_created = false;
		}
		if (p->group_created) {
			sysfs_remove_group(p->kobj, &cdh_cpufreq_attr_group);
			p->group_created = false;
		}
		kobject_put(p->kobj);
		p->kobj = NULL;
	}
	pr_info("removed cpufreq policy sysfs nodes and cpu symlinks\n");
}

int cdh_cpufreq_sysfs_init(void)
{
	const char *used_source = "builtin-default";
	struct kobject *cpufreq_root = NULL;
	unsigned long root_symbol_addr;
	unsigned int i, j;
	int ret;

	ret = cdh_cpufreq_load_once(&used_source);
	if (ret) {
		pr_err("failed to load cpufreq config (%s): %d\n",
		       used_source, ret);
		return ret;
	}

	root_symbol_addr = cdh_cpufreq_lookup_symbol("cpufreq_global_kobject");
	if (root_symbol_addr)
		cpufreq_root = *(struct kobject **)root_symbol_addr;
	if (!cpufreq_root) {
		pr_err("failed to resolve cpufreq_global_kobject\n");
		return -ENOENT;
	}

	for (i = 0; i < cdh_nr_policies; i++) {
		struct cdh_cpufreq_policy *p = &cdh_policies[i];
		char policy_name[32];

		snprintf(policy_name, sizeof(policy_name), "policy%u",
			 p->policy_id);
		p->kobj = kobject_create_and_add(policy_name, cpufreq_root);
		if (!p->kobj) {
			ret = -ENOMEM;
			goto fail;
		}

		ret = sysfs_create_group(p->kobj, &cdh_cpufreq_attr_group);
		if (ret)
			goto fail;
		p->group_created = true;

		ret = sysfs_create_group(p->kobj, &cdh_cpufreq_stats_group);
		if (ret)
			goto fail;
		p->stats_created = true;

		if (p->governor[0]) {
			p->gov_kobj = kobject_create_and_add(p->governor, p->kobj);
			if (!p->gov_kobj) {
				ret = -ENOMEM;
				goto fail;
			}
		}

		for (j = 0; j < p->nr_cpus; j++) {
			unsigned int cpu = p->cpus[j];
			struct device *cpu_dev = get_cpu_device(cpu);

			if (!cpu_dev) {
				pr_warn("cpu%u device not present, skipping symlink for %s\n",
					cpu, policy_name);
				continue;
			}
			ret = sysfs_create_link(&cpu_dev->kobj, p->kobj,
						"cpufreq");
			if (ret) {
				pr_err("failed to create cpu%u/cpufreq symlink -> %s: %d\n",
				       cpu, policy_name, ret);
				goto fail;
			}
			p->linked_cpus_mask |= (1U << cpu);
		}
	}

	pr_info("created %u cpufreq policies (%s) under /sys/devices/system/cpu/cpufreq\n",
		cdh_nr_policies, used_source);
	return 0;

fail:
	pr_err("failed to create cpufreq sysfs nodes: %d\n", ret);
	cdh_cpufreq_sysfs_exit();
	return ret;
}
