# m22 静态树 3 份 idc 已是死配置：设备身份早被 device.json 接管——里程碑证据

- 设备: flyme/m22 12.5.0.0A（MEIZU 22，Flyme 12.5.0.0A / Android 16 / lineage-23.2 virtio 底盘，内核 6.12）
- 关联: #51 `issue-m22-vnc-touch-offset`（`input` 段 + 真机身份 `fts` 的由来）、#53 `issue-m22-hardware-list-align`
- 状态: **已完成（构建层）**（2026-10-29）；运行期未复验（见第 6 节）

---

## 1. 问题

`devices/manifest/flyme/m22/vendor/usr/idc/` 下有三份 idc：

| 文件 | 当初的作用 |
| :--- | :--- |
| `QEMU Virtio Tablet.idc` | 把 tablet 钉成 `touchScreen` / `finger` |
| `QEMU Virtio MultiTouch.idc` | 同上 |
| `tablet2multitouch.idc` | 覆盖基座同名件，补 `touch.toolType = finger` |

用户提问：`device.json` 第 20–31 行已经声明了 `input`（`touch.name=fts` / `abs-to-mt` / `tablet:false`），
这三份 idc 是不是可以去掉？

答案：**可以，而且它们已经是死配置**——不是"没用"，是"连匹配对象都不存在了"。

## 2. 根因

idc 的解析规则是 `InputReader` 按 **输入设备名** 去 `/vendor/usr/idc/<设备名>.idc` 找（`.kl` 同理），
名字必须逐字一致（含空格）。#51 给 m22 补 `input` 段之后，三份 idc 的匹配对象同时消失：

1. **设备改名**：`input.touch.name = "fts"` → QEMU 侧 `virtio-input-hid.c` 的 `input-name` 会把
   `VIRTIO_INPUT_CFG_ID_NAME` 改写成 `fts`，guest 里那台触摸设备就叫 `fts`，
   `QEMU Virtio MultiTouch.idc` 永远不会被查到（真正的 keylayout 来自 ROM 自带的
   `/system/usr/keylayout/fts.kl`）。
2. **tablet 不再创建**：`input.tablet = false` → daemon 不再拼 `virtio-tablet-pci`，
   `QEMU Virtio Tablet.idc` 没有对应设备。
3. **`virt-tablet2multitouch` 也没有了**：那台设备原本来自 `input_identity` 模块，
   #51 已把它从 `device.json` 移除，实例 30 的 `dumpsys input` 只剩 7 台设备
   （`mtk-pmic-keys` / `mt6833-mt6359 Headset Jack` / `swtp` / `fpc_irq@fingerprint` / `uinput-fpc` /
   `QEMU Virtio Keyboard` / `fts`），`tablet2multitouch.idc` 同样无主。
4. **语义被 `abs-to-mt` 取代**：QEMU 把 VNC 的单点 `ABS_X/ABS_Y` 直接转成 `ABS_MT_*` + `BTN_TOUCH`
   上报，`fts` 天然是 `Classes: KEYBOARD | TOUCH | TOUCH_MT`、`Sources: KEYBOARD | TOUCHSCREEN`、
   `Touch Input Mapper (mode - DIRECT)`、`X max=1079.967`——不再需要用 idc 把它"扳"成触屏，
   也就不会再有 #42/#44 那类"被分类成 STYLUS 后悬停占住 motion state"的问题。
5. **为什么一直没被发现**：机型静态树是"无声明、放了就上树"的层（07 步骤 4 的第二层
   「机型 usr」窄层，`assemble.go` 的 `overlayLayers()` + `vendorUsrOnlyTop`）。它不参与
   `device.json` 的模块声明，也不会因为"没人读"报警，只会静默多带 3 个文件进 `vendor.img`。

## 3. 证据

### 3.1 运行态：没有匹配对象，也没有任何 idc 被读到（实例 30，`dumpsys input`）

摘自 `../issue-m22-vnc-touch-offset/dumpsys-input-fts.txt`：

| 设备（EventHub） | Classes | ConfigurationFile | KeyLayoutFile |
| :--- | :--- | :--- | :--- |
| `7: fts` | `KEYBOARD \| TOUCH \| TOUCH_MT` | **空** | `/system/usr/keylayout/fts.kl` |
| `6: QEMU Virtio Keyboard` | `KEYBOARD` | 空 | `/system/usr/keylayout/Generic.kl` |
| 其余 5 台（`mtk-pmic-keys` / `Headset Jack` / `swtp` / `fpc_irq@fingerprint` / `uinput-fpc`） | `KEYBOARD` / `SWITCH` | 空 | `Generic.kl` |

- 全表 **`ConfigurationFile` 全部为空**：idc 一份都没被加载；
- 全表 **没有** `QEMU Virtio MultiTouch` / `QEMU Virtio Tablet` / `virt-tablet2multitouch`；
- `fts` 的 `Sources: KEYBOARD | TOUCHSCREEN`、`Motion Ranges X max=1079.967`、
  `mode - DIRECT`、`RawToDisplay 0.0330 / 0.0732` —— 说明"当触屏用"这件事由 `abs-to-mt` 完成，
  与 idc 无关。

### 3.2 构建态：这三份件在 07 里只走"机型 usr"窄层

```
改前（#62 那轮的收尾统计）：
    机型静态树: 写入 26，已存在同内容 0
    机型 usr: 写入 3
改后（本轮）：
    机型静态树: 写入 23，已存在同内容 0
    机型 usr: 跳过（源目录不存在）
```

