// SPDX-License-Identifier: GPL-2.0
/*
 * hook/maps.c - /proc/<pid>/maps 与 smaps 的 dev:ino 真实化钩子
 *
 * 将落在载体分区 /mnt/vendorpatch/ 下的 VMA 行（d_path 已逆向改写为 /vendor/...）
 * 的 dev:ino 字段从 patch.img (fe:20 <patch_ino>) 替换为 /vendor 官方镜像
 * （super.img erofs，如 fd:05 <real_ino>）上对应路径的真实设备号与 inode 号。
 *
 * 因 show_map_vma 在原子/锁上下文返回，禁止在 kretprobe 内直接调用可能睡眠的
 * kern_path()；改用受自旋锁保护的静态缓存 + work_struct 异步解析。同时由于
 * cdh_redirect.ko 在 /vendor 挂载前的 first-stage init 加载，使用 delayed_work
 * 轮询 /vendor/build.prop 就绪后批量预热内建/配置规则及常见 /vendor/lib64 共享库。
 */
#define pr_fmt(fmt)	"cdh_redirect: " fmt

#include "cdh_redirect.h"

#include <linux/atomic.h>
#include <linux/dcache.h>
#include <linux/fs.h>
#include <linux/kdev_t.h>
#include <linux/kernel.h>
#include <linux/kprobes.h>
#include <linux/mm_types.h>
#include <linux/module.h>
#include <linux/namei.h>
#include <linux/path.h>
#include <linux/seq_file.h>
#include <linux/spinlock.h>
#include <linux/string.h>
#include <linux/version.h>
#include <linux/workqueue.h>

#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 13, 0)
MODULE_IMPORT_NS("VFS_internal_I_am_really_a_filesystem_and_am_NOT_a_driver");
#else
MODULE_IMPORT_NS(VFS_internal_I_am_really_a_filesystem_and_am_NOT_a_driver);
#endif

#define CDH_MAPS_CACHE_MAX	256
#define CDH_MAPS_PATH_MAX	256
#define CDH_MAPS_KRETPROBE_MAX	128
#define CDH_MAPS_PREWARM_MS	500
#define CDH_MAPS_PREWARM_TRIES	120

enum cdh_maps_state {
	CDH_MAPS_PENDING = 0,
	CDH_MAPS_READY,
	CDH_MAPS_NEGATIVE,
};

struct cdh_maps_cache_entry {
	char path[CDH_MAPS_PATH_MAX];
	dev_t dev;
	unsigned long ino;
	u8 state;
};

struct cdh_maps_data {
	struct seq_file *m;
	size_t orig_count;
};

static struct cdh_maps_cache_entry cdh_maps_cache[CDH_MAPS_CACHE_MAX];
static int cdh_maps_cache_count;
static DEFINE_SPINLOCK(cdh_maps_lock);

static bool cdh_maps_registered;
static bool cdh_maps_vendor_ready;
static bool cdh_maps_prewarm_logged;
static bool cdh_sphal_vndk_compat;
static unsigned int cdh_carrier_opencl_mask;
static atomic_t cdh_maps_exiting = ATOMIC_INIT(0);
static int cdh_maps_prewarm_tries;

static void cdh_maps_work_fn(struct work_struct *work);
static void cdh_maps_prewarm_fn(struct work_struct *work);

static DECLARE_WORK(cdh_maps_work, cdh_maps_work_fn);
static DECLARE_DELAYED_WORK(cdh_maps_prewarm_work, cdh_maps_prewarm_fn);

/*
 * /vendor 挂载完成后需要预先解析真实 super.img (dev, ino) 的高频共享库清单，
 * 避免 App 首次读取 /proc/self/maps 时因缓存未就绪漏改任何一行。
 */
