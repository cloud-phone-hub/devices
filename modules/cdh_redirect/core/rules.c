// SPDX-License-Identifier: GPL-2.0
/*
 * 规则表：装载、校验与单次前缀替换。
 *
 * 规则文件的语法（一行一条，# 起注释）：
 *
 *	dir  /vendor/etc   /mnt/vendorpatch/vendor/etc
 *	file /vendor/build.prop /mnt/vendorpatch/vendor/build.prop
 *
 *   - dir：前缀命中「精确相等」或「其后紧跟 '/'」，覆盖整棵子树；
 *   - file：只做精确匹配，不做子树；
 *   - 「屏蔽某个路径」= 把它指到一个一定不存在的目标（如
 *     /mnt/vendorpatch/system/bin/update_verifier），execv 拿到 ENOENT；
 *     `file X X` 自映射是它的对偶：命中后原样写回（grow == 0），语义是
 *     「这个路径本身不改写」——给 /odm 这类「路径本身是 init 挂载点、
 *     其下整棵要交给载体」的目录当护栏用（必须排在对应 dir 之前）。
 *
 * 逆变换不需要单独一张表：正向规则的 from/to 互换就是逆向规则（内核补丁里那两张
 * 手工对齐的表，正是这个互换关系）。
 */
#define pr_fmt(fmt)	"cdh_redirect: " fmt

#include <linux/compat.h>
#include <linux/dcache.h>
#include <linux/fs.h>
#include <linux/kernel.h>
#include <linux/kernel_read_file.h>
#include <linux/limits.h>
#include <linux/mm_types.h>
#include <linux/module.h>
#include <linux/rcupdate.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/vmalloc.h>

#ifdef CONFIG_ARM64
#include <linux/delay.h>
#include <linux/hardirq.h>
#ifndef in_hardirq
#define in_hardirq() in_irq()
#endif
#include <linux/mm.h>
#include <linux/mmap_lock.h>
#include <linux/sched/task_stack.h>
#include <linux/spinlock.h>
#include <linux/uaccess.h>
#include <asm/page.h>
#include <asm/processor.h>
#include <asm/ptrace.h>
#endif

#include "cdh_redirect.h"

struct cdh_rule cdh_rules[CDH_RULES_MAX];
int cdh_rule_count;
atomic64_t cdh_forward_hits = ATOMIC64_INIT(0);
atomic64_t cdh_reverse_hits = ATOMIC64_INIT(0);
atomic64_t cdh_uid_skipped = ATOMIC64_INIT(0);
atomic64_t cdh_forward_missed = ATOMIC64_INIT(0);
atomic64_t cdh_reverse_missed = ATOMIC64_INIT(0);
atomic64_t cdh_mount_hidden = ATOMIC64_INIT(0);
atomic64_t cdh_peergroup_rewrites = ATOMIC64_INIT(0);
atomic64_t cdh_dirent_hidden = ATOMIC64_INIT(0);
atomic64_t cdh_maps_ino_rewrites = ATOMIC64_INIT(0);
atomic64_t cdh_sphal_blocked = ATOMIC64_INIT(0);

/*
 * cdh_apply 做一次「前缀替换」：命中 from 就把 buf 里的 from 换成 to。
 *
 * 语义与内核补丁逐字一致：目录条目看「精确或其下任意路径」，文件条目只认精确；
 * 改写后（含结尾 NUL）必须落在 cap 之内，超长一律放弃（不截断、不半改）。
 *
 * 返回 1 = 已改写，0 = 不命中，-E2BIG = 命中但目标缓冲区放不下。
 */
static int cdh_apply(char *buf, size_t cap, const char *from, size_t flen,
		     const char *to, size_t tlen, bool dir)
{
	size_t len;
	ptrdiff_t grow;

	if (strncmp(buf, from, flen) != 0)
		return 0;
	if (dir) {
		if (buf[flen] != '\0' && buf[flen] != '/')
			return 0;
	} else if (buf[flen] != '\0') {
		return 0;
	}

	grow = (ptrdiff_t)tlen - (ptrdiff_t)flen;
	len = strlen(buf);
	if (grow == 0) {
		memcpy(buf, to, tlen);
		return 1;
	}
	if ((ptrdiff_t)len + grow >= (ptrdiff_t)cap)
		return -E2BIG;
	/* 尾巴（含 NUL）整体平移，再覆写新前缀；memmove 对两个方向的重叠都安全。 */
	memmove(buf + flen + grow, buf + flen, len - flen + 1);
	memcpy(buf, to, tlen);
	return 1;
}

