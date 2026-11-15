/*
 * 逆变换：把 /proc/<pid>/{exe,cwd,root,fd/N} 的 readlink 出口从载体物理路径
 * 还原成虚拟路径。
 *
 * 为什么需要：前向钩子只改写「查找」，d_path() 看到的仍是载体挂载点
 * /mnt/vendorpatch/vendor/...。而 bionic 用 realpath(/proc/self/exe) 去匹配
 * ld.config.txt 的 dir.* 段（dir.vendor = /vendor/bin/），拿到 /mnt/vendorpatch/...
 * 一段都匹配不上，vendor HAL 就落进兜底段，拿不到 sphal 命名空间。
 *
 * 挂两个钩子、同一套改写逻辑：
 *   1) proc_pid_readlink —— 与内核补丁同一层（fs/proc/base.c 里 do_proc_readlink
 *      被内联进它，是唯一出口），覆盖 cwd/exe/root/fd/map_files 这些 proc 魔术链接；
 *   2) vfs_readlink —— readlink(2) 的公共出口。do_readlinkat（fs/stat.c）与
 *      vfs_readlink（fs/namei.c）分属两个编译单元，不会被内联，因此必然走到真实调用。
 *      实测只挂 1) 时，云机里 readlink /proc/$$/cwd 拿不到改写结果（/proc/self/fd/N
 *      反而能拿到），故两个都挂。改写幂等：已还原成虚拟路径的串不再命中逆向规则，
 *      同一个调用被两层各看一遍也没有副作用（第二层只读不写）。
 *
 * 为什么在返回处改：补丁在 copy_to_user 之前改内核缓冲；kretprobe 只能在函数返回后
 * 动手，于是把用户缓冲区读回来、改写、再写回去（路径只变短，容量只会更宽裕），
 * 最后把返回值改成新长度。入口处用 regs_get_kernel_argument 记下 buffer/buflen。
 *
 * 用户缓冲区指针可能带 MTE 标签：toybox 的 readlink applet 走 xreadlinkat()，
 * 缓冲区是 malloc(64) 出来的（实测 b4000073d1edb3d0），arm64 的 access_ok() 只在
 * PF_KTHREAD / TIF_TAGGED_ADDR 时才自己剥标签，普通进程的带标签指针会被判成越界，
 * copy_from_user_nofault() 直接 -EFAULT —— 表现为「钩子进去了但什么都没改」，
 * 所以入口处先自己 untagged_addr() 一次。
 */
#define pr_fmt(fmt)	"cdh_redirect: " fmt

#include <linux/kprobes.h>
#include <linux/limits.h>
#include <linux/module.h>
#include <linux/percpu.h>
#include <linux/preempt.h>
#include <linux/ptrace.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/uaccess.h>

#ifdef CONFIG_ARM64
#include <asm/memory.h>
#endif

#include "cdh_redirect.h"

/* 剥掉顶层字节里的 MTE 标签；没有该宏的平台（x86 等）原样返回。 */
static inline void __user *cdh_untag_user(void __user *ptr)
{
#ifdef untagged_addr
	return untagged_addr(ptr);
#else
	return ptr;
#endif
}

#define CDH_READLINK_KRETPROBE_MAXACTIVE	128

struct cdh_readlink_data {
	char __user *buffer;
	int buflen;
	pid_t pid;
	char comm[TASK_COMM_LEN];
};

/*
 * 两个挂点共用一份实例数据与同一套逻辑；6.1 的 struct kretprobe_instance 里
 * 没有指向 kretprobe 的字段，所以靠各自的包装函数把挂点编号传进去。
 */
static atomic64_t cdh_readlink_entries[CDH_READLINK_HOOK_NUM];
static atomic64_t cdh_readlink_rets[CDH_READLINK_HOOK_NUM];
static atomic64_t cdh_readlink_skipped[CDH_READLINK_HOOK_NUM];
static atomic64_t cdh_readlink_hits[CDH_READLINK_HOOK_NUM];

void cdh_readlink_stats_read(int which, struct cdh_readlink_stats *out)
{
	if (which < 0 || which >= CDH_READLINK_HOOK_NUM)
		return;
	out->entries = (u64)atomic64_read(&cdh_readlink_entries[which]);
	out->rets = (u64)atomic64_read(&cdh_readlink_rets[which]);
	out->skipped = (u64)atomic64_read(&cdh_readlink_skipped[which]);
	out->hits = (u64)atomic64_read(&cdh_readlink_hits[which]);
}

static const char *cdh_hook_name(int which)
{
	return which == CDH_READLINK_HOOK_VFS ? "vfs_readlink" : "proc_pid_readlink";
}

static int cdh_readlink_entry_common(struct kretprobe_instance *ri,
				     struct pt_regs *regs, int hook)
{
	struct cdh_readlink_data *data = (struct cdh_readlink_data *)ri->data;

