# #226：官方 `vendor_service_contexts` 冲突消解与精准同步（`common`）

## 问题

1. 在批量同步真机/官方 `vendor_service_contexts` 服务标签以注册厂商 Binder 占位服务时，`servicemanager` 在开机早期触发 `SELinux: Multiple different specifications for mapper/mediatek` 与 `Multiple same specifications for android.hardware.soundtrigger3.ISoundTriggerHw/default`，导致 `initialize_backend: Error getting service handle (Invalid argument)`、`Check failed: gSehandle != nullptr` 连续 `SIGABRT` 重启，系统卡死无法开机。
2. `07_build_vendor` 合并官方 `vendor_service_contexts` 后，不仅缺少重复键与平台类型映射（如 `default_android_service`）的自动过滤/同步能力，而且 `neuralnetworks` 与 `soundtrigger3` 模块的 `vendor_service_contexts` 使用接口前缀（无 `/default`）写法，与官方精确实例键冲突，并导致 `gold` 云机中 `android.hardware.neuralnetworks.IDevice/mtk-neuron_shim` 在 App 视角丢失接口描述符（真机 `[android.hardware.neuralnetworks.IDevice]` 对云机 `[]`）。

## 根因

1. **`mapper/*` 别名冲突**：`props.go` 的 `alignVendorGrallocConfig` 将真机 `vendor_service_contexts` 的 `mapper/mediatek`（`hal_graphics_allocator_service` 或官方标签）重写为 `mapper/minigbm`（`hal_graphics_mapper_service`）以匹配云机 virtio/minigbm gralloc；而 `sepolicy_cil.go` 的 `syncOfficialVendorServices` / `mergeOfficialVendorServiceContexts` 若再次从官方 `vendor_service_contexts` 全量同步，会把官方原始 `mapper/mediatek` 重新并入并赋予不同安全上下文，触犯 libselinux 对同键异值的硬报错（`Multiple different specifications`）。
2. **`soundtrigger3` 重复定义**：`android.hardware.soundtrigger3/sepolicy/vendor_service_contexts` 与官方/合并条目对 `android.hardware.soundtrigger3.ISoundTriggerHw/default` 产生同键重复条目，触发 `Multiple same specifications`。
3. **NN 接口描述符丢失与非精确上下文**：`android.hardware.neuralnetworks/sepolicy/vendor_service_contexts` 原先使用接口前缀 `android.hardware.neuralnetworks.IDevice`；此外官方 `vendor_service_contexts` 中包含大量云机策略尚未声明的 vendor 类型或引用未引入的平台服务类型，若不按已声明 type + 允许的平台类型集合过滤并精准同步，要么 CIL 编不过，要么服务落入 `default_android_service` 且客户端描述符可见性异常。

## 证据

- 开机崩溃现场见 [`log-fail.txt`](log-fail.txt)：
  - `t=3.815s`：`SELinux: Multiple same specifications for android.hardware.soundtrigger3.ISoundTriggerHw/default.` → `servicemanager: Check failed: gSehandle != nullptr`。
  - `t=21.874s`~`67.313s`：`SELinux: Multiple different specifications for mapper/mediatek  (u:object_r:hal_graphics_allocator_service:s0 and u:object_r:hal_graphics_mapper_service:s0).`，`servicemanager` 反复 SIGABRT。
- 修复前对比快照（`batch4-pre-diffs`）：`android.hardware.neuralnetworks.IDevice/mtk-neuron_shim` 在真机为 `['android.hardware.neuralnetworks.IDevice']`，云机为 `'[]'`；`services` 域共 `59` 条 capability 差异（见 [`#225`](../issue-hal-aaudio-mmap-nn-instance-and-radio-placeholders/solution.md) 遗留）。
- 修复后实测见 [`log-pass.txt`](log-pass.txt)：`engine/build` 全 `22` 包单测通过，`qemu.log` 中 `Multiple different specifications count=0`、`Service 'servicemanager' count=0`，且 `services` 域复采结果中 `IDevice/mtk-neuron_shim` 描述符差异彻底清零。

