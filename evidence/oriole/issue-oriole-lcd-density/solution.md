# Issue: oriole 云机按「平板」口径排版（密度 160），图标与文字整体偏小

- 设备: `google/oriole`（Pixel 6），官方固件 `AP2A.240905.003.F1`（Android 14）；底包 `lineage-21.0`
- 阶段: `07_build_vendor`（基线 `vendor.build-prop-sets`）→ 09 → 11
- 状态: **已闭环**（2026-11-01，实例 8）：`ro.sf.lcd_density` 160 → **420**，可用窗口 411×914 dp，回到 Pixel 6 手机口径
- 关联: [#103 issue-vendor-lift-manifest](../issue-vendor-lift-manifest/solution.md)（同一次起机验证发现）；[#101](../issue-zygote-bootstrap-ns/solution.md)/[#102](../../common/issue-carrier-mount-unify/solution.md)（载体链路）

---

## 1. 问题

oriole 云机起来后桌面图标与文字明显偏小，整体观感像平板而不是手机。实测实例 7：

```
$ adb shell 'wm size; wm density; getprop ro.sf.lcd_density'
Physical size: 1080x2400
Physical density: 160
160
```

1080×2400 @ 160 dpi = **可用 1080 dp 宽**（`dp = px × 160 / density`）。Pixel 6 真机是 420 dpi，同一分辨率只有 411 dp 宽——差 2.6 倍，等于把手机 UI 按平板铺开。

## 2. 根因

三处配置各管一段，最后一环没人接手：

1. `device.json`（`engine/out/dist/google/oriole/AP2A.240905.003.F1/device.json`）写着 `"density": 420`，但它只进 daemon 的窗口元数据，**不进任何镜像文件**；
2. A14 基座（lineage-21.0）的 `vendor/build.prop` **没有** `ro.sf.lcd_density` 这一行（guest 内 `grep -i density /vendor/build.prop` 无命中）；
3. 底包 `/vendor/etc/init/hw/init.virt.rc` 第 22 行兜底：

```
    setprop ro.sf.lcd_density ${ro.boot.lcd_density:-160}
```

而本机型 cmdline 不带 `androidboot.lcd_density`（05 只改写 `verifiedbootstate` / `androidboot.hardware` 等），于是取 `:-160`。

## 3. 证据

### 3.1 官方 Pixel 6 的真值就在 vendor/build.prop

```
$ debugfs -R "dump /build.prop engine/out/tmp/oriole-a14-vendor-build.prop" \
    engine/out/roms/google/oriole/AP2A.240905.003.F1/super/vendor_a.img
$ sed -n '49p' engine/out/tmp/oriole-a14-vendor-build.prop
ro.sf.lcd_density=420
```

即：这个属性**本来就该由 `/vendor/build.prop` 承载**，官方件是 420，与 `device.json` 的 420 一致。我们装配出的 `/vendor/build.prop` 来自 lineage 基座，缺这一行。

### 3.2 底包兜底 160 的出处

```
$ adb shell grep -n lcd_density /vendor/etc/init/hw/init.virt.rc
22:    setprop ro.sf.lcd_density ${ro.boot.lcd_density:-160}
```

同一份 `init.virt.rc` 未导入就完全不生效，导入则把密度钉成 160——m22 的 [#70 issue-m22-init-entry-dedup](../../m22/issue-m22-init-entry-dedup/solution.md) 早就把这个坑记成遗留（「若桌面元素明显变小，说明不能再吃基底这份」），oriole 这次正是踩中。

### 3.3 属性优先级：build.prop 早于 rc

`ro.` 属性 set-once。`vendor/build.prop` 在 init 的 `property_init` 之后、任何 rc action 之前装载；`init.virt.rc` 的 `setprop` 走的是 `early-init`，此时属性已锁，会被 `PROP_ERROR_READ_ONLY_PROPERTY` 拒绝。gold/ss9380 的同类结论见各机型基线 note（「不能等 rc 的 setprop」）。

## 4. 改动

1. 基线清单 `manifest/google/oriole/baselines/oriole-ap2a.240905.003.f1-factory-655c44e7.json`：
   - `vendor.build-prop-sets` 增加 `"ro.sf.lcd_density": "420"`（第 20 项，值取官方 `vendor_a.img:/build.prop` 与 `device.json`）；
   - `vendor.note` 末尾补一段「屏幕密度」，写清 160 的来源（`init.virt.rc:22` 的 `:-160`）、真值出处（官方 build.prop:49）与「build.prop 早于 rc」的优先级理由。
2. 无代码改动：07 步骤 6 的 `props.go` 本来就负责把 `build-prop-sets` 覆盖/追加进装配树的 `vendor/build.prop`。
3. 重跑 07 → 09 → 11 让新 vendor 进盘集（vendor 走 `patch.img` → `/mnt/vendorpatch`，`super.img` 不必重打）。

## 5. 验证

- 07：`vendor 属性: 基线 build-prop-sets 20 项 / unsets 0 项 -> vendor/build.prop`，装配树 `vendor/build.prop:151: ro.sf.lcd_density=420`。
- 产物指纹：`vendor.img` sha256 `d6be06486953bb4114e461b9b40f1f5852a878614527a8d1faaba7dac102f574`；`patch.img` `40786db363c7…`；`recipe: vendor-67855d254548`（**未变**，见遗留）。
- 实例 8（adb `127.0.0.1:5911`）：

```
$ adb shell 'getprop ro.sf.lcd_density; wm size; wm density'
420
Physical size: 1080x2400
Physical density: 420

$ adb shell dumpsys display | grep -m1 DisplayDeviceInfo
DisplayDeviceInfo{... 1080 x 2400 ... density 420, 457.2 x 457.2 dpi ... state ON ...}

$ adb shell 'getprop sys.boot_completed; pidof com.google.android.apps.nexuslauncher'
1
2474
```

- 密度换算：`1080 × 160 / 420 = 411 dp` 宽、`2400 × 160 / 420 = 914 dp` 高——落在手机区间（sw < 600 dp），不再是 1080 dp 的平板布局。
- 回归：`hwc3` composer 与 `SurfaceFlinger` 仍正常（同一次起机，#103 §5 的链路未受影响）；`boot_completed=1`，launcher / system_server（403 个进程）在跑。

## 6. 遗留

1. **密度仍不在配方里**：`ComputeVendorRecipe` 只哈希基底镜像 + HAL 产物根 + APEX 摊平模式，改 `build-prop-sets` 后 `vendor-recipe.txt` 仍是 `vendor-67855d254548` 而 `vendor.img` 字节已变——与 #103 遗留 2 同一条裂缝（07 每次全量重装配兜底）。
2. **设备规格三处仍需人肉对齐**：`device.json`（daemon 窗口）、`vendor.build-prop-sets`（guest 生效值）、`manifest/<vendor>/<brand>/README.md`（取证记录）里都记 density；这次数值相同（420）所以没暴露，但没有任何机制保证三者同步。
3. **视觉验收靠 VNC**：`adb shell screencap` 在 oriole 云机会挂住（无 GPU readback），只能人工看 VNC（实例 8 为 `127.0.0.1:5912`）确认图标/字号观感。
4. 中途排查时在实例 7 上用过 `wm density 420` 做 A/B，该实例已随 11 重刷盘集销毁，`data.qcow2` 重建，无残留。
