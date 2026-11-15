# #70 m22 手写板级 init 入口下线（init.qcom.rc → 走 07 改名）

日期：2026-10-30　设备：flyme/m22（Android 16，基底 lineage-23.2）

## 1. 问题

`manifest/flyme/m22/vendor/etc/init/hw/init.qcom.rc` 是 13 个机型里**唯一**手写的板级 init 入口
（其余 12 个机型静态树里都没有 `etc/init/hw/init.*.rc`）。它只有 23 行，内容恰好是 lineage
基底 `init.virtio.rc` 的子集：

- `on fs`：`/mnt/vendor/{EFI,persist,shared}` 三个 mkdir + `mount_all --early`
- `on late-fs`：`mount_all --late`

## 2. 根因

写入时的前提已不成立：

1. **当时 board 与 ro.hardware 不一致**。device.json 的 `board` 曾是 `sun`（07 会把基底
   `init.virtio.rc` 改名成 `init.sun.rc`），而 cmdline 的 `androidboot.hardware` 是 `qcom`
   （init 实际找的是 `init.qcom.rc`）→ 改名产出的文件不会被加载，`mount_all` 全不执行，
   `/data` 只读 → zygote `signal 6` 循环。手写 `init.qcom.rc` 是当时唯一能落地的补法
   （`../issue-m22-hal-gap-fill/build-07-09-11-round3.log:28,385` 记录了 `init.sun.rc` 那一轮）。
2. **现在 board 已改成 `qcom`**，07 的改名结果与 ro.hardware 对得上：
   `fstab.go placeVendorInitRC` 在装配树没有 `init.<board>.rc` 时，会把基底的
   `init.virtio.rc` 改名成 `init.qcom.rc`。手写文件因此变成「与引擎机制重复的一份副本」，
   而且比基底那份**少内容**，还多一条维护负担（注释里 `init.sun.rc` 的说法已过时）。

## 3. 证据

- 12 个机型静态树里都没有 `etc/init/hw/init.*.rc`，全部依赖改名；gold 的构建日志就是模板：
  `out/devices/xiaomi/gold/OS3.0.10.0.VNQCNXM/img/vendor-log.txt:300 改名 init.virtio.rc -> init.mt6833.rc`。
- 基底两份 `init.virtio.rc` 差异（attach 的 22.0 vs 当前 23.2）：23.2 增了
  `stop/enable vendor.graphics.allocator.minigbm_upstream`、`setprop ro.vendor.graphics mesa_swrast`，
  删了 `setprop ro.hardware.gralloc minigbm` 与 `seriallogging`（hvc1 logcat）。**m22 现在用的是
  23.2**，所以判断必须以 23.2 那份为准（59 行）。
- 删掉手写件后 m22 会新增的行为（23.2 版，逐条来自文件本身）：
  - `on early-fs`：`vda1→EFI`、`vda3→misc`、`vda4→metadata`、`vda5→cache`、`vda6→boot`、
    `vda7→recovery`、`vda8→firmware`、`vda9→persist`（`ro.build.ab_update=false` 分支）；
  - `import init.virt.rc`：`setprop ro.sf.lcd_density ${ro.boot.lcd_density:-160}`、
    `mount vfat /dev/block/by-name/firmware /vendor/firmware_mnt`、grub-editenv 服务、
    `ro.sf.lcd_density`、virtwifi（`ro.boot.wifi_impl=virt_wifi` 时 `start setup_wifi`）、zram、hwui_renderer；
  - 图形：`setprop ro.vendor.graphics.gpu ${ro.boot.gpu:-virtio_gpu}`、
    `stop vendor.graphics.allocator.minigbm_upstream`、early-boot `exec virtgpu_detect`、
    `on property:ro.hardware.vulkan=*` → `enable …minigbm_upstream`、
    `ro.vendor.graphics=mesa` 时 `setprop ro.hardware.vulkan virtio` / `ro.opengles.version 196610`。

## 4. 改动

