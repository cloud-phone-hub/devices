# Issue: popsicle 只启用了 `cdh_redirect`，6.12 代际的四个外部模块全缺

- 设备：`xiaomi/popsicle`（Xiaomi 17 Pro Max / canoe / lineage-23.2 / kernel 6.12.81）
- 阶段：`03_build_modules`（清单 `modules.external`）→ `08_build_vendor_dlkm`（注入）
- 状态：**已闭环**（2026-11-03，03→11 重跑，12 个模块落进 vendor_dlkm）
- 关联：[#135 issue-vendor-selinux-labels-prune](../issue-vendor-selinux-labels-prune/solution.md)（同轮清理）

## 一、问题

popsicle 基线 `modules.external` 只有 `["cdh_redirect"]`。横向比一遍 17 份基线，
6.12 代际里只有它和 `oneplus/15r` 是这么写的，而 15r 与 popsicle 同平台
（`board: canoe`、`kernel: 6.12`、`hardware` 同样含 `android.hardware.media.v412`）——
同类缺口，15r 侧留给它自己的条目（见「六、遗留」第 5 条）：

| 机型 | 代际 | modules.external |
| :--- | :--- | :--- |
| `oppo/cph2797`（mt6993） | 6.12 | cdh_redirect + device_info + goodix_fingerprint + input_identity + virtio_media |
| **`xiaomi/popsicle`（canoe）** | **6.12** | **仅 cdh_redirect** |
| `oneplus/15r`（canoe） | 6.12 | 仅 cdh_redirect（同类缺口，本轮不越界） |
| `flyme/m22`（qcom） | 6.6 | 同上 5 项 |
| `samsung/ss9380`（qcom） | 6.6 | 同上 5 项 |
| `xiaomi/gold`（MT6833） | 6.6 | 同上 5 项 + `kernelsu`，且 `in-tree: ["cdh_virt_wifi.ko"]` |

同时 popsicle 的 `device.json` 里 `hardware` 明确含 `android.hardware.media.v412`
（v4l2_codec2），而这个 HAL 的硬前提就是 `cdh_media.ko` 带 `allow_userptr=1`——
没有驱动，V4L2 会清掉 USERPTR 能力位、DMABUF 又恒被拒，codec2 直接拒绝启动。

## 二、根因

基线是 `cdh device sync` 从旧设备工作区骨架生成的（`auto-sync.source` 指向
`devices/xiaomi/popsicle-16/build/baselines/…`），那次同步只带过来 `cdh_redirect`
一项；`modules.external` 又是**声明即取代**（`manifest.ExternalModules()` 返回
`declared=true` 时 03 只编清单里那几个），所以少写一项不是「走默认全编」，而是
**该项一个字节都不产物**。

`modules.in-tree` 反而**不用**照抄 gold：gold 的 `cdh_virt_wifi.ko` 是 6.6 树把上游
`virt_wifi.o` 改名的产物，6.12 换成 `mac80211_hwsim` 模型、树里没有这个目标
（见 [m22 #53](../../m22/issue-m22-in-tree-modules/solution.md)）；popsicle 用公共 in-tree 集即可。

## 三、证据

- 改动前 03 产物：`out/kernel/lineage-23-6.12/modules/` 只有 9 个 `.ko`
  （8 个 in-tree + `cdh_redirect.ko`），`cdh_device_info.ko` / `cdh_goodix_fingerprint.ko` /
  `cdh_input_identity.ko` / `cdh_media.ko` 一个都没有。
- 四个模块的 `module.json` 都声明 `"kernel": ["6.6","6.12"]`，与 popsicle 的 `kernel: "6.12"` 相容。
- `virtio_media/module.json` 的 `options: ["allow_userptr=1"]` 是 v4l2_codec2 的前提。
- 同批核对的 **cust**（用户问「popsicle 也要保留 cust」）在 popsicle 上**无源可装**，
  四条独立证据见 `log-fail.txt` 第 2 节：固件包无 `cust.img`、`rawprogram*.xml` 的物理
  分区表无 `cust`、super 的 8 个逻辑分区无 `cust`、官方 `fstab.canoe` 无 `/cust`。

## 四、改动

只改 1 份基线的一个字段：

```json
"modules": {
  "external": [
    "cdh_redirect",
    "device_info",
    "goodix_fingerprint",
    "input_identity",
    "virtio_media"
  ]
}
```

不加 `kernelsu`（gold 独有，popsicle 目前没有 su/ksud 需求），不加 `in-tree`
（6.12 走公共集）。**cust 一项不改**：`vda.partitions` 里写 `cust.img` 会让 01 抽不到源镜像、
11 直接报「取不到」，而 popsicle 本来就没有这个分区。

## 五、验证

`cdh device build popsicle --only 03_build_modules` → `--from build_vendor`（07→11），均 exit=0。

| 项 | 结果 |
| :--- | :--- |
| 03 产物数量 | 9 → **13 个 .ko**（8 in-tree + 5 外部） |
| 符号版本 | 5 个 `cdh_*.ko` 的 `__versions` CRC 与 `Module.symvers` 逐名比对：**不一致 0**（39/39、4/4、8/8、75/75、32/32） |
| 新编出的外部模块 | `cdh_device_info.ko`(39 条 CRC)、`cdh_goodix_fingerprint.ko`(4)、`cdh_input_identity.ko`(8)、`cdh_media.ko`(75)，vermagic 均为 `6.12.81-4k-cdh` |
| 08 步骤 2 | 清空 **106** 个基座 `.ko`，回读复核 `/lib/modules` 已无 `.ko` |
| 08 依赖闭包 | 成立：**12 个模块**的 `depends/softdep` 均由注入集或内核内建（677 项）提供 |
| 08 步骤 4/6 | 写入 **18** 个文件（模块 12 + 索引 5 + build.prop），回读 18/18 一致 |
| vendor_dlkm `modules.load` | 12 项：`cdh_device_info cdh_goodix_fingerprint cdh_input_identity cdh_media cfg80211 libarc4 mac80211 mac80211_hwsim rfkill virtio_console zram zsmalloc` |
| `modules.options` | `options cdh_media allow_userptr=1` |
| `modules.dep` | 4 条内核链：`cfg80211→rfkill`、`mac80211→cfg80211+libarc4`、`mac80211_hwsim→cfg80211+mac80211`、`zram→zsmalloc`；**4 个新件均无 depends** |
| 下游 | `vendor_dlkm.img` sha256 `92f9fd14d1ac`（15,831,040 B）→ `patch.img` 384.0 MiB（sha256 `a241df03630f`），`patch-info.txt` 的 `dlkm-sha256` 与磁盘件一致 |

逐项原始输出见 `log-pass.txt`（03 产物与 CRC 对账第 1–2 节、08 八步第 3 节、镜像内
`debugfs` 实读第 4 节、下游第 5 节）。

`cdh_redirect.ko` 仍走 first-stage（`inject=first-stage`），在 `vendor_boot` 的 ramdisk 里，
不在 vendor_dlkm 的 12 项内——口径与 gold/m22 一致。

## 六、遗留

1. **`device_info` 的身份口径是 gold-15 的硬编码**：`uts/uts.c` 写死
   `6.6.89-android15-8-gbe8d201b0d27-ab13762941-4k`、meminfo 按 MT6833 的
   `MemTotal=7760724 kB`。装到 popsicle（6.12.81 / SM8850 / Android 16）上，App 读到的
   `uname -r` 会变成 6.6.89。m22 / cph2797 / ss9380 也是同样装法，属既有工程决策；
   若后续要求「popsicle 呈现自己的身份」，需要给该模块加机型 profile，不是改清单。
2. **`kernelsu` 未开**：gold 用它做云机 root（`allow_shell=1` + `/data/adb/ksud`）。
   popsicle 目前没有 root 通道需求；需要时装 `kernelsu` 这一项即可，源码由
   `manifest/kernel.xml` 拉取、无需改 Kbuild。
3. **cust 结论已被推翻**（2026-11-03 补注）：本条当时判「popsicle 无 cust 分区」依据的四条证据
   （固件包无 cust 成员 / `rawprogram*.xml` 无 cust / super 无 cust / 官方 fstab 无 `/cust`）
   只覆盖「独立分区」这一层——cust 实际藏在 `userdata.img`（f2fs）的 `/preinstall/cust.img`
   （EROFS 2.53 GiB，34 个 APK），作为独立分区挂 `/cust`。修正与本轮的做法见
   [issue-cust-from-userdata](../issue-cust-from-userdata/solution.md)（#139）；
   **本条对 `modules.external` 的改动不受影响**。
4. **未上机验证**：本轮只到镜像与盘集生成，未起机复核 AIDA64/DevInfo 读数、codec2 是否
   真的能用 USERPTR 起解码。
5. **`oneplus/15r` 是同类缺口**：基线 `modules.external` 同样只有 `["cdh_redirect"]`，
   `board: canoe` / `kernel: 6.12` / `hardware` 含 `android.hardware.media.v412`，与 popsicle
   逐项同型（该机已能起机靠的是不依赖 `cdh_media` 的启动路径，codec2 仍不可用）。
   15r 侧应在其自身条目里按同一判据补 4 项，不在本条改。
