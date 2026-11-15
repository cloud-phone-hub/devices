# QEMU virtio-multitouch 支持触屏量程、槽位、压力开关、ABS_XY 与键位收敛（goodix_ts 能力对齐）

- 范围：`qemu/source/hw/input/virtio-input-hid.c`、`core/host/daemon/config/device.go` 及各机型 `device.json` `input.touch` 配置（跨机型引擎能力 → common）
- 实测机型：gold `15-OS3.0.11.0.VNQCNXM`，云机实例 `gold_15-OS3.0.11.0.VNQCNXM/2`（`4503ac70a1c898f3-cloud`）对比 USB 真机 `07cf5fc1784783d5`
- 日期：2026-11-14；前置条目：[`#204`](../issue-input-code-names-headset-jack/solution.md)；来源报告：[`report/xiaomi/gold-aligner-20261114/inputs.md`](../../../../report/xiaomi/gold-aligner-20261114/inputs.md)

## 一、问题

继 [`#204`](../issue-input-code-names-headset-jack/solution.md) 修复 `Headset Jack` 能力码表后，`aligner.inputs_full` 仍剩最后 7 条 capability 致命差异，全部出在触屏设备 `goodix_ts`（实际由 QEMU `virtio-multitouch-pci` 改名而来）：

- App 侧 `MotionRange` X/Y `max` / `range` 不等：真机为 `1079.9375` / `2399.9375`，云机为 `1079.967` / `2399.9268`。
- App 侧 `supportedKeyCodes` 与 `keyLabels`：真机为 `[26, 224]`（`KEY_POWER` + `KEY_WAKEUP`），云机为空 `[]`。
- 内核 `getevent -lp` 实测：`ABS_MT_POSITION_X/Y max = 32767/32767`（真机 `17280/38400`），`ABS_MT_SLOT max = 10`（真机 `9`），`ABS_MT_TRACKING_ID max = 10`（真机 `65535`），多出 `ABS_MT_PRESSURE`，缺单点轴 `ABS_X/ABS_Y`，键位暴露 QEMU 默认鼠标位 `BTN_MOUSE` 等而缺 `KEY_POWER / KEY_WAKEUP / KEY_GOTO`。

## 二、根因

1. `qemu/source/hw/input/virtio-input-hid.c` 的 `virtio_multitouch_config` 硬编码 `ABS_MT_POSITION_X/Y` 量程为 `0..32767`、`ABS_MT_SLOT` 与 `ABS_MT_TRACKING_ID` 上限为 `10`，并默认注册 `ABS_MT_PRESSURE` 与鼠标按键位 `EV_KEY`。Android `InputReader` 据此算出的 `MotionRange` 缩放上限为 `1080 - 1080/32768 = 1079.967`（真机为 `1080 - 1080/17280 = 1079.9375`），直接暴露虚拟化坐标范围。
2. `virtio-input-hid.c` 此前仅支持改身份（`input-name/bus/vendor/product/version`）与 `abs-to-mt`，没有可配置的量程裁剪与事件缩放通道；`core/host/daemon/config/device.go` 也没有透传这些设备属性到 QEMU 命令行。

## 三、证据

- `log-fail.txt`：修复前 `goodix_ts` 7 条 capability 差异与 `run-00086` 云机快照。
- `log-pass.txt`：修复后 QEMU 命令行参数、云机 `getevent -lp` 输出、`run-00110` 云机快照及 `compare_align.py` 对账输出（`identity=0 capability=0 state=0 availability=0`）。

## 四、改动

