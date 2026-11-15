# 输入设备身份 KO（input_identity）

按真机取证静态注册输入设备集合，使 guest 的
InputReader 算出的 `name` / `descriptor` / `sources` 与真机 `hardware.inputs`
一致。设备不发事件，只声明身份与 capability。

另含 **goodix_fp platform driver 形态**（原 `goodix_fingerprint` 模块，
2026-11 收归后删除）：只生成 AIDA64/DevInfo 会读的 kernel-visible 形态，
见下文「goodix_fp 形态」一节。

## 结构：配置驱动（无 C 档案、无缺省）

自 2026-11 起设备数据**全部来自基线清单 `modules.input_identity`**，不再有任何
按机型硬编码的 C 档案。复用 `device_info` 已验证的三段式：

```text
清单 modules.input_identity 声明
        │  08_build_vendor_dlkm 按 assets.from-manifest=input-devices 渲染
        ▼
/lib/modules/cdh_input_identity.devices   （行式文本，名称已在 Go 构建期转成数字码）
        │  模块加载时 kernel_read_file_from_path 读取
        ▼
config.c 解析 → main.c 泛化注册（零机型分支、零机型缺省）
```

源码组织（执行侧与数据侧都零机型名）：

```text
input_identity/
├── config.h     运行时动态结构（devs/aligns/touch_major/goodix_fp）+ 路径常量 + 上限
├── config.c     行式文本解析器（照 device_info uts.c 范式：读文件→状态机→动态数组）
├── main.c       泛化执行侧：能力铺设/注册/回滚 + 声明式对齐 + goodix_fp
├── module.json  自述：assets.from-manifest=input-devices（伴随文件由清单渲染）
└── Kbuild       cdh_input_identity-y := main.o config.o
```

**无缺省**（用户拍板）：删除了 `profiles/`、`profile.c`、`profile.h` 与 `profile`
模块参数。每份机型清单各自声明完整设备集；清单缺 `modules.input_identity` →
**08 构建失败**；文件缺失或解析失败 → 模块加载 `-EINVAL`，不静默回退任何机型。
校验硬闸门放在**构建期 Go**（`internal/manifest` 解析校验 + 能力码转码），
不把失败推迟到设备。

**新增机型 / 改设备集**：只改该机型清单的 `modules.input_identity`（见下 schema），
执行侧零改动；不再新建 C 档案、不再改 Kbuild。

**多版本编译**：只用 5.10–6.18 各代内核的稳定导出符号（input_* /
platform_driver_* / bitmap / kernel_read_file_from_path），同源码编全代际、
无版本分支；已在本地 6.6 / 6.12 两代构建卷实编验证。

## 清单 schema（modules.input_identity）

与 `modules.cpufreq` 平级。能力码（keys/switches/msc/abs.axis）接受**名称或整数**：
名称经 Go 映射表转码（自文档化），整数直通（swtp 的 249/250 无宏，用整数）。
`bus/vendor/product/version` 用十进制数字（与 `modules.input` 一致，缺省 0）。

```json
"input_identity": {
  "touch_major": true,
  "goodix_fp": true,
  "devices": [
    { "name": "uinput-fpc", "bus": 6, "vendor": 1638, "product": 2184, "version": 0,
      "keys": ["KEY_UP","KEY_LEFT","KEY_RIGHT","KEY_DOWN"],
      "abs": [{"axis":"ABS_Z","min":0,"max":255,"fuzz":0,"flat":0}] },
    { "name": "mt6833-mt6359 Headset Jack", "bus": 25, "phys": "ALSA",
      "keys": ["KEY_PLAYPAUSE","BTN_1","BTN_2","KEY_VOICECOMMAND"],
      "switches": ["SW_HEADPHONE_INSERT","SW_MICROPHONE_INSERT","SW_LINEOUT_INSERT","SW_JACK_PHYSICAL_INSERT"] }
  ],
  "aligns": [ { "name": "mtk-kpd", "keys": ["KEY_VOLUMEDOWN"] } ]
}
```

- `devices` 可为空数组（某机型无静态注册设备时）；`aligns`/`touch_major`/`goodix_fp`
  可缺省（缺省 aligns=空、touch_major=false、goodix_fp=false）。