## 改动

1. **`engine/build/stages/07_build_vendor/sepolicy_cil.go`**：
   - 新增 `syncOfficialVendorServices`（L299–472）与 `mergeOfficialVendorServiceContexts`（L474–531），由 `alignSepolicyVersion`（L725–741）流程接入。
   - 通过 `collectCILTypesAndAttrs` 收集策略已声明类型，仅同步当前策略已声明且类型/平台映射合法的官方 `vendor_service_contexts` 条目，并确保相同前缀键不重复追加。
   - 在 `mergeOfficialVendorServiceContexts` 中，当目标上下文集合 `seenKeys["mapper/minigbm"]` 已存在时，主动跳过官方原始 `mapper/*` 条目，防止 `mapper/mediatek` 与 gralloc 映射产生双重冲突。
2. **`engine/build/stages/07_build_vendor/props.go`**：
   - 在 `alignVendorGrallocConfig`（L471–490）重写 `mapper/minigbm` 之前调用 `dropDuplicateServiceContexts` 做纵深兜底，清除冗余的 `mapper/<suffix>` 旧条目。
3. **`engine/build/stages/07_build_vendor/{sepolicy_test.go,props_test.go}`**：
   - 新增 `TestSyncOfficialVendorServices`、`TestSyncOfficialVendorServicesPlatformTypeMappings` 与 `TestAlignVendorGrallocAndGpuZeroBinaryMutation` 等单元测试覆盖官方上下文同步、平台映射及 `mapper/*` 去重兜底。
4. **模块上下文精确化**：
   - `engine/vendor/hardware/android.hardware.neuralnetworks/sepolicy/vendor_service_contexts`（L1–4）：收紧为精确实例名 `android.hardware.neuralnetworks.IDevice/default        u:object_r:hal_neuralnetworks_service:s0`（`mtk-neuron_shim` 等机型扩展实例改由 `07_build_vendor` 从官方 `vendor_service_contexts` 同步）。
   - `engine/vendor/hardware/android.hardware.soundtrigger3/sepolicy/vendor_service_contexts`（L9）：精确映射到 `android.hardware.soundtrigger3.ISoundTriggerHw/default      u:object_r:hal_audio_service:s0`。

## 验证

1. **Go 单测**：在 `/Volumes/work/cloud-phone-hub/engine/build` 执行 `TMPDIR=/Volumes/work/cloud-phone-hub/engine/out/tmp go test -count=1 ./...`，全部 `22` 个测试包（含 `07_build_vendor` 的 `TestSyncOfficialVendorServices` 与 `TestSyncOfficialVendorServicesPlatformTypeMappings`）均为 `ok`。
2. **真机对照与干净云机开机验收**（机型 `gold`，镜像 `15-OS3.0.11.0.VNQCNXM`，`patch.img` sha256 `1b8fdab3ff6437e4`）：
   - 在全新冷启动实例 `gold_15-OS3.0.11.0.VNQCNXM/2`（`9f0cf68f69184fc9-cloud`，`127.0.0.1:5911`）验证：`getenforce=Enforcing`、`sys.boot_completed=1`、`qemu.log` 中 `Multiple different specifications count=0`、`Service 'servicemanager' count=0`。
   - 19 域重采差异报告（`gold-aligner-20261114`）显示 `services` 域中 `android.hardware.neuralnetworks.IDevice/mtk-neuron_shim` 描述符已对齐为 `[android.hardware.neuralnetworks.IDevice]`，不再出现在差异表中；配合 [`#227`](../../gold/issue-gold-vendor-binder-placeholders-and-init-svc/solution.md) 一并将 `services` 域 `fatal/capability` 由 `59` 降至 `14`（`-45`）。

## 遗留

- 无引擎级阻塞遗留；未声明在策略 CIL 中的官方第三方服务若后续需要引入，只需在对应机型或模块的 `vendor_sepolicy.cil` 声明对应 `_service` 类型与注册域 `allow` 规则，构建期会自动同步其 `vendor_service_contexts` 条目。