static const char * const cdh_maps_prewarm_libs[] = {
	"/vendor/lib64/android.hardware.common-V2-ndk.so",
	"/vendor/lib64/android.hardware.graphics.allocator-V1-ndk.so",
	"/vendor/lib64/android.hardware.graphics.allocator-V2-ndk.so",
	"/vendor/lib64/android.hardware.graphics.common-V3-ndk.so",
	"/vendor/lib64/android.hardware.graphics.common-V4-ndk.so",
	"/vendor/lib64/android.hardware.graphics.common-V5-ndk.so",
	"/vendor/lib64/android.hardware.graphics.common-V6-ndk.so",
	"/vendor/lib64/android.hardware.graphics.common-V7-ndk.so",
	"/vendor/lib64/libaconfig_storage_read_api_cc.so",
	"/vendor/lib64/android.hardware.graphics.common@1.0.so",
	"/vendor/lib64/android.hardware.graphics.common@1.1.so",
	"/vendor/lib64/android.hardware.graphics.common@1.2.so",
	"/vendor/lib64/android.hardware.graphics.mapper@2.0.so",
	"/vendor/lib64/android.hardware.graphics.mapper@2.1.so",
	"/vendor/lib64/android.hardware.graphics.mapper@3.0.so",
	"/vendor/lib64/android.hardware.graphics.mapper@4.0.so",
	"/vendor/lib64/arm.graphics-V5-ndk.so",
	"/vendor/lib64/egl/libGLES_mali.so",
	"/vendor/lib64/egl/libGLES_meow.so",
	"/vendor/lib64/egl/libMEOW_data.so",
	"/vendor/lib64/egl/libMEOW_gift.so",
	"/vendor/lib64/egl/libMEOW_qt.so",
	"/vendor/lib64/hw/gralloc.default.so",
	"/vendor/lib64/hw/mapper.mediatek.so",
	"/vendor/lib64/hw/vulkan.mali.so",
	"/vendor/lib64/libc++.so",
	"/vendor/lib64/libc++_shared.so",
	"/vendor/lib64/libbase.so",
	"/vendor/lib64/libcutils.so",
	"/vendor/lib64/libdmabufheap.so",
	"/vendor/lib64/libdrm.so",
	"/vendor/lib64/libFrameRecord.so",
	"/vendor/lib64/libRSCpuRef.so",
	"/vendor/lib64/libaal_sec.so",
	"/vendor/lib64/libged.so",
	"/vendor/lib64/libgpu_aux.so",
	"/vendor/lib64/libgpud.so",
	"/vendor/lib64/libgpudataproducer.so",
	"/vendor/lib64/libgralloc_extra.so",
	"/vendor/lib64/libgralloc_metadata.so",
	"/vendor/lib64/libgralloctypes.so",
	"/vendor/lib64/libgralloctypes_mtk.so",
	"/vendor/lib64/libhardware.so",
	"/vendor/lib64/libhidlbase.so",
	"/vendor/lib64/liblzma.so",
	"/vendor/lib64/libOpenCL.so",
	"/vendor/lib64/libOpenCL-pixel.so",
	"/vendor/lib64/libAlgoProcess.so",
	"/odm/lib64/libAlgoProcess.so",
	"/vendor/lib64/libpng.so",
	"/vendor/lib64/libunwindstack.so",
	"/vendor/lib64/libutils.so",
	"/vendor/lib64/libz.so",
	"/vendor/lib64/mt6833/libdpframework.so",
	"/dev/__properties__/u:object_r:vendor_mtk_soc_prop:s0",
	"/dev/__properties__/u:object_r:vendor_mtk_camera_prop:s0",
	"/vendor/lib/android.hardware.common-V2-ndk.so",
	"/vendor/lib/android.hardware.graphics.allocator-V1-ndk.so",
	"/vendor/lib/android.hardware.graphics.allocator-V2-ndk.so",
	"/vendor/lib/android.hardware.graphics.common-V3-ndk.so",
	"/vendor/lib/android.hardware.graphics.common-V4-ndk.so",
	"/vendor/lib/android.hardware.graphics.common-V5-ndk.so",
	"/vendor/lib/android.hardware.graphics.common-V6-ndk.so",
	"/vendor/lib/android.hardware.graphics.common-V7-ndk.so",
	"/vendor/lib/android.hardware.graphics.common@1.0.so",
	"/vendor/lib/android.hardware.graphics.common@1.1.so",
	"/vendor/lib/android.hardware.graphics.common@1.2.so",
	"/vendor/lib/android.hardware.graphics.mapper@2.0.so",
	"/vendor/lib/android.hardware.graphics.mapper@2.1.so",
	"/vendor/lib/android.hardware.graphics.mapper@3.0.so",
	"/vendor/lib/android.hardware.graphics.mapper@4.0.so",
	"/vendor/lib/libc++.so",
	"/vendor/lib/libbase.so",
	"/vendor/lib/libcutils.so",
	"/vendor/lib/libdrm.so",
	"/vendor/lib/libhardware.so",
	"/vendor/lib/libhidlbase.so",
	"/vendor/lib/libOpenCL.so",
	"/vendor/lib/libOpenCL-pixel.so",
	"/vendor/lib/libutils.so",
	"/vendor/lib/libz.so",
};