enum cdh_sphal_tier {
	CDH_SPHAL_NONE = 0,
	CDH_SPHAL_CFG,
	CDH_SPHAL_TIER_A,
	CDH_SPHAL_TIER_B,
	CDH_SPHAL_VNDK_FALLBACK,
};

#define CDH_VMA_FLAG_LINKER	BIT(0)
#define CDH_VMA_FLAG_CARRIER	BIT(1)
#define CDH_VMA_FLAG_GFX_FW	BIT(2)

/* 按路径与当前系统调用 ABI 分类 SP-HAL 库级别（纯载体内部 vs 原厂同名入口/标准库回退）。 */
static enum cdh_sphal_tier cdh_classify_sphal_lib(const char *buf)
{
	static const char *const tier_b_exact[] = {
		"libapexsupport.so",
		"libgralloctypes.so",
		"android.hardware.graphics.common-V6-ndk.so",
		"android.hardware.graphics.common-V7-ndk.so",
		"libaconfig_storage_read_api_cc.so",
	};
	static const char *const tier_b_fallback[] = {
		"libdrm.so",
		"android.hardware.common-V2-ndk.so",
		"android.hardware.graphics.allocator-V1-ndk.so",
		"android.hardware.graphics.allocator-V2-ndk.so",
		"android.hardware.graphics.common-V3-ndk.so",
		"android.hardware.graphics.common-V4-ndk.so",
		"android.hardware.graphics.common-V5-ndk.so",
		"android.hardware.graphics.common@1.0.so",
		"android.hardware.graphics.common@1.1.so",
		"android.hardware.graphics.common@1.2.so",
		"android.hardware.graphics.mapper@2.0.so",
		"android.hardware.graphics.mapper@2.1.so",
		"android.hardware.graphics.mapper@3.0.so",
		"android.hardware.graphics.mapper@4.0.so",
		"libc++.so",
		"libbase.so",
		"libcutils.so",
		"libutils.so",
		"libhidlbase.so",
		"libhardware.so",
		"libz.so",
	};
	const char *sub;
	bool is_64;
	size_t i;

	if (!strcmp(buf, "/system_ext/lib64/libOpenCL_system.so")) {
		if (!in_compat_syscall() && cdh_maps_has_carrier_opencl(CDH_CARRIER_OPENCL_64))
			return CDH_SPHAL_TIER_B;
		return CDH_SPHAL_NONE;
	}
	if (!strcmp(buf, "/system_ext/lib/libOpenCL_system.so")) {
		if (in_compat_syscall() && cdh_maps_has_carrier_opencl(CDH_CARRIER_OPENCL_32))
			return CDH_SPHAL_TIER_B;
		return CDH_SPHAL_NONE;
	}

	if (strncmp(buf, "/vendor/lib64/", 14) == 0) {
		if (in_compat_syscall())
			return CDH_SPHAL_NONE;
		sub = buf + 14;
		is_64 = true;
	} else if (strncmp(buf, "/vendor/lib/", 12) == 0) {
		if (!in_compat_syscall())
			return CDH_SPHAL_NONE;
		sub = buf + 12;
		is_64 = false;
	} else {
		return CDH_SPHAL_NONE;
	}

	/* 仅当载体分区实际携带 OpenCL 兼容桩库时放行 SP-HAL 加载，避免其他机型落空。 */
	if (!strcmp(sub, "libOpenCL.so")) {
		if (cdh_maps_has_carrier_opencl(is_64 ? CDH_CARRIER_OPENCL_64
						      : CDH_CARRIER_OPENCL_32))
			return CDH_SPHAL_TIER_B;
		return CDH_SPHAL_NONE;
	}
	if (!strcmp(sub, "libOpenCL-pixel.so")) {
		if (cdh_maps_has_carrier_opencl(is_64 ? CDH_CARRIER_OPENCL_PIXEL_64
						      : CDH_CARRIER_OPENCL_PIXEL_32))
			return CDH_SPHAL_TIER_B;
		return CDH_SPHAL_NONE;
	}

	if (!strcmp(sub, "egl/gpu_id.cfg"))
		return CDH_SPHAL_CFG;

	/* Tier A：纯载体 Mesa/Virtio 及其独有的 libui/allocator@2..4 传递依赖（超图官方无这些库）。 */
	if (!strcmp(sub, "hw/vulkan.virtio.so") ||
	    !strcmp(sub, "hw/vulkan.pastel.so") ||
	    !strcmp(sub, "egl/libEGL_mesa.so") ||
	    !strcmp(sub, "egl/libGLESv1_CM_mesa.so") ||
	    !strcmp(sub, "egl/libGLESv2_mesa.so") ||
	    !strcmp(sub, "libgallium_dri.so") ||
	    !strcmp(sub, "libglapi.so") ||
	    !strcmp(sub, "libui.so") ||
	    !strcmp(sub, "android.hardware.graphics.allocator@2.0.so") ||
	    !strcmp(sub, "android.hardware.graphics.allocator@3.0.so") ||
	    !strcmp(sub, "android.hardware.graphics.allocator@4.0.so"))
		return CDH_SPHAL_TIER_A;

