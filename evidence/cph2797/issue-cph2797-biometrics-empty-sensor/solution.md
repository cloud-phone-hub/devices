# CPH2797 system_server 反复自杀：ColorOS 指纹扩展对「空传感器表 / createSession 异常」零容错

- 设备：OPPO Find X9 / CPH2797 / ColorOS 16.0.8.301_EX01（`board=mt6993`，联发科）
- 实例：失败 11（空表）→ 失败 12（createSession 抛异常）→ 通过 13
- 日期：2026-10-30
- 上游条目：#77（bringup）、#78（soundtrigger 空 HAL）
- 日志：`log-fail.txt`（实例 11 + 12）/ `log-pass.txt`（实例 13）

## 1. 问题

soundtrigger 空 HAL 落地后（#78），实例 11 的 `sysrq: Resetting` 归零，但
`sys.boot_completed` 仍恒空，system_server 每 ~15 s 自杀一次：

```
*** FATAL EXCEPTION IN SYSTEM PROCESS: BiometricServiceRegistry
java.lang.ArrayIndexOutOfBoundsException: Array index out of range: 0
    at android.util.SparseArray.valueAt(SparseArray.java:224)
    at android.hardware.biometrics.common.SensorList.valueAt(SensorList.java:67)
    at com.oplus.server.fingerprint.FingerprintServiceProviderExtImpl.init(FingerprintServiceProviderExtImpl.java:211)
    at com.android.server.biometrics.sensors.fingerprint.FingerprintProvider.initSensors(FingerprintProvider.java:...)
    at com.android.server.biometrics.sensors.fingerprint.FingerprintService.getProviders(FingerprintService.java:...)
    at com.android.server.biometrics.BiometricServiceRegistry.registerAllInBackground(...)
```

随后 `Process: Sending signal. PID:<self> SIG: 9` + `Quit itself`：不是 watchdog，
是未捕获异常把 system_server 自己打死，zygote 再拉起，循环。

## 2. 根因

两个叠加事实：

1. **框架必然走 AIDL 指纹探测**。官方 `/odm/etc/vintf/manifest/manifest_oplus_fingerprint_aidl_v3.xml`
   声明了 `<name>android.hardware.biometrics.fingerprint</name><version>3</version>
   <fqname>IFingerprint/default</fqname>`，所以 `BiometricServiceRegistry` 一定会
   枚举指纹提供者，不能靠「删声明」绕过。
2. **ColorOS 扩展不判空**。`FingerprintServiceProviderExtImpl.init()` 对
   `SensorList` 直接 `valueAt(0)`；我们的 `fingerprint_service.cpp`（基础变体）如实
   上报「云端没有指纹传感器」→ 空表 → 越界 → 主线程崩。

### 2.1 第二因：createSession 抛异常同样致命

补上占位传感器后（实例 12），越界消失，但出现新的一类自杀：

```
*** FATAL EXCEPTION IN SYSTEM PROCESS: FingerprintHandler
java.lang.UnsupportedOperationException: no fingerprint sensor
    at android.hardware.biometrics.fingerprint.IFingerprint$Stub$Proxy.createSession(IFingerprint.java:...)
    at com.android.server.biometrics.sensors.fingerprint.FingerprintStartUserClient.startHalOperation(FingerprintStartUserClient.java:...)
    at com.android.server.biometrics.sensors.fingerprint.FingerprintProvider.scheduleInternalCleanup(FingerprintProvider.java:...)
```

即框架开机时自己会 `createSession` 做内部清理，我们把
`EX_UNSUPPORTED_OPERATION` 抛回去，ColorOS 侧同样未捕获。

## 3. 证据

- 实例 11：`ArrayIndexOutOfBoundsException` 82 次、`FATAL EXCEPTION IN SYSTEM
  PROCESS` 40 次、zygote 重启多轮、`boot_progress_enable_screen` 0。
- 实例 12：`ArrayIndexOutOfBoundsException` 0（传感器上报生效），但
  `no fingerprint sensor` 21 次、`FingerprintHandler` 11 次、FATAL 11 次。
- 官方 VINTF 片段（`dump.erofs --cat odm_a.img`）确认 v3 声明存在，且同目录还有
  `manifest_oplus_face.xml`（face v4 `IFace/default`）。
- 变体隔离核对：`build.go:336-419` 每次 `hal build` 都 `rm gen` 后全量重编
  （obj 固定命名 `g<i>.o`），不存在跨变体复用旧对象，`-D` 必然生效——实例 12 的
  崩溃点从越界换成 createSession，就是实证。

## 4. 改动

全部改在 biometrics 模块与机型清单，**基础变体行为不变**（m22 / flyme 不受影响）：

- `engine/vendor/build/modules/android.hardware.biometrics.json`：新增变体

  ```json
  "variants": {
    "fakefingerprint": { "compile": { "extraFlags": ["-DCDH_FINGERPRINT_FAKE_SENSOR=1"] } }
  }
  ```

- `devices/manifest/oppo/cph2797/device.json`：biometrics 条目的
  `variant` 由 `default` 改为 `fakefingerprint`。
- `engine/vendor/hardware/android.hardware.biometrics/src/fingerprint_service.cpp`：
  在 `CDH_FINGERPRINT_FAKE_SENSOR` 下
  1. `getSensorProps` 上报 1 块占位传感器（`sensorId=0`、`SensorStrength::CONVENIENCE`、
     `maxEnrollmentsPerUser=1`、`FingerprintSensorType::REAR`）；
  2. `createSession` 返回占位 `BnSession`（22 个方法全 no-op，`enroll` /
     `authenticate` / `detectInteraction` 系列回一个 `BnCancellationSignal`），
     绝不抛 `EX_UNSUPPORTED_OPERATION`。

## 5. 验证

实例 13（`out/core/daemon/runtime/cph2797_16.0.8.301_EX01/13/qemu.log`）：

| 指标 | 实例 11/12 | 实例 13 |
| --- | --- | --- |
| `ArrayIndexOutOfBoundsException` | 82 | **0** |
| `no fingerprint sensor` | 21（实例 12） | **0** |
| `FATAL EXCEPTION IN SYSTEM PROCESS` | 40 / 11 | **0** |
| 启动进度 | 卡在 biometrics | 推进到 `OnBootPhase_500_*` |

指纹不再挡住开机；后续 `power.stats`、`contexthub` 两个卡点各自独立（见 #80 / #81），
最终实例 15 达成 `sys.boot_completed=1`。

## 6. 遗留

- 变体只解决「ColorOS 扩展不判空」，`createSession` 之后任何真正调用指纹的入口都
  会拿到 no-op，表现为「录入无反应」——云机没有指纹硬件，这是预期行为。
- 同一变体机制可复用到其它「ColorOS 对空/异常零容错」的 HAL（后续 issue 沿用）。
- 官方 `/odm` 还有 `manifest_oplus_face.xml`（face v4）；本轮 face 未触发崩溃，
  若后续人脸相关路径报同类异常，按本条同法处理。
