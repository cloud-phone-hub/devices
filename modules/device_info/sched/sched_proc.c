// SPDX-License-Identifier: GPL-2.0-only
/*
 * 虚拟调度与帧感知内核探针节点（/proc/vip/ioctl、/proc/fas/ioctl 与 /proc/ged）：
 *   1. 荣耀 MagicOS / 华为 EMUI 框架层（VipSchedProxy / FASProxy）在触摸滑动与帧绘制时打开
 *      /proc/vip/ioctl 与 /proc/fas/ioctl 下发线程 VIP 调度提示；
 *   2. 联发科（MTK）平台 libged_sys.so / libgui.so 在 SurfaceFlinger、cameraserver 及所有 UI
 *      进程的每次 BufferQueue queue/acquire/dequeue 与 Gralloc Extra 查询中打开 /proc/ged
 *      执行 GED_BRIDGE_PACKAGE ioctl（_IOWR('g', nr, 0x28)）。缺失 /proc/ged 时每帧触发多行
 *      ALOGE（Failed to open /proc/ged + ged queue/acquire/dequeue fail）导致 logd 高负载与掉帧。
 *
 *   此处注册轻量级空操作 proc 节点（0666），对读方向及 GED 桥接输出缓冲执行安全清零并返回 0
 *   （其中 GED_BRIDGE_COMMAND_GE_ALLOC 0xc0286764 保留调用方预置的 -1 fd），彻底消除热路径
 *   open/ioctl 失败与日志风暴。
 */
#define pr_fmt(fmt) "cdh_device_info_sched: " fmt

#include <linux/fs.h>
#include <linux/ioctl.h>
#include <linux/kernel.h>
#include <linux/proc_fs.h>
#include <linux/types.h>
#include <linux/uaccess.h>

#include "cdh_device_info.h"

#define CDH_SCHED_IOCTL_MAX_CLEAR 256U
#define CDH_GED_IOCTL_MAGIC       'g'
#define CDH_GED_IOCTL_GE_ALLOC    0xc0286764U
#define CDH_GED_IOCTL_MAX_OUT     4096

struct cdh_ged_bridge_package {
	u32 function_id;
	s32 size;
	u64 param_in;
	s32 in_buf_size;
	s32 pad0;
	u64 param_out;
	s32 out_buf_size;
	s32 pad1;
};

static struct proc_dir_entry *cdh_vip_dir;
static struct proc_dir_entry *cdh_vip_ioctl;
static struct proc_dir_entry *cdh_fas_dir;
static struct proc_dir_entry *cdh_fas_ioctl;
static struct proc_dir_entry *cdh_ged_entry;
static struct proc_dir_entry *cdh_bootprof_entry;

static int cdh_sched_proc_open(struct inode *inode, struct file *file)
{
	return 0;
}

static ssize_t cdh_sched_proc_read(struct file *file, char __user *buf,
				   size_t count, loff_t *ppos)
{
	return 0;
}

static ssize_t cdh_sched_proc_write(struct file *file, const char __user *buf,
				    size_t count, loff_t *ppos)
{
	return (ssize_t)count;
}

static long cdh_sched_proc_ioctl(struct file *file, unsigned int cmd,
				 unsigned long arg)
{
	unsigned int dir = _IOC_DIR(cmd);
	unsigned int size = _IOC_SIZE(cmd);

	if ((dir & _IOC_READ) && size > 0 && size <= CDH_SCHED_IOCTL_MAX_CLEAR && arg != 0) {
		if (clear_user((void __user *)arg, size))
			return -EFAULT;
	}
	return 0;
}

