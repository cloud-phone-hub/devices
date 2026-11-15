// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2025-2026 fei_cong(https://github.com/feicong/feicong-course)
 *
 * thermal sysfs 遥测节点对齐层：
 *   1) 支持从伴随文件 /mnt/vendorpatch/vendor_dlkm/lib/modules/cdh_device_info.thermal
 *      （回退 /vendor_dlkm/lib/modules/cdh_device_info.thermal）或模块参数 thermal
 *      加载机型温区列表（每行格式：<type> <base_temp_mc> [jitter_mc]）；
 *   2) 未提供伴随文件时回退内置默认 31 个硬件温区（对齐 MTK Android 15 温区拓扑）；
 *   3) 通过内核标准 thermal_tripless_zone_device_register / thermal_zone_device_register
 *      在 /sys/devices/virtual/thermal/ 与 /sys/class/thermal/ 下注册并启用
 *      thermal_zone0..N，提供 type 与微幅动态波动 temp 读取。
 */
#define pr_fmt(fmt) "cdh_device_info_thermal: " fmt

#include <linux/errno.h>
#include <linux/jiffies.h>
#include <linux/kernel.h>
#include <linux/kernel_read_file.h>
#include <linux/moduleparam.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/thermal.h>
#include <linux/version.h>
#include <linux/vmalloc.h>

#include "cdh_device_info.h"

#define CDH_THERMAL_MAX_ZONES 64
#define CDH_THERMAL_FILE_MAX  (16 * 1024)

static char *cdh_thermal_path;
module_param_named(thermal_path, cdh_thermal_path, charp, 0444);
MODULE_PARM_DESC(thermal_path, "thermal 伴随文件路径（缺省 " CDH_DEFAULT_THERMAL_PATH "）");

static char *cdh_thermal_inline;
module_param_named(thermal, cdh_thermal_inline, charp, 0444);
MODULE_PARM_DESC(thermal, "调试用内联 thermal 配置文本（非空时忽略 thermal_path）");

struct cdh_thermal_zone_spec {
	unsigned int index;
	char type[THERMAL_NAME_LENGTH];
	int base_temp_mc;
	int jitter_mc;
	struct thermal_zone_device *tzd;
};

struct cdh_thermal_default_entry {
	const char *type;
	int base_temp_mc;
	int jitter_mc;
};

static const struct cdh_thermal_default_entry cdh_default_zones[] = {
	{ "sc-cp-standalone",   2800,    25 },
	{ "mtk-master-charger", 25000,   120 },
	{ "mtk-slave-charger",  0,       0 },
	{ "mtk-mst-div-chg",    0,       0 },
	{ "mtk-slv-div-chg",    0,       0 },
	{ "mtk-mst-hvdiv-chg",  0,       0 },
	{ "mtk-slv-hvdiv-chg",  0,       0 },
	{ "mtk-gauge",          28000,   100 },
	{ "battery",            28000,   100 },
	{ "mtktscpu",           33607,   350 },
	{ "mtktspmic",          29164,   180 },
	{ "mtktsbattery",       28000,   100 },
	{ "mtktsAP",            30479,   220 },
	{ "mtktsbtsmdpa",       29413,   160 },
	{ "mtktspa",            31000,   200 },
	{ "mtktscharger",       28734,   150 },
	{ "tzimgs0",            -127000, 0 },
	{ "tzimgs1",            -127000, 0 },
	{ "tzimgs2",            -127000, 0 },
	{ "tzimgs3",            -127000, 0 },
	{ "tzimgs4",            -127000, 0 },
	{ "tzimgs5",            -127000, 0 },
	{ "tzimgs6",            -127000, 0 },
	{ "tzimgs7",            -127000, 0 },
	{ "tzimgs8",            -127000, 0 },
	{ "tzimgs9",            -127000, 0 },
	{ "tzimgs10",           -127000, 0 },
	{ "tzimgs11",           -127000, 0 },
	{ "tzimgs12",           -127000, 0 },
	{ "mtktsdctm",          28925,   140 },
	{ "mtkcsCPU",           29941,   280 },
};

