// SPDX-License-Identifier: GPL-2.0
/*
 * 前向重定向：kretprobe getname_flags()，在返回处改写 struct filename->name。
 *
 * 为什么挂这里（而不是 sys_open 之类）：内核补丁就是在 getname_flags() 里就地改写
 * kname，那是「用户态路径串」进入 VFS 的唯一入口；挂在这里一次覆盖 open/stat/
 * exec/access/… 全部路径系统调用，语义与补丁完全一致。
 *
 * 为什么必须 kretprobe：getname_flags 的入参是 __user 指针，入口处拿不到内核地址；
 * 而它的返回值就是 struct filename *，name 已经指向内核缓冲区，可以安全改写。
 *
 * 缓冲区容量：getname_flags 有两种分配——常见路径把字符串嵌在 struct filename 的
 * iname[]（EMBEDDED_NAME_MAX），超长路径把整个 names_cache 页当字符串（PATH_MAX）。
 * 判据就是 name 是否等于 iname（内核 putname() 也用同一判据决定怎么释放）。
 */
#define pr_fmt(fmt)	"cdh_redirect: " fmt

#include <linux/err.h>
#include <linux/fs.h>
#include <linux/kprobes.h>
#include <linux/limits.h>
#include <linux/module.h>
#include <linux/ptrace.h>
#include <linux/stddef.h>

#include "cdh_redirect.h"

/*
 * 嵌入式 name 缓冲区的可用长度。与内核 fs/namei.c 的 EMBEDDED_NAME_MAX 同式子：
 * 整页 names_cache（PATH_MAX）减去 struct filename 头部。
 */
#define CDH_EMBEDDED_NAME_MAX	(PATH_MAX - offsetof(struct filename, iname))

/* 热路径：8 核上并发进 getname_flags 的线程数远小于此，够用且不至于白占内存。 */
#define CDH_NAMEI_KRETPROBE_MAXACTIVE	4096

static int cdh_namei_ret(struct kretprobe_instance *ri, struct pt_regs *regs)
{
	struct filename *f = (struct filename *)regs_return_value(regs);
	const char *name;
	size_t cap;

	if (IS_ERR_OR_NULL(f) || !f->name)
		return 0;
	name = f->name;
	/*
	 * 与 putname() 同判据：name 指回 iname 就是嵌入式分配，否则是独占的
	 * names_cache 整页。判错只会让改写更保守（容量算小），不会越界。
	 */
	if (name == (const char *)f->iname)
		cap = CDH_EMBEDDED_NAME_MAX;
	else
		cap = PATH_MAX;

	cdh_rewrite_forward((char *)name, cap);
	return 0;
}

static struct kretprobe cdh_namei_kretprobe = {
	.handler = cdh_namei_ret,
	.data_size = 0,
	.maxactive = CDH_NAMEI_KRETPROBE_MAXACTIVE,
	.kp = {
		.symbol_name = "getname_flags",
	},
};

static bool cdh_namei_registered;

int cdh_namei_hook_init(void)
{
	int ret;

	ret = register_kretprobe(&cdh_namei_kretprobe);
	if (ret) {
		pr_err("注册 getname_flags kretprobe 失败：%d\n", ret);
		return ret;
	}
	cdh_namei_registered = true;
	pr_info("前向钩子就绪：getname_flags（maxactive %d）\n",
		CDH_NAMEI_KRETPROBE_MAXACTIVE);
	return 0;
}

void cdh_namei_hook_exit(void)
{
	if (!cdh_namei_registered)
		return;
	unregister_kretprobe(&cdh_namei_kretprobe);
	cdh_namei_registered = false;
	/* nmissed 非 0 说明有并发调用没拿到实例，那几次查找会漏掉重定向。 */
	if (cdh_namei_kretprobe.nmissed)
		pr_warn("getname_flags kretprobe 漏抓 %d 次（提高 maxactive）\n",
			cdh_namei_kretprobe.nmissed);
	pr_info("前向钩子已卸载\n");
}