static bool cdh_maps_enqueue_locked(const char *path)
{
	int i;
	struct cdh_maps_cache_entry *e;

	if (!path || path[0] != '/' || strlen(path) >= CDH_MAPS_PATH_MAX)
		return false;

	for (i = 0; i < cdh_maps_cache_count; i++) {
		if (strcmp(cdh_maps_cache[i].path, path) == 0)
			return false;
	}
	if (cdh_maps_cache_count >= CDH_MAPS_CACHE_MAX)
		return false;

	e = &cdh_maps_cache[cdh_maps_cache_count++];
	strscpy(e->path, path, CDH_MAPS_PATH_MAX);
	e->dev = 0;
	e->ino = 0;
	e->state = CDH_MAPS_PENDING;
	return true;
}

/* 查询缓存；未命中则入队交由工作队列异步解析，返回是否已命中 READY 项。 */
static bool cdh_maps_lookup_or_queue(const char *path, dev_t *out_dev,
				     unsigned long *out_ino)
{
	unsigned long flags;
	int i;
	bool hit = false;
	bool queued = false;

	if (!path || strlen(path) >= CDH_MAPS_PATH_MAX)
		return false;

	spin_lock_irqsave(&cdh_maps_lock, flags);
	for (i = 0; i < cdh_maps_cache_count; i++) {
		if (strcmp(cdh_maps_cache[i].path, path) == 0) {
			if (cdh_maps_cache[i].state == CDH_MAPS_READY) {
				*out_dev = cdh_maps_cache[i].dev;
				*out_ino = cdh_maps_cache[i].ino;
				hit = true;
			}
			goto unlock;
		}
	}
	if (!atomic_read(&cdh_maps_exiting))
		queued = cdh_maps_enqueue_locked(path);
unlock:
	spin_unlock_irqrestore(&cdh_maps_lock, flags);

	if (queued && !atomic_read(&cdh_maps_exiting))
		schedule_work(&cdh_maps_work);
	return hit;
}

bool cdh_maps_has_real_file(const char *path)
{
	dev_t dummy_dev;
	unsigned long dummy_ino;

	return cdh_maps_lookup_or_queue(path, &dummy_dev, &dummy_ino);
}

bool cdh_maps_sphal_vndk_compat(void)
{
	return READ_ONCE(cdh_sphal_vndk_compat);
}

bool cdh_maps_has_carrier_opencl(unsigned int bit)
{
	return !!(READ_ONCE(cdh_carrier_opencl_mask) & bit);
}

