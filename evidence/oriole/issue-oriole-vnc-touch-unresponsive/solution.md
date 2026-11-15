# #162 oriole（CP1A.260405.005-20260310）：清单缺 `modules.input` 致 `virtio-tablet` 被屏蔽且 `virtio-multitouch` 未开启 `abs-to-mt`，VNC 无法点击

## 1. 问题

`oriole_CP1A.260405.005-20260310`（Pixel 6，Android 16 / `lineage-23.2`）开机进入桌面后，在 Web / noVNC / VNC 中点击或滑动屏幕完全无响应（通过 `adb shell input tap` 则可正常点击）。

## 2. 根因

与 `#158`（OnePlus 15R）同源，四层原因叠加导致 VNC 指针事件被 Guest 100% 丢弃：
1. **`oriole` 未声明 `modules.input` 段**：`devices.db` 中 `oriole` 各版本的 `buildModules` 未配置 `input`，`11_pack_vda` 生成的 `dist/google/oriole/*/device.json` 不含 `input` 字段，`daemon` 的 `inputArgs()` 回退默认三件套（`-device virtio-multitouch-pci -device virtio-tablet-pci -device virtio-keyboard-pci`，无 `abs-to-mt=on`）。
2. **`QEMU Virtio Tablet` 被底盘排除表屏蔽**：`lineage-21.0` / `22.2` / `23.2` 的 `/vendor/etc/excluded-input-devices.xml` 均将 `QEMU Virtio Tablet` 列入排除名单（`ExcludedDeviceNames: [QEMU QEMU USB Tablet, QEMU Virtio Tablet]`），`EventHub` 只打开 `/dev/input/event1`（`QEMU Virtio MultiTouch`）。
3. **未开启 `abs-to-mt=on` 时 `virtio-multitouch-pci` 下发单指 `ABS_X`/`ABS_Y` 与 `BTN_LEFT`**：QEMU 的 `virtio_input_handle_event()` 在 `abs_to_mt == false` 时将 VNC 指针事件原样发为 `EV_ABS(ABS_X/ABS_Y)` 与 `EV_KEY(BTN_LEFT)`。
4. **Guest 内核按能力位图静默过滤**：`virtio-multitouch-pci` 仅声明 `ABS_MT_*`（`B: ABS=660800000000000`）与 `BTN_TOUCH`，不含 `ABS_X`/`ABS_Y`/`BTN_LEFT`，被 Guest 内核 `input_handle_abs_event()` 全部丢弃，`InputReader` 的 `Last Raw Touch` 恒停在初始态 `When: -9223372036854775808`。

### 真机触控输入身份溯源

从官方 Pixel 6 固件 `super/vendor_dlkm_a.img:/lib/modules/sec_touch.ko` 反汇编 `sec_ts_fw_init` 与 `sec_ts_set_input_prop`：
- `input_dev->name = "sec_touchscreen"`（`.rodata+0x198b`）；
- `input_dev->id.bustype = 0x1c`（`28`，`BUS_SPI`，`mov w8, #0x1c; strh w8, [x19, #0x18]`）；
- `input_dev->id.{vendor,product,version}` 保持 `0 / 0 / 0`。

## 3. 证据

- 失败日志见 [log-fail.txt](log-fail.txt)
- 通过日志见 [log-pass.txt](log-pass.txt)

## 4. 改动

1. **设备数据库（`engine/build/assets/devices.db`）**：更新 `oriole`（`deviceId=1`）全部 4 个版本（`AP2A.240905.003.F1-20240815`、`BP1A.250505.005-20250326`、`CP1A.260405.005-20260310`、`CP3A.261005.002.A1-20260905`）的 `versions.buildModules` 中 `$.input` 为：
   ```json
   {
     "keyboard": {},
     "tablet": false,
     "touch": {
       "abs-to-mt": true,
       "bus": 28,
       "name": "sec_touchscreen",
       "product": 0,
       "vendor": 0,
       "version": 0
     }
   }
   ```
2. **盘集重打包**：对 `CP1A.260405.005-20260310`（及 `BP1A`、`AP2A`）重跑 `--only 11_pack_vda`，更新 `out/dist/google/oriole/*/device.json`。

## 5. 验证

1. `out/dist/google/oriole/CP1A.260405.005-20260310/device.json` 已包含透传的 `input` 段（配方 `vda-242680371250`）。
2. 停止、删除并重新创建 `oriole_CP1A.260405.005-20260310` 实例 1（`pid=76100`，ADB `5911`，VNC `5912`）：
   - QEMU 命令行参数为 `-device virtio-multitouch-pci,input-name=sec_touchscreen,input-bus=28,input-vendor=0,input-product=0,input-version=0,abs-to-mt=on -device virtio-keyboard-pci`（无 `virtio-tablet-pci`）；
   - `dumpsys input` 中 `Device 4: sec_touchscreen` 被识别为 `TOUCHSCREEN` / `mode - DIRECT` / `DisplayBounds: 1080x2400`；
   - 通过 VNC（`127.0.0.1:5912`）发送 `(540, 1200)` 点击事件后，`sec_touchscreen` 的 `AbsState` 实时更新为 `ABS_MT_SLOT=0, ABS_MT_POSITION_X=16263, ABS_MT_POSITION_Y=16383`，`Last Raw Touch` 时间戳正常推进，VNC 触控点击完全恢复。

## 6. 遗留

- 无。

