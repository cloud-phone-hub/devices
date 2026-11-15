# patch.img 改挂 /mnt/patch 后被 vold 撕掉（vendor/vendor_dlkm/odm 三棵子树整体 ENOENT）—— 里程碑证据

- 设备: Xiaomi Redmi Note 13 5G (gold-15)
- 宿主: macOS (darwin, QEMU + hvf)
- 状态: **已闭环**（2026-10-26，实例 `gold_OS3.0.10.0.VNQCNXM/34`）：内核在 `ksys_umount()`
  拒绝卸载 `/mnt/patch`，vold 启动时的 `MNT_DETACH` 清理空转，patch 挂载全程保留，
  `/data` 正常挂载、`sys.boot_completed=1`、zygote 无重启。
  同主题的另一条路线 merge-v5（patch 根直接充当 vendor 根，挂 `/mnt/vendor`）
  **已实测通过但被回退**，见第 7 节。

---

## 1. 问题

按「patch.img 挂到 `/mnt/patch`，内部放 `/vendor`、`/vendor_dlkm`、`/odm` 三棵并列子树」改完后，
first-stage 挂载本身是成功的（`__mount(source=/dev/block/vdc,target=/mnt/patch,type=ext4)=0: Success`），
但整机起不来（实例 32）：

```
# 0.709s：vold（T202）把 vdc 挂载卸掉
[    0.709234][  T202] EXT4-fs (vdc): unmounting filesystem 25902bea-b62b-464c-b52f-756e57bd0be7.

# 0.711s：vold 读不到默认 fstab
[    0.711779][  T202] vold: [libfstab] ReadDefaultFstab(): failed to find device default fstab
[    0.712117][  T202] vold: Failed to open default fstab: No such file or directory

# 5.8s 起：keymint rc 的 wait /dev/hvc3 卡满 30s 超时
[   35.820819][  T117] init: wait for '/dev/hvc3' timed out and took 30001ms

# 35.8s：mount_all --late 因默认 fstab 不可读直接失败
[   35.868931][    T1] init: Command 'mount_all --late' action=late-fs (/vendor/etc/init/hw/init.mt6833.rc:34) took 0ms and failed: Could not read default fstab
```

`/data` 从未挂载（`chown/chmod/mkdir /data...: Read-only file system`），从 35.9s 起
keystore2/zygote 每 ~5s `received signal 6` 一次，一直循环到 642s（共 454 次，zygote pid
已重启到 11681），adb 始终 offline。

同一镜像只把挂载点换回旧的 `/mnt/vendor`（实例 30）则一切正常 —— 变量只有一个：挂载点名字。

## 2. 根因

**system/vold 的 `VolumeManager` 启动时会把 `/proc/mounts` 里 `/mnt` 下不在其私有白名单中的
子挂载全部 `umount2(MNT_DETACH)` 掉。** `/mnt/vendor` 恰在白名单里（所以旧布局幸存），
`/mnt/patch` 不在，first-stage 挂载的 patch.img 在 ~0.7s 被撕，之后 `/vendor/...` 经内核
`getname` 重定向落到 `/mnt/patch/vendor/...` 全部 ENOENT：

| 连锁点 | 直接后果 |
|---|---|
| `/vendor/etc/fstab.mt6833` 不可读 | vold / init 的 `ReadDefaultFstab()` 失败 |
| `/vendor/bin/hw/android.hardware.security.keymint-service` 不可读 | keymint 服务 exec 失败，其 rc 的 `wait /dev/hvc3 30` 卡满超时 |
| `mount_all --late` 读不到 fstab | `/data` 永不挂载 |
| `/data` 不在 | zygote/system_server 反复 signal 6，adb offline |

**白名单来自 vold 二进制（源码 `system/vold/VolumeManager.cpp`）**，从 system_a.img（ext4）
`debugfs dump /system/bin/vold` 后 `objdump -d` 取证，`0x9ac58-0x9ad88` 为 6 条
`android::base::StartsWith`：

```
/mnt/scratch(12)  /mnt/vendor(11)  /mnt/product(12)
/mnt/installer(14)  /mnt/androidwritable(20)  /mnt/vm(7)
```

命中即跳过，未命中入待拆链表，随后打印 `Tearing down stale mount ` 并卸载。
卸载封装在 `0x57724`：`umount2(path, w1=8)`，**flags = MNT_DETACH**，最多重试三轮；
关键语义是 `errno == EBUSY(16)` 或 `ENOENT(2)` 都被当作「没有可卸载的挂载」直接返回成功
（`cmp w9,#0x16; b.eq 成功`），因此**内核只要返回 EBUSY，vold 就静默放过，且不打错误日志**。

实例 33 验证了「只在 init rc 里 post-fs 补挂」不够：补挂后 keymint 能起来
（`wait /dev/hvc3 took 52ms`），但 vold 启动时已经读过一次（失败的）fstab，内部状态为空，
`mount_all --late` 仍失败：