static void cdh_maps_work_fn(struct work_struct *work)
{
	char tmp[CDH_MAPS_PATH_MAX];
	unsigned long flags;
	bool vendor_ready;

	for (;;) {
		int idx = -1;
		int i;
		int ready_cnt = 0;
		int total_cnt = 0;
		bool do_log = false;
		struct path p;
		int err;
		dev_t dev = 0;
		unsigned long ino = 0;
		u8 next_state = CDH_MAPS_PENDING;

		if (atomic_read(&cdh_maps_exiting))
			return;

		spin_lock_irqsave(&cdh_maps_lock, flags);
		vendor_ready = cdh_maps_vendor_ready;
		for (i = 0; i < cdh_maps_cache_count; i++) {
			if (cdh_maps_cache[i].state == CDH_MAPS_PENDING) {
				strscpy(tmp, cdh_maps_cache[i].path,
					sizeof(tmp));
				idx = i;
				break;
			}
		}
		if (idx < 0 && vendor_ready && !cdh_maps_prewarm_logged) {
			cdh_maps_prewarm_logged = true;
			total_cnt = cdh_maps_cache_count;
			for (i = 0; i < cdh_maps_cache_count; i++) {
				if (cdh_maps_cache[i].state == CDH_MAPS_READY)
					ready_cnt++;
			}
			do_log = true;
		}
		spin_unlock_irqrestore(&cdh_maps_lock, flags);

		if (idx < 0) {
			if (do_log)
				pr_info("maps 缓存预热完成：ready=%d/%d\n",
					ready_cnt, total_cnt);
			return;
		}

		/* kern_path 内部走 getname_kernel()，不会触发用户态 getname_flags 重定向。 */
		err = kern_path(tmp, LOOKUP_FOLLOW, &p);
		if (err == 0) {
			struct inode *inode = d_backing_inode(p.dentry);

			if (inode && inode->i_sb) {
				dev = inode->i_sb->s_dev;
				ino = inode->i_ino;
				next_state = CDH_MAPS_READY;
			} else if (vendor_ready) {
				next_state = CDH_MAPS_NEGATIVE;
			}
			path_put(&p);
		} else if (vendor_ready) {
			next_state = CDH_MAPS_NEGATIVE;
		}

		spin_lock_irqsave(&cdh_maps_lock, flags);
		if (idx < cdh_maps_cache_count &&
		    strcmp(cdh_maps_cache[idx].path, tmp) == 0) {
			if (next_state == CDH_MAPS_READY) {
				cdh_maps_cache[idx].dev = dev;
				cdh_maps_cache[idx].ino = ino;
				cdh_maps_cache[idx].state = CDH_MAPS_READY;
			} else if (next_state == CDH_MAPS_NEGATIVE) {
				cdh_maps_cache[idx].state = CDH_MAPS_NEGATIVE;
			}
		}
		spin_unlock_irqrestore(&cdh_maps_lock, flags);

		/* /vendor 尚未就绪前只处理当前批次，不原地空转。 */
		if (!vendor_ready && next_state == CDH_MAPS_PENDING)
			return;
	}
}

static bool cdh_probe_sphal_vndk_compat(void)
{
	static const char *const vers[] = { "V4", "V5", "V6", "V7", "V8", "V9" };
	char carrier_path[CDH_MAPS_PATH_MAX];
	char official_path[CDH_MAPS_PATH_MAX];
	struct path p, cp, op;
	bool found_any = false;
	size_t i;

	if (kern_path(CDH_CARRIER_MOUNT_POINT "/vendor/lib64", LOOKUP_FOLLOW, &p) != 0)
		return false;
	path_put(&p);

	for (i = 0; i < ARRAY_SIZE(vers); i++) {
		snprintf(carrier_path, sizeof(carrier_path),
			 CDH_CARRIER_MOUNT_POINT "/vendor/lib64/android.hardware.graphics.common-%s-ndk.so",
			 vers[i]);
		if (kern_path(carrier_path, LOOKUP_FOLLOW, &cp) != 0)
			continue;
		path_put(&cp);
		found_any = true;

		snprintf(official_path, sizeof(official_path),
			 "/vendor/lib64/android.hardware.graphics.common-%s-ndk.so",
			 vers[i]);
		if (kern_path(official_path, LOOKUP_FOLLOW, &op) == 0) {
			path_put(&op);
		} else {
			pr_info("跨代 GRF vendor（原厂缺 %s），SP-HAL 标准库走载体\n",
				official_path);
			return false;
		}
	}
	return found_any;
}

