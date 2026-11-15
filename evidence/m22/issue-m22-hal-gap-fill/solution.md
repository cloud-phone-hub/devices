# issue-m22-hal-gap-fill — m22 补齐缺失 HAL 服务（nfc + biometrics virtualhal + 相机/10 个现成模块落地）

- **设备**：`flyme/m22` 12.5.0.0A（QTI `sun`），云机实例 31 取证
- **构建侧**：`engine/vendor`（cdh-vendor）→ `devices/manifest/flyme/m22/device.json`
- **状态**：构建层完成并逐模块编译通过；**未装配实机**（07 实跑 + 实例启动复测待做）
- **日期**：2026-10-29
- **关联条目**：#52 [`issue-m22-instance31-binder-stall`](../issue-m22-instance31-binder-stall/solution.md)
  （本条目处理该条「遗留 ④：三个缺失 HAL 需镜像层解决」）

## 1. 问题

m22 云机把 Flyme 的 `system/` 与 lineage 的 `vendor/` 拼在一起，运行期实测出两类 HAL 缺口：

1. **被框架按名同步等待、镜像里完全没有实现**（`Waited one second for …`，20 分钟各 1472 次）：

   ```
   android.hardware.nfc.INfc/default
   android.hardware.biometrics.fingerprint.virtualhal.IVirtualHal/virtual
   android.hardware.biometrics.face.virtualhal.IVirtualHal/virtual
   ```

   三者都是**同步等待且不超时**，`com.android.nfc` 主线程因此永久阻塞（
   `ANR in com.android.nfc (Broadcast … BOOT_COMPLETED)`），同时是 #52 binder async 池被打满的
   三个「每秒制造一次失败调用」的来源之一。

2. **现成模块编出来了，但没落地**：`engine/vendor/hardware` 下 10 个已有源码的模块
   （authsecret / boot / confirmationui / drm / identity / memtrack / usb / weaver /
   biometrics 的 fingerprint、face 标准名）**没有 `vendor/` 静态树**：07 只按白名单把二进制复制到
   `vendor/bin/hw`，没有 `.rc` 与 VINTF 片段 ⇒ init 不拉起、framework 查不到，等于没接入。
   相机虽然实现最完整（`camera_provider_service.cpp`，gold 线在用），m22 也从未启用。

3. `device.json` 的 `hardware` 只有 **15** 条，上述模块即便修好也不会进镜像。

**要回答的问题是**：在 `engine/vendor` 里把缺口补齐，并让 m22 的 `device.json` 勾上需要的服务。

## 2. 根因

- **nfc 三无**：`engine/vendor` 里既无 `android.hardware.nfc` 模块，`profile.go` 的 `halTable`
  也没有 nfc 的 AIDL 下载来源 ⇒ `out/android{15,16,17}/aosp/aidl/` 下没有 `nfc/` 快照，
  想写源码也编不过。
- **virtualhal 三无**：`IVirtualHal` 是 `hardware/interfaces` 的 **unstable** AIDL（不进 FCM，
  lineage 底座不提供实现），`engine/vendor` 侧 grep `virtualhal` **0 命中**；
  且引擎的 codegen 固定 `--stability=vintf`，直接拿上游源会因缺 `@VintfStability` 而失败
  （见 `log-fail.txt` 第 4 节）。
- **标准名 ≠ virtualhal 名**：biometrics 模块原本只注册
  `biometrics.fingerprint.IFingerprint/default` 与 `biometrics.face.IFace/default`，
  与 Flyme 要的 `*.virtualhal.IVirtualHal/virtual`**不是同一个名字**，故两条都要有。
- **静态树缺失**：模块 spec 只声明了 `services`（编译产物），未提供 `vendor/etc/init` 与
  `vendor/etc/vintf/manifest`，07 的合并步骤无事可做。
- **`device.json` 未启用**：`hardware` 数组是 07 的输入集合，未列出即不编译、不复制。

## 3. 证据