- 构建期校验：name 非空无首尾空白；id 各字段 0..65535；能力码范围
  （KEY≤0x2ff、SW≤0x10、MSC≤0x7、ABS≤0x3f，按 linux/input-event-codes.h）；
  abs min≤max、fuzz/flat≥0；aligns.keys 非空；未知键一律报错。

## 内核侧文本格式（cdh_input_identity.devices）

照 cpufreq 的「记录头 + 子行」风格，两空格分隔，`#` 注释。内核只解析数字
（名称已在 Go 渲染期转码）：

```text
touch_major  1
goodix_fp  1
dev  uinput-fpc
id  6 1638 2184 0
key  103 105 106 108
abs  2 0 255 0 0
dev  mt6833-mt6359 Headset Jack
id  25 0 0 0
phys  ALSA
key  164 257 258 582
sw  2 4 6 7
align  mtk-kpd
key  114
```

- `dev`/`align` 取**整行剩余**为名（容忍空格，如 Headset Jack），起一条记录；
  后续 `id/phys/key/sw/msc/abs` 子行归属当前记录。
- `id` 4 个数字；`key/sw/msc` 变长数字（空格分隔）；`abs` 每行 5 数字
  （axis min max fuzz flat），一轴一行；`phys` 取整行剩余。
- 非法行 → `-EINVAL` 带行号 `pr_err`，不加载。

## 读取路径与模块参数

`kernel_read_file_from_path` 不走 cdh_redirect，故照 device_info uts 先读 patch
物理路径再回退直挂路径（常量在 config.h）：

1. `/mnt/vendorpatch/vendor_dlkm/lib/modules/cdh_input_identity.devices`
2. `-ENOENT` 回退 `/vendor_dlkm/lib/modules/cdh_input_identity.devices`

模块参数（照 uts 的 uts_path/uts）：

- `devices_path=`（charp）：覆盖上述读取路径，缺省走路径回退。
- `devices=`（charp）：调试用内联文本，非空时忽略 path 直接解析。

缺文件 / 解析失败 → `-EINVAL`，不加载（无内置缺省设备集）。

## 注册的设备（历史实测样例）

> 下列设备集是 gold/oriole 真机取证结果，**现由各自清单 `modules.input_identity`
> 声明**（32 份非 oriole 清单写 gold 设备集，4 份 oriole 清单写 oriole 设备集），
> 不再硬编码于 C 档案。保留此表仅作真机对照与新增机型时的参考样例。

### gold（Redmi Note 13 5G）

| name | bus | vendor:product:version | 能力 |
| --- | --- | --- | --- |
| uinput-fpc | 0x06 BUS_VIRTUAL | 0x0666:0x0888:0 | KEY_UP/LEFT/RIGHT/DOWN + ABS_Z 0..255 |
| fpc_irq@fingerprint | 0 | 0:0:0 | KEY_POWER/KEY_WAKEUP + MSC_SCAN |
| swtp | 0 | 0:0:0 | KEY 0xf9/0xfa |
| mt6833-mt6359 Headset Jack | 0x19 BUS_HOST | 0:0:0 | KEY_PLAYPAUSE/BTN_1/BTN_2/KEY_VOICECOMMAND + 4 个 SW_JACK |
| mtk-pmic-keys | 0x19 BUS_HOST | 1:1:1 | KEY_VOLUMEUP/KEY_POWER |

`fpc_irq@fingerprint` 与 `swtp` 身份全零且 Location 均为空，InputReader 会把
两者合并成一台设备（真机 `EventHub Devices: [ 5 3 ]`）。gold 另声明
`mtk-kpd → KEY_VOLUMEDOWN` 键盘对齐，touch_major=开、goodix_fp=开。

### oriole（Pixel 6）

| name | bus | vendor:product:version | 能力 |
| --- | --- | --- | --- |
| goodix_fingerprint | 0 | 0:0:0 | KEY_POWER/KEY_WAKEUP + MSC_SCAN |
| gpio_keys | 0x19 BUS_HOST | 1:1:1 | KEY_VOLUMEUP/KEY_VOLUMEDOWN/KEY_POWER |