static unsigned int cdh_probe_carrier_opencl_mask(void)
{
	static const struct {
		const char *path;
		unsigned int bit;
	} probes[] = {
		{ CDH_CARRIER_MOUNT_POINT "/vendor/lib64/libOpenCL.so",
		  CDH_CARRIER_OPENCL_64 },
		{ CDH_CARRIER_MOUNT_POINT "/vendor/lib64/libOpenCL-pixel.so",
		  CDH_CARRIER_OPENCL_PIXEL_64 },
		{ CDH_CARRIER_MOUNT_POINT "/vendor/lib/libOpenCL.so",
		  CDH_CARRIER_OPENCL_32 },
		{ CDH_CARRIER_MOUNT_POINT "/vendor/lib/libOpenCL-pixel.so",
		  CDH_CARRIER_OPENCL_PIXEL_32 },
		{ CDH_CARRIER_MOUNT_POINT "/vendor/lib64/libAlgoProcess.so",
		  CDH_CARRIER_ALGO_PROCESS_64 },
	};
	unsigned int mask = 0;
	struct path p;
	size_t i;

	for (i = 0; i < ARRAY_SIZE(probes); i++) {
		if (kern_path(probes[i].path, LOOKUP_FOLLOW, &p) == 0) {
			path_put(&p);
			mask |= probes[i].bit;
		}
	}
	if (mask)
		pr_info("载体 OpenCL 桩库就绪：mask=0x%x\n", mask);
	return mask;
}

static void cdh_maps_prewarm_fn(struct work_struct *work)
{
	struct path p;
	unsigned long flags;
	int i;
	int builtin_cnt;
	bool need_work = false;
	const char *carrier_probe = NULL;

	if (atomic_read(&cdh_maps_exiting))
		return;

	if (kern_path("/vendor/build.prop", LOOKUP_FOLLOW, &p) != 0) {
		if (++cdh_maps_prewarm_tries < CDH_MAPS_PREWARM_TRIES &&
		    !atomic_read(&cdh_maps_exiting))
			schedule_delayed_work(&cdh_maps_prewarm_work,
					      msecs_to_jiffies(CDH_MAPS_PREWARM_MS));
		return;
	}
	path_put(&p);

	for (i = 0; i < cdh_rule_count; i++) {
		if (!cdh_rules[i].maps_only && cdh_rules[i].to &&
		    strncmp(cdh_rules[i].to, CDH_CARRIER_MOUNT_POINT "/",
			    CDH_CARRIER_MOUNT_LEN + 1) == 0) {
			if (cdh_rules[i].dir) {
				carrier_probe = cdh_rules[i].to;
				break;
			}
			if (!carrier_probe)
				carrier_probe = cdh_rules[i].to;
		}
	}
	if (carrier_probe) {
		if (kern_path(carrier_probe, LOOKUP_FOLLOW, &p) != 0) {
			if (++cdh_maps_prewarm_tries < CDH_MAPS_PREWARM_TRIES &&
			    !atomic_read(&cdh_maps_exiting))
				schedule_delayed_work(&cdh_maps_prewarm_work,
						      msecs_to_jiffies(CDH_MAPS_PREWARM_MS));
			return;
		}
		path_put(&p);
	}

	WRITE_ONCE(cdh_sphal_vndk_compat, cdh_probe_sphal_vndk_compat());
	WRITE_ONCE(cdh_carrier_opencl_mask, cdh_probe_carrier_opencl_mask());

	builtin_cnt = cdh_gpu_maps_builtin_count();

	spin_lock_irqsave(&cdh_maps_lock, flags);
	cdh_maps_vendor_ready = true;

	for (i = 0; i < builtin_cnt; i++) {
		const char *virt = cdh_gpu_maps_builtin_virt(i);

		if (virt && strncmp(virt, "/vendor/", 8) == 0 &&
		    cdh_maps_enqueue_locked(virt))
			need_work = true;
	}

	for (i = 0; i < cdh_rule_count; i++) {
		if (!cdh_rules[i].dir && cdh_rules[i].from &&
		    strncmp(cdh_rules[i].from, "/vendor/", 8) == 0 &&
		    cdh_maps_enqueue_locked(cdh_rules[i].from))
			need_work = true;
	}

	for (i = 0; i < ARRAY_SIZE(cdh_maps_prewarm_libs); i++) {
		if (cdh_maps_enqueue_locked(cdh_maps_prewarm_libs[i]))
			need_work = true;
	}

	/* 把 /vendor 挂载前已入队的 PENDING 项也一并交给工作队列重扫。 */
	for (i = 0; i < cdh_maps_cache_count; i++) {
		if (cdh_maps_cache[i].state == CDH_MAPS_PENDING) {
			need_work = true;
			break;
		}
	}
	spin_unlock_irqrestore(&cdh_maps_lock, flags);

	if (need_work && !atomic_read(&cdh_maps_exiting))
		schedule_work(&cdh_maps_work);
}

