# m22 云机 VNC 点击落点错位：默认输入三件套里的 virtio-tablet 抢走绝对坐标——里程碑证据

- 设备: flyme/m22 12.5.0.0A（MEIZU 22，Flyme 12.5.0.0A / Android 16 / lineage-23.2 virtio 底盘，内核 6.12）
- 宿主: macOS (darwin, QEMU 10 + hvf)，daemon 控制台 `127.0.0.1:9050`
- 状态: **已闭环**（2026-10-29，实例 28 → 29，同槽位 vnc 5920 / adb 5919）；
  同日二次收尾：输入身份已按官方镜像取证对齐到真机 `fts`（实例 30，vnc 5928 / adb 5927，见第 7 节）

---

## 1. 问题

在 noVNC 里操作 m22 云机时，点击落点与目标位置不一致（用户描述为"往上偏移三四百"），
桌面图标、卡片很难点中，只有部分区域有反应；同一宿主上的
`gold_cn_OS3.0.10.0.VNQCNXM` 实例完全正常。

排查中先怀疑新加的静态身份模块 `devices/modules/input_identity`，
取 `dumpsys input` 后否定：该模块注册的 5 台设备（mtk-pmic-keys、Headset Jack、swtp、
uinput-fpc、fpc_irq@fingerprint）全是 `KEYBOARD` / `SWITCH`，无 TOUCH 类、无 X/Y motion
range、不发事件，不在坐标路径上。问题出在真正吃 VNC 事件的那台设备上。

## 2. 根因

四步叠加，缺一不会表现成"点不准"：

1. **`devices/manifest/flyme/m22/device.json` 没有 `input` 段**。daemon 的
   `inputArgs()` 在 `spec.Input == nil` 时回退历史默认三件套：
   `virtio-multitouch-pci` + **`virtio-tablet-pci`** + `virtio-keyboard-pci`。
2. **VNC 的绝对指针只能落到 tablet**。QEMU 的 `pointer_event()` 发
   `qemu_input_queue_abs(INPUT_AXIS_X/Y)`，而 `virtio-multitouch` 只暴露 `ABS_MT_*`，
   没有 `ABS_X/ABS_Y` 的 handler，于是事件全部路由到 `virtio-tablet`
   （guest 侧 `event2` 的 `AbsState: ABS_X=22286, ABS_Y=16724` 与 RFB 下发的
   framebuffer 坐标精确对应，说明**宿主→guest 的 raw 值本身是对的**）。
3. **tablet 拿不到 idc**。manifest 里有
   `vendor/usr/idc/{QEMU Virtio MultiTouch,QEMU Virtio Tablet,tablet2multitouch}.idc`，
   运行态也确实躺在 `/mnt/patch/vendor/usr/idc/`；但 6.12 内核的
   `cdh_redirect_vendor_path()` 前向表 11 条里**没有 `/vendor/usr`**，
   `/vendor/usr` 在 guest 里不存在 → `ConfigurationFile:` 为空。
4. **无 idc 的 tablet 被 Android 当成鼠标**：`Sources: KEYBOARD | MOUSE`、
   `Touch Input Mapper (mode - POINTER)`、`DeviceType: POINTER`，X 轴 MotionRange 按
   `2399.927`（= 屏高）建立，`RawToDisplay` 的 X scale 变成 `0.0732`（= 2400/32767），
   正确值应是 `0.0330`（= 1080/32767）。点击 x 被放大约 2.22 倍并裁到屏幕边缘，
   落点整体错位。

对比 gold：`devices/manifest/xiaomi/gold/device.json` 有 `input` 段
（`touch: goodix_ts, abs-to-mt: true`、`keyboard: mtk-kpd`、`tablet: false`），
`tablet: false` 时 daemon 根本不创建 `virtio-tablet-pci`，`abs-to-mt=on` 让 QEMU 把
绝对坐标直接改发单点 `ABS_MT_*`，由唯一的多点触控节点（TOUCHSCREEN、X max=1079.967）
接住，全程不依赖 idc。m22 是后接进来的机型（#42~#50），沿用流水线时**漏了这一段**，
于是退回默认三件套。

## 3. 证据

- `log-fail.txt`：修复前 `dumpsys input`（Tablet=POINTER、X max 2399.927；
  MultiTouch 空收）、`/vendor/usr` 三个路径 ENOENT 而 `/mnt/patch/vendor/usr/idc` 有文件、
  内核重定向 11 条白名单、默认三件套 QEMU 参数、input_identity 的 5 台设备归类。