Pixel 6 真机仅这两台内核输入设备（外加 Virtual 与触摸屏 fts，run-00001
取证）；`goodix_fingerprint` 零身份，EventHub 走 name-only 描述符
`b7b9c5afc0b05e8bbf15d2ca5b330d3bc3d421d4`。oriole 无 QEMU 键盘对齐，
touch_major=开、goodix_fp=关。

oriole14/15/16/17 四份清单均声明 oriole 设备集（15/16/17 历史上靠 gold 缺省
误拿 Redmi 设备集，2026-11 起纠正为 Pixel 设备集）。

## 配套 vendor 文件

- `/vendor/usr/idc/uinput-fpc.idc`：`keyboard.layout = uinput-fpc`。
- `/vendor/usr/keylayout/uinput-fpc.kl`：只保留 LEFT→DPAD_CENTER、
  RIGHT→BUTTON_C，使 sources = KEYBOARD|GAMEPAD（0x501）。

缺 `.kl` 时按键走 Generic.kl，uinput-fpc 会被归成 DPAD，sources 不再是 0x501。

## 与 QEMU 侧的分工

QEMU virtio-input 设备（触屏 / 键盘）的**身份**（name/bus/vendor/product/version）
由清单 `modules.input` 声明，11_pack_vda 透传 device.json，宿主 daemon 据此创建
设备——本模块不重复注册这些名字。清单 `modules.input_identity` 与 `modules.input`
合起来覆盖该机型**全部**输入设备。QEMU 设备的**能力位**由本模块按清单声明对齐：

- **键盘 `aligns`**（按名匹配）：QEMU virtio-keyboard 默认是全键盘，清单声明
  真机按键集，connect 时把 keybit 收敛为该集合，并清 EV_MSC/EV_LED/EV_REP。
  gold 声明 `mtk-kpd → KEY_VOLUMEDOWN`（真机仅音量下键一颗，电源/音量上
  由 mtk-pmic-keys 上报）；oriole 无 QEMU 键盘，不声明。KEY_Q 作为 QEMU 全键盘
  特征位保证幂等（已对齐的设备不再含它）。
- **触屏 `touch_major`**（按 MT 能力位匹配，不绑名）：QEMU virtio 多点触控缺
  `ABS_MT_TOUCH_MAJOR`，真机有；补齐后 InputReader TouchInputMapper 自动合成
  SIZE/TOUCH_MAJOR/TOUCH_MINOR/TOOL_MAJOR/TOOL_MINOR（共 8 轴）。同一设备集被多
  机型共用、各机型清单触屏名不同（goodix_ts/fts/sec_touchscreen/touchpanel/
  gcore/input_mt_wrapper），故按能力位匹配而非设备名。

对齐 handler 的 connect 返回 `-ENODEV`，只就地补齐 absbit/keybit，不绑定
input_handle，事件热路径零开销。

## goodix_fp 形态（原 goodix_fingerprint 模块收归）

注册一个空 `platform_driver`（name = `goodix_fp`），生成
`/sys/bus/platform/drivers/goodix_fp` 目录，供 AIDA64/DevInfo 一类检测
应用读取。边界：

- 不提供真实指纹能力，不注册 platform device，不触发真实硬件 probe；
- 不发送 `KEY_POWER`、`KEY_HOME` 或任何其它 input event；
- 不使用 uinput daemon、framework.jar、native hook、kallsyms、kprobe、
  ftrace、syscall hook 或 text patch；
- **由清单 `goodix_fp` 开关控制**（gold=开、oriole=关）：历史上所有非 oriole
  机型清单都成对启用 `input_identity` + `goodix_fingerprint`，oriole 清单
  历来只有前者，收归后按清单保持原行为。

运行验证（模块加载后）：

```text
adb shell 'grep cdh_input_identity /proc/modules'
adb shell 'ls -ld /sys/bus/platform/drivers/goodix_fp'
adb shell 'dmesg | grep "registered goodix_fp"'
```

预期：`/sys/bus/platform/drivers/goodix_fp` 是 `drwxr-xr-x` driver 目录；
`getevent -il` / `dumpsys input` 里的 `goodix_fingerprint` 输入设备由本模块
的输入设备身份部分提供（oriole 设备集），与 platform driver 形态互不影响。

模块 ABI 口径不变：`modinfo vermagic` 必须与目标内核产物精确匹配，
由 03 的 `kbuild.VerifyModules` 符号版本校验把关。