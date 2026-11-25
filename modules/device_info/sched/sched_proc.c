// SPDX-License-Identifier: GPL-2.0-only
/*
 * 虚拟调度与帧感知内核探针节点（/proc/vip/ioctl 与 /proc/fas/ioctl）：
 *   荣耀 MagicOS / 华为 EMUI 框架层（system_server、SystemUI 及各应用进程的
 *   VipSchedProxy 与 FASProxy）在每次触摸滑动、窗口切换与帧绘制时都会打开
 *   /proc/vip/ioctl 与 /proc/fas/ioctl 下发线程 VIP 调度提示（ioctl 0xac01..0xac0d）。
 *   GKI 云机内核未合入厂商私有 vip/fas 调度补丁时，每次滑动都会产生大量 ENOENT 系统调用失败
 *   及 HiLog 错误刷屏（E [VipSchedProxy:]: Vip failed to open /proc/vip/ioctl, errno=2）。
 *
 *   此处注册轻量级空操作 proc 节点（0666；system_ext_sepolicy.cil 的 genfscon 自动将其标记为
 *   u:object_r:proc_vip_ioctl:s0 与 u:object_r:proc_fas:s0），对读方向 ioctl 清零用户态结构体后
 *   返回 0，彻底消除热路径 open 失败与日志风暴。
 */
#define pr_fmt(fmt) "cdh_device_info_sched: " fmt

#include <linux/fs.h>
#include <linux/ioctl.h>
#include <linux/kernel.h>
#include <linux/proc_fs.h>
#include <linux/uaccess.h>

#include "cdh_device_info.h"

#define CDH_SCHED_IOCTL_MAX_CLEAR 256U

static struct proc_dir_entry *cdh_vip_dir;
static struct proc_dir_entry *cdh_vip_ioctl;
static struct proc_dir_entry *cdh_fas_dir;
static struct proc_dir_entry *cdh_fas_ioctl;

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

	return 0;
}

void cdh_sched_proc_exit(void)
{
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