static struct cdh_thermal_zone_spec cdh_zones[CDH_THERMAL_MAX_ZONES];
static unsigned int cdh_nr_zones;

static int cdh_thermal_get_temp(struct thermal_zone_device *tz, int *temp)
{
#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 4, 0)
	const struct cdh_thermal_zone_spec *spec = thermal_zone_device_priv(tz);
#else
	const struct cdh_thermal_zone_spec *spec = tz ? tz->devdata : NULL;
#endif
	int delta = 0;

	if (!temp)
		return -EINVAL;
	if (!spec) {
		*temp = 28000;
		return 0;
	}
	if (spec->jitter_mc > 0) {
		unsigned long tick = jiffies / (HZ ? HZ : 100);
		int phase = (int)((tick + spec->index * 3U) % 7U) - 3;

		delta = (phase * spec->jitter_mc) / 3;
	}
	*temp = spec->base_temp_mc + delta;
	return 0;
}

static struct thermal_zone_device_ops cdh_thermal_zone_ops = {
	.get_temp = cdh_thermal_get_temp,
};

static struct thermal_zone_params cdh_thermal_zone_params = {
	.no_hwmon = true,
};

static char *cdh_thermal_trim_left(char *s)
{
	while (*s == ' ' || *s == '\t' || *s == '\r')
		s++;
	return s;
}

static void cdh_thermal_trim_right(char *s)
{
	size_t len = strlen(s);

	while (len > 0 && (s[len - 1] == ' ' || s[len - 1] == '\t' ||
			   s[len - 1] == '\r' || s[len - 1] == '\n'))
		s[--len] = '\0';
}

static char *cdh_thermal_next_token(char **cursor)
{
	char *start, *walk;

	if (!cursor || !*cursor)
		return NULL;
	start = cdh_thermal_trim_left(*cursor);
	if (!*start) {
		*cursor = start;
		return NULL;
	}
	walk = start;
	while (*walk && *walk != ' ' && *walk != '\t' && *walk != '\r')
		walk++;
	if (*walk) {
		*walk++ = '\0';
		walk = cdh_thermal_trim_left(walk);
	}
	*cursor = walk;
	return start;
}

static int cdh_thermal_parse_text(char *text)
{
	char *cursor = text;
	char *line;

	cdh_nr_zones = 0;
	while ((line = strsep(&cursor, "\n")) != NULL) {
		char *p, *tok_type, *tok_temp, *tok_jitter;
		struct cdh_thermal_zone_spec *z;
		int base_temp = 0, jitter = 0;

		cdh_thermal_trim_right(line);
		p = cdh_thermal_trim_left(line);
		if (!*p || *p == '#')
			continue;

		tok_type = cdh_thermal_next_token(&p);
		tok_temp = cdh_thermal_next_token(&p);
		tok_jitter = cdh_thermal_next_token(&p);
		if (!tok_type || !tok_temp)
			continue;
		if (kstrtoint(tok_temp, 10, &base_temp) != 0)
			continue;
		if (tok_jitter && kstrtoint(tok_jitter, 10, &jitter) != 0)
			jitter = 0;
		if (cdh_nr_zones >= CDH_THERMAL_MAX_ZONES)
			break;

		z = &cdh_zones[cdh_nr_zones];
		memset(z, 0, sizeof(*z));
		z->index = cdh_nr_zones;
		strscpy(z->type, tok_type, sizeof(z->type));
		z->base_temp_mc = base_temp;
		z->jitter_mc = jitter >= 0 ? jitter : 0;
		cdh_nr_zones++;
	}
	return cdh_nr_zones > 0 ? 0 : -EINVAL;
}

