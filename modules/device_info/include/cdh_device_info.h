/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2025-2026 fei_cong(https://github.com/feicong/feicong-course)
 */

#ifndef CDH_DEVICE_INFO_H
#define CDH_DEVICE_INFO_H

#include <linux/types.h>

/* 默认 UTS 数据文件位置：由 08_build_vendor_dlkm 从清单 rom.vendor.kernel.uname-version
 * 渲染并注入 vendor_dlkm.img 的 /lib/modules/，再由 09_build_patch 汇入 patch.img 的
 * /mnt/vendorpatch/vendor_dlkm/lib/modules/。
 * 注意：kernel_read_file_from_path() 走 filp_open -> getname_kernel，不经过 cdh_redirect
 * 挂在用户态路径入口的 getname_flags kretprobe，因此内核态直读优先取载体路径
 * CDH_PATCH_UTS_PATH，不存在时再回退 CDH_DEFAULT_UTS_PATH。
 */
#define CDH_DEFAULT_UTS_PATH "/vendor_dlkm/lib/modules/cdh_device_info.uts"
#define CDH_PATCH_UTS_PATH   "/mnt/vendorpatch/vendor_dlkm/lib/modules/cdh_device_info.uts"

/* 默认 cpufreq 数据文件位置：由 08_build_vendor_dlkm 从清单 modules.cpufreq 渲染并注入
 * vendor_dlkm.img 的 /lib/modules/，再由 09_build_patch 汇入 patch.img。
 * 未配置时文件不存在（-ENOENT），模块自动回退内置默认频点簇。
 */
#define CDH_DEFAULT_CPUFREQ_PATH "/vendor_dlkm/lib/modules/cdh_device_info.cpufreq"
#define CDH_PATCH_CPUFREQ_PATH   "/mnt/vendorpatch/vendor_dlkm/lib/modules/cdh_device_info.cpufreq"

/* 默认 thermal 数据文件位置：支持按机型提供温区列表，未提供时回退内置默认 31 个温区。 */
#define CDH_DEFAULT_THERMAL_PATH "/vendor_dlkm/lib/modules/cdh_device_info.thermal"
#define CDH_PATCH_THERMAL_PATH   "/mnt/vendorpatch/vendor_dlkm/lib/modules/cdh_device_info.thermal"

#define CDH_CPU_CORES_MAX 8

struct cdh_cpu_core_spec {
	unsigned int cpu;
	char bogo_mips[16];
	char implementer[16];
	unsigned int variant;
	char part[16];
	unsigned int revision;
	unsigned long capacity;
	u64 midr;
	u64 revidr;
	unsigned int cluster_id;
	unsigned int cluster_first_cpu;
	unsigned int cluster_last_cpu;
};

int cdh_cpufreq_load_once(const char **used_source);
const struct cdh_cpu_core_spec *cdh_get_cpu_core_spec(unsigned int cpu);
bool cdh_is_package_per_cluster(void);

int cdh_uts_init(void);
void cdh_uts_exit(void);

int cdh_meminfo_init(void);
void cdh_meminfo_exit(void);

int cdh_cpu_display_init(void);
void cdh_cpu_display_exit(void);

int cdh_cpu_topology_init(void);
void cdh_cpu_topology_exit(void);

int cdh_cpu_midr_sysfs_init(void);
void cdh_cpu_midr_sysfs_exit(void);

int cdh_cpu_cache_sysfs_init(void);
void cdh_cpu_cache_sysfs_exit(void);

int cdh_cpufreq_sysfs_init(void);
void cdh_cpufreq_sysfs_exit(void);

int cdh_time_uptime_init(void);
void cdh_time_uptime_exit(void);

int cdh_thermal_sysfs_init(void);
void cdh_thermal_sysfs_exit(void);

int cdh_ethtool_init(void);
void cdh_ethtool_exit(void);

int cdh_nfc_sysfs_init(void);
void cdh_nfc_sysfs_exit(void);

int cdh_sched_proc_init(void);
void cdh_sched_proc_exit(void);

#endif