	if (!strcmp(sub, "hw") ||
	    !strcmp(sub, "hw/") ||
	    !strncmp(sub, "hw/mapper.", 10) ||
	    !strncmp(sub, "hw/android.hardware.graphics.mapper@", 36) ||
	    !strncmp(sub, "hw/gralloc.", 11) ||
	    !strncmp(sub, "hw/vulkan.", 10) ||
	    !strncmp(sub, "egl/libEGL_", 11) ||
	    !strncmp(sub, "egl/libGLES", 11))
		return CDH_SPHAL_TIER_B;

	for (i = 0; i < ARRAY_SIZE(tier_b_exact); i++) {
		if (!strcmp(sub, tier_b_exact[i]))
			return CDH_SPHAL_TIER_B;
	}

	for (i = 0; i < ARRAY_SIZE(tier_b_fallback); i++) {
		if (!strcmp(sub, tier_b_fallback[i]))
			return CDH_SPHAL_VNDK_FALLBACK;
	}

	return CDH_SPHAL_NONE;
}

#ifdef CONFIG_ARM64
static bool cdh_path_is_linker(const char *p, size_t len)
{
	if (len >= 13 && !strcmp(p + len - 13, "/bin/linker64"))
		return true;
	if (len >= 11 && !strcmp(p + len - 11, "/bin/linker"))
		return true;
	return false;
}

static bool cdh_path_is_gfx_framework(const char *p)
{
	static const char *const libs[] = {
		"libEGL.so",
		"libGLESv1_CM.so",
		"libGLESv2.so",
		"libGLESv3.so",
		"libvulkan.so",
		"libui.so",
		"libgui.so",
		"libhwui.so",
		"libgraphicsenv.so",
		"libhardware.so",
		"libvndksupport.so",
		"libhidlbase.so",
		"android.hardware.graphics.mapper@2.0.so",
		"android.hardware.graphics.mapper@2.1.so",
		"android.hardware.graphics.mapper@3.0.so",
		"android.hardware.graphics.mapper@4.0.so",
	};
	const char *base;
	size_t i;

	if (strncmp(p, "/system/lib64/", 14) != 0 &&
	    strncmp(p, "/system/lib/", 12) != 0 &&
	    strncmp(p, "/system_ext/lib64/", 18) != 0 &&
	    strncmp(p, "/system_ext/lib/", 16) != 0 &&
	    strncmp(p, "/apex/com.android.vndk.", 23) != 0)
		return false;

	base = strrchr(p, '/');
	if (!base || !base[1])
		return false;
	base++;
	for (i = 0; i < ARRAY_SIZE(libs); i++) {
		if (!strcmp(base, libs[i]))
			return true;
	}
	return false;
}

#define CDH_USER_VA_MASK	(DEFAULT_MAP_WINDOW_64 - 1UL)
#define CDH_LINKER_CACHE_SIZE	32

struct cdh_linker_vma_entry {
	struct mm_struct *mm;
	pid_t tgid;
	u64 start_time;
	unsigned long vm_start;
	unsigned long vm_end;
};

static struct cdh_linker_vma_entry cdh_linker_cache[CDH_LINKER_CACHE_SIZE];
static unsigned int cdh_linker_cache_next;
static DEFINE_SPINLOCK(cdh_linker_cache_lock);

static void cdh_linker_cache_record(struct mm_struct *mm,
				    unsigned long vm_start,
				    unsigned long vm_end)
{
	pid_t tgid = task_tgid_nr(current);
	u64 start_time = current->group_leader ? current->group_leader->start_time : 0;
	unsigned long flags;
	unsigned int i;

	spin_lock_irqsave(&cdh_linker_cache_lock, flags);
	for (i = 0; i < CDH_LINKER_CACHE_SIZE; i++) {
		struct cdh_linker_vma_entry *e = &cdh_linker_cache[i];

		if (e->mm == mm && e->tgid == tgid && e->start_time == start_time &&
		    e->vm_start == vm_start && e->vm_end == vm_end) {
			spin_unlock_irqrestore(&cdh_linker_cache_lock, flags);
			return;
		}
	}
	i = cdh_linker_cache_next++ % CDH_LINKER_CACHE_SIZE;
	cdh_linker_cache[i].mm = mm;
	cdh_linker_cache[i].tgid = tgid;
	cdh_linker_cache[i].start_time = start_time;
	cdh_linker_cache[i].vm_start = vm_start;
	cdh_linker_cache[i].vm_end = vm_end;
	spin_unlock_irqrestore(&cdh_linker_cache_lock, flags);
}