#define CDH_FAKE_MALI0_PATH	"/dev/mali0"
#define CDH_FAKE_MALI0_INO	1562UL

struct cdh_dev_map_rule {
	const char *from;
	const char *to;
	bool is_mali;
};

/* 普通 App 视角下需将虚拟 GPU/属性节点归一化为对应真机 MTK 节点路径。 */
static const struct cdh_dev_map_rule cdh_dev_map_rules[] = {
	{ "/dev/dri/renderD128", CDH_FAKE_MALI0_PATH, true },
	{ "/dev/dri/card0",      CDH_FAKE_MALI0_PATH, true },
	{ "/dev/__properties__/u:object_r:vendor_mesa_prop:s0",
	  "/dev/__properties__/u:object_r:vendor_mtk_soc_prop:s0", false },
	{ "/dev/__properties__/u:object_r:vendor_minigbm_debug_prop:s0",
	  "/dev/__properties__/u:object_r:vendor_mtk_camera_prop:s0", false },
};

static const struct cdh_dev_map_rule *cdh_find_dev_map_rule(const char *path)
{
	int i;

	if (!path)
		return NULL;
	for (i = 0; i < (int)ARRAY_SIZE(cdh_dev_map_rules); i++) {
		if (strcmp(path, cdh_dev_map_rules[i].from) == 0)
			return &cdh_dev_map_rules[i];
	}
	return NULL;
}

static int cdh_maps_entry(struct kretprobe_instance *ri, struct pt_regs *regs)
{
	struct cdh_maps_data *data = (struct cdh_maps_data *)ri->data;
	struct seq_file *m;
	struct vm_area_struct *vma;
	struct file *file;
	char raw_buf[320];
	char *raw_path;

	/* 仅面向普通应用 (uid > max_uid) 伪装载体及虚拟设备 VMA。 */
	if (!cdh_should_skip_uid())
		return 1;

	m = (struct seq_file *)regs_get_kernel_argument(regs, 0);
	vma = (struct vm_area_struct *)regs_get_kernel_argument(regs, 1);
	if (!m || !vma || !m->buf || m->size == 0 || m->count >= m->size)
		return 1;

	file = vma->vm_file;
	if (!file)
		return 1;

	raw_path = cdh_file_raw_path(file, raw_buf, sizeof(raw_buf));
	if (IS_ERR_OR_NULL(raw_path))
		return 1;

	if (strncmp(raw_path, "/mnt/vendorpatch/", 17) != 0 &&
	    !cdh_find_dev_map_rule(raw_path))
		return 1;

	data->m = m;
	data->orig_count = m->count;
	return 0;
}