	data->buffer = (char __user *)cdh_untag_user(
		(void __user *)regs_get_kernel_argument(regs, 1));
	data->buflen = (int)regs_get_kernel_argument(regs, 2);
	data->pid = (pid_t)task_tgid_nr(current);
	get_task_comm(data->comm, current);
	atomic64_inc(&cdh_readlink_entries[hook]);
	if (cdh_debug)
		pr_info("%s 进入 pid=%d comm=%s buf=%px buflen=%d\n",
			cdh_hook_name(hook), data->pid, data->comm,
			data->buffer, data->buflen);
	return 0;
}

static int cdh_readlink_ret_common(struct kretprobe_instance *ri,
				   struct pt_regs *regs, int hook)
{
	struct cdh_readlink_data *data = (struct cdh_readlink_data *)ri->data;
	long ret = regs_return_value(regs);
	unsigned int len;
	char *buf;
	int copy_err;
	bool rewritten;

	atomic64_inc(&cdh_readlink_rets[hook]);
	if (ret <= 0 || !data->buffer || data->buflen <= 0) {
		if (cdh_debug)
			pr_info("%s pid=%d 返回 ret=%ld buf=%px buflen=%d，不处理\n",
				cdh_hook_name(hook), data->pid, ret,
				data->buffer, data->buflen);
		atomic64_inc(&cdh_readlink_skipped[hook]);
		return 0;
	}
	/*
	 * readlink 语义：返回实际写入长度、不补 NUL，且被 buflen 截断过。
	 * 这里按同一口径取长度，留一个字节自己补 NUL。
	 */
	len = (unsigned int)ret;
	if (len > (unsigned int)data->buflen)
		len = (unsigned int)data->buflen;
	if (len >= PATH_MAX) {
		atomic64_inc(&cdh_readlink_skipped[hook]);
		return 0;
	}

	/* 返回处只剩用户缓冲区，改写要暂存；proc readlink 不是热路径，按原子分配。 */
	buf = kmalloc(PATH_MAX, GFP_ATOMIC);
	if (!buf) {
		atomic64_inc(&cdh_readlink_skipped[hook]);
		return 0;
	}
	if (copy_from_user_nofault(buf, data->buffer, len)) {
		if (cdh_debug)
			pr_info("%s pid=%d(%s) 读回失败 buf=%px len=%u\n",
				cdh_hook_name(hook), data->pid, data->comm,
				data->buffer, len);
		atomic64_inc(&cdh_readlink_skipped[hook]);
		goto out;
	}
	buf[len] = '\0';
	if (cdh_debug)
		pr_info("%s pid=%d(%s) ret=%ld buflen=%d 读回=%s\n",
			cdh_hook_name(hook), data->pid, data->comm, ret,
			data->buflen, buf);
	rewritten = cdh_rewrite_reverse(buf, PATH_MAX);
	if (cdh_should_skip_uid() &&
	    (strcmp(buf, "/dev/dri/renderD128") == 0 ||
	     strcmp(buf, "/dev/dri/card0") == 0)) {
		strscpy(buf, "/dev/mali0", PATH_MAX);
		rewritten = true;
	}
	if (!rewritten) {
		atomic64_inc(&cdh_readlink_skipped[hook]);
		goto out;
	}

	len = (unsigned int)strlen(buf);
	if (len > (unsigned int)data->buflen)
		len = (unsigned int)data->buflen;
	copy_err = copy_to_user_nofault(data->buffer, buf, len);
	if (cdh_debug)
		pr_info("%s pid=%d 写回=%s len=%u err=%d\n",
			cdh_hook_name(hook), data->pid, buf, len, copy_err);
	if (copy_err) {
		atomic64_inc(&cdh_readlink_skipped[hook]);
		goto out;
	}
	/* 新串更短：返回值跟着缩小，否则调用方会读到上一版留下的尾巴。 */
	regs_set_return_value(regs, len);
	atomic64_inc(&cdh_readlink_hits[hook]);
out:
	kfree(buf);
	return 0;
}

static int cdh_proc_readlink_entry(struct kretprobe_instance *ri,
				   struct pt_regs *regs)
{
	return cdh_readlink_entry_common(ri, regs, CDH_READLINK_HOOK_PROC);
}

static int cdh_proc_readlink_ret(struct kretprobe_instance *ri,
				 struct pt_regs *regs)
{
	return cdh_readlink_ret_common(ri, regs, CDH_READLINK_HOOK_PROC);
}

static int cdh_vfs_readlink_entry(struct kretprobe_instance *ri,
				  struct pt_regs *regs)
{
	return cdh_readlink_entry_common(ri, regs, CDH_READLINK_HOOK_VFS);
}

static int cdh_vfs_readlink_ret(struct kretprobe_instance *ri,
				struct pt_regs *regs)
{
	return cdh_readlink_ret_common(ri, regs, CDH_READLINK_HOOK_VFS);
}