static bool cdh_linker_cache_hit(struct mm_struct *mm, u64 raw_pc, u64 raw_lr)
{
	unsigned long pc = (unsigned long)(raw_pc & CDH_USER_VA_MASK);
	unsigned long lr = (unsigned long)(raw_lr & CDH_USER_VA_MASK);
	pid_t tgid = task_tgid_nr(current);
	u64 start_time = current->group_leader ? current->group_leader->start_time : 0;
	unsigned long flags;
	unsigned int i;
	bool hit = false;

	spin_lock_irqsave(&cdh_linker_cache_lock, flags);
	for (i = 0; i < CDH_LINKER_CACHE_SIZE; i++) {
		const struct cdh_linker_vma_entry *e = &cdh_linker_cache[i];

		if (e->mm != mm || e->tgid != tgid || e->start_time != start_time)
			continue;
		if ((pc >= e->vm_start && pc < e->vm_end) ||
		    (lr >= e->vm_start && lr < e->vm_end)) {
			hit = true;
			break;
		}
	}
	spin_unlock_irqrestore(&cdh_linker_cache_lock, flags);
	return hit;
}

static unsigned int cdh_vma_classify_addr(struct mm_struct *mm, u64 raw_addr)
{
	unsigned long addr = (unsigned long)(raw_addr & CDH_USER_VA_MASK);
	struct vm_area_struct *vma;
	char pathbuf[256];
	char *p;
	size_t len;
	unsigned int flags = 0;

	if (addr < PAGE_SIZE)
		return 0;
	vma = find_vma(mm, addr);
	if (!vma || vma->vm_start > addr || !vma->vm_file)
		return 0;
	p = cdh_file_raw_path(vma->vm_file, pathbuf, sizeof(pathbuf));
	if (IS_ERR(p))
		return 0;
	len = strlen(p);
	if (cdh_path_is_linker(p, len)) {
		flags |= CDH_VMA_FLAG_LINKER;
		cdh_linker_cache_record(mm, vma->vm_start, vma->vm_end);
	}
	if (!strncmp(p, CDH_CARRIER_MOUNT_POINT "/", CDH_CARRIER_MOUNT_LEN + 1))
		flags |= CDH_VMA_FLAG_CARRIER;
	if (cdh_path_is_gfx_framework(p))
		flags |= CDH_VMA_FLAG_GFX_FW;
	return flags;
}

/*
 * 两级 SP-HAL 调用栈门控（ARM64）：
 *   - CFG (gpu_id.cfg)：仅允许载体库 (/mnt/vendorpatch/...) 通过调用链读取，拦截 App 直接探测；
 *   - Tier A (Mesa/Virtio 载体内部库)：必须 pc/lr 位于 linker 且 FP 链经过载体库，
 *     拦截 App 直接探测与绕过 interposer 的直连 dlopen；
 *   - Tier B (原厂同名 SP-HAL 入口)：必须 pc/lr 位于 linker 或调用链含载体/框架图形库，
 *     拦截 App Java/libc 直接 open/stat/md5sum（使其回落官方 super.img 拿到原厂字节与 MD5）。
 */