static int cdh_maps_ret(struct kretprobe_instance *ri, struct pt_regs *regs)
{
	struct cdh_maps_data *data = (struct cdh_maps_data *)ri->data;
	struct seq_file *m = data->m;
	char *line;
	size_t line_len;
	size_t end;
	size_t pos = 0;
	size_t dev_start, dev_end;
	size_t ino_start, ino_end;
	size_t path_start, path_len;
	char virt[CDH_MAPS_PATH_MAX];
	const char *lookup_path;
	const struct cdh_dev_map_rule *dev_rule;
	const char *new_path = NULL;
	dev_t new_dev = 0;
	unsigned long new_ino = 0;
	char repl[64];
	int repl_len = 0;
	size_t out_path_len;
	size_t old_header_span, new_header_span;
	size_t old_total, new_total;
	size_t tail_pos, tail_len;
	int col;

	if (!m || !m->buf || m->size == 0)
		return 0;
	/* 若 show_map_vma 自身已触发 seq_file 溢出，留待上层扩容重读。 */
	if (m->count >= m->size || m->count <= data->orig_count)
		return 0;

	line = m->buf + data->orig_count;
	line_len = m->count - data->orig_count;
	end = line_len;
	if (end > 0 && line[end - 1] == '\n')
		end--;

	/* 跳过 addr、perms、pgoff 三列，定位 dev 与 ino 字段。 */
	for (col = 0; col < 3; col++) {
		while (pos < end && line[pos] != ' ')
			pos++;
		if (pos >= end)
			return 0;
		while (pos < end && line[pos] == ' ')
			pos++;
	}

	dev_start = pos;
	while (pos < end && line[pos] != ' ')
		pos++;
	dev_end = pos;
	if (dev_end <= dev_start || pos >= end)
		return 0;

	while (pos < end && line[pos] == ' ')
		pos++;
	ino_start = pos;
	while (pos < end && line[pos] != ' ')
		pos++;
	ino_end = pos;
	if (ino_end <= ino_start || pos >= end)
		return 0;

	while (pos < end && line[pos] == ' ')
		pos++;
	path_start = pos;
	if (path_start >= end || line[path_start] != '/')
		return 0;

	/* path_len 取到换行或空格前，兼容尾部 "(deleted)" 等后缀。 */
	path_len = 0;
	while (path_start + path_len < end && line[path_start + path_len] != ' ')
		path_len++;
	if (path_len == 0 || path_len >= sizeof(virt))
		return 0;

	memcpy(virt, line + path_start, path_len);
	virt[path_len] = '\0';

	dev_rule = cdh_find_dev_map_rule(virt);
	if (dev_rule) {
		new_path = dev_rule->to;
		if (dev_rule->is_mali) {
			if (cdh_maps_lookup_or_queue(CDH_FAKE_MALI0_PATH,
						     &new_dev, &new_ino)) {
				repl_len = scnprintf(repl, sizeof(repl),
						     "%02x:%02x %lu",
						     MAJOR(new_dev),
						     MINOR(new_dev), new_ino);
			} else {
				size_t dev_len = dev_end - dev_start;

				if (dev_len + 16 < sizeof(repl)) {
					memcpy(repl, line + dev_start, dev_len);
					repl_len = (int)dev_len +
						scnprintf(repl + dev_len,
							  sizeof(repl) - dev_len,
							  " %lu",
							  CDH_FAKE_MALI0_INO);
				}
			}
		} else if (cdh_maps_lookup_or_queue(dev_rule->to,
						    &new_dev, &new_ino)) {
			repl_len = scnprintf(repl, sizeof(repl), "%02x:%02x %lu",
					     MAJOR(new_dev), MINOR(new_dev),
					     new_ino);
		} else if (ino_end - dev_start < sizeof(repl)) {
			/* 缓存尚未就绪前先保留原 dev:ino，确保路径先被改写不泄漏。 */
			repl_len = (int)(ino_end - dev_start);
			memcpy(repl, line + dev_start, repl_len);
			repl[repl_len] = '\0';
		}
	} else {
		if (strncmp(virt, "/mnt/vendorpatch/vendor/", 24) == 0)
			lookup_path = virt + 16;
		else if (strncmp(virt, "/vendor/", 8) == 0)
			lookup_path = virt;
		else
			return 0;

		if (!cdh_maps_lookup_or_queue(lookup_path, &new_dev, &new_ino))
			return 0;

		repl_len = scnprintf(repl, sizeof(repl), "%02x:%02x %lu",
				     MAJOR(new_dev), MINOR(new_dev), new_ino);
	}
	if (repl_len <= 0)
		return 0;

	out_path_len = new_path ? strlen(new_path) : path_len;
	old_header_span = path_start - dev_start;
	new_header_span = ((size_t)repl_len + 1 <= old_header_span)
				  ? old_header_span
				  : (size_t)repl_len + 1;
	old_total = old_header_span + path_len;
	new_total = new_header_span + out_path_len;
	tail_pos = path_start + path_len;
	tail_len = line_len - tail_pos;

	if (new_total > old_total &&
	    m->count + (new_total - old_total) >= m->size) {
		/* 触及缓冲上限时标记溢出，交由 seq_read_iter 倍增缓冲后重跑。 */
		m->count = m->size;
		return 0;
	}

	if (new_total != old_total)
		memmove(line + dev_start + new_total, line + tail_pos, tail_len);

	memcpy(line + dev_start, repl, repl_len);
	memset(line + dev_start + repl_len, ' ',
	       new_header_span - (size_t)repl_len);
	if (new_path)
		memcpy(line + dev_start + new_header_span, new_path,
		       out_path_len);

	if (new_total >= old_total)
		m->count += (new_total - old_total);
	else
		m->count -= (old_total - new_total);

	atomic64_inc(&cdh_maps_ino_rewrites);
	return 0;
}

