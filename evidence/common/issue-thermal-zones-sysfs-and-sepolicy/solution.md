# `#180` — `cdh_device_info` 内核温区 `thermal_zone0..30` 注册与 `sysfs_thermal` SELinux 授权对齐（`F2` & `H-115b`）

## 1. 问题

在 `gold`（Xiaomi Redmi Note 13 5G，`OS3.0.10.0.VNQCNXM`）真机与云机全维度对齐自检中：
1. `aligner_xiaomi_gold.battery_power_thermal` 与 `aligner_xiaomi_gold.all` 的 `F2_thermalZonesPresentInSysfs` 失败：真机在 `/sys/class/thermal/` 下暴露 31 个 `thermal_zone0..30`（`sc-cp-standalone`、`mtk-master-charger`、`mtktscpu`、`mtktspmic`、`mtktsbattery`、`mtktsAP` 等）并可读出 `type` 与 `temp`，而云机 `thermalZoneCount = 0`；
2. `ultimate.cloud_emulator` 触发 `H-115b`（`noThermal: true, thermalZones: "0"`）以及 `H-56` 中的 `"无温度遥测节点(物理量缺失)"` 风险信号。

## 2. 根因

1. **内核缺少硬件温区 `thermal_zone*` 注册**：QEMU `virt` 机型未挂接 SoC 传感器与 PMIC/电池 NTC 热敏电阻驱动，内核 `/sys/class/thermal/` 下仅有 8 个 `cooling_device0..7`，不存在任何 `thermal_zone*` 节点；
2. **`/sys/class/thermal` SELinux 标签与普通 App 访问受阻**：即便内核注册了 `thermal_zone*`，在通用 GKI / Lineage `plat_sepolicy.cil` 中 `/sys/class/thermal` 与 `/sys/devices/virtual/thermal` 默认落入未细分的 `u:object_r:sysfs:s0`，Enforcing 模式下普通 App（`untrusted_app`）遍历 `/sys/class/thermal` 会触发 `avc: denied { read } name="thermal" dev="sysfs" tcontext=u:object_r:sysfs:s0 tclass=dir`（内核审计日志 `ino=1760`），导致 `File("/sys/class/thermal").list()` 返回 `null`。

## 3. 证据

- 修复前失败日志见 [log-fail.txt](log-fail.txt)：
  - `dmesg` 明确记录 `avc: denied { read } for comm="checker-run" name="thermal" dev="sysfs" ino=1760 scontext=u:r:untrusted_app:s0:c30,c257,c512,c768 tcontext=u:object_r:sysfs:s0 tclass=dir permissive=0 app=dev.checker`；
  - `run-00230` 中 `F2_thermalZonesPresentInSysfs = false`，`H-115b` `hit = true`。
- 修复后通过日志见 [log-pass.txt](log-pass.txt)：
  - `/sys/class/thermal` 与 `/sys/devices/virtual/thermal/thermal_zone0..30` 及其 `type`、`temp` 属性文件均被打标为 `u:object_r:sysfs_thermal:s0`；
  - `run-00231` 中 `thermalZoneCount = 31`、`hasHardwareThermalZones = true`、`F2_thermalZonesPresentInSysfs = true`（总分 `27 / 32`，维度 F 100% 绿灯），`H-115b` 降为 `hit = false`，`H-56` 中 `"无温度遥测节点(物理量缺失)"` 彻底消除。

## 4. 改动

1. **新增 [`devices/modules/device_info/thermal/thermal_sysfs.c`](../../../modules/device_info/thermal/thermal_sysfs.c)**：
   - 支持从伴随配置文件 `/mnt/vendorpatch/vendor_dlkm/lib/modules/cdh_device_info.thermal`（回退 `/vendor_dlkm/lib/modules/cdh_device_info.thermal`）或模块参数 `thermal` 读取机型温区定义（每行 `<type> <base_temp_mc> [jitter_mc]`）；
   - 未提供伴随文件时回退内置默认 31 个温区（`sc-cp-standalone`、`mtk-master-charger`、`mtk-slave-charger`、`mtk-mst-div-chg`、`mtk-slv-div-chg`、`mtk-mst-hvdiv-chg`、`mtk-slv-hvdiv-chg`、`mtk-gauge`、`battery`、`mtktscpu`、`mtktspmic`、`mtktsbattery`、`mtktsAP`、`mtktsbtsmdpa`、`mtktspa`、`mtktscharger`、`tzimgs0..12`、`mtktsdctm`、`mtkcsCPU`）；
   - 通过内核导出符号 `thermal_tripless_zone_device_register`（Linux `>= 6.4`）与 `thermal_zone_device_enable` 注册 `thermal_zone0..30`（设置 `.no_hwmon = true` 以兼容含连字符 `-` 的温区名），并在 `get_temp` 回调中提供基于 `jiffies` 的毫摄氏度级微幅热波动。
2. **接入 [`cdh_device_info.h`](../../../modules/device_info/include/cdh_device_info.h)、[`core/main.c`](../../../modules/device_info/core/main.c) 与 [`Kbuild`](../../../modules/device_info/Kbuild)**：
   - 在模块初始化与卸载链路中挂接 `cdh_thermal_sysfs_init()` / `cdh_thermal_sysfs_exit()`。
3. **补齐 [`android.hardware.thermal/sepolicy/vendor_sepolicy.cil`](../../../vendor/hardware/android.hardware.thermal/sepolicy/vendor_sepolicy.cil)**：
   - 新增 `(genfscon sysfs "/class/thermal" (u object_r sysfs_thermal ((s0) (s0))))` 与 `(genfscon sysfs "/devices/virtual/thermal" (u object_r sysfs_thermal ((s0) (s0))))`；
   - 授权 `hal_thermal_default` 与 `appdomain` 对 `sysfs_thermal_{{vers}}` 的 `dir` / `file` / `lnk_file` 只读访问（`plat_sepolicy.cil` 对 `sysfs_thermal` 无 `neverallow` 限制，且 `dev.dirty_policy` 零命中）。

## 5. 验证

1. 按 `AGENTS.md` 规则 6 执行 `03_build_modules → 07_build_vendor → 08_build_vendor_dlkm → 09_build_patch → 11_pack_vda`（`patch.img` sha256 `2c22271fc688`，`vda-e4b1d448fe1d`），重启云机实例 `gold_15-OS3.0.10.0.VNQCNXM/1`；
2. 触发 `device-checker` `run-00231`：
   - `aligner_xiaomi_gold.battery_power_thermal`：`thermalZoneCount = 31`、`hasHardwareThermalZones = true`；
   - `aligner_xiaomi_gold.all`：**`27 / 32`** 通过（`F1=true, F2=true`，维度 F 100% 绿灯）；
   - `ultimate.cloud_emulator`：`H-115b` `hit = false`（`noThermal: false, thermalZones: "31"`），`H-56` 仅保留与真机一致的 `["overlay 根文件系统 x24"]`。

## 6. 遗留

- 维度 F（电池、功耗配置文件与温控遥测）2 项指标已 100% 绿灯闭环；若有其它 SoC 机型需定制不同于默认 31 温区的列表，可在机型配置下提供 `cdh_device_info.thermal` 伴随文件覆盖。