static bool cdh_sphal_caller_allowed(enum cdh_sphal_tier tier)
{
	struct mm_struct *mm = current->mm;
	const struct pt_regs *uregs;
	unsigned int pc_flags, lr_flags, chain_flags = 0;
	bool in_linker, has_carrier, has_gfx;
	u64 fp;
	int hop, tries;

	if (tier == CDH_SPHAL_NONE)
		return false;
	if (!mm || in_nmi() || in_hardirq() || in_serving_softirq())
		return false;
	uregs = task_pt_regs(current);
	if (!uregs || !user_mode(uregs))
		return false;

	if ((tier == CDH_SPHAL_TIER_B || tier == CDH_SPHAL_VNDK_FALLBACK) &&
	    cdh_linker_cache_hit(mm, uregs->pc, uregs->regs[30]))
		return true;

	for (tries = 0; tries < 82; tries++) {
		if (mmap_read_trylock(mm))
			break;
		if (tries >= 32)
			udelay(10);
		else
			cpu_relax();
	}
	if (tries == 82) {
		pr_warn_ratelimited("sphal trylock timeout pid=%d comm=%s tier=%d\n",
				    task_tgid_nr(current), current->comm, (int)tier);
		return false;
	}

	pc_flags = cdh_vma_classify_addr(mm, uregs->pc);
	lr_flags = cdh_vma_classify_addr(mm, uregs->regs[30]);
	chain_flags = pc_flags | lr_flags;

	fp = uregs->regs[29] & CDH_USER_VA_MASK;
	for (hop = 0; hop < 32 && fp >= PAGE_SIZE && !(fp & 7); hop++) {
		u64 frame[2];
		u64 next_fp;

		if (copy_from_user_nofault(frame, (const void __user *)(unsigned long)fp,
					   sizeof(frame)))
			break;
		chain_flags |= cdh_vma_classify_addr(mm, frame[1]);
		next_fp = frame[0] & CDH_USER_VA_MASK;
		if (next_fp <= fp || (next_fp - fp) > 0x40000)
			break;
		fp = next_fp;
	}
	mmap_read_unlock(mm);

	in_linker = !!((pc_flags | lr_flags) & CDH_VMA_FLAG_LINKER);
	has_carrier = !!(chain_flags & CDH_VMA_FLAG_CARRIER);
	has_gfx = !!(chain_flags & CDH_VMA_FLAG_GFX_FW);

	switch (tier) {
	case CDH_SPHAL_CFG:
		return has_carrier;
	case CDH_SPHAL_TIER_A:
		return in_linker && has_carrier;
	case CDH_SPHAL_TIER_B:
	case CDH_SPHAL_VNDK_FALLBACK:
		return in_linker || has_carrier || has_gfx;
	default:
		return false;
	}
}
#else
static bool cdh_sphal_caller_allowed(enum cdh_sphal_tier tier)
{
	return tier != CDH_SPHAL_NONE;
}
#endif

/*
 * cdh_gpu_maps_builtin 是底包预编译 Mesa/Virtio 载体库在 d_path / readlink 逆向出口的
 * 默认身份映射表（物理载体路径 -> 虚拟原厂路径）。
 *
 * 每项均满足：
 *   1) strlen(phys) > strlen(virt)，保证 d_path 原地缩小平移成立；
 *   2) CLOUD_MAP <= 原厂文件实测字节数，且全表与清单 maps 规则之间 1 对 1 唯一映射；
 *   3) 不与进程已挂载的标准 VNDK/HAL 库（如 libc++.so）碰撞。
 */
struct cdh_static_pair {
	const char *phys;
	const char *virt;
};

static const struct cdh_static_pair cdh_gpu_maps_builtin[] = {
	{ "/mnt/vendorpatch/vendor/lib64/egl/libEGL_mesa.so",       "/vendor/lib64/libRSCpuRef.so" },
	{ "/mnt/vendorpatch/vendor/lib64/egl/libGLESv1_CM_mesa.so", "/vendor/lib64/egl/libMEOW_gift.so" },
	{ "/mnt/vendorpatch/vendor/lib64/egl/libGLESv2_mesa.so",    "/vendor/lib64/libged.so" },
	{ "/mnt/vendorpatch/vendor/lib64/libgallium_dri.so",        "/vendor/lib64/egl/libGLES_mali.so" },
	{ "/mnt/vendorpatch/vendor/lib64/libglapi.so",              "/vendor/lib64/libgpud.so" },
	{ "/mnt/vendorpatch/vendor/lib64/hw/vulkan.virtio.so",      "/vendor/lib64/libc++_shared.so" },
	{ "/mnt/vendorpatch/vendor/lib/egl/libEGL_mesa.so",         "/vendor/lib/libRSCpuRef.so" },
	{ "/mnt/vendorpatch/vendor/lib/egl/libGLESv1_CM_mesa.so",   "/vendor/lib/egl/libMEOW_gift.so" },
	{ "/mnt/vendorpatch/vendor/lib/egl/libGLESv2_mesa.so",      "/vendor/lib/libged.so" },
	{ "/mnt/vendorpatch/vendor/lib/libgallium_dri.so",          "/vendor/lib/egl/libGLES_mali.so" },
	{ "/mnt/vendorpatch/vendor/lib/libglapi.so",                "/vendor/lib/libgpud.so" },
	{ "/mnt/vendorpatch/vendor/lib/hw/vulkan.virtio.so",        "/vendor/lib/libc++_shared.so" },
};

int cdh_gpu_maps_builtin_count(void)
{
	return (int)ARRAY_SIZE(cdh_gpu_maps_builtin);
}

const char *cdh_gpu_maps_builtin_virt(int idx)
{
	if (idx < 0 || idx >= (int)ARRAY_SIZE(cdh_gpu_maps_builtin))
		return NULL;
	return cdh_gpu_maps_builtin[idx].virt;
}