删除 `manifest/flyme/m22/vendor/etc/init/hw/init.qcom.rc`（内容留档在本节末尾，便于回退）。
板级入口改由 07 步骤 6 从基底改名产出，与其余 12 个机型一致。

只删这一个文件，引擎零改动：`fstab.go` 的 `placeVendorInitRC` 本来就有两条路径
（装配树有 `init.<board>.rc` → 保留并删基底件；没有 → 把基底改名），这轮只是把 m22 从
第一条切到第二条，与 gold 的 `init.virtio.rc -> init.mt6833.rc` 同一条链路。

回退用的原文：

```text
on fs
    mkdir /mnt/vendor/EFI 0770 root root
    mkdir /mnt/vendor/persist 0770 system system
    mkdir /mnt/vendor/shared 0770 system system
    mount_all --early

on late-fs
    mount_all --late
```

（原文另含 11 行头注释，说明当时 `init.sun.rc`/`ro.hardware=qcom` 不一致的背景，已随文件删除。）

## 5. 验证

见 `log-pass.txt`（改动后）与 `log-fail.txt`（改动前）。

| 项 | 结果 |
| --- | --- |
| 07 全跑 | `EXIT=0`，步骤 6 打印「**改名 init.virtio.rc -> init.qcom.rc**（init 按 ro.hardware=qcom 找板级入口）」；改动前同一位置是「保留 init.qcom.rc（已由上层提供），删除 lineage 的 init.virtio.rc」 |
| 装配树 `etc/init/hw` | 4 件：`init.low_performance.rc` 1416 B、`init.mainline.rc` 307 B、`init.qcom.rc` **2164 B**、`init.virt.rc` 2080 B；`init.virtio.rc` 不再残留 |
| 内容一致性 | 装配树 `init.qcom.rc` sha256 = 基底 23.2 `init.virtio.rc` = `c77d9905…`；原手写件为 `ddb23240…`（1180 B） |
| 装配树条目 | **670**（与改动前一致：手写件出、改名件入，条目数不变） |
| `vendor.img` | **206,704,640 B**（与改动前一致；内容等长走不进新块组） |
| 镜像回读 | `debugfs dump` sha `c77d9905…`、Mode 0644、User/Group 0:0、Size 2164、xattr `u:object_r:vendor_configs_file:s0`；`ls /etc/init/hw` 仅 4 件、无 `init.virtio.rc` |
| 单测 | `go test ./07_build_vendor/` 通过（替身未改代码，仅删机型静态树文件） |

**sha 不作一致性判据**（mke2fs 非确定性），判据 = 装配树条目 + 逐文件 sha + 镜像 xattr。

## 6. 遗留

- **实机未复验**，需要盯三处：
  1. 密度：m22 的 `vendor/build.prop` 没有 `ro.sf.lcd_density`，改名版会在 early-init 写
     `${ro.boot.lcd_density:-160}`。若桌面元素明显变小/变大，说明不能再吃基底这份，
     要么在 `vendor/build.prop` 明确写 440，要么恢复手写入口。
  2. 图形：`exec virtgpu_detect` 与 `ro.vendor.graphics=mesa → setprop ro.hardware.vulkan virtio`
     现在会真的跑（qcom 平台层 + APEX 摊平的 composer/gralloc）。`ro.hardware.egl=mesa` 由基线写入，
     `ro.hardware.vulkan=adreno` 是只读属性，`setprop` 会被拒；要确认 SF/HWC 仍正常、`boot_completed` 正常。
  3. `/vendor/firmware_mnt`：`mount vfat /dev/block/by-name/firmware`（vda8）现在会尝试，
     若 vda8 恰好是 vfat 会**遮住**Flyme 放进来的 firmware 文件；挂载失败只打日志。
- 若三处任一异常，回退方式：把第 4 节的 `on fs` / `on late-fs` 两段写回
  `manifest/flyme/m22/vendor/etc/init/hw/init.qcom.rc`（07 会自动恢复「保留并删基底件」分支）。
