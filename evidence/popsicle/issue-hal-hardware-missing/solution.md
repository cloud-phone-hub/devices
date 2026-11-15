# device.json 缺整个 hardware 数组 → 07 静默不装 HAL → system_server 连环自杀

- 机型：xiaomi / Xiaomi 17 Pro Max（`popsicle`，HyperOS 3 `OS3.0.319.0.WPBCNXM`，Android 16，lineage-23.2 + kernel 6.12.81）
- 发现时间：2026-11-02
- 盘集：popsicle 首次整线（07→11），实例 4 → 6 → 7 三轮
- 状态：已解决

## 一、问题

07→11 整线构建 EXIT=0，镜像起得来、adbd 可达、`init.svc.bootanim=running`，但
`sys.boot_completed` 永不置位，zygote 无限重启，实例 4 的
`grep -c 'FATAL EXCEPTION IN SYSTEM PROCESS'` = **3054**：

```
E AndroidRuntime: *** FATAL EXCEPTION IN SYSTEM PROCESS: main
E AndroidRuntime: Caused by: java.util.NoSuchElementException:
    IHealth service instance default isn't available. Perhaps no permission?
```

补齐 health 后换成 power，补齐 power 后换成 IR，**一轮一个卡点**，每轮都要重跑构建
（2~7 分钟）+ 起实例（3.5 分钟）：

| 轮次 | 缺失项 | 失败点 | 计数 |
| :--- | :--- | :--- | :--- |
| 实例 4 | `IHealth/default` | `BatteryService.onStart` → `NoSuchElementException` | 3054 |
| 实例 6 | `IPower/default` | `HintManagerService` 构造 → `NullPointerException: SupportInfo.headroom on a null object reference` | 38 |
| 实例 7 | `IConsumerIr/default` | `ConsumerIrService.<init>` → `RuntimeException: FEATURE_CONSUMER_IR present, but no IR HAL loaded!` | 60 |

## 二、根因

`devices/manifest/xiaomi/popsicle/device.json` 只有 18 个键，**缺整个 `hardware` 数组**
（也没有 `hardware-platform`）。07 的 `buildPlan` 只在 device.json **文件不存在**时打
`[warn]`；文件存在但 `hardware` 为空时既不装配也不发声，于是一整轮构建看起来完全正常。

后果是 vendor 里只剩 lineage 基座散件 + `apexSelected(mode=default)` 摊平的
graphics/wifi 两个 APEX，**一个 engine/vendor 自研 HAL 都没有**。而 system_server 对
这三项是单向硬依赖（不是可选 HAL）：缺 `IHealth/default`、`IPower/default`、
`IConsumerIr/default` 任一项，对应的 SystemService 构造/onStart 直接抛，随后
`SystemServer.run` 的 FATAL 让 zygote 重启。

为什么是这三项、以及为什么看不出还缺什么：它们都由**system 分区**的 framework 决定，
vendor 侧无法反推。popsicle 的 `vendor/etc/permissions` 24 个 xml 里连
`android.hardware.consumerir.xml` 都没有（特性登记在 system 分区），因此"查 vendor 有没有
对应 feature xml"这条路走不通；可靠口径是**照抄同厂同系列机型**（gold 是小米 HyperOS，
device.json 17 项）再逐项补齐。

## 三、证据

- 失败态：`log-fail.txt`（实例 4 的 3054 次 health 栈、实例 6 的 headroom NPE、实例 7 的
  `FEATURE_CONSUMER_IR` 栈原文）
- 通过态：`log-pass.txt`（`sys.boot_completed=1`、`topResumedActivity=com.miui.home/…Launcher`、
  `service list` 里 IHealth / IConsumerIr / IPower / IKeyMintDevice 四项齐全、
  `FATAL EXCEPTION IN SYSTEM PROCESS` = 0）
- 截图：本实例 `screencap -p` 会 hang（命令挂住不返回，文件不生成），首屏证据以
  `dumpsys activity activities` 的 `topResumedActivity=com.miui.home/.launcher.Launcher` 为准，
  已记入遗留
- 构建侧：`.tmp/popsicle-hal.log`、`.tmp/popsicle-power.log`、`.tmp/popsicle-ir.log`
- 代码侧：`engine/build/07_build_vendor/plan.go`（空数组告警）、
  `engine/build/07_build_vendor/files.go`（IR 与 FEATURE_CONSUMER_IR 的成对约束，本轮补充
  「特性在 system 分区、vendor 侧查不到」的实测结论）

## 四、改动

1. `devices/manifest/xiaomi/popsicle/device.json`：按 gold 口径补 `hardware-platform: qcom`
   与 `hardware` 18 项 —— audio / bluetooth / camera / gnss / gpu / health / **ir** /
   keymint / light / media / media.v412 / memtrack / **power** / sensors / thermal /
   vibrator / wifi / `android.service.daemon`。
2. `engine/build/07_build_vendor/plan.go`：`moduleBuilds` 之后加空数组告警，避免同型问题再次静默：

   ```go
   if len(item.Hardware) == 0 {
       fmt.Fprintf(stdout, "[warn] 设备清单未声明 hardware，本次不装配任何自研 HAL（system_server 会因缺 IHealth/default 循环重启）: %s\n", engine.Shown(plan.DeviceManifest))
   }
   ```
3. `engine/build/07_build_vendor/files.go`：在 IR 借件注释后补 popsicle 实测结论——
   `vendor/etc/permissions` 没有 consumerir.xml 也照样触发，别拿它当「要不要装 IR HAL」的判据。

## 五、验证

1. `cd engine && ./out/bin/cdh device build popsicle --from build_vendor` → EXIT=0，
   计划摘要 `hardware 18 项`，`android.hardware.ir@default` 在列；
2. 产物落 patch 层：`out/devices/xiaomi/popsicle/OS3.0.319.0.WPBCNXM/patch/stage/root/vendor/`
   下 `bin/hw/android.hardware.ir-service`、`etc/init/android.hardware.ir-service.rc`、
   `etc/vintf/manifest/android.hardware.ir-service.xml` 齐备（不在 `vendor/` 装配树里，
   这一点容易误判成「没装上」）；
3. 新实例（`POST /api/create {"index":5}` → 序号 7）240 s 内 `sys.boot_completed=1`、
   `init.svc.bootanim=stopped`；
4. `service list` 四项 HAL 全在；`grep -c 'FATAL EXCEPTION IN SYSTEM PROCESS'` = 0；
5. `dumpsys activity activities` 首屏是 `com.miui.home/.launcher.Launcher`。

## 六、遗留

- 18 项只覆盖 system_server 与 ImageReader 链路的硬依赖，`android.hardware.power.stats`
  （功耗统计）、`android.hardware.usb`、`android.hardware.nfc`、`android.hardware.radio`
  等仍按「厂商固件是否真有该能力」逐台确认，不为凑数而装；
- `Perf HAL 2.2 is not available`（`ANDR-PERF-CLIENT-SYS`，607 条）与
  `DolbyRenderer: Failed to get DVS Hal Service`（1911 条）是厂商框架的可选依赖，未处理，
  不影响 boot_completed；
- 该卡点的通用收口只做到「告警」，没做「按 system 分区 feature 自动推导 HAL 清单」——
  那需要解包 system/product，成本高于照抄同厂清单；
- `adb shell screencap -p`（无论 `exec-out` 还是写 `/data/local/tmp`）在本实例上挂住不返回，
  与 boot 无关（`dumpsys activity`、`service list` 都正常），待单独取证：优先怀疑图形栈
  （`libGLES_meow` interposer + 摊平 graphics APEX）的 screenshot 路径。