/*
 * 判定当前进程是否为 app_process64 / app_process32（如 zygote64 / zygote）。
 * 运行在 getname_flags kretprobe 原子上下文中，仅通过 RCU 读取 mm->exe_file
 * 的短文件名，绝不阻塞或触发页面错误。
 */
static bool cdh_is_app_process(void)
{
	struct mm_struct *mm = current->mm;
	struct file *exe;
	struct dentry *dentry;
	const unsigned char *name;
	bool match = false;

	if (!mm)
		return false;
	rcu_read_lock();
	exe = rcu_dereference(mm->exe_file);
	if (exe) {
		dentry = exe->f_path.dentry;
		if (dentry) {
			name = READ_ONCE(dentry->d_name.name);
			if (name && (!strcmp((const char *)name, "app_process64") ||
				     !strcmp((const char *)name, "app_process32")))
				match = true;
		}
	}
	rcu_read_unlock();
	return match;
}

bool cdh_rewrite_forward(char *buf, size_t cap)
{
	int i, ret;

	if (!buf || buf[0] != '/')
		return false;

	if (cdh_should_skip_uid()) {
		enum cdh_sphal_tier tier;

		/*
		 * 普通应用进程（uid > cdh_max_uid）直接访问 /mnt/vendorpatch 或 /data/adb
		 * （含其子路径）时，转向不存在路径，使其与无载体/无 root 管理目录的标准设备一致
		 * 返回 -ENOENT：
		 *   - 合法图形加载链访问 SP-HAL 时走上方两级调用栈门控，不经此分支；
		 *   - system/shell（如 adb shell su 访问 /data/adb/ksud）uid <= cdh_max_uid，不进此分支。
		 */
		if ((!strncmp(buf, CDH_CARRIER_MOUNT_POINT, CDH_CARRIER_MOUNT_LEN) &&
		     (buf[CDH_CARRIER_MOUNT_LEN] == '\0' ||
		      buf[CDH_CARRIER_MOUNT_LEN] == '/')) ||
		    (!strncmp(buf, "/data/adb", 9) &&
		     (buf[9] == '\0' || buf[9] == '/'))) {
			static const char blocked[] = "/mnt/.cdh_nonexistent";

			if (sizeof(blocked) <= cap) {
				memcpy(buf, blocked, sizeof(blocked));
				atomic64_inc(&cdh_mount_hidden);
				return true;
			}
			return false;
		}
		tier = cdh_classify_sphal_lib(buf);
		/* 若官方 /vendor 自带同名 4KB 原件且与载体代际兼容，标准库直通原件以保持文件页跨度一致；跨代 GRF 或缺失时回退载体。 */
		if (tier == CDH_SPHAL_VNDK_FALLBACK &&
		    cdh_maps_sphal_vndk_compat() &&
		    cdh_maps_has_real_file(buf))
			tier = CDH_SPHAL_NONE;
		if (tier == CDH_SPHAL_NONE || !cdh_sphal_caller_allowed(tier)) {
			if (tier != CDH_SPHAL_NONE)
				atomic64_inc(&cdh_sphal_blocked);
			for (i = 0; i < cdh_rule_count; i++) {
				struct cdh_rule *rule = &cdh_rules[i];

				if (rule->maps_only)
					continue;
				if (strncmp(buf, rule->from, rule->flen) != 0)
					continue;
				if (rule->dir) {
					if (buf[rule->flen] != '\0' && buf[rule->flen] != '/')
						continue;
				} else if (buf[rule->flen] != '\0') {
					continue;
				}
				atomic64_inc(&cdh_uid_skipped);
				if (cdh_debug)
					pr_info("skip forward (uid=%u > %u, tier=%d) %s\n",
						__kuid_val(current_uid()), cdh_max_uid,
						(int)tier, buf);
				break;
			}
			return false;
		}
	} else if (cdh_is_app_process()) {
		/*
		 * zygote64 / zygote 以 uid=0 运行并在 preloadOpenGL() 中预加载 SP-HAL 依赖，
		 * 普通应用 fork 后会继承其 VMA。若官方 /vendor 存在同名 4KB 原件且与载体代际兼容，
		 * 让 app_process 进程族同样回退直通原厂文件，保证子进程映射跨度一致；跨代 GRF 时走载体以免符号缺失。
		 */
		if (cdh_classify_sphal_lib(buf) == CDH_SPHAL_VNDK_FALLBACK &&
		    cdh_maps_sphal_vndk_compat() &&
		    cdh_maps_has_real_file(buf))
			return false;
	}

	for (i = 0; i < cdh_rule_count; i++) {
		struct cdh_rule *rule = &cdh_rules[i];

		if (rule->maps_only)
			continue;
		ret = cdh_apply(buf, cap, rule->from, rule->flen,
				rule->to, rule->tlen, rule->dir);
		if (ret == 0)
			continue;
		if (ret < 0) {
			atomic64_inc(&cdh_forward_missed);
			pr_warn_ratelimited("放不下（cap %zu）：%s\n", cap, buf);
			return false;
		}
		atomic64_inc(&cdh_forward_hits);
		if (cdh_debug)
			pr_info_ratelimited("forward %s\n", buf);
		return true;
	}
	return false;
}