07 步骤 2 白名单会把基底解包的 `usr` 顶层整棵丢弃（基底只有 `usr/idc/tablet2multitouch.idc` +
`usr/keylayout/Generic.kl` 两件全平台共用集合），静态树的 `vendor/usr/**` 由单独一层补上
（`assemble.go` 的 `{name: "机型 usr", src: StaticTree/usr, base: "usr"}` + `pruneVendorDir` 的
`vendorUsrOnlyTop` 放行）。所以这 3 件既能上树、也不受 `device.json` 任何声明约束。

## 4. 改动

| 文件 | 改动 |
| :--- | :--- |
| `devices/manifest/flyme/m22/vendor/usr/idc/QEMU Virtio MultiTouch.idc` | 删除（305 B） |
| `devices/manifest/flyme/m22/vendor/usr/idc/QEMU Virtio Tablet.idc` | 删除（305 B） |
| `devices/manifest/flyme/m22/vendor/usr/idc/tablet2multitouch.idc` | 删除（476 B） |
| `devices/manifest/flyme/m22/vendor/usr/` | 空目录一并删除（`usr/idc`、`usr`） |
| `devices/manifest/flyme/m22/vendor/etc/excluded-input-devices.xml` | 注释改写：不再声称"触摸语义由 `vendor/usr/idc/` 的 idc 钉成 finger"，改为说明触摸设备叫 `fts`、由 `abs-to-mt` 上报，只排除 `Power Button` |
| `devices/manifest/flyme/m22/README.md` | `### vendor/usr/idc/` 整节重写为"已于 2026-10-29 整体删除"：判据表（设备名 / `ConfigurationFile` / 为什么不需 要 / 键位从哪来）+ 恢复条件 + gold 线不受影响的说明 |
| `devices/evidence/README.md` | 新增 #63 索引行；#51 遗留 1 标注"已按第二条处理" |

**刻意没做的事**：

- 没删 07 的「机型 usr」窄层与 `vendorUsrOnlyTop`（gold 的 `usr/idc/uinput-fpc.idc` +
  `usr/keylayout/uinput-fpc.kl` 仍在用，且它是"基底 usr 不得整棵带入"的唯一防线）；
- 没动内核 `cdh_redirect_vendor_path()` 的 `rules[]`（`/vendor/usr` 是否可达属于 #51 遗留的另一条线）；
- 没重跑 `09_build_patch` / `11_pack_vda`（`out/dist` 与 `patch.img` 仍是旧树，见第 6 节）。

## 5. 验证

`./out/bin/cdh device build m22 --only 07_build_vendor` → `exit=0`，完整日志见 `log-pass.txt`。

| 项目 | 改动前（#62 收尾） | 改动后 | 说明 |
| :--- | :--- | :--- | :--- |
| 机型静态树文件数 | 26 | **23** | 步骤 0 摘要实测 |
| 「机型 usr」层 | 写入 3 | **跳过（源目录不存在）** | 收尾统计第 2 行 |
| 装配树条目 | 661（文件 428 / 链接 199 / 目录 34） | **656（425 / 199 / 32）** | −3 文件 −2 目录（`usr`、`usr/idc`） |
| `vendor/usr/` 是否在装配树 | 有 | **无**（顶层只剩 `bin etc firmware_mnt lib lib64` + `build.prop` / `pci.ids`） | `ls vendor` |
| `vendor.img` 顶层 | 含 `usr` | **无 `usr`** | `debugfs -R "ls -l /"` |
| `vendor.img` SHA256 | `10198921…` | `94df88de3a6acbfa0e5b4e0b957c8eecf7dd94593cf06fd87cbff7c73eeafb69` | 差异仅来自被删的 3 文件 2 目录 |
| `vendor.img` 尺寸 | 205,897,728 B | **205,877,248 B** | −20,480 B（0.01%） |
| VINTF schema | 37 片段 / 0 待改写 | 37 片段 / 0 待改写 | 本次未动任何 xml |
| 元数据回写 | 661 条 | **656 条** | 官方镜像标签真值 4,594 条不变 |
| `e2fsck` / `resize2fs -M` | 通过 | 通过 | 尺寸收紧到 205,877,248 B |

结论：删掉这三份件对装配树、镜像结构、VINTF、SELinux 元数据均无副作用，
只是把"没人读的死配置"从 `vendor.img` 里去掉。

## 6. 遗留

1. **运行期未复验**：本轮只到构建层。下次起 m22 实例时确认：`dumpsys input` 仍是那 7 台设备、
   `ConfigurationFile` 全空、RFB 点击仍精确命中（可用 #51 的 `vnc_tap.py` 复跑），
   结果回填本目录 `log-pass.txt`。
2. **`patch.img` 仍带旧副本**：当前 `out/devices/flyme/m22/12.5.0.0A/img/patch.img`（17:33 构建）里
   `/vendor/usr/idc/` 三件还在——09 是整树镜像 vendor，重跑 `09_build_patch` 后才会消失。
   在重跑之前，`/mnt/patch/vendor/usr/idc/` 仍能看到它们（但 `/vendor/usr` 是否重定向到那里
   另当别论，见 #51 遗留 1）。
3. **`out/dist` 是旧版**：`cdh pack-vda` 未重跑，`vda.img` / `patch.img` 里都还是含 usr 的版本；
   要发布需重跑 09 + 11（或整条流水线）。
4. **恢复 idc 的两个触发条件**（写进 m22 README 了）：`input.touch.abs-to-mt` 改回 false，
   或 `input.tablet` 改回 true。后者尤其要注意：tablet 拿不到 idc 时会被当鼠标，
   X 轴按屏高建映射，#51 就是这么爆的。