```
[    6.298136][    T1] init: Command 'mount_all --late' ... failed: queue_fs_event() failed: Invalid code: -1
```

补挂只能恢复文件路径，恢复不了 vold 的内部 fstab 状态。必须让 vold **根本不撕**。

## 3. 证据

- `log-fail.txt`：实例 32（vold 卸载 → fstab/ENOENT → 30s 超时 → /data 只读 → 454 次
  signal 6）+ 实例 33（rc 补挂后 `queue_fs_event() failed: Invalid code: -1`）
- `log-pass.txt`：实例 34（`Failed to unmount /mnt/patch: Device or resource busy` →
  `/data` 挂载成功 → boot_completed → signal 6 计数 1）
- 二进制取证：`debugfs -R "dump /system/bin/vold" out/roms/lineage/lineage-22.0/super/system_a.img`
  + `objdump -d`（0x9ac58-0x9ad88 白名单、0x57724 卸载封装、0x1093a 字符串 `/mnt/vendor`）
- 时序对照（同一镜像，唯一变量是挂载点名字）：

| 实例 | patch 挂载点 | vold 撕挂载 | mount_all --late | /data | 结果 |
|---|---|---|---|---|---|
| 30 | `/mnt/vendor` | 否（白名单） | 成功 570ms | 正常 | 开机成功 |
| 32 | `/mnt/patch` | 是（0.709s） | 失败（无 fstab） | 未挂 | zygote 454 次 signal 6 |
| 33 | `/mnt/patch` + rc 补挂 | 是 | 失败（queue_fs_event -1） | 未挂 | keymint 起来但仍崩 |
| 34 | `/mnt/patch` + 内核保护 | **否（EBUSY 拒绝）** | 成功 611ms | 正常 | boot_completed |

## 4. 改动

### 4.1 内核 `fs/namespace.c`（提交 `ccb0313347194`）

新增 `cdh_patch_mount()`（`CONFIG_CDH_PATH_REDIRECT` 下），在 `ksys_umount()` 解析出 path 后、
`path_umount()` 之前拦截：

```c
if (path.dentry == path.mnt->mnt_root && cdh_patch_mount(&path)) {
    path_put(&path);
    return -EBUSY;
}
```

- 只拦裸挂载点本身（子路径本来就不是挂载点，umount 本就是 EINVAL）；
- 含 `MNT_DETACH` 一起拦——vold 用的正是 8；
- 返回 `EBUSY` 是刻意选择：vold 视其为「没有需要卸载的挂载」，不打错误日志；
- `cdh_patch_mount()` 用 `d_path()` 比对 `/mnt/patch`，与挂载点名字强绑定（与本项目
  「载体固定 `/mnt/patch`」的约定一致）。

### 4.2 `fs/Kconfig`

`CONFIG_CDH_PATH_REDIRECT` 的 help 补一句：同时拒绝卸载 `/mnt/patch` 本身，以及 vold
为什么会撕它（白名单 + MNT_DETACH）。

### 4.3 构建链（本次一并生效）

- `06_build_odm`：产出 `img/odm.img`（8 MiB ext4）
- `09_build_patch`：三镜像解包 → `/vendor`、`/vendor_dlkm`、`/odm` 三棵子树，970 条目
- `05_build_vendor_boot`：first-stage fstab 挂载点 `/mnt/patch`
- 重建顺序：`02 build-kernel -image ghcr.io/feicong/droid-ddk:android15-6.6` →
  `04 build-boot --kernel-out out/kernel/lineage-22-6.6` →
  `07 build-vendor --manifest gold_images_OS3.0.10.0.VNQCNXM_...json` →
  `09 build-patch` → `11 pack-vda`（`CDH_LINEAGE_ZIP=...`）

> 注：`07_build_vendor` 里一度加过「板级 init rc 的 on post-fs 补挂」并被实例 33 证伪
> （vold 内部状态无法恢复），最终回滚，保留内核单一机制。

## 5. 验证（实例 34，`out/core/daemon/runtime/gold_OS3.0.10.0.VNQCNXM/34/qemu.log`）

1. **vold 撕不掉**（内核保护生效）：
   ```
   [    4.703906][  T202] vold: ForceUnmount failed. Trying MNT_DETACH /mnt/patch: Device or resource busy
   [    4.704429][  T202] vold: Failed to unmount /mnt/patch: Device or resource busy
   ```
2. **/data 正常挂载、late 阶段成功**：
   ```
   [   15.548139][  T202] vold: [libfs_mgr] __mount(source=/dev/block/mapper/userdata,target=/data,type=ext4)=0: Success
   [   15.550547][    T1] init: Userdata mounted using (default fstab) result : 7
   [   15.551108][    T1] init: Command 'mount_all --late' ... took 611ms and succeeded
   ```