bool cdh_rewrite_reverse(char *buf, size_t cap)
{
	int i;

	if (!buf || buf[0] != '/')
		return false;

	/* 1) 优先匹配机型显式配置的 maps 逆向覆盖规则。 */
	for (i = 0; i < cdh_rule_count; i++) {
		struct cdh_rule *rule = &cdh_rules[i];

		if (!rule->maps_only)
			continue;
		if (cdh_apply(buf, cap, rule->to, rule->tlen,
			      rule->from, rule->flen, false) == 1) {
			atomic64_inc(&cdh_reverse_hits);
			return true;
		}
	}

	/* 2) 在通用 dir /vendor 规则之前，将底包 Mesa/Virtio 载体库逆向还原为原厂驱动路径。 */
	for (i = 0; i < ARRAY_SIZE(cdh_gpu_maps_builtin); i++) {
		const struct cdh_static_pair *p = &cdh_gpu_maps_builtin[i];

		if (cdh_apply(buf, cap, p->phys, strlen(p->phys),
			      p->virt, strlen(p->virt), false) == 1) {
			atomic64_inc(&cdh_reverse_hits);
			return true;
		}
	}

	/* 3) 常规 file / dir 规则逆变换：物理前缀 -> 虚拟前缀。 */
	for (i = 0; i < cdh_rule_count; i++) {
		struct cdh_rule *rule = &cdh_rules[i];

		if (rule->maps_only)
			continue;
		if (cdh_apply(buf, cap, rule->to, rule->tlen,
			      rule->from, rule->flen, rule->dir) != 1)
			continue;
		atomic64_inc(&cdh_reverse_hits);
		return true;
	}
	return false;
}

void cdh_rules_free(void)
{
	int i;

	for (i = 0; i < CDH_RULES_MAX; i++) {
		kfree(cdh_rules[i].from);
		kfree(cdh_rules[i].to);
		cdh_rules[i].from = NULL;
		cdh_rules[i].to = NULL;
	}
	cdh_rule_count = 0;
}

/* cdh_add 追加一条规则；非法输入返回负错误码（调用方负责整表回滚）。 */
static int cdh_add(const char *kind, const char *from, const char *to, int line)
{
	struct cdh_rule *rule;
	size_t flen, tlen;

	if (cdh_rule_count >= CDH_RULES_MAX) {
		pr_err("规则超过 %d 条（第 %d 行），拒绝装载\n", CDH_RULES_MAX, line);
		return -E2BIG;
	}
	if (!from || !to || from[0] != '/' || to[0] != '/') {
		pr_err("第 %d 行：from/to 必须是绝对路径（%s -> %s）\n",
		       line, from ? from : "(空)", to ? to : "(空)");
		return -EINVAL;
	}
	flen = strlen(from);
	tlen = strlen(to);
	if (!flen || !tlen || flen >= CDH_RULE_PATH_MAX || tlen >= CDH_RULE_PATH_MAX) {
		pr_err("第 %d 行：路径长度越界（%zu -> %zu）\n", line, flen, tlen);
		return -EINVAL;
	}

	rule = &cdh_rules[cdh_rule_count];
	rule->maps_only = false;
	if (!strcmp(kind, "dir")) {
		rule->dir = true;
	} else if (!strcmp(kind, "file")) {
		rule->dir = false;
	} else if (!strcmp(kind, "maps")) {
		rule->dir = false;
		rule->maps_only = true;
	} else {
		pr_err("第 %d 行：类型只能是 dir、file 或 maps（给的是 %s）\n", line, kind);
		return -EINVAL;
	}

	rule->from = kstrdup(from, GFP_KERNEL);
	rule->to = kstrdup(to, GFP_KERNEL);
	if (!rule->from || !rule->to) {
		kfree(rule->from);
		kfree(rule->to);
		rule->from = NULL;
		rule->to = NULL;
		return -ENOMEM;
	}
	rule->flen = (u16)flen;
	rule->tlen = (u16)tlen;
	cdh_rule_count++;
	return 0;
}