- 运行期缺口数据（1472×3、`Can't find service: nfc`、`dumpsys media.camera → FAILED_TRANSACTION`、
  `/vendor/bin/hw` 仅 19 个二进制）：`log-fail.txt` 第 1~3 节，原始盘点见
  `../issue-m22-instance31-binder-stall/hal-inventory.txt`。
- 改造中的三次失败（AIDL 缺 `@VintfStability`、C++ override 签名、单进程跨域注册）：
  `log-fail.txt` 第 4~6 节，其中第一条用 cuttlefish **原始未改动**的源复现，可重复。
- 改造后的编译与产物：`log-pass.txt`（`hal list` 32 个模块、11 个模块逐个编译、产物 rc/xml 清单）。

## 4. 改动

### 4.1 依赖：nfc 的 AIDL 快照进引擎

| 文件 | 改动 |
| :--- | :--- |
| `engine/vendor/build/internal/profile/profile.go` | `halTable` 新增 `"nfc"`：`Downloads` 取 `nfc/aidl/aidl_api`，`Packages` 为 `android.hardware.nfc` v1（android14/16 表同时生效） |
| `engine/vendor/out/android{15,16,17}/aosp/aidl/nfc/` | 落地 1/2/current 版本化快照（含 `.hash`）+ 根 `.aidl-complete` marker；`deps --check nfc` 通过 |

### 4.2 新模块 `android.hardware.nfc`（空实现）

| 文件 | 说明 |
| :--- | :--- |
| `hardware/android.hardware.nfc/src/nfc_service.cpp` | `BnNfc` 子类：`open/coreInitialized/powerCycle/preDiscover` 返 `EX_UNSUPPORTED_OPERATION("no nfc controller")`，`close/factoryReset` 返 ok，`getConfig` 返默认 `NfcConfig`，`write` 返 0 + 同一异常；注册名 `android.hardware.nfc.INfc/default` |
| `build/modules/android.hardware.nfc.json` | `kind=aidl`、`depsHal=nfc`、`aidlPackages=["android.hardware.nfc"]` v1、一个 service、`runtimeLibs` 与 `extraLibs` 对齐 biometrics |
| `hardware/android.hardware.nfc/vendor/etc/init/android.hardware.nfc-service.rc` | `class hal` / `user nfc` / `group nfc` / `seclabel u:r:hal_nfc_default:s0` / `interface aidl android.hardware.nfc.INfc/default` |
| `hardware/android.hardware.nfc/vendor/etc/vintf/manifest/android.hardware.nfc-service.xml` | `<hal format="aidl"><name>android.hardware.nfc</name><version>1</version><fqname>INfc/default</fqname></hal>` |

**为什么是空实现**：云机没有 NFC 控制器；目标只是让框架的同步 `getService` 立刻拿到 binder
（不再每秒重试），而不是伪造一个能开卡的 NFC。

### 4.3 `android.hardware.biometrics` 增补 virtualhal（两个独立进程）

| 文件 | 说明 |
| :--- | :--- |
| `hardware/android.hardware.biometrics/aidl/android/hardware/biometrics/{fingerprint,face}/virtualhal/*.aidl` | 8 个文件：从 cuttlefish 源复制一份**本仓维护的副本**，声明前补 `@VintfStability`（上游原件不动） |
| `src/fingerprint_virtualhal_service.cpp` | 32 个方法全 no-op；`getFingerprintHal` 把出参 `reset()`；注册 `…fingerprint.virtualhal.IVirtualHal/virtual` |
| `src/face_virtualhal_service.cpp` | 23 个方法全 no-op；`getFaceHal` 把出参 `reset()`；注册 `…face.virtualhal.IVirtualHal/virtual` |
| `build/modules/android.hardware.biometrics.json` | 新增 `extraAidlRoots`（root `aidl`，包为 fp/face 的 `virtualhal`，`version=1`、无 hash）+ 两个 service |
| `hardware/.../vendor/etc/init/android.hardware.biometrics.virtualhal-service.rc` | **两个** service 块：`vendor.fingerprint-virtualhal`（`seclabel hal_fingerprint_default`）与 `vendor.face-virtualhal`（`hal_face_default`） |