- `log-pass.txt`：修复后 QEMU 参数（`virtio-multitouch-pci,abs-to-mt=on` + 键盘，无 tablet）、
  `dumpsys input`（Tablet 0 次命中，MultiTouch 仍是 TOUCHSCREEN / X max=1079.967）、
  两次 RFB 点击的落点。
- 截图：`tap-0-home.png`（点击前桌面）→ `tap-1-settings.png`（点 (917,1461) 精确打开
  设置的隐私政策弹窗）→ `tap-2-consent.png`（点 (755,2158) 精确命中"同意"，进入设置主页）。
- 代码位置：`core/host/daemon/run/qemu.go` `inputArgs()`；
  `core/host/daemon/config/device.go` `Input/AbsToMT`；
  `devices/kernels/lineage-23-6.12/fs/namei.c:208` `rules[]`；
  `qemu/source/hw/input/virtio-input-hid.c` 的 `abs-to-mt` 属性（#37 引入）。
- 相关先例：#37 `issue-input-source-align` 在 gold 上做过同一套改造（`tablet:false` +
  `abs-to-mt` + `BTN_TOUCH`），本条是 m22 补齐同一模型。

## 4. 改动

| 文件 | 改动 |
| :--- | :--- |
| `devices/manifest/flyme/m22/device.json` | 新增 `input` 段：`touch: {abs-to-mt: true}`、`keyboard: {}`（沿用 QEMU 内建身份，保留键盘）、`tablet: false`（不再创建 `virtio-tablet-pci`） |
| `engine/out/dist/flyme/m22/12.5.0.0A/device.json` | 同步该段（键序与 `11_pack_vda/device_json.go` 的 `smp → input → host-hvc-*` 一致） |
| 同上两份 `device.json`（二次收尾） | `touch` 补真机身份：`name: "fts"`、`bus: 28`、`vendor: 1`、`product: 2`、`version: 256`（来源见第 7 节）；`keyboard` 仍为 `{}` |

`engine/build/11_pack_vda/device_json.go` 本就把清单 `input` 段原样透传到盘集
`device.json`，且 `recipeLabel()` 里含 `device:<sha256(DeviceDoc)>`，
所以清单改动后下一次 `cdh pack-vda` 会因配方变化自动重做盘集并写出同样内容；
本次为抢验证时效先手工对齐了 dist 副本，随后（2026-10-29 10:55）已由 `pack-vda`
正式重发：dist 的 `device.json` 554 B 为生成器输出（`input` 段压缩成单行），
`vda-recipe.txt` = `vda-5fe78d51e413`，与新清单一致。

## 5. 验证

1. 停实例 28 → `/api/create {"index":0}` 拉起实例 29（槽位复用，vnc 5920 / adb 5919），
   `sys.boot_completed=1`。
2. QEMU 命令行只剩 `virtio-multitouch-pci,abs-to-mt=on` 与 `virtio-keyboard-pci`。
3. `dumpsys input`：`QEMU Virtio Tablet` 命中 0 次；`QEMU Virtio MultiTouch` 仍为
   `KEYBOARD | TOUCHSCREEN`、`X max=1079.967`、`Y max=2399.927`、`mode - DIRECT`、
   `DeviceType: TOUCH_SCREEN`；`QEMU Virtio Keyboard` 仍在。
4. RFB 点击（`vnc_tap.py`，含 `SetPixelFormat`/`SetEncodings`，复刻 noVNC 行为）：
   `(917,1461)` 精确命中桌面"设置"图标并打开设置；`(755,2158)` 精确命中弹窗"同意"
   并进入设置主页。两次都一次命中，无偏移。
5. 键盘路径未受影响（键盘设备仍在、sources=KEYBOARD）。
6. **身份对齐复查（实例 30）**：QEMU 参数为 `virtio-multitouch-pci,input-name=fts,input-bus=28,
   input-vendor=1,input-product=2,input-version=256,abs-to-mt=on` + `virtio-keyboard-pci`；
   `dumpsys input` 里 EventHub `7: fts` 的 `Identifier: bus=0x001c, vendor=0x0001,
   product=0x0002, version=0x0100`、`KeyLayoutFile: /system/usr/keylayout/fts.kl`（与真机同款键位），
   `Device 2: fts` 仍是 `KEYBOARD | TOUCHSCREEN`、`mode - DIRECT`、`X max=1079.967`、
   `RawToDisplay 0.0330 / 0.0732`；RFB 点 `(894,1388)` 打开设置隐私弹窗、`(757,2178)` 命中"同意"
   （`tap-3-settings-fts.png` / `tap-4-consent-fts.png`），落点与改名前的 (917,1461)/(755,2158) 一致无偏移。

