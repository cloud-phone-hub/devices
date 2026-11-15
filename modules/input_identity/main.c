// Copyright (c) 2025-2026 fei_cong(https://github.com/feicong/feicong-course)
//
// 按真机取证注册输入设备身份：名称、bus/vendor/product/version 与
// capability 与目标机型的对应节点一致，使 InputReader 算出的
// descriptor/sources 与真机对齐。设备不发任何事件，仅静态声明能力。
//
// 结构：机型差异全部在基线清单 modules.input_identity 声明（一机型一份），
// 由 08_build_vendor_dlkm 渲染为行式文本 /lib/modules/cdh_input_identity.devices，
// 本模块运行时读取解析（config.c，照 device_info/uts 范式）；本文件是泛化执行侧，
// 零机型分支、零机型缺省。缺文件/解析失败 → -EINVAL，不加载。所用 API 均为
// 5.10–6.18 各代内核的稳定导出符号，多版本同源码编译。
//
// 另含 goodix_fp platform driver 形态（原 goodix_fingerprint 模块，
// 2026-11 收归）：只生成 AIDA64/DevInfo 会读的
// /sys/bus/platform/drivers/goodix_fp，不提供真实指纹能力；
// 是否注册由配置的 goodix_fp 开关决定。
#include "config.h"

#include <linux/bitmap.h>
#include <linux/init.h>
#include <linux/input.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/string.h>

static char *cdh_devices_path;
module_param_named(devices_path, cdh_devices_path, charp, 0444);
MODULE_PARM_DESC(devices_path,
                 "输入设备数据文件路径（缺省先读 patch 物理路径再回退直挂路径）");

static char *cdh_devices_inline;
module_param_named(devices, cdh_devices_inline, charp, 0444);
MODULE_PARM_DESC(devices, "调试用内联设备数据文本（非空时忽略 devices_path）");

static struct input_dev **cdh_input_devs;
static int cdh_input_dev_count;
/* 当前生效配置：init 里解析后登记，供对齐 handler 查声明（handler 注册前赋值）。 */
static struct cdh_input_config cdh_config;
static const struct cdh_input_config *cdh_active_config;

static struct input_dev *cdh_input_alloc(const struct cdh_input_dev_spec *spec) {
  struct input_dev *dev = input_allocate_device();

  if (!dev)
    return NULL;
  dev->name = spec->name;
  dev->phys = spec->phys;
  dev->id.bustype = spec->bustype;
  dev->id.vendor = spec->vendor;
  dev->id.product = spec->product;
  dev->id.version = spec->version;
  return dev;
}

static int cdh_input_add(struct input_dev *dev) {
  int ret;

  if (!dev)
    return -ENOMEM;
  ret = input_register_device(dev);
  if (ret) {
    input_free_device(dev);
    return ret;
  }
  cdh_input_devs[cdh_input_dev_count++] = dev;
  return 0;
}

/* 泛化注册：按配置声明逐类铺能力，不含任何设备/机型专属逻辑。 */
static int cdh_input_register_spec(const struct cdh_input_dev_spec *spec) {
  struct input_dev *dev;
  size_t i;

  dev = cdh_input_alloc(spec);
  if (!dev)
    return -ENOMEM;
  for (i = 0; i < spec->n_keys; i++)
    input_set_capability(dev, EV_KEY, spec->keys[i]);
  for (i = 0; i < spec->n_switches; i++)
    input_set_capability(dev, EV_SW, spec->switches[i]);
  for (i = 0; i < spec->n_msc; i++)
    input_set_capability(dev, EV_MSC, spec->msc[i]);
  for (i = 0; i < spec->n_abs; i++)
    input_set_abs_params(dev, spec->abs[i].axis, spec->abs[i].min,
                         spec->abs[i].max, spec->abs[i].fuzz,
                         spec->abs[i].flat);
  return cdh_input_add(dev);
}

/* QEMU 侧虚拟输入设备的能力对齐，全部由配置声明驱动（零机型分支）：
 * - touch_major：真机触屏驱动额外声明 ABS_MT_TOUCH_MAJOR，使 InputReader
 *   TouchInputMapper 自动合成 SIZE / TOUCH_MAJOR / TOUCH_MINOR / TOOL_MAJOR /
 *   TOOL_MINOR（共 8 轴）。QEMU virtio 多点触控缺该轴，按 MT 能力位匹配补齐，
 *   不绑定设备名（同一档案被多机型共用，各机型触屏名不同）。
 * - aligns：按名匹配 QEMU 键盘，把默认全键盘的 keybit 收敛为档案声明的
 *   真机按键集，并清 EV_MSC/EV_LED/EV_REP。KEY_Q 是 QEMU 全键盘特征位，
 *   已对齐过的设备不再含它，天然幂等。
 * connect 返回 -ENODEV 仅就地补齐能力位，不绑定 input_handle，事件热路径零开销。 */
