// SPDX-License-Identifier: GPL-2.0-only
/*
 * ETHTOOL_GDRVINFO 驱动指纹收敛：
 *   - 将 wlan*（mac80211_hwsim）的 driver/bus_info/version 收敛为真机形态
 *     （driver="wlan"，bus_info=""，version=init_uts_ns.name.release）；
 *   - 对 virtio_net 与 ccmni* / p2p* / eth* / mgmt* 拦截并返回 -EOPNOTSUPP；
 *   - 对 dummy* 等其余网卡将编译期 UTS_RELEASE 替换为当前 init_uts_ns.name.release。
 */
#define pr_fmt(fmt) "cdh_device_info_ethtool: " fmt

#include <linux/ethtool.h>
#include <linux/kernel.h>
#include <linux/kprobes.h>
#include <linux/netdevice.h>
#include <linux/ptrace.h>
#include <linux/string.h>
#include <linux/utsname.h>
#include <linux/version.h>

#include "cdh_device_info.h"

#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 1, 0)

struct devlink;

/* 与 net/ethtool/ioctl.c 保持严格一致的结构体布局，于 copy_to_user 前原地修正 rsp->info。 */
struct cdh_ethtool_devlink_compat {
	struct devlink *devlink;
	union {
		struct ethtool_flash efl;
		struct ethtool_drvinfo info;
	};
};

struct cdh_ethtool_data {
	struct net_device *dev;
	struct cdh_ethtool_devlink_compat *rsp;
};

static bool cdh_ethtool_should_suppress(const struct net_device *dev,
					const struct ethtool_drvinfo *info)
{
	if (!strcmp(info->driver, "virtio_net"))
		return true;
	if (!strncmp(dev->name, "ccmni", 5) ||
	    !strncmp(dev->name, "p2p", 3) ||
	    !strncmp(dev->name, "eth", 3) ||
	    !strncmp(dev->name, "mgmt", 4))
		return true;
	return false;
}

static void cdh_ethtool_set_str(char *dst, size_t size, const char *src)
{
	memset(dst, 0, size);
	if (src && *src)
		strscpy(dst, src, size);
}

static int cdh_ethtool_entry(struct kretprobe_instance *ri, struct pt_regs *regs)
{
	struct cdh_ethtool_data *data = (struct cdh_ethtool_data *)ri->data;

	data->dev = (struct net_device *)regs_get_kernel_argument(regs, 0);
	data->rsp = (struct cdh_ethtool_devlink_compat *)regs_get_kernel_argument(regs, 1);
	if (!data->dev || !data->rsp)
		return 1;
	return 0;
}

static int cdh_ethtool_ret(struct kretprobe_instance *ri, struct pt_regs *regs)
{
	struct cdh_ethtool_data *data = (struct cdh_ethtool_data *)ri->data;
	struct net_device *dev = data->dev;
	struct cdh_ethtool_devlink_compat *rsp = data->rsp;
	int rc = (int)regs_return_value(regs);

	if (rc != 0 || !dev || !rsp)
		return 0;
	if (rsp->info.cmd != ETHTOOL_GDRVINFO)
		return 0;

	if (cdh_ethtool_should_suppress(dev, &rsp->info)) {
		regs_set_return_value(regs, (unsigned long)(long)-EOPNOTSUPP);
		return 0;
	}

	if (!strncmp(dev->name, "wlan", 4) ||
	    !strcmp(rsp->info.driver, "mac80211_hwsim")) {
		cdh_ethtool_set_str(rsp->info.driver, sizeof(rsp->info.driver), "wlan");
		cdh_ethtool_set_str(rsp->info.fw_version, sizeof(rsp->info.fw_version), "");
		cdh_ethtool_set_str(rsp->info.bus_info, sizeof(rsp->info.bus_info), "");
		cdh_ethtool_set_str(rsp->info.erom_version, sizeof(rsp->info.erom_version), "");
		cdh_ethtool_set_str(rsp->info.version, sizeof(rsp->info.version),
				    init_uts_ns.name.release);
		return 0;
	}

	cdh_ethtool_set_str(rsp->info.version, sizeof(rsp->info.version),
			    init_uts_ns.name.release);
	return 0;
}

static struct kretprobe cdh_ethtool_krp = {
	.entry_handler = cdh_ethtool_entry,
	.handler = cdh_ethtool_ret,
	.data_size = sizeof(struct cdh_ethtool_data),
	.maxactive = 32,
	.kp = {
		.symbol_name = "ethtool_get_drvinfo",
	},
};

static bool cdh_ethtool_registered;

/* 注册 ETHTOOL_GDRVINFO kretprobe 钩子（若当前内核符号缺失则仅告警放行）。 */
int cdh_ethtool_init(void)
{
	int ret;

	ret = register_kretprobe(&cdh_ethtool_krp);
	if (ret) {
		pr_warn("注册 ethtool_get_drvinfo kretprobe 失败（%d），跳过网卡驱动指纹收敛\n", ret);
		cdh_ethtool_registered = false;
		return 0;
	}
	cdh_ethtool_registered = true;
	pr_info("ethtool_get_drvinfo 钩子就绪\n");
	return 0;
}

void cdh_ethtool_exit(void)
{
	if (!cdh_ethtool_registered)
		return;
	unregister_kretprobe(&cdh_ethtool_krp);
	cdh_ethtool_registered = false;
	if (cdh_ethtool_krp.nmissed)
		pr_warn("ethtool_get_drvinfo kretprobe 漏抓 %d 次\n",
			cdh_ethtool_krp.nmissed);
	pr_info("ethtool_get_drvinfo 钩子已卸载\n");
}

#else

int cdh_ethtool_init(void)
{
	return 0;
}

void cdh_ethtool_exit(void)
{
}

#endif