static long cdh_ged_proc_ioctl(struct file *file, unsigned int cmd,
			       unsigned long arg)
{
	struct cdh_ged_bridge_package pkg;

	if (_IOC_TYPE(cmd) == CDH_GED_IOCTL_MAGIC &&
	    _IOC_SIZE(cmd) == sizeof(pkg) && arg != 0) {
		if (copy_from_user(&pkg, (const void __user *)arg, sizeof(pkg)))
			return -EFAULT;
		/*
		 * GED_BRIDGE_COMMAND_GE_ALLOC：调用方将输出缓冲内的 ge_fd 预置为 -1，
		 * 若清零为 0 会使调用方误把标准输入 fd=0 当成 ge_fd 并在释放时 close(0)。
		 * 保持原值直接返回 0，libged_sys 将静默走无 GE 缓冲快路径且不打印错误日志。
		 */
		if (cmd == CDH_GED_IOCTL_GE_ALLOC)
			return 0;
		if (pkg.param_out != 0 && pkg.out_buf_size > 0 &&
		    pkg.out_buf_size <= CDH_GED_IOCTL_MAX_OUT) {
			if (clear_user(u64_to_user_ptr(pkg.param_out),
				       (unsigned long)pkg.out_buf_size))
				return -EFAULT;
		}
		return 0;
	}
	return cdh_sched_proc_ioctl(file, cmd, arg);
}

static const struct proc_ops cdh_sched_proc_ops = {
	.proc_open = cdh_sched_proc_open,
	.proc_read = cdh_sched_proc_read,
	.proc_write = cdh_sched_proc_write,
	.proc_ioctl = cdh_sched_proc_ioctl,
#ifdef CONFIG_COMPAT
	.proc_compat_ioctl = cdh_sched_proc_ioctl,
#endif
	.proc_lseek = noop_llseek,
};

static const struct proc_ops cdh_ged_proc_ops = {
	.proc_open = cdh_sched_proc_open,
	.proc_read = cdh_sched_proc_read,
	.proc_write = cdh_sched_proc_write,
	.proc_ioctl = cdh_ged_proc_ioctl,
#ifdef CONFIG_COMPAT
	.proc_compat_ioctl = cdh_ged_proc_ioctl,
#endif
	.proc_lseek = noop_llseek,
};

int cdh_sched_proc_init(void)
{
	cdh_vip_dir = proc_mkdir("vip", NULL);
	if (cdh_vip_dir) {
		cdh_vip_ioctl = proc_create("ioctl", 0666, cdh_vip_dir,
					    &cdh_sched_proc_ops);
		if (!cdh_vip_ioctl) {
			pr_warn("proc_create(/proc/vip/ioctl) failed\n");
			proc_remove(cdh_vip_dir);
			cdh_vip_dir = NULL;
		} else {
			pr_info("registered /proc/vip/ioctl\n");
		}
	}

	cdh_fas_dir = proc_mkdir("fas", NULL);
	if (cdh_fas_dir) {
		cdh_fas_ioctl = proc_create("ioctl", 0666, cdh_fas_dir,
					    &cdh_sched_proc_ops);
		if (!cdh_fas_ioctl) {
			pr_warn("proc_create(/proc/fas/ioctl) failed\n");
			proc_remove(cdh_fas_dir);
			cdh_fas_dir = NULL;
		} else {
			pr_info("registered /proc/fas/ioctl\n");
		}
	}

	cdh_ged_entry = proc_create("ged", 0666, NULL, &cdh_ged_proc_ops);
	if (!cdh_ged_entry)
		pr_warn("proc_create(/proc/ged) failed\n");
	else
		pr_info("registered /proc/ged\n");

	cdh_bootprof_entry = proc_create("bootprof", 0666, NULL, &cdh_sched_proc_ops);
	if (!cdh_bootprof_entry)
		pr_warn("proc_create(/proc/bootprof) failed\n");
	else
		pr_info("registered /proc/bootprof\n");

	return 0;
}

void cdh_sched_proc_exit(void)
{
	if (cdh_bootprof_entry) {
		proc_remove(cdh_bootprof_entry);
		cdh_bootprof_entry = NULL;
	}
	if (cdh_ged_entry) {
		proc_remove(cdh_ged_entry);
		cdh_ged_entry = NULL;
	}
	if (cdh_fas_ioctl) {
		proc_remove(cdh_fas_ioctl);
		cdh_fas_ioctl = NULL;
	}
	if (cdh_fas_dir) {
		proc_remove(cdh_fas_dir);
		cdh_fas_dir = NULL;
	}
	if (cdh_vip_ioctl) {
		proc_remove(cdh_vip_ioctl);
		cdh_vip_ioctl = NULL;
	}
	if (cdh_vip_dir) {
		proc_remove(cdh_vip_dir);
		cdh_vip_dir = NULL;
	}
}