static struct kretprobe cdh_maps_kretprobe = {
	.kp.symbol_name = "show_map_vma",
	.handler = cdh_maps_ret,
	.entry_handler = cdh_maps_entry,
	.data_size = sizeof(struct cdh_maps_data),
	.maxactive = CDH_MAPS_KRETPROBE_MAX,
};

int cdh_maps_hook_init(void)
{
	int ret;

	atomic_set(&cdh_maps_exiting, 0);
	cdh_maps_prewarm_tries = 0;
	ret = register_kretprobe(&cdh_maps_kretprobe);
	if (ret) {
		pr_warn("maps dev:ino 钩子注册失败：%d（跳过）\n", ret);
		return ret;
	}
	cdh_maps_registered = true;
	schedule_delayed_work(&cdh_maps_prewarm_work,
			      msecs_to_jiffies(CDH_MAPS_PREWARM_MS));
	pr_info("maps dev:ino 钩子就绪：show_map_vma\n");
	return 0;
}

void cdh_maps_hook_exit(void)
{
	atomic_set(&cdh_maps_exiting, 1);
	if (cdh_maps_registered) {
		unregister_kretprobe(&cdh_maps_kretprobe);
		cdh_maps_registered = false;
	}
	cancel_delayed_work_sync(&cdh_maps_prewarm_work);
	cancel_work_sync(&cdh_maps_work);
	if (cdh_maps_kretprobe.nmissed)
		pr_warn("maps kretprobe 漏抓：%d\n", cdh_maps_kretprobe.nmissed);
	pr_info("maps dev:ino 钩子已卸载\n");
}
