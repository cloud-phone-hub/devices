// SPDX-License-Identifier: GPL-2.0-only
/*
 * 虚拟 NFC 内核探针节点：
 *   1. /sys/class/nfc/nfc_support（只读 sysfs）：
 *      三星 One UI SecNfc（NfcUtil#isKernelDriverExist）在启动前硬检该路径，
 *      缺失则拒绝触发 NFC 引导。
 *   2. /dev/pn544 与 /dev/nq-nci（只读杂项字符设备）及 /sys/nfc/nfc_chip_type（只读 sysfs）：
 *      - 荣耀 MagicOS NfcNci（NfcApplication#onCreate）硬检
 *        new File(SystemProperties.get("nfc.node", "/dev/pn544")).exists()；
 *      - 小米澎湃 OS XMNfcNci（NfcApplication#onCreate）硬检
 *        new File("/dev/nq-nci").exists()；
 *      缺失对应节点则直接 return 跳过 new NfcService(...)。
 *      随后 HnVendorAdapter 读 /sys/nfc/nfc_chip_type 决定加载哪套 JNI 库（返回 "sn220u"）。
 *      AOSP / vendor file_contexts 为 /dev/pn544 与 /dev/nq-nci 绑定 u:object_r:nfc_device:s0。
 *   由 cdh_device_info 在加载期按需注册，供云机虚拟 NFC HAL 正常启用。
 */
#define pr_fmt(fmt) "cdh_device_info_nfc: " fmt

#include <linux/device.h>
#include <linux/err.h>
#include <linux/fs.h>
#include <linux/kernel.h>
#include <linux/kernel_read_file.h>
#include <linux/kobject.h>
#include <linux/miscdevice.h>
#include <linux/sysfs.h>
#include <linux/version.h>
#include <linux/vmalloc.h>

#include "cdh_device_info.h"

static struct class *cdh_nfc_class;
static struct kobject *cdh_nfc_kobj;
static bool cdh_nfc_pn544_registered;
static bool cdh_nfc_nq_nci_registered;

static bool cdh_nfc_hal_present(void)
{
	static const char * const rc_paths[] = {
		"/mnt/vendorpatch/vendor/etc/init/android.hardware.nfc-service.rc",
		"/vendor/etc/init/android.hardware.nfc-service.rc",
	};
	size_t i;

	for (i = 0; i < ARRAY_SIZE(rc_paths); i++) {
		void *buf = NULL;
		size_t size = 0;
		int rc = kernel_read_file_from_path(rc_paths[i], 0, &buf, 16384,
						    &size, READING_POLICY);
		if (buf)
			vfree(buf);
		if (rc >= 0 && size > 0)
			return true;
	}
	return false;
}

static const struct file_operations cdh_nfc_pn544_fops = {
	.owner = THIS_MODULE,
};

static struct miscdevice cdh_nfc_pn544_miscdev = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "pn544",
	.fops = &cdh_nfc_pn544_fops,
	.mode = 0444,
};

static struct miscdevice cdh_nfc_nq_nci_miscdev = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "nq-nci",
	.fops = &cdh_nfc_pn544_fops,
	.mode = 0444,
};

static ssize_t nfc_chip_type_show(struct kobject *kobj,
				  struct kobj_attribute *attr, char *buf)
{
	/*
	 * HnVendorAdapter.java 静态初始化直接用
	 * HnNfcService.readFileByChars("/sys/nfc/nfc_chip_type").equalsIgnoreCase("sn220u")
	 * 比对且不做 trim/strip，不能带尾部换行符。
	 */
	return sysfs_emit(buf, "sn220u");
}

static struct kobj_attribute cdh_nfc_chip_type_attr =
	__ATTR_RO(nfc_chip_type);

#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 4, 0)
static ssize_t nfc_support_show(const struct class *cls,
				const struct class_attribute *attr, char *buf)
#else
static ssize_t nfc_support_show(struct class *cls,
				struct class_attribute *attr, char *buf)
#endif
{
	return sysfs_emit(buf, "1\n");
}

static CLASS_ATTR_RO(nfc_support);

int cdh_nfc_sysfs_init(void)
{
	int ret;

	if (!cdh_nfc_hal_present()) {
		pr_info("nfc HAL service absent, skip virtual nfc nodes\n");
		return 0;
	}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 4, 0)
	cdh_nfc_class = class_create("nfc");
#else
	cdh_nfc_class = class_create(THIS_MODULE, "nfc");
#endif
	if (IS_ERR(cdh_nfc_class)) {
		pr_warn("class_create(nfc) failed (%ld), skip virtual nfc_support\n",
			PTR_ERR(cdh_nfc_class));
		cdh_nfc_class = NULL;
	} else {
		ret = class_create_file(cdh_nfc_class, &class_attr_nfc_support);
		if (ret) {
			pr_warn("class_create_file(nfc_support) failed (%d)\n", ret);
			class_destroy(cdh_nfc_class);
			cdh_nfc_class = NULL;
		} else {
			pr_info("registered /sys/class/nfc/nfc_support\n");
		}
	}

	cdh_nfc_kobj = kobject_create_and_add("nfc", NULL);
	if (!cdh_nfc_kobj) {
		pr_warn("kobject_create_and_add(nfc) failed, skip /sys/nfc/nfc_chip_type\n");
	} else {
		ret = sysfs_create_file(cdh_nfc_kobj, &cdh_nfc_chip_type_attr.attr);
		if (ret) {
			pr_warn("sysfs_create_file(nfc_chip_type) failed (%d)\n", ret);
			kobject_put(cdh_nfc_kobj);
			cdh_nfc_kobj = NULL;
		} else {
			pr_info("registered /sys/nfc/nfc_chip_type (sn220u)\n");
		}
	}

	ret = misc_register(&cdh_nfc_pn544_miscdev);
	if (ret) {
		pr_warn("misc_register(pn544) failed (%d), skip virtual /dev/pn544\n",
			ret);
	} else {
		cdh_nfc_pn544_registered = true;
		pr_info("registered /dev/pn544\n");
	}

	ret = misc_register(&cdh_nfc_nq_nci_miscdev);
	if (ret) {
		pr_warn("misc_register(nq-nci) failed (%d), skip virtual /dev/nq-nci\n",
			ret);
	} else {
		cdh_nfc_nq_nci_registered = true;
		pr_info("registered /dev/nq-nci\n");
	}
	return 0;
}

void cdh_nfc_sysfs_exit(void)
{
	if (cdh_nfc_nq_nci_registered) {
		misc_deregister(&cdh_nfc_nq_nci_miscdev);
		cdh_nfc_nq_nci_registered = false;
	}
	if (cdh_nfc_pn544_registered) {
		misc_deregister(&cdh_nfc_pn544_miscdev);
		cdh_nfc_pn544_registered = false;
	}
	if (cdh_nfc_kobj) {
		sysfs_remove_file(cdh_nfc_kobj, &cdh_nfc_chip_type_attr.attr);
		kobject_put(cdh_nfc_kobj);
		cdh_nfc_kobj = NULL;
	}
	if (!cdh_nfc_class)
		return;
	class_remove_file(cdh_nfc_class, &class_attr_nfc_support);
	class_destroy(cdh_nfc_class);
	cdh_nfc_class = NULL;
}
