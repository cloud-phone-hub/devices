# #260 Honor Magic7 RSR（`honor/sun`）基带版本显示不全与电池 `technology` 缺失

- 编号：`#260`
- 分组：`sun`（`honor/sun` 机型层与 radio/battery HAL 适配）
- 机型：`honor/sun`（Honor Magic7 RSR Porsche Design，`16-AQ3A.250214.001-20260904`）
- 状态：**已闭环**（2026-11-24，实例 1）

## 1. 问题

客户机「设置 → 关于手机」中，`基带版本` 为空（补齐注入值后又变成被截断的
`-02024.3-PAKALA_GEN_PACK-1.80805.123.93847.33,-02024.3-PAKALA_GEN_PACK-1.80805.123.93847.33`），
`电池 technology` 显示 `unknown`（真机为 `3rd Gen Silicon-Carbon Battery`）。

## 2. 根因

1. **基带为空**：荣耀固件不在 `vendor/build.prop` 提供 `gsm.version.baseband`，该键属
   `radio_prop`，`vendor_init` 装载被 SELinux 拒绝；非 MTK 变体的 `kBasebandVersion`
   为空串，`getBasebandVersion` 只能返回空。版本真值来自官方固件内层
   `Software/dload/update_sd_base.zip!image/NON-HLOS.bin` 的
   `QC_IMAGE_VERSION_STRING=MPSS.DE.7.0-02024.3-PAKALA_GEN_PACK-1.80805.123.93847.33`。
2. **补齐后被截断**：`gsm.version.baseband` 值上限 91 字节（`PROP_VALUE_MAX-1`）。框架
   把两条 Phone 各上报的基带按槽拼成逗号列表回写同一属性，且**按手机数把每项限制在
   `(91/槽数)=45` 字节、保留尾部**。sun 的完整串 56 字节，双槽拼接 113 字节，于是
   `MPSS.DE.7.0` 前缀被切掉；只消掉重复后单条仍被截到 45 字节（见
   [log-fail.txt](log-fail.txt) 第 3 节），证明截尾发生在框架拼写属性环节而非
   HAL/host 侧。
3. **电池 unknown**：`/vendor/etc/battery_config.xml` 缺失/非 sun 口径，`HealthService`
   取不到 `technology` 与满充容量。

## 3. 证据

- 失败现场：[log-fail.txt](log-fail.txt)（91 字节双段截尾值、`build.prop` 注入完整值、
  `qemu.log` 启动期写入完整值后被覆盖、单条仍被截到 45 字节的对照实验）
- 通过现场：[log-pass.txt](log-pass.txt)（`getprop gsm.version.baseband` 56 字节完整单值、
  `dumpsys battery` 的 `technology` 与 `Charge counter`）

## 4. 改动

1. [devices/manifest/configs/honor/sun/sun-android15/vendor/config.json](../../../../manifest/configs/honor/sun/sun-android15/vendor/config.json)：
   `build-prop-sets` 首位新增
   `"gsm.version.baseband": "MPSS.DE.7.0-02024.3-PAKALA_GEN_PACK-1.80805.123.93847.33"`，
   经 `09_build_vendor` 写入 patch.img 的 `/vendor/build.prop`（第 411 行）。
2. [devices/manifest/configs/honor/sun/sun-android15/vendor/etc/battery_config.xml](../../../../manifest/configs/honor/sun/sun-android15/vendor/etc/battery_config.xml)：
   改为 sun 口径——`technology="3rd Gen Silicon-Carbon Battery"`、
   `full_uah=5850000` / `counter=5810000`、`4480 mV` / `29.0°C`、`wireless=1`。
3. [devices/hardware/android.hardware.radio/src/radio_modem.cpp](../../../../hardware/android.hardware.radio/src/radio_modem.cpp)
   `getBasebandVersion`：当 `基带串 × kLogicalModemCount + (槽数-1) > 91` 时返回
   `RadioError::REQUEST_NOT_SUPPORTED`。框架的
   `EVENT_GET_BASEBAND_VERSION_DONE` 遇到异常直接跳过写属性，属性保留启动时
   `room-daemon` 从 `/vendor/build.prop` 注入的完整值；短基带机型（如 gold 31 字节）
   不触发该分支，双槽行为不变。

## 5. 验证

1. `pipeline device build sun --manifest 16-AQ3A.250214.001-20260904 --from build_hardware --refresh`
   成功：`vda.img` 14.94 GiB（5 个分区已刷新）、`patch.img` 384.0 MiB。
2. 全新 `data.qcow2` 冷启动后 `sys.boot_completed=1`，
   `getprop gsm.version.baseband` =
   `MPSS.DE.7.0-02024.3-PAKALA_GEN_PACK-1.80805.123.93847.33`（len=56，单条、无重复）。
3. `dumpsys battery`：`technology: 3rd Gen Silicon-Carbon Battery`、
   `Charge counter: 5810000`、`level: 100`。

## 6. 遗留

- `--refresh` 在 `09_build_vendor` 里是「清空装配树 + 无条件重编 + 重建镜像」的粗粒度开关，
  会把 23 个 HAL 模块全部重编；**只改 HAL 源码时应省略 `--refresh`**，靠源码指纹只重编
  受影响模块（本次两轮构建的主要耗时来源）。是否需要把该开关拆成
  `--refresh-images` / `--force-hal` 待定。
- 当前单值显示依赖「框架不写属性」这一行为；若后续框架侧或机型配置改为按槽分列展示，
  需重新评估 91 字节上限（真机双卡是否同样截尾未实测）。
- 实测基带值来自官方 `NON-HLOS.bin`，`02_prepare` 的自动提取只扫顶层 zip、不递归内层
  `update_sd_base.zip`，本机型仍是手工写入清单；后续可考虑补内层包提取。