**为什么拆两个二进制**：底座 `plat_service_contexts` 已把两个名字分别映射到
`hal_fingerprint_service` / `hal_face_service`，同进程只能有一个 `seclabel`，
第二个 `addService` 会被 servicemanager 拒（`log-fail.txt` 第 6 节）。
**为什么机型层 sepolicy 不用改**：`plat_service_contexts` 与
`plat_pub_versioned.cil` 已预留 `hal_nfc_default` / `hal_fingerprint_default` / `hal_face_default`
三个域，`m22/sepolicy/vendor_sepolicy.cil` 无需新增规则。

### 4.4 10 个现成模块补 `vendor/` 静态树（rc + VINTF）

| 模块 | 落地文件 |
| :--- | :--- |
| `android.hardware.authsecret` | `authsecret-service.rc` + `.xml` |
| `android.hardware.boot` | `boot-service.default.rc` + `.xml` |
| `android.hardware.confirmationui` | `confirmationui-service.rc` + `.xml` |
| `android.hardware.drm` | `drm-service.clearkey.rc` + `.xml`（clearkey 变体） |
| `android.hardware.identity` | `identity-service.rc` + `.xml` |
| `android.hardware.memtrack` | `memtrack-service.rc` + `.xml` |
| `android.hardware.usb` | `usb-service.rc` + `usb.gadget-service.rc` + 两份 `.xml` |
| `android.hardware.weaver` | `weaver-service.rc` + `.xml` |
| `android.hardware.biometrics` | 另有 `fingerprint-service.rc` / `face-service.rc` + 两份 `.xml`（标准名 IFingerprint / IFace） |

### 4.5 m22 `device.json` 启用

`devices/manifest/flyme/m22/device.json` 的 `hardware` 从 **15 → 26** 条（保持字母序），
新增：`authsecret`、`biometrics`、`boot`、`camera`、`confirmationui`、`drm`、`identity`、
`memtrack`、`nfc`、`usb`、`weaver`。其余段（`rom` / `input` / `display` / sepolicy 等）未动。

## 5. 验证

| 项 | 命令 | 结果 |
| :--- | :--- | :--- |
| 模块清单 | `cdh-vendor hal list --android 16` | 32 个模块；`android.hardware.nfc` 在列；biometrics 共 4 个服务（含两个 virtualhal） |
| 逐模块编译 | `cdh-vendor hal build <m> --android 16`（11 个） | 11/11 `ok`，每个都打印「合并 vendor 静态树」 |
| AIDL 依赖 | `cdh-vendor deps --check nfc` | `ok`（`.hash` 校验 + `.aidl-complete` marker 通过） |
| 产物 | `engine/out/vendor/android16/` | `bin/hw` 含 nfc / camera / fingerprint-virtualhal / face-virtualhal 等（32 个二进制）；`etc/init` 34 个 rc（其中 5 个 `cph_*.rc` 为引擎自有）；`etc/vintf/manifest` 28 个 xml |
| 流水线计划 | `cd 07_build_vendor && go run . --dry-run …` | 26 个 HAL 模块全部「复用」，biometrics 的 9 项（2 个标准服务 + 2 个 virtualhal + rc/xml）齐全 |
| 回归 | `cd engine/vendor/build && go test ./...`；`cd engine/build && go test ./07_build_vendor/... ./internal/device/...` | 全过；`out/bin/cdh-vendor` 已重建 |

完整输出见 `log-pass.txt`。

### 5.1 实机复测（2026-10-29，实例 34）

07→09→11 共跑三轮（`build-07-09-11.log` / `-round2.log` / `-round3.log`，均 EXIT=0），
实例链 32 → 33 → 34，前两轮各暴露一个卡点并已单独立条目：

