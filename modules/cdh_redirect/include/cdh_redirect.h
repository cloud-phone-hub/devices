/* SPDX-License-Identifier: GPL-2.0 */
/*
 * cdh_redirect：把「路径重定向白名单」从内核源码里搬出来，做成外部模块。
 *
 * 内核里原来的做法是 fs/namei.c 的 CONFIG_CDH_PATH_REDIRECT：在 getname_flags()
 * 内就地改写 kname，再在 fs/proc/base.c 的 do_proc_readlink() 里做逆变换。那份实现
 * 每换一个机型就要改内核树 + 重编 Image，规则也不能按设备配置。
 *
 * 本模块用两个 kretprobe 复刻同一套语义，内核保持原样：
 *   1) getname_flags 返回处 —— 拿到 struct filename *，改写 f->name（内核内存，
 *      与内核补丁同一处缓冲区），实现 /vendor/etc -> /mnt/vendorpatch/vendor/etc；
 *   2) proc_pid_readlink / vfs_readlink 返回处 —— /proc/<pid>/{exe,cwd,root,fd/N}
 *      的 readlink 出口把物理路径还原成虚拟路径（bionic linker 靠 realpath 选
 *      ld.config 段）。两层都挂：前者覆盖 proc 魔术链接，后者是 readlink(2)
 *      的公共出口（分属两个编译单元，必然真实调用），实测缺一不可。
 *
 * 规则来自数据文件（默认 /lib/modules/cdh_redirect.rules），按机型一份；模块本身
 * 与机型无关，二进制对所有设备通用，只要规则不同。
 */
#ifndef _CDH_REDIRECT_H
#define _CDH_REDIRECT_H

#include <linux/bits.h>
#include <linux/cred.h>
#include <linux/sched.h>
#include <linux/types.h>
#include <linux/uidgid.h>

struct file;

/* 规则表上限：够 32 条，超出的行在解析时报错而不是被静默丢弃。 */
#define CDH_RULES_MAX		32
/* 规则里单条路径的长度上限（够 PATH_MAX 的一半，重定向只做前缀替换）。 */
#define CDH_RULE_PATH_MAX	2048
/* 默认规则文件位置：由 05_build_vendor_boot 注入 vendor_ramdisk 根级 lib/modules/。 */
#define CDH_DEFAULT_RULES_PATH	"/lib/modules/cdh_redirect.rules"
/* 载体物理挂载点（与 build/internal/carrier.MountPoint 保持一致）。 */
#define CDH_CARRIER_MOUNT_POINT	"/mnt/vendorpatch"
#define CDH_CARRIER_MOUNT_LEN	16
/* 默认最大生效 UID：大于此 UID（如普通 App uid > 10000）的进程不做路径重定向。 */
#define CDH_DEFAULT_MAX_UID	10000

struct cdh_rule {
	char *from;	/* 命中的源前缀（虚拟路径） */
	char *to;	/* 替换成的目标前缀（物理路径） */
	u16 flen;
	u16 tlen;
	bool dir;	/* true：目录（前缀或其后紧跟 '/'）；false：文件（精确匹配） */
	bool maps_only;	/* true：仅在 d_path / readlink 逆向出口生效，不做前向重定向 */
};

extern struct cdh_rule cdh_rules[CDH_RULES_MAX];
extern int cdh_rule_count;
/* 重定向生效的最大 UID（module_param max_uid，缺省 10000）。 */
extern unsigned int cdh_max_uid;
/* 命中次数 / 因 uid > max_uid 跳过次数 / 命中但改写放不下的次数（后者异常，正常为 0）。 */
extern atomic64_t cdh_forward_hits;
extern atomic64_t cdh_reverse_hits;
extern atomic64_t cdh_uid_skipped;
extern atomic64_t cdh_forward_missed;
extern atomic64_t cdh_reverse_missed;
/* 挂载表与目录项过滤统计（uid > max_uid 时生效）。 */
extern atomic64_t cdh_mount_hidden;
extern atomic64_t cdh_peergroup_rewrites;
extern atomic64_t cdh_dirent_hidden;
/* /proc/<pid>/maps dev:ino 还原与 SP-HAL 调用栈门控拦截统计。 */
extern atomic64_t cdh_maps_ino_rewrites;
extern atomic64_t cdh_sphal_blocked;
/* 调试开关（module_param debug=1）：打开后逆向钩子逐次打印改写前后串。 */
extern bool cdh_debug;

/* 判断当前进程是否因 UID 超过上限而不做重定向（uid > cdh_max_uid 时跳过）。 */
static inline bool cdh_should_skip_uid(void)
{
	uid_t uid = __kuid_val(current_uid());

	return uid > cdh_max_uid;
}

/* 解析并装载规则：inline_rules 非空时只用它，否则读 path。成功返回规则条数。 */
int cdh_rules_load(const char *path, const char *inline_rules);
void cdh_rules_free(void);

/* 前向（查找）：命中的话就地改写 buf，返回 true。cap 是 buf 的真实容量。 */
bool cdh_rewrite_forward(char *buf, size_t cap);
/* 逆向（readlink 出口）：命中的话就地改写 buf，返回 true。 */
bool cdh_rewrite_reverse(char *buf, size_t cap);

/* 内部取 file 原始物理路径（绕过 d_path 逆向钩子）。 */
char *cdh_file_raw_path(struct file *file, char *buf, int buflen);
/* 供 maps.c 预热缓存读取内置 Mesa/Virtio 虚拟映射目标。 */
int cdh_gpu_maps_builtin_count(void);
const char *cdh_gpu_maps_builtin_virt(int idx);
/* 查询 /vendor 标准库在底层只读分区是否存在 4KB 对齐原件（未就绪时异步入队）。 */
bool cdh_maps_has_real_file(const char *path);
/* 判断原厂 /vendor 与载体 /mnt/vendorpatch/vendor 的图形 NDK 代际是否一致。 */
bool cdh_maps_sphal_vndk_compat(void);
/* 载体分区 OpenCL 桩库存在掩码位定义。 */
#define CDH_CARRIER_OPENCL_64		BIT(0)
#define CDH_CARRIER_OPENCL_PIXEL_64	BIT(1)
#define CDH_CARRIER_OPENCL_32		BIT(2)
#define CDH_CARRIER_OPENCL_PIXEL_32	BIT(3)
/* 判断载体分区是否提供指定的 OpenCL 兼容桩库（避免未提供该库的机型落空 ENOENT）。 */
bool cdh_maps_has_carrier_opencl(unsigned int bit);

int cdh_namei_hook_init(void);
void cdh_namei_hook_exit(void);
int cdh_readlink_hook_init(void);
void cdh_readlink_hook_exit(void);
int cdh_mountinfo_hook_init(void);
void cdh_mountinfo_hook_exit(void);
int cdh_maps_hook_init(void);
void cdh_maps_hook_exit(void);

/* 逆向钩子有两个挂点，诊断计数按挂点分开统计（/proc/cdh_redirect 展示）。 */
#define CDH_READLINK_HOOK_PROC	0
#define CDH_READLINK_HOOK_VFS	1
#define CDH_READLINK_HOOK_NUM	2
struct cdh_readlink_stats {
	u64 entries;	/* 入口钩子命中次数 */
	u64 rets;	/* 返回钩子命中次数 */
	u64 skipped;	/* 返回时无需改写或改写失败 */
	u64 hits;	/* 成功写回用户缓冲区 */
};
void cdh_readlink_stats_read(int which, struct cdh_readlink_stats *out);

#endif /* _CDH_REDIRECT_H */
