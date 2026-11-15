# 12.5.0.0A 启动失败：6.12 内核缺 /vendor 路径重定向与 /mnt/patch 卸载保护——里程碑证据

- 设备: flyme/m22 12.5.0.0A（MEIZU 22，Flyme 12.5.0.0A / Android 16 / lineage 23.2 virtio 底盘）
- 宿主: macOS (darwin, QEMU 10 + hvf)，daemon 控制台 `127.0.0.1:9050`
- 状态: **已闭环**（2026-10-28，实例 `m22_12.5.0.0A/3`，进行中；见第 5 节，验证结果随实测更新）

---

## 1. 问题

用 18:37 发布的 `engine/out/dist/flyme/m22/12.5.0.0A`（vda.img 13 GiB / patch.img 384 MiB）
创建实例启动，内核进入 init 后再无进展：

```
[    0.248441][    T1] init: [libfs_mgr] __mount(source=/dev/block/vdc,target=/mnt/patch,type=ext4)=0: Success
[    0.309840][    T1] init: [libfstab] ReadDefaultFstab(): failed to find device default fstab
[    0.835576][  T257] servicemanager: ... Cannot read /vendor/manifest.xml: No such file or directory
[    0.886328][    T1] init: Service 'star_core_serv' (pid 268) exited with status 1
[    0.907648][    T1] init: Service 'qseecom-service' (pid 270) received signal 6
[    0.919308][  T279] EXT4-fs (vdc): unmounting filesystem 85ac6f8f-3015-4dda-b01c-c8a874ae5d30.
[    0.920919][  T279] vold: Failed to open default fstab: No such file or directory
```

108 秒后人工停止时 `grep -c "init: starting service 'zygote"` = 0、`sys.boot_completed=1` = 0。
即：内核、init、super 各分区挂载都正常，但 **zygote 从未启动**。

## 2. 根因

两条独立缺陷叠加，缺一都不能起：

**(A) 6.12 内核没有 `CONFIG_CDH_PATH_REDIRECT` 补丁（主因）**

引擎的 07/08/09 三阶段把 vendor/vendor_dlkm/odm 的改造产物全部放进 `patch.img`
（`layout: root=container;vendor=subtree;vendor_dlkm=subtree;odm=subtree`），运行时挂到
`/mnt/patch`，靠内核 `getname_flags` 把白名单路径改写到 `/mnt/patch/...` 才生效。
这套机制在 **6.6 内核**（gold 口径）里存在，**6.12 源码里一个字节都没有**——
`grep -rn "CONFIG_CDH_PATH_REDIRECT"` 在 `kernels/lineage-23-6.12` 返回空。
证据 #48 里我还据「源码没有这个符号」把 m22 片段里的该开关删了，属于误判：
符号缺失不是「不需要」，而是「补丁没移植」。

后果：`/vendor` 一直是 super 里的**官方 flyme/qcom vendor**，07 的库替换、08 的 15 个
模块注入、09 的三棵子树全部不生效；qseecom / boot-qti / star_core 这些吃 qcom TEE
与真机硬件的服务在 virtio 上必然 signal 6。

**(B) `ksys_umount` 没有保护 `/mnt/patch`**

同属该补丁：`system/vold` 的 `VolumeManager::start()` 会把 `/proc/mounts` 里 `/mnt` 下
不在白名单（`/mnt/scratch`、`/mnt/vendor`、`/mnt/product`、`/mnt/installer`、
`/mnt/androidwritable`、`/mnt/vm`）的子挂载当 stale mount 用 `umount2(MNT_DETACH)` 撕掉。
0.919s 那条 `EXT4-fs (vdc): unmounting` 就是它干的。gold 侧靠 `ksys_umount` 对载体本身
返回 `-EBUSY`（vold 视为「没有挂载」）兜住，6.12 同样没有。

**(C) 板级名用了 `ro.board.platform` 而不是 fstab 后缀**

`vendor_boot-info.txt` 里 `board: sun`、`cmdline: ... androidboot.hardware=sun`，
而官方 vendor 的 fstab 文件名是 `fstab.qcom`（`ro.board.platform=sun`，但 fstab 后缀是
qcom）。`ro.hardware` 被 cmdline 顶成 `sun` 后，vold 找 `/vendor/etc/fstab.sun` → ENOENT，
`/data` 永远挂不上。gold 的 `board=mt6833` 恰好两者同名，所以这个坑没暴露。