static struct kretprobe cdh_proc_readlink_kretprobe = {
	.handler = cdh_proc_readlink_ret,
	.entry_handler = cdh_proc_readlink_entry,
	.data_size = sizeof(struct cdh_readlink_data),
	.maxactive = CDH_READLINK_KRETPROBE_MAXACTIVE,
	.kp = {
		.symbol_name = "proc_pid_readlink",
	},
};

static struct kretprobe cdh_vfs_readlink_kretprobe = {
	.handler = cdh_vfs_readlink_ret,
	.entry_handler = cdh_vfs_readlink_entry,
	.data_size = sizeof(struct cdh_readlink_data),
	.maxactive = CDH_READLINK_KRETPROBE_MAXACTIVE,
	.kp = {
		.symbol_name = "vfs_readlink",
	},
};

static DEFINE_PER_CPU(int, cdh_raw_d_path_depth);

/* 获取 file 对应的原始挂载路径，绕开 cdh_d_path_ret 逆向改写。 */
char *cdh_file_raw_path(struct file *file, char *buf, int buflen)
{
	char *p;

	if (!file || !buf || buflen <= 0)
		return ERR_PTR(-EINVAL);
	preempt_disable();
	this_cpu_inc(cdh_raw_d_path_depth);
	p = d_path(&file->f_path, buf, buflen);
	this_cpu_dec(cdh_raw_d_path_depth);
	preempt_enable();
	return p;
}

/*
 * d_path 返回钩子：覆盖 /proc/<pid>/{maps,smaps,numa_maps}（seq_file_path ->
 * seq_path -> d_path）与 do_proc_readlink 等内核路径合成出口，将载体物理路径
 * /mnt/vendorpatch/vendor/... 在源头还原为虚拟路径 /vendor/...。
 */
static int cdh_d_path_ret(struct kretprobe_instance *ri, struct pt_regs *regs)
{
	char *p = (char *)regs_return_value(regs);
	size_t old_len, new_len, shift;

	if (this_cpu_read(cdh_raw_d_path_depth) > 0)
		return 0;

	if (IS_ERR_OR_NULL(p) || p[0] != '/')
		return 0;

	old_len = strlen(p);
	if (!old_len || old_len >= PATH_MAX)
		return 0;

	if (!cdh_rewrite_reverse(p, old_len + 1))
		return 0;

	new_len = strlen(p);
	if (new_len < old_len) {
		/*
		 * d_path() 约定字符串末尾 NUL 紧贴 buf + buflen - 1
		 * （例如 do_proc_readlink 用 tmp + PATH_MAX - 1 - pathname 算长度）。
		 * 将缩短后的新串右移 shift 字节，并同步前移返回指针。
		 */
		shift = old_len - new_len;
		memmove(p + shift, p, new_len + 1);
		regs_set_return_value(regs, (unsigned long)(p + shift));
	}
	return 0;
}

static struct kretprobe cdh_d_path_kretprobe = {
	.handler = cdh_d_path_ret,
	.maxactive = CDH_READLINK_KRETPROBE_MAXACTIVE,
	.kp = {
		.symbol_name = "d_path",
	},
};

static bool cdh_readlink_registered;
static bool cdh_d_path_registered;

int cdh_readlink_hook_init(void)
{
	int ret;

	ret = register_kretprobe(&cdh_proc_readlink_kretprobe);
	if (ret) {
		pr_err("注册 proc_pid_readlink kretprobe 失败：%d\n", ret);
		return ret;
	}
	ret = register_kretprobe(&cdh_vfs_readlink_kretprobe);
	if (ret) {
		pr_err("注册 vfs_readlink kretprobe 失败：%d\n", ret);
		unregister_kretprobe(&cdh_proc_readlink_kretprobe);
		return ret;
	}
	ret = register_kretprobe(&cdh_d_path_kretprobe);
	if (ret) {
		pr_warn("注册 d_path kretprobe 失败（%d），继续仅使用 readlink 钩子\n", ret);
		cdh_d_path_registered = false;
	} else {
		cdh_d_path_registered = true;
	}
	cdh_readlink_registered = true;
	pr_info("逆向钩子就绪：proc_pid_readlink + vfs_readlink%s\n",
		cdh_d_path_registered ? " + d_path" : "");
	return 0;
}

void cdh_readlink_hook_exit(void)
{
	if (!cdh_readlink_registered)
		return;
	if (cdh_d_path_registered) {
		unregister_kretprobe(&cdh_d_path_kretprobe);
		cdh_d_path_registered = false;
	}
	unregister_kretprobe(&cdh_vfs_readlink_kretprobe);
	unregister_kretprobe(&cdh_proc_readlink_kretprobe);
	cdh_readlink_registered = false;
	if (cdh_vfs_readlink_kretprobe.nmissed || cdh_proc_readlink_kretprobe.nmissed)
		pr_warn("readlink kretprobe 漏抓：vfs=%d proc=%d\n",
			cdh_vfs_readlink_kretprobe.nmissed,
			cdh_proc_readlink_kretprobe.nmissed);
	pr_info("逆向钩子已卸载\n");
}
