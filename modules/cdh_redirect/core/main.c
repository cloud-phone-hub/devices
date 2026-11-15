// SPDX-License-Identifier: GPL-2.0
/*
 * cdh_redirect 入口：装规则 -> 装两个 kretprobe 钩子 -> 暴露 /proc/cdh_redirect。
 *
 * 加载时机：first stage init 的 LoadKernelModules()（SwitchRoot 之前、SELinux 策略
 * 加载之前）从 ramdisk 的 /lib/modules/<release>/ 读 modules.load。因此本模块初始化
 * 时根文件系统还是 initramfs，规则文件必须和 .ko 一起放在 initramfs 里
 * （/lib/modules/cdh_redirect.rules）；等到 /vendor 挂上再读就太晚了。
 *
 * 失败口径：一条规则都没有就拒绝加载（返回 -EINVAL）。空转的钩子比加载失败更危险
 * ——启动看起来正常，实则 /vendor 还指着 super 里的官方树。
 */
#define pr_fmt(fmt)	"cdh_redirect: " fmt

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>

#include "cdh_redirect.h"

static char *cdh_rules_path;
module_param_named(rules_path, cdh_rules_path, charp, 0444);
MODULE_PARM_DESC(rules_path, "规则文件路径（缺省 " CDH_DEFAULT_RULES_PATH "）");

static char *cdh_rules_inline;
module_param_named(rules, cdh_rules_inline, charp, 0444);
MODULE_PARM_DESC(rules, "调试用内联规则：dir:/源:/目标;file:/源:/目标（非空时忽略 rules_path）");

bool cdh_debug;
module_param_named(debug, cdh_debug, bool, 0644);
MODULE_PARM_DESC(debug, "调试开关：逆向钩子逐次打印改写前后的串（运行时 /sys/module/cdh_redirect/parameters/debug 可改）");

unsigned int cdh_max_uid = CDH_DEFAULT_MAX_UID;
module_param_named(max_uid, cdh_max_uid, uint, 0644);
MODULE_PARM_DESC(max_uid, "重定向生效的最大 UID（uid > max_uid 时不重定向，缺省 10000）");

static struct proc_dir_entry *cdh_proc_entry;

static int cdh_status_show(struct seq_file *m, void *unused)
{
	int i;

	seq_printf(m, "rules: %d/%d\n", cdh_rule_count, CDH_RULES_MAX);
	for (i = 0; i < cdh_rule_count; i++)
		seq_printf(m, "  %-4s %s -> %s\n",
			   cdh_rules[i].dir ? "dir" : "file",
			   cdh_rules[i].from, cdh_rules[i].to);
	seq_printf(m, "max_uid: %u\n", cdh_max_uid);
	seq_printf(m, "forward_hits: %llu\n",
		   (unsigned long long)atomic64_read(&cdh_forward_hits));
	seq_printf(m, "reverse_hits: %llu\n",
		   (unsigned long long)atomic64_read(&cdh_reverse_hits));
	seq_printf(m, "uid_skipped: %llu\n",
		   (unsigned long long)atomic64_read(&cdh_uid_skipped));
	seq_printf(m, "forward_missed: %llu\n",
		   (unsigned long long)atomic64_read(&cdh_forward_missed));
	seq_printf(m, "reverse_missed: %llu\n",
		   (unsigned long long)atomic64_read(&cdh_reverse_missed));
	seq_printf(m, "mount_hidden: %llu\n",
		   (unsigned long long)atomic64_read(&cdh_mount_hidden));
	seq_printf(m, "peergroup_rewrites: %llu\n",
		   (unsigned long long)atomic64_read(&cdh_peergroup_rewrites));
	seq_printf(m, "dirent_hidden: %llu\n",
		   (unsigned long long)atomic64_read(&cdh_dirent_hidden));
	seq_printf(m, "sphal_blocked: %llu\n",
		   (unsigned long long)atomic64_read(&cdh_sphal_blocked));
	seq_printf(m, "maps_ino_rewrites: %llu\n",
		   (unsigned long long)atomic64_read(&cdh_maps_ino_rewrites));
	for (i = 0; i < CDH_READLINK_HOOK_NUM; i++) {
		struct cdh_readlink_stats st;

		cdh_readlink_stats_read(i, &st);
		seq_printf(m, "readlink[%s]: entries=%llu rets=%llu hits=%llu skipped=%llu\n",
			   i == CDH_READLINK_HOOK_VFS ? "vfs" : "proc",
			   (unsigned long long)st.entries,
			   (unsigned long long)st.rets,
			   (unsigned long long)st.hits,
			   (unsigned long long)st.skipped);
	}
	return 0;
}

static int __init cdh_redirect_init(void)
{
	const char *path = cdh_rules_path ? cdh_rules_path : CDH_DEFAULT_RULES_PATH;
	int count, ret;

	count = cdh_rules_load(path, cdh_rules_inline);
	if (count < 0) {
		pr_err("规则装载失败（%d），模块不加载\n", count);
		return count;
	}
	if (count == 0) {
		pr_err("%s 里没有任何规则，模块不加载\n", path);
		cdh_rules_free();
		return -EINVAL;
	}

	/*
	 * /proc 在 first stage 已挂（init 自己挂的 proc）；万一没挂也继续，
	 * 只影响 /proc/cdh_redirect 观测，不影响重定向本身。
	 */
	cdh_proc_entry = proc_create_single("cdh_redirect", 0444, NULL,
					    cdh_status_show);
	if (!cdh_proc_entry)
		pr_warn("/proc/cdh_redirect 创建失败，观测口不可用\n");

	ret = cdh_namei_hook_init();
	if (ret)
		goto err_proc;

	ret = cdh_readlink_hook_init();
	if (ret)
		goto err_namei;

	(void)cdh_mountinfo_hook_init();
	(void)cdh_maps_hook_init();

	pr_info("已加载：%d 条规则（%s），内核未打补丁\n",
		count, cdh_rules_inline ? "模块参数" : path);
	return 0;

err_namei:
	cdh_namei_hook_exit();
err_proc:
	if (cdh_proc_entry) {
		proc_remove(cdh_proc_entry);
		cdh_proc_entry = NULL;
	}
	cdh_rules_free();
	return ret;
}

static void __exit cdh_redirect_exit(void)
{
	cdh_maps_hook_exit();
	cdh_mountinfo_hook_exit();
	cdh_readlink_hook_exit();
	cdh_namei_hook_exit();
	if (cdh_proc_entry) {
		proc_remove(cdh_proc_entry);
		cdh_proc_entry = NULL;
	}
	cdh_rules_free();
	pr_info("已卸载\n");
}

module_init(cdh_redirect_init);
module_exit(cdh_redirect_exit);

MODULE_DESCRIPTION("CDH 路径重定向（getname_flags / proc_pid_readlink 双 kretprobe，规则走数据文件）");
MODULE_AUTHOR("fei_cong <fei_cong@hotmail.com>");
MODULE_LICENSE("GPL");
