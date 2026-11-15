# input 源与真机对齐（gold / Redmi Note 13 5G，Android 15）

## 问题

云机 App 侧 `InputManager` 看到的输入设备集合与 gold 真机不一致，device-checker
的 `hardware.inputs` 逐项 diff 不收敛：

| 侧 | 数量 | 设备 |
| :--- | :--- | :--- |
| 真机 | 7 | Virtual、uinput-fpc、mtk-kpd、mtk-pmic-keys、fpc_irq@fingerprint、mt6833-mt6359 Headset Jack、goodix_ts |
| 云机（对齐前） | 6 | Virtual、QEMU Virtio MultiTouch、QEMU Virtio Keyboard、Power Button、goodix_fingerprint、virt-tablet2multitouch |

name、descriptor、sources 三项全不一致；tablet 设备额外带出 `SOURCE_STYLUS`
（sources=0x5103），`Power Button` 则是宿主机 ACPI 电源键被透传进 guest。

## 根因

1. **设备身份来自 QEMU 硬编码**：virtio-input-hid 只报
   `QEMU Virtio MultiTouch` / `QEMU Virtio Keyboard` 与 virtio 总线号，
   descriptor 由 (vendor, product, uniqueId, name) 哈希而来，必然与真机不同。
2. **真机的多路输入源在云机没有对应物**：uinput-fpc（FPC HAL 建的虚拟按键设备）、
   mtk-kpd、mtk-pmic-keys、Headset Jack、fpc_irq@fingerprint + swtp（二者零身份、
   空 Location，被 InputReader 合并成一台）都来自 MTK/FPC 驱动栈。
3. **uinput-fpc 的 GAMEPAD 归类依赖 vendored 配置**：真机
   `/vendor/usr/idc/uinput-fpc.idc` 把 layout 指到 `uinput-fpc.kl`，后者只保留
   `LEFT→DPAD_CENTER`、`RIGHT→BUTTON_C`（103/108 被注释），sources 因此是
   `KEYBOARD|GAMEPAD`（0x501）；云机镜像缺这两个文件，回退 `Generic.kl` 后
   方向键变成 `DPAD`（0x301）。
4. **排除表被平台层覆盖**：`excluded-input-devices.xml` 机型静态树里有，但平台
   覆盖层（lineage unpack_vendor）同名文件后叠，把 `Power Button` 又放了出来。
5. **usr 窄层落错位置**：07 的「机型 usr」层只设置了 `src=静态树/usr`，没设置
   `base="usr"`，登记键变成 `idc/…`、`keylayout/…`，随后的白名单 prune 把顶层
   `idc/`、`keylayout/` 当垃圾删掉；日志却仍报「机型 usr: 写入 2」。
6. **ABS→MT 转换缺 BTN_TOUCH**：去掉 tablet 后，VNC 客户端的绝对坐标只能由
   multitouch handler 承接。转换只发 `ABS_MT_TRACKING_ID` 不发 `BTN_TOUCH`；
   guest 侧设备声明了 `BTN_TOUCH` 能力，Android `TouchButtonAccumulator::isHovering()`
   返回 `mHaveBtnTouch && !mBtnTouch` → 指针被判为「悬停」，
   `touchingPointers=[] hoveringPointers=[Pointer(id=0, FINGER)]`，点击永远不下发。

## 证据

- 对齐前云机 6 台设备清单：`.tmp/compare-run-00022.json` 的 `hardware.inputs`
  云机段（QEMU Virtio MultiTouch / Vertio Keyboard / Power Button /
  goodix_fingerprint / virt-tablet2multitouch）。
- 真机 EventHub 身份：`dumpsys-input-real.txt`；descriptor 用
  `sha1(":"+%04x vendor+":"+%04x product+":"+uniqueId(+name 当 vendor=product=0))`
  复核，7 台逐一命中（见 `compare-final.txt`）。
- usr 层缺失：07 日志「机型 usr: 写入 2」，但
  `ls engine/out/devices/xiaomi/gold/OS3.0.10.0.VNQCNXM/vendor/usr/idc/`
  → `No such file or directory`（fail 日志第 2 段）。
- 悬停判定：`dumpsys input | sed -n '/TouchStatesByDisplay/,+3p'`
  → `touchingPointers=[], hoveringPointers=[Pointer(id=0, FINGER)]`
  （fail 日志第 3 段，按压保持状态抓取）。
- 编译期拦路：03 报 `use of undeclared identifier 'SW_JACK_PHYSICAL_INS'`，
  内核头文件实际名字是 `SW_JACK_PHYSICAL_INSERT`（fail 日志第 1 段）。

## 改动