| 轮次 | 暴露的问题 | 处置 | 条目 |
| :--- | :--- | :--- | :--- |
| 实例 32 | 加 boot HAL 后 vold 等 `IBootControl`、init 被 `vdc` 同步占住 → 互锁 | `device.json` 撤下 `android.hardware.boot`（26→25） | [#54](../issue-m22-bootctl-vold-deadlock/solution.md) |
| 实例 33 | 13 个新二进制被标 `vendor_file` → `entrypoint` 被拒 → `status 127` | 07 新增步骤 6b 按 rc `seclabel` 自动补 `_exec` 标签 | [#55](../issue-m22-hal-exec-label/solution.md) |
| 实例 34 | —（验收通过） | — | 本条目 |

实例 34 的验收结果：

| 项 | 结果 |
| :--- | :--- |
| `sys.boot_completed` | 1（71.7 s 处理 `sys.boot_completed=1` 动作） |
| 三项 `Waited one second`（nfc / fingerprint virtualhal / face virtualhal） | 0 / 0 / 0 |
| `ignored duplicate definition of service` | 0 |
| `denied { entrypoint }` / `exited with status 127` | 0 / 0 |
| `ls /vendor/bin/hw` | 32 个二进制，nfc / camera / 两个 virtualhal 均在 |
| `service list` | `nfc`、`android.hardware.nfc.INfc/default`、两个 `IVirtualHal/virtual`、`ICameraProvider/internal/0`、`memtrack`/`usb.gadget`/`weaver`/`identity`/`authsecret`/`confirmationui` 全部在册；框架侧 `face` / `fingerprint` / `media.camera` 也已起来 |
| 进程 | 10 个服务 PID 824-849 稳定，无重启 |
| 功能面 | NFC HAL 被真正调通：`INfcAidl::fromBinder returned ver(1)` + `HalOpenInternal: Open Error=OK`（但 `INfc` 仍是空实现，见遗留 2） |
| `dumpsys nfc` / `dumpsys media.camera` | 仍 `FAILED_TRANSACTION`；定因为 vendor 域对 pioneer 的 dump 管道 fd `{ use }` 被拒，**不是 HAL 判据**（本目录 README 备注已记原厂实例同样如此） |

## 6. 遗留

1. **实机复测已完成**（实例 34，见 5.1）；过程中新发现的两个卡点已各自立条目：#54 boot HAL
   与 `vdc checkpoint` 互锁、#55 新二进制缺 `_exec` 标签。遗留的关注点：
   - `android.hardware.boot` 模块代码保留但未声明；若要重启该 HAL，必须先解决懒启动时序（见 #54）。
   - 07 补的 13 条规则落在 `# cdh-patch-end` 标记块外，若以后策略改成整份重写需并入 patch 块（见 #55）。
2. **virtualhal 只是控制面空桩**：`getFingerprintHal` / `getFaceHal` 返回 null 指针，
   所以「每秒重试」消失、但**指纹/人脸本身不可用**。Flyme 若走虚拟回环流程
   （HIDL/AIDL 的 `IFingerprint`/`IFace` 桩），需要在同模块里再补一套帧序列实现。
3. **NN（`android.hardware.neuralnetworks`）未启用**：底座 `vendor_sepolicy.cil` /
   `plat_pub_versioned.cil` 里**没有** `hal_neuralnetworks_default` 域，启用需自造 type + 规则，
   收益（NNAPI 本来就 CPU 回退）不抵风险，故本次跳过。
4. **其余可选缺口未做**：`secure_element`、`input.processor`、`power.stats`、`health.storage`、
   `bluetooth.audio` / `bluetooth.{socket,finder,lmp_event,ranging}`、`wifi.supplicant/hostapd`、
   `soundtrigger3`、`oemlock`、`dumpstate`、`net.nlinterceptor`、`tetheroffload`、
   `cas`（已有 framework 侧）等，均不影响开机与界面，可按需补桩。
5. **相机变体**：m22 用 `variant: default`，与 gold 一致；但 camera 之前只在 gold 上跑过，
   m22 首次启用，若 `cameraserver` 报 provider 版本不符，需对照
   `build/modules/android.hardware.camera.json` 的 variant 表再调。
6. **`out/bin/cdh-vendor` 与产物已重建，但 `out/` 不进版本库**：换机后需先
   `cd engine/vendor/build && go build -o ../../out/bin/cdh-vendor .` 再跑 07。