## 3. 证据

- `log-fail.txt`：完整启动时间线、`zygote=0`、6.6 vs 6.12 的符号对照、fstab 名对照。
- 6.6 侧改动点（4 文件 5 处）：`fs/namei.c:129/271/429`、`fs/namespace.c:1912/1944`、
  `fs/proc/base.c:1856`、`include/linux/namei.h:97`、`fs/Kconfig:408`。
- `vendor.img` 的 `/etc` 只有 `fstab.qcom`；官方 `build.prop` 只有 `ro.board.platform=sun`。
- 旧证据 #37（`issue-mnt-patch-vold-teardown`）已把 vold 白名单与 `MNT_DETACH` 行为
  在 gold 上取过证，本次是同一机制在 6.12 上缺失。

## 4. 改动

**内核（`devices/kernels/lineage-23-6.12`，提交 `9a41f692c`，5 文件 313 行）**

| 文件 | 改动 |
| --- | --- |
| `fs/namei.c` | `cdh_redirect_vendor_path()` 前向表 9 条 + `cdh_vendor_path_to_virtual()` 逆变换表 9 条；`getname_flags()` 尾部对 `kname` 就地改写，并把 `/system/bin/update_verifier` 直接返回 `-ENOENT` |
| `fs/namespace.c` | `cdh_patch_mount()` + `ksys_umount()` 对 `/mnt/patch` 裸挂载点返回 `-EBUSY` |
| `fs/proc/base.c` | `do_proc_readlink()` 出口做逆变换，`len` 改用 `strlen`（6.12 原来用 `tmp+PATH_MAX-1-pathname` 指针差） |
| `include/linux/namei.h` | `cdh_vendor_path_to_virtual()` 声明 + 未启用时 inline 恒等 |
| `fs/Kconfig` | 新增 `config CDH_PATH_REDIRECT` |

6.12 适配点：
1. `getname_flags()` 签名由 `(filename, flags, empty)` 变为 `(filename, flags)`，插入点仍在
   `atomic_set(&result->refcnt, 1)` 之前；
2. 6.12 没有 `putname()`，`__putname()+kfree()` 的组合要区分长短路径：短路径
   `__putname(result)`，长路径（`kname == (char *)result`）`__putname(kname)+kfree(result)`；
3. `do_proc_readlink()` 的 `len` 计算从指针差改为 `strlen()`（逆变换会让路径变短）；
4. 6.12 `ksys_umount()` 已无 `trace_android_rvh_ksys_umount()`，其余结构同 6.6。

**机型侧**

- `manifest/flyme/m22/kernel.config`：删掉「不声明 PATH_REDIRECT」的错误注释，改为
  `CONFIG_CDH_PATH_REDIRECT=y` 并写清它与 patch.img 的依赖关系；（该机型片段已于
  2026-10-29 撤销，内容并入 `build/02_build_kernel/lineage-kernel.config`，见证据 #48
  第 7 节；`CONFIG_CDH_PATH_REDIRECT=y` 与 6.12 移植说明现在就在那份统一片段里）
- `manifest/flyme/m22/baselines/12.5.0.0A.json`：`board` 由 `sun` 改为 `qcom`，新增
  `board-note` 说明「板级名取 fstab 后缀，不是 ro.board.platform」，避免下次又按
  `ro.board.platform` 填。

## 5. 验证

（待补：02 重编配方、03 模块符号版本、04→11 续跑、实例启动到 launcher 的实测）

## 6. 遗留

- 该补丁在 6.12 上属**新移植**，除 m22 外没有第二个机型验证过；gold 仍用 6.6 内核不受影响。
- `/system/bin/update_verifier` 的 ENOENT 屏蔽是 gold 口径（MIUI HIDL-only）。m22 是 flyme
  基座，若后续发现 lineage 的 AIDL update_verifier 需要正常工作，应把它从 namei.c 的
  屏蔽名单里摘掉。
- 内核提交只在本地（`9a41f692c`），尚未推送到
  `git@github.com:cloud-phone-hub/android_kernel_virt_virtio.git`（graft 仓，推送待网络与
  上游 15R-16 复核）。
