// SPDX-License-Identifier: GPL-2.0-only
/*
 * 虚拟电池 power_supply sysfs 节点（/sys/class/power_supply/battery/）：
 *   荣耀 MagicOS Settings（com.android.settings.fuelgauge.q#b）等厂商设置页在展示
 *   「关于手机 -> 电池」额定/典型容量时，不走 Android Health AIDL HAL 或 PowerProfile，
 *   而是直接读取 /sys/class/power_supply/battery/charge_full_design（单位 µAh，除以 1000 后拼 " mAh"），
 *   节点缺失时直接显示「未知 / Unknown」。
 *
 *   本子模块复用机型现有的 /mnt/vendorpatch/vendor/etc/battery_config.xml（回退
 *   /vendor/etc/battery_config.xml）中的 <capacity full_uah="..."/>（自动兼容 mAh 与 µAh 两种量纲），
 *   通过标准 power_supply_register 注册只读 "battery" 设备，对全部云机统一暴露真机电池容量。
 */
#define pr_fmt(fmt) "cdh_device_info_bat: " fmt

#include <linux/ctype.h>
#include <linux/err.h>
#include <linux/kernel.h>
#include <linux/kernel_read_file.h>
#include <linux/moduleparam.h>
#include <linux/mutex.h>
#include <linux/power_supply.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/vmalloc.h>

#include "cdh_device_info.h"

#define CDH_BAT_XML_MAX_BYTES (16 * 1024)
#define CDH_BAT_DEFAULT_UAH   5000000

static int cdh_battery_uah_param;
module_param_named(battery_uah, cdh_battery_uah_param, int, 0644);
MODULE_PARM_DESC(battery_uah, "虚拟电池设计容量（µAh 或 <100000 的 mAh；0 表示自动从 battery_config.xml 读取）");

static char *cdh_battery_xml_path;
module_param_named(battery_xml_path, cdh_battery_xml_path, charp, 0444);
MODULE_PARM_DESC(battery_xml_path, "battery_config.xml 路径（缺省优先读 " CDH_PATCH_BATTERY_XML_PATH "）");

static DEFINE_MUTEX(cdh_bat_lock);
static struct power_supply *cdh_bat_psy;
static bool cdh_bat_xml_loaded;
static int cdh_bat_design_uah = CDH_BAT_DEFAULT_UAH;
static int cdh_bat_counter_uah = 4965000;
static int cdh_bat_voltage_uv = 4450000;
static int cdh_bat_temp_decic = 280;
static int cdh_bat_technology = POWER_SUPPLY_TECHNOLOGY_LIPO;

static int cdh_normalize_uah(long raw)
{
	if (raw <= 0)
		return CDH_BAT_DEFAULT_UAH;
	/* 兼容 battery_config.xml 中写 mAh（如 5000 / 7000）或 µAh（如 5850000 / 7000000） */
	if (raw < 100000L)
		raw *= 1000L;
	if (raw > 50000000L)
		raw = 50000000L;
	return (int)raw;
}

static bool cdh_extract_xml_attr(const char *xml, const char *attr_name,
				 char *out, size_t out_sz)
{
	size_t name_len = strlen(attr_name);
	const char *p = xml;

	if (!xml || !attr_name || !out || out_sz < 2)
		return false;

	while ((p = strstr(p, attr_name)) != NULL) {
		const char *q = p + name_len;
		char quote;
		size_t n = 0;

		if (p > xml && (isalnum((unsigned char)p[-1]) || p[-1] == '_' || p[-1] == '-')) {
			p += name_len;
			continue;
		}
		while (*q == ' ' || *q == '\t')
			q++;
		if (*q != '=') {
			p += name_len;
			continue;
		}
		q++;
		while (*q == ' ' || *q == '\t')
			q++;
		if (*q != '"' && *q != '\'') {
			p += name_len;
			continue;
		}
		quote = *q++;
		while (q[n] && q[n] != quote && n + 1 < out_sz) {
			out[n] = q[n];
			n++;
		}
		out[n] = '\0';
		return n > 0;
	}
	return false;
}

static bool cdh_try_load_battery_xml_one(const char *path)
{
	void *buf = NULL;
	size_t size = 0;
	int rc;
	char *text;
	char val[64];
	long parsed;

	if (!path || !path[0])
		return false;

	rc = kernel_read_file_from_path(path, 0, &buf, CDH_BAT_XML_MAX_BYTES,
					&size, READING_POLICY);
	if (rc < 0 || !buf || size == 0) {
		if (buf)
			vfree(buf);
		return false;
	}

	text = kmalloc(size + 1, GFP_KERNEL);
	if (!text) {
		vfree(buf);
		return false;
	}
	memcpy(text, buf, size);
	text[size] = '\0';
	vfree(buf);

	if (cdh_extract_xml_attr(text, "full_uah", val, sizeof(val)) &&
	    kstrtol(val, 10, &parsed) == 0 && parsed > 0) {
		cdh_bat_design_uah = cdh_normalize_uah(parsed);
		cdh_bat_counter_uah = cdh_bat_design_uah - 35000;
		if (cdh_bat_counter_uah <= 0)
			cdh_bat_counter_uah = cdh_bat_design_uah;
	}
	if (cdh_extract_xml_attr(text, "counter_max", val, sizeof(val)) &&
	    kstrtol(val, 10, &parsed) == 0 && parsed > 0) {
		cdh_bat_counter_uah = cdh_normalize_uah(parsed);
	}
	if (cdh_extract_xml_attr(text, "full_mv", val, sizeof(val)) &&
	    kstrtol(val, 10, &parsed) == 0 && parsed >= 3000 && parsed <= 15000) {
		cdh_bat_voltage_uv = (int)(parsed * 1000L);
	}
	if (cdh_extract_xml_attr(text, "temperature_full_decic", val, sizeof(val)) &&
	    kstrtol(val, 10, &parsed) == 0 && parsed >= 0 && parsed <= 800) {
		cdh_bat_temp_decic = (int)parsed;
	}
	if (cdh_extract_xml_attr(text, "technology", val, sizeof(val))) {
		if (strcasecmp(val, "Li-ion") == 0)
			cdh_bat_technology = POWER_SUPPLY_TECHNOLOGY_LION;
		else
			cdh_bat_technology = POWER_SUPPLY_TECHNOLOGY_LIPO;
	}

	kfree(text);
	pr_info("loaded battery spec from %s: design_uah=%d\n", path, cdh_bat_design_uah);
	return true;
}

