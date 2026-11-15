# 15r 云机启动后 VNC 无法操作界面：清单缺 `modules.input` 致 `virtio-tablet` 被屏蔽且 `virtio-multitouch` 未开启 `abs-to-mt`

- 设备: oneplus/15r `CPH2769_16.0.10.500_EX01_A51_full`（OnePlus 15R，ColorOS 16.0.10.500_EX01 / Android 16 / lineage-23.2 底盘，内核 6.12.81）
- 宿主: macOS (darwin, QEMU 10 + hvf)，daemon 控制台 `127.0.0.1:9050`
- 状态: **已闭环**（2026-11-04，重建 `pack_vda` 配方 `vda-8fb1ac195553` → `vda-21205fef0615` 并重起实例 1）

---

## 1. 问题

OnePlus 15R（`15r_16.0.10.500_EX01`）完成引导进入桌面（`sys.boot_completed=1`）后，在 noVNC / VNC 客户端中点击或滑动屏幕没有任何反应：

1. 屏幕底部弹出 `com.oplus.battery` 的系统弹窗 `Set charging limit to 80%`（`Window{c9b195a u0 com.oplus.battery}`，`ty=SYSTEM_ALERT`），将桌面背景压暗；无论点击弹窗的 `Cancel` / `Turn on` 还是底部导航栏，界面均无响应。
2. 通过 `adb shell input tap` / `input keyevent` 可以正常操作，说明 Android InputDispatcher 与窗口焦点正常，断裂点在 **QEMU VNC 指针事件 → Guest `/dev/input/event*` → Android EventHub/InputReader** 这一段。

---

## 2. 根因

四层原因叠加，导致 VNC 下发的指针事件在 Guest 内被 100% 丢弃：

1. **`devices/manifest/oneplus/15r/device.json` 未声明 `modules.input` 段**。
   `core/host/daemon/run/qemu.go` 的 `inputArgs()` 在 `spec.Input == nil` 时回退历史默认三件套：
   `-device virtio-multitouch-pci -device virtio-tablet-pci -device virtio-keyboard-pci`（未附加 `abs-to-mt=on`）。
2. **`QEMU Virtio Tablet`（`/dev/input/event2`）被 lineage-23.2 底盘的排除表屏蔽**。
   Guest 的 `/vendor/etc/excluded-input-devices.xml` 明确声明：
   ```xml
   <devices>
       <device name="QEMU QEMU USB Tablet"/>
       <device name="QEMU Virtio Tablet"/>
   </devices>
   ```
   因此 `dumpsys input` 的 `Event Hub State` 中 `ExcludedDeviceNames: [QEMU QEMU USB Tablet, QEMU Virtio Tablet]`，`EventHub` 根本不打开 `/dev/input/event2`，只打开 `/dev/input/event1`（`QEMU Virtio MultiTouch`）。（与此同时，6.12 内核没有旧分支的 `virt-tablet2multitouch` 内核桥接驱动。）
3. **未开启 `abs-to-mt=on` 时，`virtio-multitouch-pci` 发送的是单指绝对坐标 `ABS_X`/`ABS_Y` 与 `BTN_LEFT`**。
   在 `qemu/source/hw/input/virtio-input-hid.c` 中，`virtio_multitouch_handler` 的事件掩码包含 `INPUT_EVENT_MASK_ABS | INPUT_EVENT_MASK_BTN`，因此 VNC 收到的鼠标移动（`INPUT_EVENT_KIND_ABS`）与左键点击（`INPUT_BUTTON_LEFT`）会进入 `virtio_input_handle_event()`。但当 `vinput->abs_to_mt == false`（默认关闭）时，该函数不会把事件转换为多点触控序列（`ABS_MT_SLOT` / `ABS_MT_TRACKING_ID` / `ABS_MT_POSITION_X` / `ABS_MT_POSITION_Y` + `BTN_TOUCH`），而是走默认分支直接发送 `EV_ABS`（`ABS_X=0x00`、`ABS_Y=0x01`）与 `EV_KEY`（`BTN_LEFT=0x110`）。
