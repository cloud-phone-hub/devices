# ss9380 补齐 vendor/etc 下的 HAL 机型配置（battery/light/thermal/vibrator/wifi/gnss/sensors）

- 设备：`samsung/ss9380`（Galaxy S25 Ultra，SM8750/pakala，官方固件 `S9380ZCSDCZH1`）
- 日期：2026-10-31
- 结果：7 个配置文件全部落地并逐项生效；**vibrator 必须零 vibrator id**，否则 OneUI 在
  `StartVibratorManagerService` 阶段空指针崩溃、系统永远起不来（本 issue 的核心卡点）。

## 一、问题

用户对照 `devices/manifest/xiaomi/gold/vendor/etc` 指出：`devices/manifest/samsung/ss9380/vendor`
里缺少电池、GNSS、灯、传感器、温控、振动、WiFi 这些 HAL 的机型配置——即各 HAL 服务启动时
读的 `/vendor/etc/<x>_config.xml` 一个都没有，虚拟设备只能吃引擎内置的兜底默认值。

对照 `devices/manifest/*/*/vendor/etc/*_config.xml`：只有 `flyme/m22` 与 `xiaomi/gold` 两棵树
带这套文件，ss9380 静态树原本只有 12 个文件（build.prop、audio_effects 系列、media_config.xml、
excluded-input-devices.xml、ueventd.qcom.rc、init/*.rc）。

## 二、根因

1. **缺文件 = 缺机型能力口径**。各 HAL 的配置路径是硬编码的（`health_service.cpp:105` /
   `lights_service.cpp:95` / `thermal_service.cpp:119` / `vibrator_service.cpp:70` /
   `gnss_service.cpp:276` / `wifi_config.h:61` / `sensors_service.cpp:113`），缺文件就回退
   编译期默认：sensors 回退**空清单**、health 回退 6000000µAh 的假电池、thermal 回退无温区、
   vibrator 回退零 id、audio 回退内置音量组。虚拟设备因此既不反映真机器件，也无法按机型调值。
2. **vibrator 的坑（本 issue 卡点）**：把 vibrator 配置按 m22/gold 的样子写成
   `register-manager="true"` + `<vibrator id="1"/>` 后，系统进入**开机死循环**。
   机理：OneUI 的 `VibratorManagerService` 只要 `getVibratorIds()` 非空，就会为每个 id 建
   `VibratorController`，并在 `initVibratorGroup()` 里无条件调
   `NativeWrapper.supportsHapticEngine()`；该实现在 `/system/lib64/libvibratorservice.so`
   里是：

   ```
   bl   getSehHal()       ; 取 aidl::vendor::samsung::hardware::vibrator::ISehVibrator
   ldr  x0, [sp]          ; x0 = sp<ISehVibrator>.get()
   ldr  x8, [x0]          ; 无判空 —— x0 == nullptr 即 SIGSEGV（fault addr 0x0）
   ```

   `ISehVibrator` 只有官方 `/vendor/bin/hw/vendor.samsung.hardware.vibrator-service` 才注册
   （该 so 静态依赖 `vendor.samsung.hardware.vibrator-V5-ndk.so`）；本虚拟 vendor 树只实现
   AOSP `android.hardware.vibrator.IVibrator`，`getSehHal()` 必然返回空 → system_server 崩。
   而 id 清单为空时 framework 根本不建 controller，也就不碰三星私有 HAL —— 这正是本文件引入
   前的已验收状态（缺文件 → 空清单 → 能开机）。m22/gold 能带非空 id，是因为 Flyme/HyperOS
   的 vibrator 框架不调 `supportsHapticEngine`。

## 三、证据

官方固件取证（`dump.erofs` / `strings`，产物在 `engine/out/tmp/ss9380-official-*`）：

- `/etc/sensors/config/pakala_*.json`：`lsm6dsv`(加速度+陀螺)、`ak991x`(磁力)、`lps22df`/`bmp5`
  (气压)、`stk3afx`(光+接近) → sensors_config.xml 的器件名与 type-id 从这份清单来。
- `/etc/thermal-engine.conf` 只有一行 `File empty by default.` → 真机阈值不在文本里，
  温区名/阈值只能沿用 m22（同为 Qualcomm）那套，并在文件头注明取证边界。
- `/bin/hw/android.hardware.lights-service.samsung` 的 strings 只暴露
  `lcd-backlight`/`panel0-backlight`/`panel`/`button-backlight`/`touchkey-led` → 官方 HAL 只有
  BACKLIGHT/BUTTONS/KEYBOARD，没有通知灯，故 light_config.xml 只报 3 盏。
- `/bin/hw/vendor.samsung.hardware.vibrator-service` 的 strings 含
  `[supportsHapticEngine] [getResonantFrequency] [getQFactor] [getFrequencyResolution]
  [getFrequencyMinimum] [getBandwidthAmplitudeMap]` 与 SEC 的 `timed_output`/`leds`/`vib_info_class`
  sysfs → 证明 `supportsHapticEngine` 是三星私有 AIDL 扩展，不是 AOSP IVibrator 的方法。

崩溃链（`log-fail.txt`，instance 5，baseline `patch.img` sha256 `3d1a497a7fec`）：

```
signal 11 (SIGSEGV), fault addr 0x0, x0=0  → Cause: null pointer dereference
#00 libvibratorservice.so (android::vibrator::AidlHalWrapper::supportsHapticEngine()+48)
#01 libandroid_servers.so (HalController::doWithRetry<bool>)
#02 libandroid_servers.so (vibratorSupportsHapticEngine)
#05 services.jar (VibratorController$NativeWrapper.supportsHapticEngine)
#07 services.jar (VibratorController.initVibratorGroup)
#09 services.jar (VibratorManagerService.<init>)
```

反汇编（`llvm-objdump -d --start-address=0x1a594`，与本机 guest 直连拉取的 so 对齐）：
`0x1a5b8 bl getSehHal()` → `0x1a5c4 ldr x8,[x0]`（正是崩溃 pc）。
guest 侧 `service list | grep vibrator` 只有 AOSP 的 `IVibrator/default` 与
`IVibratorManager/default`，`/vendor/bin/hw` 下也只有 `android.hardware.vibrator-service`，
确认 `ISehVibrator` 不存在。

## 四、改动

`devices/manifest/samsung/ss9380/vendor/etc/`（静态树 12 → 19 个文件，全部 `xmllint --noout` 通过）：

| 文件 | 关键口径 |
| --- | --- |
| `battery_config.xml` | 5000mAh：`full_uah=5000000`、`counter 4945000~4972000`；`ac=1/usb=0`；cycle `4380~4450mV`、`30.0~38.0℃`、600s |
| `light_config.xml` | 仅 3 盏：BACKLIGHT(id0)/BUTTONS(id1)/KEYBOARD(id2)，注明官方 HAL 无通知灯 |
| `thermal_config.xml` | m22 的 8 温区（CPU/GPU/BATTERY/SKIN/PA/NPU/TPU/SOC）+ 加热冷却模型；`coolingDevices` 故意为空 |
| `vibrator_config.xml` | `register-manager="true" capabilities="0" manager-capabilities="0"`，**无 `<vibrator>` 条目**，文件头写明 OneUI 空指针链 |
| `wifi_config.xml` | `channel=/dev/hvc9`（对应 device.json `host-hvc-wifi: hvc9`）、1MiB 帧上限、1000/30000ms、协议 1 |
| `gnss_config.xml` | `Qualcomm,SM8750,QC_GNSS_1.0`、杭州默认 fix、`control-file=/data/vendor/gps/fix` |
| `sensors_config.xml` | 21 项：lsm6dsv_acc/gyro、ak09919、stk3fx_l/p、lps22df（新增气压 handle 30）+ 组合/触发类；去掉了 m22 专有的 12 个 `xiaomi.sensor.*` |

其中 vibrator 这一条是**本 issue 的实质修复**（其余 6 个是补齐），已把踩坑结论写进文件头，
避免以后有人照抄 m22 加上 `<vibrator id="1"/>` 再炸一次。

## 五、验证

`out/bin/cdh device build ss9380`（在 `engine/` 下执行）→ `[ok] 已完成`，
`机型静态树: manifest/samsung/ss9380/vendor（19 个文件）`，`vendor.img 212,492,288 B`，
`patch.img sha256 fb9d1b646449`。新实例 index 6（adb 5911）：

- 开机：`sys.boot_completed=1`，`init.svc.zygote=running`（约 160s，不再循环）。
- battery：`Charge counter: 4963000`（落在 4945000~4972000）、`voltage: 4428`（4380~4450）、
  `AC powered: true`、`level 100 / temperature 353`。
- sensors：`dumpsys sensorservice` 列出 21 项，器件名为 lsm6dsv_acc/lsm6dsv_gyro/ak09919/
  stk3afx_l/stk3afx_p/lps22df + Samsung 组合类；`Recent Sensor events` 有真实事件流。
- light：`dumpsys lights` → `Light id=0/1/2`，与配置的 3 盏一一对应。
- thermal：`dumpsys thermalservice` → `HAL Ready: true`、`ThermalHAL AIDL 3 connected: yes`、
  8 个温区与阈值（`CPU [NaN,NaN,NaN,85,90,100,117]`、`SKIN [NaN,43,45,50,70,80,90]` …）完全一致，
  cooling devices 为空（符合配置）。
- vibrator：`isVibratorControllerRegistered = false`，crash buffer 中 vibrator 相关 0 条。
- wifi：`android.hardware.wifi.IWifi/default` 已注册，`/dev/hvc9` 存在（crw-rw-rw- 229,9）。
- gnss：`android.hardware.gnss.IGnss/default` 已注册并被 framework 连接
  （`SLocationProxy: GNSS batching is disabled`），location 总开关 `true`。
- 崩溃扫描（`logcat -b crash`）：`system_server` 0 条，vibrator/lights/sensors/thermal 0 条；
  仅剩 `com.sec.android.sdhms`(464) 与 `com.qualcomm.location`(148) 等**既有噪音**
  （见 README #86，与本轮改动无关）。

## 六、遗留

1. gnss 的默认 fix 值（杭州坐标、7 星）本轮只验证到「文件下发 + IGnss 注册 + framework 连接」，
   没有客户端发起定位请求，因此 `dumpsys location` 的 `last location` 仍为 null；
   要观测 fix 需要装一个定位客户端或后续加一个自检入口。
2. wifi 只验证到 `IWifi` 注册与 `/dev/hvc9` 存在，未做真实扫描/关联（需要宿主 mac80211_hwsim）。
3. light 的能力面受官方 HAL 限制：通知灯在真机走 AOD/LED 私有通路，本文件不报。
4. thermal 无 cooling device（云机没有风扇/散热节点），温升模型会让 SKIN 逐渐升到 status 1，
   与 m22 行为一致，不是缺陷。
5. `com.sec.android.sdhms` / `com.qualcomm.location` 的崩溃循环是三星 Device Health 与 QTI IZat
   在非真机环境的既有问题（README #86 已记录止损手法：`pm disable-user`），未在本轮处理。