3. **keymint 秒起**：`wait /dev/hvc3 took 54ms`，4 个 keymint 实例全部注册。
4. **纯 64 位 zygote**：只解析 `init.zygote64.rc`，`init.zygote32` 计数 0。
5. **开机完成并稳定**：`98.45s` 处理 `sys.boot_completed=1`；SystemServer 持续运行至
   177s+，qemu.log 121284 行且继续增长；`received signal 6` 计数 1（仅
   `vendor.setup_sound_card` oneshot，与本改动无关）。

## 6. 遗留

- **adb 仍 offline**（既有问题，非本改动引入；实例 30 同样如此）：guest 侧 adbd 在
  `sys.boot_completed=1` 后由 `cph_start_adbd.rc` 启动，但宿主 `adb connect 127.0.0.1:5911`
  停在 offline。本轮的验证全部走 qemu.log 串口通道。
  （2026-10-27 复核：实例 4 的 5911 已可正常 adb，见 `../issue-cmd-binder-transfer-fd/`；
  旧实例 30 的 offline 未重新复核。）
- ~~`vendor.setup_sound_card` oneshot 收到 signal 6~~ **已消失**：
  `../issue-qemu-hda-sound-card/` 给 QEMU 接上 intel-hda 后，该 oneshot 正常退出
  （`I setup_sound_card: No recognized sound card found, Exiting.`）。
- MIUI 专有分区 `/cust` 的 `wait` 固定耗 5s（既有）。
- 内核保护与挂载点名字强绑定：若以后 patch 载体改名，需同步
  `fs/namespace.c` 的 `cdh_patch_mount()`、`fs/namei.c` 的两张重定向表与 `05` 的 fstab。

## 7. 附：merge-v5（patch 根直接充当 vendor 根）—— 已否决

同一主题上的另一条路线。**曾实测通过，但现行主线已回退到 merge-v4**；
记录在此以免重复尝试。

| | merge-v4（现行） | merge-v5（已否决） |
|---|---|---|
| 挂载点 | `/mnt/patch` | `/mnt/vendor` |
| patch 内部布局 | `vendor/`、`vendor_dlkm/`、`odm/` 三棵子树 + 内核重定向 | 三棵子树仍在，但挂载后由 `/mnt/vendor` 充当根 |
| fstab 的 vdc 行 | 文件末尾 | 插在**第一个 logical `first_stage_mount` 之前** |
| 内核重定向 target | `/mnt/patch/...` | `/mnt/vendor/...` |
| `mi_ext`/`EFI`/`persist`/`shared` | 挂 tmpfs，父子关系与真机不同 | 预建成 `/mnt/vendor` 的子挂载，与真机一致 |
| EBUSY 保护 | 必需（本目录第 4.1 节） | 保留作第二道防线 |

- **动机**：gold-15 原厂把 patch 分区当「vendor 的替身」（挂到哪儿哪儿就是 vendor），
  engine 把 patch 当「补丁包」（内部按分区名分目录，再由内核重定向拼回）。
  merge-v5 是把语义切换成真机那种，消除重定向表的双层前缀与父子关系失真。
- **实测**（实例 38，patch md5 `f2a80ff1…`）：mountinfo
  `48 47 254:32 / /mnt/vendor ro,relatime - ext4 /dev/block/vdc`，
  `/mnt/vendor/mi_ext` 的 parent 正是 48；`/vendor/lib64/egl` 11 项且无
  `libGLES_meow.so`，与 gold-15 一致；`sys.boot_completed=1 @92.96 s`。
  92.96 s 明显慢于同期 50~80 s，但**未做单变量对照，不能归因**。
- **否决依据**（现行主线是 merge-v4）：
  ```
  $ grep -n "fstabMergeRevision" engine/build/05_build_vendor_boot/fstab.go
  51: fstabMergeRevision = "# fstab-rev: merge-v4"
  $ grep -n "lineVdc" engine/build/05_build_vendor_boot/fstab.go
  47: lineVdc = "/dev/block/vdc /mnt/patch ext4 ro first_stage_mount"
  ```
  内核 `fs/namei.c` 的 rules 表首条也是 `/vendor/etc → /mnt/patch/vendor/etc`；
  `05_build_vendor_boot/README.md` 的备注写明「若将来出现依赖 patch 内容在
  first_stage 早期先就位的挂载点，需要改回『插在首个 logical `first_stage_mount`
  之前』并递增版本」——回退是有意为之（当时判定收益不抵风险）。
- **复活条件**：需要 patch 内容在 first_stage **早期**就位（早于第一个 logical
  `first_stage_mount`），或需要 `mi_ext` 等与 vendor 的真实父子关系。复活步骤：
  递增 `fstabMergeRevision`、vdc 行前移、内核 rules 表改前缀，并**删掉
  `out/kernel` 对应产物重编**（增量构建会复用旧 `boot.img`，strings 里还是旧路径）。
- **原始证据**：`merge-v5-log-fail.txt`（三棵子树布局与 gold-15 的差异）、
  `merge-v5-log-pass.txt`（实例 38 的 mountinfo / 内核 strings / boot 实测）。
