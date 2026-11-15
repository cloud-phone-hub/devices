# input 能力码名称表写错 6 个值：Headset Jack 在云机上变成 HOME / HEADSETHOOK + SW_LID / SW_TABLET_MODE

- 范围：`engine/build/internal/manifest/manifest.go` 的 `inputCodeNames`，被 34 份机型清单的 `modules.input_identity` 共用（跨机型 → common）
- 实测机型：gold `15-OS3.0.11.0.VNQCNXM`，云机实例 `gold_15-OS3.0.11.0.VNQCNXM/2`（`4503ac70a1c898f3-cloud`）对比 USB 真机 `07cf5fc1784783d5`
- 日期：2026-11-14；来源报告：[`report/xiaomi/gold-aligner-20261114/inputs.md`](../../../../report/xiaomi/gold-aligner-20261114/inputs.md)

## 一、问题

aligner 19 域对比中，inputs 域有 17 条能力差异，其中 10 条出在 `mt6833-mt6359 Headset Jack` 和 `Virtual`：

- App 侧 `InputDevice.getKeyCharacterMap` 的键码：真机 `85 / 189 / 190 / 231`（MEDIA_PLAY_PAUSE / BUTTON_1 / BUTTON_2 / VOICE_ASSIST），云机 `3 / 79 / 189 / 190`（HOME / HEADSETHOOK）。
- 内核能力位（`getevent -lp`）：
  - 真机：KEY `KEY_PLAYPAUSE BTN_1 BTN_2 KEY_VOICECOMMAND`，SW `SW_HEADPHONE_INSERT SW_MICROPHONE_INSERT SW_LINEOUT_INSERT SW_JACK_PHYSICAL_INSERT`。
  - 云机：KEY `KEY_HOMEPAGE KEY_MEDIA BTN_1 BTN_2`，SW `SW_LID SW_TABLET_MODE SW_HEADPHONE_INSERT SW_MICROPHONE_INSERT`。

## 二、根因

清单里写的名称是对的，错在构建期的名称→数字转码表：

| 名称 | 表中旧值（实际含义） | 正确值（`linux/input-event-codes.h`） |
| :--- | :--- | :--- |
| `KEY_PLAYPAUSE` | 172（KEY_HOMEPAGE） | 164 |
| `KEY_VOICECOMMAND` | 226（KEY_MEDIA） | 0x246 = 582 |
| `SW_HEADPHONE_INSERT` | 0（SW_LID） | 0x02 |
| `SW_MICROPHONE_INSERT` | 1（SW_TABLET_MODE） | 0x04 |
| `SW_LINEOUT_INSERT` | 2（SW_HEADPHONE_INSERT） | 0x06 |
| `SW_JACK_PHYSICAL_INSERT` | 4（SW_MICROPHONE_INSERT） | 0x07 |

`input_identity_test.go` 的黄金用例照抄了渲染出来的错值（`key  172 257 258 226` / `sw  0 1 2 4`），所以单测一直是绿的，没能拦住。`Virtual`（id = −1）的 `hasKeys` 是全部设备按键能力的并集，跟着一起错。

## 三、证据

- `log-fail.txt`：修复前 inputs 域 17 条差异原文（`run-00087` / `run-00086`）、两机 `getevent -lp`、修复前渲染出的 `cdh_input_identity.devices`。
- `log-pass.txt`：修复后渲染产物、云机 `getevent -lp`、复采 `run-00109` 的对比结果。

## 四、改动

- `engine/build/internal/manifest/manifest.go`：上表 6 个值改正，附注释说明旧值的实际含义。
- `engine/build/internal/manifest/input_identity_test.go`：黄金文本改为 `key  164 257 258 582`、`sw  2 4 6 7`，并加强对键码与开关码的逐位断言；新增守卫用例 `TestInputCodeNamesMatchKernelHeader`，把 `inputCodeNames` 每一项与按内核头文件逐条抄写的期望表比对。表里新增名称时，必须同时在该用例登记期望值，否则用例失败。
- `devices/modules/input_identity/README.md`：内核侧文本格式样例同步改正。
- 内核模块 `.ko` 不变（只改了伴随数据文件），按 AGENTS.md §6 只需重跑 `08 → 09 → 11`。

## 五、验证

- `cd engine/build && go test ./...` 全绿，`gofmt` / `go vet` 干净；重编 `engine/out/bin/cdh`。
- `cdh device build gold --manifest 15-OS3.0.11.0.VNQCNXM --from 08_build_vendor_dlkm --skip 10_pack_super`：
  - 08 日志依次出现「回读复核: /lib/modules 已无 .ko」「依赖闭包成立」「伴随文件 cdh_input_identity.devices 350 B」；
  - 渲染产物 Headset Jack 段为 `key  164 257 258 582` / `sw  2 4 6 7`；
  - 09 重新发布 `patch.img`（sha256 前缀 `7095db0b660d`）；11 原位刷新 vda 的 6 个分区。
- 实例 2 重启后，云机 `getevent -lp` 的 Headset Jack 与真机**逐项一致**（KEY_PLAYPAUSE / BTN_1 / BTN_2 / KEY_VOICECOMMAND + 4 个 SW_JACK）。
- aligner inputs 复采（`run-00109`）：能力差异从 **17 降到 7**。Headset Jack 与 Virtual 的 10 条全部消失，剩下 7 条都是 goodix_ts 的问题（见遗留）。

## 六、遗留

- **goodix_ts（7 条）**：云机触摸屏实际是 QEMU virtio 多点触控改名而来，`ABS_MT_POSITION_X/Y max = 32767`（真机 17280 / 38400），`MT_SLOT max = 10`（真机 9），多出 `ABS_MT_PRESSURE`，缺 `ABS_X / ABS_Y`，缺 `KEY_POWER / KEY_WAKEUP / KEY_GOTO`。改坐标量程需要同时缩放注入坐标（改 QEMU 设备侧或在 guest 内核里重映射），不是改数据就能解决的，单独立项。
- 其余 33 份清单引用了同样这些名称，下次重建时自动生效；目前只验证了 gold 11.0。