4. **Guest 内核 `input_handle_abs_event` / `input_handle_key` 因能力位图不匹配将事件静默丢弃**。
   `virtio-multitouch-pci` 在 `/proc/bus/input/devices` 中只声明了多点触控轴 `B: ABS=660800000000000`（仅 `ABS_MT_SLOT`、`ABS_MT_POSITION_X/Y`、`ABS_MT_TRACKING_ID`、`ABS_MT_PRESSURE`，**不含 bit 0 `ABS_X` 和 bit 1 `ABS_Y`**）以及 `BTN_TOUCH`（不含 `BTN_LEFT`）。Linux 内核 `drivers/input/input.c` 校验 `is_event_supported(code, dev->absbit, ABS_MAX)` 不通过，将所有 `ABS_X`、`ABS_Y` 事件全部静默过滤。结果 `InputReader` 中 `Device 2: QEMU Virtio MultiTouch` 的 `Last Raw Touch` 恒停在初始态 `When: -9223372036854775808, Read time: 0, 0 pointers`。

### 真机触控输入身份溯源（不依赖真机）

为同时将触控设备身份对齐到 OnePlus 15R 真机，从官方解包固件取证：
1. `/odm/firmware/tp/macan/vnd_touch_project_config.xml` 声明主屏（`device id="0"`）触控 IC 为 `synaptics-s3910`（`<device id="0" ic_name="synaptics-s3910">`）。
2. 反汇编官方 `super/vendor_dlkm_a.img:/lib/modules/oplus_hbp_core.ko` 的 `hbp_device_create` 与 `/lib/modules/oplus_bsp_tp_common.ko` 的 `register_common_touch_device`：
   - 当 `bus_index == 0`（主屏）时，`snprintf(name, "touchpanel")`（副屏才带 `"touchpanel%d"` 后缀）；
   - `input_allocate_device()` 后只设置 `input_dev->name = "touchpanel"`，未给 `input_dev->id.{bustype,vendor,product,version}` 赋非零值（保持 `0 / 0 / 0 / 0`）。
3. 核对官方 `system_a` / `vendor_a` / `odm_a` 的 `usr/idc/` 与 `usr/keylayout/`：无 `touchpanel.idc` 或 `touchpanel.kl`，回退 `/system/usr/keylayout/Generic.kl`，与 `Touch Input Mapper (mode - DIRECT)` 完全兼容。

---

## 3. 证据

- [log-fail.txt](log-fail.txt)：
  - 修复前 QEMU 命令行（`-device virtio-multitouch-pci -device virtio-tablet-pci -device virtio-keyboard-pci`，无 `abs-to-mt=on`）；
  - `/proc/bus/input/devices` 与 `getevent -lp`（`event1` 为 `QEMU Virtio MultiTouch`，`event2` 为 `QEMU Virtio Tablet`）；
  - `dumpsys input`：`ExcludedDeviceNames: [QEMU QEMU USB Tablet, QEMU Virtio Tablet]`，`QEMU Virtio MultiTouch` 的 `Last Raw Touch` 为 `0 pointers`（`When: -9223372036854775808`）；
  - `dumpsys window windows`：`mCurrentFocus=Window{c9b195a u0 com.oplus.battery}` 模态框驻留前台。