static int cdh_ts_align_connect(struct input_handler *handler,
                                struct input_dev *dev,
                                const struct input_device_id *id) {
  const struct cdh_input_config *p = cdh_active_config;
  size_t i;

  (void)handler;
  (void)id;
  if (p->touch_major && test_bit(ABS_MT_POSITION_X, dev->absbit) &&
      test_bit(ABS_MT_POSITION_Y, dev->absbit) &&
      !test_bit(ABS_MT_TOUCH_MAJOR, dev->absbit)) {
    input_set_abs_params(dev, ABS_MT_TOUCH_MAJOR, 0, 255, 0, 0);
    pr_info("cdh_input_identity: aligned ABS_MT_TOUCH_MAJOR on %s\n",
            dev->name ? dev->name : "unknown");
  }
  for (i = 0; i < p->n_aligns; i++) {
    const struct cdh_input_align_spec *a = &p->aligns[i];
    size_t j;

    if (!dev->name || strcmp(dev->name, a->name) != 0 ||
        !test_bit(KEY_Q, dev->keybit))
      continue;
    bitmap_zero(dev->keybit, KEY_CNT);
    for (j = 0; j < a->n_keys; j++)
      __set_bit(a->keys[j], dev->keybit);
    __clear_bit(EV_MSC, dev->evbit);
    __clear_bit(EV_LED, dev->evbit);
    __clear_bit(EV_REP, dev->evbit);
    bitmap_zero(dev->mscbit, MSC_CNT);
    bitmap_zero(dev->ledbit, LED_CNT);
    pr_info("cdh_input_identity: aligned keybit (%zu keys) on %s\n", a->n_keys,
            dev->name);
  }
  return -ENODEV;
}

static void cdh_ts_align_disconnect(struct input_handle *handle) {
  (void)handle;
}

static const struct input_device_id cdh_ts_align_ids[] = {
    {
        .flags = INPUT_DEVICE_ID_MATCH_EVBIT,
        .evbit = {BIT_MASK(EV_ABS)},
    },
    {
        .flags = INPUT_DEVICE_ID_MATCH_EVBIT,
        .evbit = {BIT_MASK(EV_KEY)},
    },
    {},
};

static struct input_handler cdh_ts_align_handler = {
    .connect = cdh_ts_align_connect,
    .disconnect = cdh_ts_align_disconnect,
    .name = "cdh_ts_align",
    .id_table = cdh_ts_align_ids,
};
static bool cdh_ts_handler_registered;

/* goodix_fp：空 platform_driver，只生成 /sys/bus/platform/drivers/goodix_fp
 * 这一 kernel-visible 形态（AIDA64/DevInfo 会读）。不注册 platform device、
 * 不触发真实硬件 probe、不发任何 input event。 */
static struct platform_driver cdh_goodix_fp_driver = {
    .driver =
        {
            .name = "goodix_fp",
            .owner = THIS_MODULE,
        },
};
static bool cdh_goodix_fp_registered;

static void cdh_input_identity_teardown(void) {
  if (cdh_goodix_fp_registered) {
    platform_driver_unregister(&cdh_goodix_fp_driver);
    cdh_goodix_fp_registered = false;
  }
  if (cdh_ts_handler_registered) {
    input_unregister_handler(&cdh_ts_align_handler);
    cdh_ts_handler_registered = false;
  }
  while (cdh_input_dev_count > 0)
    input_unregister_device(cdh_input_devs[--cdh_input_dev_count]);
  kfree(cdh_input_devs);
  cdh_input_devs = NULL;
  cdh_active_config = NULL;
  cdh_input_config_free(&cdh_config);
}

static int __init cdh_input_identity_init(void) {
  const struct cdh_input_config *p;
  const char *used_path = CDH_DEFAULT_INPUT_DEVICES_PATH;
  size_t i;
  int ret;

  ret = cdh_input_config_load(cdh_devices_path, cdh_devices_inline, &cdh_config,
                              &used_path);
  if (ret) {
    pr_err("cdh_input_identity: 输入设备配置装载失败（%d），模块不加载\n", ret);
    return ret;
  }
  p = &cdh_config;
  cdh_active_config = p;

  if (p->n_devs > 0) {
    cdh_input_devs = kcalloc(p->n_devs, sizeof(*cdh_input_devs), GFP_KERNEL);
    if (!cdh_input_devs) {
      ret = -ENOMEM;
      goto fail;
    }
  }

  for (i = 0; i < p->n_devs; i++) {
    ret = cdh_input_register_spec(&p->devices[i]);
    if (ret)
      goto fail;
  }

  ret = input_register_handler(&cdh_ts_align_handler);
  if (ret)
    goto fail;
  cdh_ts_handler_registered = true;

  if (p->goodix_fp) {
    ret = platform_driver_register(&cdh_goodix_fp_driver);
    if (ret)
      goto fail;
    cdh_goodix_fp_registered = true;
    pr_info("cdh_input_identity: registered goodix_fp platform driver\n");
  }

  pr_info("cdh_input_identity: %s registered %d input devices\n", used_path,
          cdh_input_dev_count);
  return 0;

fail:
  cdh_input_identity_teardown();
  return ret;
}

static void __exit cdh_input_identity_exit(void) {
  cdh_input_identity_teardown();
  pr_info("cdh_input_identity: removed\n");
}

module_init(cdh_input_identity_init);
module_exit(cdh_input_identity_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("fei_cong <fei_cong@hotmail.com>");
MODULE_DESCRIPTION("Static input device identity set + goodix_fp sysfs shape for CDH (config-driven from manifest modules.input_identity)");