static void cdh_thermal_load_defaults(void)
{
	unsigned int i;

	cdh_nr_zones = 0;
	for (i = 0; i < ARRAY_SIZE(cdh_default_zones) && i < CDH_THERMAL_MAX_ZONES; i++) {
		struct cdh_thermal_zone_spec *z = &cdh_zones[cdh_nr_zones];

		memset(z, 0, sizeof(*z));
		z->index = cdh_nr_zones;
		strscpy(z->type, cdh_default_zones[i].type, sizeof(z->type));
		z->base_temp_mc = cdh_default_zones[i].base_temp_mc;
		z->jitter_mc = cdh_default_zones[i].jitter_mc;
		cdh_nr_zones++;
	}
}

static int cdh_thermal_load_config(const char **used_source)
{
	void *file_buf = NULL;
	size_t file_size = 0;
	const char *path;
	char *text_copy;
	int ret;

	if (cdh_thermal_inline && cdh_thermal_inline[0]) {
		text_copy = kstrdup(cdh_thermal_inline, GFP_KERNEL);
		if (!text_copy)
			return -ENOMEM;
		ret = cdh_thermal_parse_text(text_copy);
		kfree(text_copy);
		if (ret == 0) {
			if (used_source)
				*used_source = "module_param:thermal";
			return 0;
		}
	}

	path = (cdh_thermal_path && cdh_thermal_path[0]) ?
	       cdh_thermal_path : CDH_PATCH_THERMAL_PATH;
	ret = kernel_read_file_from_path(path, 0, &file_buf,
					 CDH_THERMAL_FILE_MAX, &file_size,
					 READING_POLICY);
	if (ret < 0 && (!cdh_thermal_path || !cdh_thermal_path[0])) {
		path = CDH_DEFAULT_THERMAL_PATH;
		ret = kernel_read_file_from_path(path, 0, &file_buf,
						 CDH_THERMAL_FILE_MAX,
						 &file_size, READING_POLICY);
	}
	if (ret >= 0 && file_buf && file_size > 0) {
		text_copy = kzalloc(file_size + 1, GFP_KERNEL);
		if (text_copy) {
			memcpy(text_copy, file_buf, file_size);
			ret = cdh_thermal_parse_text(text_copy);
			kfree(text_copy);
		} else {
			ret = -ENOMEM;
		}
		vfree(file_buf);
		if (ret == 0) {
			if (used_source)
				*used_source = path;
			return 0;
		}
	} else if (file_buf) {
		vfree(file_buf);
	}

	cdh_thermal_load_defaults();
	if (used_source)
		*used_source = "builtin-default";
	return 0;
}

void cdh_thermal_sysfs_exit(void)
{
	unsigned int i;

	for (i = 0; i < cdh_nr_zones; i++) {
		if (cdh_zones[i].tzd && !IS_ERR(cdh_zones[i].tzd)) {
			thermal_zone_device_unregister(cdh_zones[i].tzd);
			cdh_zones[i].tzd = NULL;
		}
	}
	pr_info("unregistered %u thermal zones\n", cdh_nr_zones);
}

int cdh_thermal_sysfs_init(void)
{
	const char *used_source = "builtin-default";
	unsigned int i, registered = 0;
	int ret;

	ret = cdh_thermal_load_config(&used_source);
	if (ret)
		return ret;

	for (i = 0; i < cdh_nr_zones; i++) {
		struct cdh_thermal_zone_spec *spec = &cdh_zones[i];
		struct thermal_zone_device *tz;

#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 4, 0)
		tz = thermal_tripless_zone_device_register(
			spec->type, (void *)spec, &cdh_thermal_zone_ops,
			&cdh_thermal_zone_params);
#else
		tz = thermal_zone_device_register(
			spec->type, 0, 0, (void *)spec, &cdh_thermal_zone_ops,
			&cdh_thermal_zone_params, 0, 0);
#endif
		if (IS_ERR(tz)) {
			pr_warn("failed to register thermal zone %u (%s): %ld\n",
				i, spec->type, PTR_ERR(tz));
			spec->tzd = NULL;
			continue;
		}
		spec->tzd = tz;
		(void)thermal_zone_device_enable(tz);
		registered++;
	}

	pr_info("registered %u/%u thermal zones (%s)\n",
		registered, cdh_nr_zones, used_source);
	return 0;
}