| 文件 | 要点 |
| :--- | :--- |
| `qemu/source/include/hw/virtio/virtio-input.h` | `VirtIOInputHID` 增加 `input_name/input_bus/input_vendor/input_product/input_version/abs_to_mt/touching/last_abs_x/last_abs_y` |
| `qemu/source/hw/input/virtio-input-hid.c` | 新增 `input-name/input-bus/input-vendor/input-product/input-version/abs-to-mt` 属性；realize 时改写 `VIRTIO_INPUT_CFG_ID_NAME/ID_DEVIDS`；ABS→MT-B 单点转换（含 `BTN_TOUCH`）；multitouch handler 的 mask 增加 `INPUT_EVENT_MASK_ABS`，保证 `qemu_input_is_absolute()` 命中 |
| `core/host/daemon/config/device.go` | device.json 的 `input` 段解析为 `Input{Touch,Keyboard *InputDevice; Tablet bool}` |
| `core/host/daemon/run/qemu.go` | 新增 `inputArgs()`：按规格下发 `virtio-multitouch-pci` / `virtio-keyboard-pci` 身份，`tablet:false` 时不再下发 `virtio-tablet-pci` |
| `devices/manifest/xiaomi/gold/device.json` | `input` 段：touch=`goodix_ts`(bus 2, 0x27c6:0x0001:0x0100, abs-to-mt)，keyboard=`mtk-kpd`(bus 25, 0:0:0)，tablet=false |
| `devices/modules/input_identity/` | 新增静态身份模块：uinput-fpc(0x0666:0x0888)、fpc_irq@fingerprint、swtp、Headset Jack(ALSA)、mtk-pmic-keys(0x0001:0x0001:0x0001)，只声明能力不发事件 |
| `devices/modules/goodix_fingerprint/` | 删掉自定义 input_dev 注册，只保留 `goodix_fp` platform driver |
| `engine/build/07_build_vendor/assemble.go` | 「机型 usr」层补 `base:"usr"`；prune 放行顶层 `usr`；装配末尾调用 `placeExcludedInputDevices()` |
| `engine/build/07_build_vendor/input.go` | 排除表在平台层之后落盘，避免被同名文件覆盖 |
| `devices/manifest/xiaomi/gold/vendor/` | 新增 `etc/excluded-input-devices.xml`（排除 QEMU USB/Virtio Tablet、Power Button）、`usr/idc/uinput-fpc.idc`、`usr/keylayout/uinput-fpc.kl`（与真机 `adb cat` 逐字节一致） |
| `engine/build/internal/device/device.go`、`engine/build/11_pack_vda/device_json.go` | `input` 段透传到 dist/device.json（daemon 读 dist 副本） |

## 验证

1. 全链路重跑：02→03→07→08→09；停实例后 11 发布 dist。patch.img sha256
   `843a6ca25cce…`；vendor_dlkm 的 `modules.load` 含 `cdh_input_identity`；
   patch 装配树含 `/vendor/usr/idc/uinput-fpc.idc`、`/vendor/usr/keylayout/uinput-fpc.kl`
   与三行排除表。
2. 实例 5 重启后 `dumpsys input`：7 台设备，
   **(name, Sources, Descriptor) 三元组集合与真机完全一致**（`compare-final.txt`，
   集合差为空）。
3. `KeyLayoutFile: /system/usr/keylayout/uinput-fpc.kl`、
   `ConfigurationFile: /system/usr/idc/uinput-fpc.idc` 命中 → `/vendor/usr`
   经 system 符号链可读，SELinux `vendor_file` 不拦。
4. 触摸：`getevent -lt /dev/input/event1` 抓到
   `BTN_TOUCH DOWN → ABS_MT_TRACKING_ID 1 → PRESSURE 0x32` 与对应的 UP 序列；
   QMP 点击设置图标后 `mCurrentFocus=com.android.settings/…MainSettings`；
   从状态栏下拉能打开通知面板（截图 `out/tmp/ui-shade.png`）。
5. 键盘：QMP 发 `a`，guest `event2` 收到 `KEY_A DOWN/UP`。
6. 云机不再出现 `Power Button` 与 `virt-tablet2multitouch`。

## 遗留

1. **id 顺序是启动时序产物**：真机 `2=mtk-kpd, 3=mtk-pmic-keys, 4=fpc_irq,
   5=Headset, 6=goodix_ts`；云机两次启动分别出现
   `2=goodix_ts,3=mtk-kpd` 与 `2=mtk-kpd,3=goodix_ts`，pmic 恒在 6。
   InputReader 的 id 由 EventHub 打开顺序决定，而 goodix_ts 是 virtio 设备、
   在模块加载前 probe，pmic 在 vendor_dlkm 模块里后注册，顺序无法确定性复刻。
   device-checker 若把 id 计入 diff，会有 1~2 项残差。
2. **Location**：真机 `goodix_ts` 为 `goodix_ts/input0`，云机为 `virtio8/input0`；
   该串由 guest `virtio_input.c` 按 `virtioN/input0` 生成，QEMU 侧无法配置；
   mtk-kpd 真机为空、云机 `virtio9/input0`。二者都不参与 descriptor/sources。
3. **Classes**：真机 uinput-fpc/goodix_ts 带 `BATTERY`（FPC HAL 建的电池节点），
   云机没有；云机 mtk-kpd 多 `ALPHAKEY`（virtio 键盘全键位）。不影响 sources。
4. 远端 device-checker（101.33.73.101）上的云机 agent 未指向本机实例，本轮未重跑
   整批 compare；结论以 adb 直读 `dumpsys input` 为准。