- **QEMU（`qemu/source/include/hw/virtio/virtio-input.h`、`qemu/source/hw/input/virtio-input-hid.c`）**：
  - 给 `VirtIOInputHID` 新增属性 `abs-x-max`、`abs-y-max`、`mt-slots`、`mt-pressure`、`abs-xy`、`touch-keys`（字符串以 `,` 或 `:` 分隔键码）。
  - 新增 `virtio_input_hid_apply_touch_shape` 在 `realize` 阶段重写 `VIRTIO_INPUT_CFG_ABS_INFO` 与 `VIRTIO_INPUT_CFG_EV_BITS`：按需更新 `ABS_MT_POSITION_X/Y` 最大值、`ABS_MT_SLOT` (`mt_slots - 1`)、`ABS_MT_TRACKING_ID` (`65535`)，按 `mt_pressure` 裁剪 `ABS_MT_PRESSURE`，按 `abs_xy` 补齐 `ABS_X/ABS_Y`，按 `touch_keys` 将 `EV_KEY` 收敛为 `BTN_TOUCH + BTN_TOOL_FINGER + 指定键码`。
  - 新增 `virtio_input_scale_abs` 将 VNC/QEMU 客户端的 `0..32767` 绝对坐标按比例线性缩放到 `0..max`，同步调整 `INPUT_EVENT_KIND_MTT` 与 `abs-to-mt` 模式下的 `SYN_REPORT` 帧下发（补发 `BTN_TOOL_FINGER` 与 `ABS_X/ABS_Y`，抑制未开启的 `ABS_MT_PRESSURE`）。
- **宿主 daemon（`core/host/daemon/config/device.go`、`device_input_test.go`）**：
  - `InputDevice` 增加 `AbsXMax`、`AbsYMax`、`MTSlots`、`MTPressure *bool`（指针避免零值误输出 `,mt-pressure=off`）、`AbsXY`、`TouchKeys`，在 `Props()` 中拼接到 `-device virtio-multitouch-pci`。
  - 新增同目录单测 `device_input_test.go` 覆盖零值守卫与 `ParseJSON` -> `Props()` 完整形态拼接。
- **机型配置（`devices/manifest/configs/redmi/gold/gold-android15.json`）**：
  - `buildModules.input.touch` 新增 `"abs-x-max": 17280, "abs-y-max": 38400, "mt-slots": 10, "mt-pressure": false, "abs-xy": true, "touch-keys": [116, 143, 354]`。
  - `engine/build` 全链路本身以 `json.RawMessage` 透传 `input` 段，无需改 Go 代码。

## 五、验证

- QEMU 重编并发布到 `out/qemu/dist/darwin/bin/qemu-system-aarch64`。
- `cd core/host/daemon && go test -v ./config ./run` 全绿；`just core host build` 重编 daemon 并重启 `daemon web`。
- 重跑 `cdh device build gold --manifest 15-OS3.0.11.0.VNQCNXM --only 11_pack_vda` 刷新 `device.json`。
- 启动 `gold_15-OS3.0.11.0.VNQCNXM` 实例 `2`：
  - `adb shell getevent -lp` 实测 `goodix_ts` 的 `KEY` 为 `KEY_POWER KEY_WAKEUP BTN_TOOL_FINGER BTN_TOUCH KEY_GOTO`，`ABS` 为 `ABS_X(0..17280)`、`ABS_Y(0..38400)`、`ABS_MT_SLOT(0..9)`、`ABS_MT_TOUCH_MAJOR(0..255)`、`ABS_MT_POSITION_X(0..17280)`、`ABS_MT_POSITION_Y(0..38400)`、`ABS_MT_TRACKING_ID(0..65535)`，无 `ABS_MT_PRESSURE`、无鼠标按键位。
  - `aligner.inputs_full` 复采 `run-00110`，经 `compare_align.py` 对比真机 `07cf5fc1784783d5`：
    `汇总: identity=0 capability=0 state=0 availability=0 → 已对齐`（`EXIT=0`，capability 差异从 `7` 降至 `0`）。

## 六、遗留

- 当前仅 `gold-android15.json` 配置了触屏形态字段；其余机型清单（`m22`、`ss9380`、`cph2797`、`oriole` 等）未显式填写时自动沿用 QEMU 内建默认行为，后续按各机型真机 `getevent -lp` 逐台补齐配置即可。