/* split_fields 把一行按空格/制表符切成最多 max 个字段（就地写 NUL）。 */
static int split_fields(char *line, char *fields[], int max)
{
	char *walk = line;
	int count = 0;

	while (*walk && count < max) {
		while (*walk == ' ' || *walk == '\t' || *walk == '\r')
			walk++;
		if (!*walk)
			break;
		fields[count++] = walk;
		while (*walk && *walk != ' ' && *walk != '\t' && *walk != '\r')
			walk++;
		if (*walk)
			*walk++ = '\0';
	}
	return count;
}

/* cdh_parse_text 解析行式文本（规则文件与 inline 参数共用）。 */
static int cdh_parse_text(char *text, const char *origin)
{
	char *cursor = text;
	int line_no = 0;

	while (cursor && *cursor) {
		char *line = cursor;
		char *newline = strchr(cursor, '\n');
		char *fields[3];
		char *hash;
		int count;

		if (newline) {
			*newline = '\0';
			cursor = newline + 1;
		} else {
			cursor = NULL;
		}
		line_no++;

		hash = strchr(line, '#');
		if (hash)
			*hash = '\0';

		count = split_fields(line, fields, ARRAY_SIZE(fields));
		if (!count)
			continue;
		if (count != 3) {
			pr_err("%s 第 %d 行：字段数应为 3（类型 源 目标），实际 %d\n",
			       origin, line_no, count);
			cdh_rules_free();
			return -EINVAL;
		}
		if (cdh_add(fields[0], fields[1], fields[2], line_no)) {
			cdh_rules_free();
			return -EINVAL;
		}
	}
	return 0;
}

static int cdh_load_from_file(const char *path)
{
	void *data = NULL;
	size_t size = 0;
	char *text;
	ssize_t read;
	int ret;

	/*
	 * 注意返回值语义：6.1 的 kernel_read_file_from_path() 成功时返回**读到的
	 * 字节数**（kernel_read_file() 的 "ret == 0 ? copied : ret"），只有负数
	 * 才是错误。按"非零即错"写会把 2377 字节的规则文件当成失败，规则表留空。
	 */
	read = kernel_read_file_from_path(path, 0, &data, INT_MAX, &size,
					  READING_UNKNOWN);
	if (read < 0) {
		pr_err("读不到规则文件 %s（%zd）\n", path, read);
		return (int)read;
	}
	if (!size || size > 64 * 1024) {
		pr_err("规则文件 %s 大小异常（%zu B）\n", path, size);
		vfree(data);
		return -EINVAL;
	}
	/* 补一个 NUL 再解析：文件正文本身不保证以 NUL 结尾。 */
	text = kmalloc(size + 1, GFP_KERNEL);
	if (!text) {
		vfree(data);
		return -ENOMEM;
	}
	memcpy(text, data, size);
	text[size] = '\0';
	vfree(data);

	ret = cdh_parse_text(text, path);
	kfree(text);
	return ret;
}

/* inline 参数：dir:/from:/to;file:/from:/to（调试按需覆盖，不进正式镜像）。 */
static int cdh_load_from_param(const char *param)
{
	char *copy, *record, *save_record;
	int ret = 0;
	int line = 0;

	copy = kstrdup(param, GFP_KERNEL);
	if (!copy)
		return -ENOMEM;

	save_record = copy;
	while ((record = strsep(&save_record, ";"))) {
		char *kind, *from, *to, *save_field;

		if (!*record)
			continue;
		save_field = record;
		kind = strsep(&save_field, ":");
		from = strsep(&save_field, ":");
		to = strsep(&save_field, ":");
		line++;
		if (!kind || !from || !to) {
			pr_err("规则参数第 %d 条格式应为 类型:源:目标\n", line);
			ret = -EINVAL;
			break;
		}
		ret = cdh_add(kind, from, to, line);
		if (ret)
			break;
	}
	kfree(copy);
	if (ret) {
		cdh_rules_free();
		return ret;
	}
	return 0;
}

int cdh_rules_load(const char *path, const char *inline_rules)
{
	int ret;

	cdh_rules_free();

	if (inline_rules && *inline_rules) {
		ret = cdh_load_from_param(inline_rules);
		if (ret)
			return ret;
		pr_info("规则来自模块参数：%d 条\n", cdh_rule_count);
		return cdh_rule_count;
	}

	ret = cdh_load_from_file(path);
	if (ret)
		return ret;
	pr_info("规则来自 %s：%d 条\n", path, cdh_rule_count);
	return cdh_rule_count;
}