## 6. 遗留

1. **`/vendor/usr` 仍不在内核重定向白名单**（11 条）。m22 改用 `abs-to-mt` 后不再需要
   idc，但 `devices/manifest/flyme/m22/vendor/usr/idc/` 那三个文件（含
   `tablet2multitouch.idc`）现在在运行态永远读不到，属死配置。后续要么在
   `cdh_redirect_vendor_path()` 补 `/vendor/usr`（需重编 6.6/6.12 内核），要么删掉这层
   usr 静态树并让 07 的 usr 窄层不再为 m22 写 idc。
2. **盘集已重新发布**（原遗留，2026-10-29 10:55 已处理）：`cdh pack-vda` 因
   `device:<sha256(DeviceDoc)>` 入配方而重做（`vda-d3da89ad6175` → `vda-5fe78d51e413`），
   dist 的 `device.json` 现在是生成器输出（554 B，`input` 段透传），不再是手工副本。
3. **输入身份已与真机对齐**（原遗留，2026-10-29 已处理）：m22 真机触摸是 ST FTS
   （dtbo 的魅族 overlay `qcom,touch-active = "st,fts"`，`st_fts.ko` 注册 `name=fts` /
   `bus=28` / `vendor=1` / `product=2` / `version=256`），已写进 `touch` 段，云机侧
   `dumpsys input` 与真机同形（唯一差异：`Location` 仍是 `virtio8/input0`、`ConfigurationFile` 仍为空，
   因无需 idc）。键盘身份仍保留 QEMU 内建名，原因见 `log-identity.txt` 第 7 节。
4. `out/core/daemon/runtime/m22_12.5.0.0A/28` 的旧实例目录仍在，确认无用后可手工清理。
5. 旧现象里"往上偏移三四百"的**垂直分量**在本次取证中未单独量化：修复前 tablet 的 Y 轴
   MotionRange（2399.927 / scale 0.0732）与正确值一致，能测到的确定性偏差是 X 轴 2.22 倍
   放大；用户看到的斜向偏移是 X 放大 + 边缘裁剪叠加后的视觉结果。修复后 X/Y 同时正确，
   不再区分。

---

## 7. 附：输入身份从官方镜像取证（不依赖真机）

完整命令与输出见 `log-identity.txt`，结论链四步：

1. `super/vendor_a.img:/build.prop` → `ro.board.platform=sun`、`ro.product.vendor.device=Meizu22`、
   `model=MEIZU 22`；`vendor.meizu.fingerprint.vendor=Goodix`（屏下指纹，不是触摸）。
2. `firmware/dtbo.img`（81 段 dtb，大端 dt_table_header）里唯一带魅族机型的 overlay 是
   `Meizu M2582 DVT1/EVT0/EVB、M2582 PRO DVT1/EVT0`，其 `fragment@0 -> qupv3_se4_spi`
   挂 `st_fts@0 { compatible = "st,fts" }`，并 `qcom,touch-active = "st,fts"`；同 overlay 里
   `synaptics-tcm-spi@0` 在 i2c 上 `status = "disabled"`。即 goodix/focaltech/synaptics 只是
   QTI 参考设计的并存候选。
3. `super/system_a.img:/system/usr/keylayout/fts.kl` 是 Flyme 手势键映射，与 name=`fts` 对应；
   `main_touch.kl` / `ndt.kl` 属其它方案，m22 不用。
4. 反汇编 `super/vendor_dlkm_a.img:/lib/modules/st_fts.ko` 的 `st_fts_probe_entry`：
   `name = "fts"`（`.rodata.str1.1+0x38bd`）、`phys = "%s/input0"`、
   `id.bustype = ts->bus_type`（spi_probe 写 0x1c=28、i2c_probe 写 0x18=24）、
   `id.vendor = 1`、`id.product = 2`、`id.version = 0x0100`。m22 的节点在 SPI 上，故 bus=28。

键盘：真机是 `gpio-keys`（音量，dtb `label = "gpio-keys"`）+ PMIC 电源键；云机未改名，
因为 `gpio-keys.kl` 只定义音量键，改名会让 virtio 键盘的其它扫描码映射不到 keycode。