- [tap-0-before.png](tap-0-before.png)：修复前 VNC 画面，停留在 `Set charging limit to 80%` 弹窗，无法点击。
- [log-pass.txt](log-pass.txt)：
  - 修复后 QEMU 命令行（`-device virtio-multitouch-pci,input-name=touchpanel,input-bus=0,input-vendor=0,input-product=0,input-version=0,abs-to-mt=on -device virtio-keyboard-pci`）；
  - 修复后 `dist/oneplus/15r/16.0.10.500_EX01/device.json` 与 `vda-recipe.txt`（`vda-21205fef0615`）；
  - 修复后 `dumpsys input`：EventHub `3: touchpanel`（`bus=0x0000, vendor=0x0000, product=0x0000, version=0x0000`），`AbsState: ABS_MT_SLOT=0, ABS_MT_POSITION_X=20069, ABS_MT_POSITION_Y=18478, ABS_MT_TRACKING_ID=1, ABS_MT_PRESSURE=50`；InputReader `Device 2: touchpanel`（`TOUCHSCREEN`、`mode - DIRECT`、`X max=1271.961`、`Y max=2799.915`、`RawToDisplay: 0.0388 / 0.0854`），`Last Raw Touch` / `Last Cooked Touch` 实时更新（`x=779.046, y=1578.931, toolType=FINGER`）。
- [boot-pass.png](boot-pass.png) / [tap-1-current.png](tap-1-current.png)：修复后开机截图与通过 noVNC 操作点掉电池弹窗、进入 `Settings -> About device -> Version`（显示 `CPH2769_16.0.10.500(EX01)` / `Kernel version 6.12.81-4k-cdh`）的实测画面。
- 相关先例：[../../m22/issue-m22-vnc-touch-offset/solution.md](../../m22/issue-m22-vnc-touch-offset/solution.md)（#65）、[../../gold/issue-input-source-align/solution.md](../../gold/issue-input-source-align/solution.md)（#37）。

---

## 4. 改动

| 文件 | 改动 |
| :--- | :--- |
| `devices/manifest/oneplus/15r/device.json` | 在 `modules` 下新增 `"input"` 段：`touch: {name: "touchpanel", bus: 0, vendor: 0, product: 0, version: 0, abs-to-mt: true}`、`keyboard: {}`、`tablet: false` |
| `engine/out/dist/oneplus/15r/16.0.10.500_EX01/device.json` | 跑 `cdh device build 15r --only pack_vda` 重新生成盘集与 `device.json`（配方 `vda-8fb1ac195553 -> vda-21205fef0615`，同时生效 `grub.cfg` 零超时直启） |

---

## 5. 验证

1. **构建校验**：执行 `cd engine/build && ../out/bin/cdh device build 15r --only pack_vda`，`vda-recipe.txt` 更新为 `vda-21205fef0615`，`engine/out/dist/oneplus/15r/16.0.10.500_EX01/device.json` 已包含透传的 `input` 段。
2. **引导与 QEMU 参数校验**：删除旧实例并通过 `/api/create {"index":2}` 创建新实例（PID 32684，adb 5911 / vnc 5912 / ws 5913），`boot_check.py` 52 秒判定 `[pass] 引导完成`。QEMU 进程参数为：
   `-device virtio-multitouch-pci,input-name=touchpanel,input-bus=0,input-vendor=0,input-product=0,input-version=0,abs-to-mt=on -device virtio-keyboard-pci`（不再创建 `virtio-tablet-pci`）。
3. **Input 子系统与 noVNC 交互校验**：
   - `dumpsys input` 中 `touchpanel` 被识别为内部主屏 `TOUCH_SCREEN`（`mode - DIRECT`，`DisplayBounds: 1272x2800`，缩放矩阵 `0.0388 × 0.0854` = `1272/32767 × 2800/32767`）；
   - 在 noVNC 界面中点击消除 `Set charging limit to 80%` 弹窗，滑动桌面并点击打开 `Settings -> About device -> Version`（`Window{11aaab u0 com.android.settings/com.android.settings.SubSettings}`），点击与滑动均实时响应且落点准确。

---

## 6. 遗留

- 其余尚未声明 `modules.input` 的机型（如 `cph2797`、`popsicle`、`oriole` 等，见 `devices/manifest/README.md`「已知缺口」），若其底盘同样包含 `excluded-input-devices.xml` 屏蔽 `QEMU Virtio Tablet`，也会在 VNC 下出现无法触控的问题，需按同一流程从各自官方 `vendor_dlkm` 触控驱动取证并补齐 `modules.input`（`abs-to-mt: true`、`tablet: false`）。