static void cdh_ensure_battery_spec_loaded(void)
{
	if (cdh_battery_uah_param > 0) {
		cdh_bat_design_uah = cdh_normalize_uah(cdh_battery_uah_param);
		cdh_bat_counter_uah = cdh_bat_design_uah;
		return;
	}
	if (READ_ONCE(cdh_bat_xml_loaded))
		return;

	mutex_lock(&cdh_bat_lock);
	if (!cdh_bat_xml_loaded) {
		if ((cdh_battery_xml_path && cdh_battery_xml_path[0] &&
		     cdh_try_load_battery_xml_one(cdh_battery_xml_path)) ||
		    cdh_try_load_battery_xml_one(CDH_PATCH_BATTERY_XML_PATH) ||
		    cdh_try_load_battery_xml_one(CDH_DEFAULT_BATTERY_XML_PATH)) {
			WRITE_ONCE(cdh_bat_xml_loaded, true);
		}
	}
	mutex_unlock(&cdh_bat_lock);
}

static enum power_supply_property cdh_bat_props[] = {
	POWER_SUPPLY_PROP_STATUS,
	POWER_SUPPLY_PROP_HEALTH,
	POWER_SUPPLY_PROP_PRESENT,
	POWER_SUPPLY_PROP_TECHNOLOGY,
	POWER_SUPPLY_PROP_CAPACITY,
	POWER_SUPPLY_PROP_CHARGE_FULL_DESIGN,
	POWER_SUPPLY_PROP_CHARGE_FULL,
	POWER_SUPPLY_PROP_CHARGE_COUNTER,
	POWER_SUPPLY_PROP_VOLTAGE_NOW,
	POWER_SUPPLY_PROP_TEMP,
};

static int cdh_bat_get_property(struct power_supply *psy,
				enum power_supply_property psp,
				union power_supply_propval *val)
{
	cdh_ensure_battery_spec_loaded();

	switch (psp) {
	case POWER_SUPPLY_PROP_STATUS:
		val->intval = POWER_SUPPLY_STATUS_FULL;
		return 0;
	case POWER_SUPPLY_PROP_HEALTH:
		val->intval = POWER_SUPPLY_HEALTH_GOOD;
		return 0;
	case POWER_SUPPLY_PROP_PRESENT:
		val->intval = 1;
		return 0;
	case POWER_SUPPLY_PROP_TECHNOLOGY:
		val->intval = cdh_bat_technology;
		return 0;
	case POWER_SUPPLY_PROP_CAPACITY:
		val->intval = 100;
		return 0;
	case POWER_SUPPLY_PROP_CHARGE_FULL_DESIGN:
	case POWER_SUPPLY_PROP_CHARGE_FULL:
		val->intval = cdh_bat_design_uah;
		return 0;
	case POWER_SUPPLY_PROP_CHARGE_COUNTER:
		val->intval = cdh_bat_counter_uah;
		return 0;
	case POWER_SUPPLY_PROP_VOLTAGE_NOW:
		val->intval = cdh_bat_voltage_uv;
		return 0;
	case POWER_SUPPLY_PROP_TEMP:
		val->intval = cdh_bat_temp_decic;
		return 0;
	default:
		return -EINVAL;
	}
}

static const struct power_supply_desc cdh_bat_psy_desc = {
	.name = "battery",
	.type = POWER_SUPPLY_TYPE_BATTERY,
	.properties = cdh_bat_props,
	.num_properties = ARRAY_SIZE(cdh_bat_props),
	.get_property = cdh_bat_get_property,
};

int cdh_battery_sysfs_init(void)
{
	struct power_supply_config psy_cfg = {};
	struct power_supply *existing;

	existing = power_supply_get_by_name("battery");
	if (existing) {
		power_supply_put(existing);
		pr_info("power_supply/battery already present, skip virtual battery\n");
		return 0;
	}

	cdh_ensure_battery_spec_loaded();
	cdh_bat_psy = power_supply_register(NULL, &cdh_bat_psy_desc, &psy_cfg);
	if (IS_ERR(cdh_bat_psy)) {
		pr_warn("power_supply_register(battery) failed (%ld), skip virtual battery\n",
			PTR_ERR(cdh_bat_psy));
		cdh_bat_psy = NULL;
		return 0;
	}
	pr_info("registered /sys/class/power_supply/battery (design_uah=%d)\n",
		cdh_bat_design_uah);
	return 0;
}

void cdh_battery_sysfs_exit(void)
{
	if (!cdh_bat_psy)
		return;
	power_supply_unregister(cdh_bat_psy);
	cdh_bat_psy = NULL;
}
