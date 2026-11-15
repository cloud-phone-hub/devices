/* Copyright (c) 2025-2026 fei_cong(https://github.com/feicong/feicong-course)
 *
 * 输入设备身份配置：机型差异的唯一来源是基线清单 modules.input_identity，
 * 由 08_build_vendor_dlkm 渲染为行式文本 /lib/modules/cdh_input_identity.devices，
 * 本模块运行时读取解析（照 device_info/uts 范式）。声明该机型全部内核侧输入设备——
 * 静态注册设备（devices）+ QEMU 侧虚拟设备的能力对齐（aligns/touch_major）+
 * goodix_fp 形态开关。执行侧（main.c）泛化注册与对齐，零机型分支、零机型缺省。
 * 全部字段只描述「静态身份与能力」，不含任何事件逻辑。
 *
 * 与旧 profiles/ 声明式档案的区别：设备数据不再编进 .ko，而是运行时从伴随文件
 * 动态解析（krealloc 动态数组）；缺文件/解析失败 → -EINVAL，不加载（无内置缺省）。
 */
#ifndef CDH_INPUT_CONFIG_H
#define CDH_INPUT_CONFIG_H

#include <linux/stddef.h>
#include <linux/types.h>

/* 伴随文件大小上限（与 uts 同口径，64 KiB 足够数十台设备）。 */
#define CDH_INPUT_DEVICES_FILE_MAX (64 * 1024)

/* 解析上限，与构建期 Go 校验（internal/manifest MaxInput*）对齐。 */
#define CDH_INPUT_MAX_DEVICES 32
#define CDH_INPUT_MAX_ALIGNS 16
#define CDH_INPUT_MAX_CAPS 256
#define CDH_INPUT_MAX_ABS 64
#define CDH_INPUT_NAME_MAX 128

/* 能力码范围上限（对齐 linux/input-event-codes.h 的 *_MAX）。 */
#define CDH_INPUT_KEY_MAX 0x2ff
#define CDH_INPUT_SW_MAX 0x10
#define CDH_INPUT_MSC_MAX 0x7
#define CDH_INPUT_ABS_MAX 0x3f

/*
 * kernel_read_file_from_path() 走 filp_open -> getname_kernel，不经过 cdh_redirect
 * 的 getname_flags 用户态路径重定向；因此缺省先读 patch.img 载体物理路径
 * /mnt/vendorpatch/vendor_dlkm/...，-ENOENT 时再回退直挂路径（照 uts.c）。
 */
#define CDH_PATCH_INPUT_DEVICES_PATH \
  "/mnt/vendorpatch/vendor_dlkm/lib/modules/cdh_input_identity.devices"
#define CDH_DEFAULT_INPUT_DEVICES_PATH \
  "/vendor_dlkm/lib/modules/cdh_input_identity.devices"

/* 一条 abs 轴声明（input_set_abs_params 的参数组）。 */
struct cdh_input_abs_spec {
  unsigned int axis;
  int min;
  int max;
  int fuzz;
  int flat;
};

/* 一台输入设备的完整身份：名称、bus/vendor/product/version、Location，
 * 以及 KEY/SW/MSC/ABS 能力集。数组为空（NULL + 0）表示不声明该类能力。
 * 所有指针均为动态分配，由 cdh_input_config_free 统一释放。 */
struct cdh_input_dev_spec {
  char *name;
  __u16 bustype;
  __u16 vendor;
  __u16 product;
  __u16 version;
  char *phys;
  unsigned int *keys;
  size_t n_keys;
  unsigned int *switches;
  size_t n_switches;
  unsigned int *msc;
  size_t n_msc;
  struct cdh_input_abs_spec *abs;
  size_t n_abs;
};

/* QEMU 侧键盘设备的能力对齐声明。设备本体由宿主 daemon 按清单
 * modules.input.keyboard 创建（name/bus/vendor/product 以清单为准），
 * 本模块只在 connect 时把 keybit 收敛为真机按键集，并清掉
 * EV_MSC/EV_LED/EV_REP（真机按键设备无扫描码/指示灯/重复上报）。 */
struct cdh_input_align_spec {
  char *name;
  unsigned int *keys;
  size_t n_keys;
};

/* 一台机型的完整配置：静态注册设备集 + QEMU 侧设备对齐 + goodix_fp 形态开关
 * （/sys/bus/platform/drivers/goodix_fp，供 AIDA64/DevInfo 读取）。 */
struct cdh_input_config {
  struct cdh_input_dev_spec *devices;
  size_t n_devs;
  struct cdh_input_align_spec *aligns;
  size_t n_aligns;
  /* QEMU 多点触控屏补 ABS_MT_TOUCH_MAJOR（QEMU virtio 缺该轴，真机有）。
   * 按 MT 能力位匹配任意触屏、不绑定设备名——同一配置被多机型共用，
   * 各机型清单 modules.input.touch 的触屏名不同。 */
  bool touch_major;
  bool goodix_fp;
};

/* 读取并解析配置：inline_text 非空时优先（调试内联），否则按 path（空则走
 * patch 物理路径→直挂路径回退）。成功返回 0 并填 cfg（调用方负责 free），
 * used_path 回填实际来源展示串；失败返回负 errno，cfg 保持已释放状态。 */
int cdh_input_config_load(const char *path, const char *inline_text,
                          struct cdh_input_config *cfg, const char **used_path);

/* 释放 cfg 内全部动态数组与字符串，并清零（可重复调用）。 */
void cdh_input_config_free(struct cdh_input_config *cfg);

#endif /* CDH_INPUT_CONFIG_H */