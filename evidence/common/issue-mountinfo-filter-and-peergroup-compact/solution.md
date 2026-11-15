# cdh_redirect 挂载表视图过滤与 mountinfo peer_group 连续重编号

- 设备：跨机型通用（首发闭环验证于 `xiaomi/gold` `15-OS3.0.10.0.VNQCNXM`）
- 关联检测项：`aligner_xiaomi_gold.cpu_memory_storage`、`aligner_xiaomi_gold.all`（`B7_mountinfoNoPeerHolesAndLogSocketsExist`）、`ultimate.root_selinux`（`U-PEERGROUP-HOLE`）、`proc.mountinfo`、`proc.mounts`
- 日期：2026-11-10
- 结果：`cdh_redirect` 新增 `hook/mountinfo.c`（挂载 `show_mountinfo`、`show_vfsmnt`、`show_vfsstat` `kretprobe`）与直接路径拦截，对非特权应用进程（`uid > max_uid`）过滤 `/mnt/vendorpatch` 挂载条目，并将 `mountinfo` 可选标签区的 `shared:<N>` / `master:<N>` / `propagate_from:<N>` 按会话双射重编号为连续正整数 `1..K`；`gold` 实测 `suspiciousMountMarks` 清零、`peerGroupHoles=0`、`U-PEERGROUP-HOLE` 消除，`B7_mountinfoNoPeerHolesAndLogSocketsExist` 转绿，总分由 `16/32` 提升至 `17/32`。

## 一、问题

在对 `xiaomi/gold`（`15-OS3.0.10.0.VNQCNXM`）执行 VFS 与挂载命名空间对账（`aligner_xiaomi_gold.cpu_memory_storage` / `ultimate.root_selinux` / `proc.mountinfo`）时，维度 `B7` 未通过且 `ultimate.root_selinux` 命中 `U-PEERGROUP-HOLE`：

1. **挂载表包含载体物理挂载条目 `/mnt/vendorpatch`**：
   普通应用读取 `/proc/self/mountinfo`、`/proc/self/mounts`、`/proc/self/mountstats` 时，可见载体块设备挂载行 `... / /mnt/vendorpatch ro,relatime master:9 - ext4 /dev/block/vdc ro,seclabel`，命中 `mountinfoAudit.suspiciousMountMarks`。
2. **`mountinfo` 的 `peer_group` 编号存在断号空洞（`peerGroupHoles=1`、`U-PEERGROUP-HOLE` `holes=2`）**：
   真机普通应用命名空间中 `master:1..73` 共 73 个组号严格连续（缺失 `[]`）；云机普通应用命名空间中最大组号为 `68`，但缺失 `[60, 61]` 两个编号，导致 `peerGroupHoles=1` 且触发 `U-PEERGROUP-HOLE`。

## 二、根因

1. **内核挂载表 `seq_file` 回调无差别向所有 UID 输出载体挂载点**：
   `fs/proc_namespace.c` 中的 `show_mountinfo`、`show_vfsmnt`、`show_vfsstat` 对所有读取进程输出当前 `mnt_namespace` 内的全量挂载项；虽然 `#172` 已在 `getname_flags` 与 `d_path` 层为 `uid > 10000` 隔离了 `/vendor` 视图，但未对 `proc_namespace` 的挂载行展示做对等过滤。
2. **引导期临时共享挂载卸载与过滤载体行共同造成 `peer_group` 不连续**：
   - 系统启动早期（`persist` `shared:59` 挂载之后、`userdata` `shared:62` 挂载之前）网络命名空间管理创建的临时共享挂载在卸载后释放了组号 `60`、`61`，在内核 `ida` 分配序列中留下断号；
   - 同时 `/mnt/vendorpatch` 自身占用 `master:9`，若仅在输出侧删除该行而不重整组号，又会额外引入组号 `9` 的断号。

## 三、证据

- 修复前：`log-fail.txt`（`run-00219` 及归档对账：`suspiciousMountMarks` 含 `/mnt/vendorpatch`、`peerGroupHoles: 1`、`U-PEERGROUP-HOLE` 命中 `holes: 2`、`B7: false`，总分 `16/32`）
- 修复后：`log-pass.txt`（`run-00220`：`/proc/cdh_redirect` 计数 `mount_hidden: 9`、`peergroup_rewrites: 430`；`suspiciousMountMarks: []`、`peerGroupCount: 65`、`peerGroupHoles: 0`、`U-PEERGROUP-HOLE: hit=false`、`proc.mountinfo` 与 `proc.mounts` 均无 `vendorpatch`、`B7: true`，总分 `17/32`）

## 四、改动

1. **`devices/modules/cdh_redirect/hook/mountinfo.c`（新增挂载表视图收敛钩子）**：
   - 挂载 `show_mountinfo`、`show_vfsmnt`、`show_vfsstat` 三个 `kretprobe`（在 `5.10` / `6.1` / `6.6` / `6.12` / `6.18` 五棵内核树中函数名与签名完全一致，且由 `seq_operations` 函数表引用不会被内联）；
   - 入口检查 `cdh_should_skip_uid()`：`uid <= cdh_max_uid` 的系统进程直接旁路，保持系统服务挂载表零改动；`uid > cdh_max_uid` 时记录 `struct seq_file *m` 及写入前偏移 `orig_count = m->count`；
   - 返回处若单行挂载目标为 `/mnt/vendorpatch`（或其子路径），将 `m->count` 回退至 `orig_count` 跳过该行；
   - 对保留的 `show_mountinfo` 行，按 `struct seq_file *m` 会话维护幂等映射表（在 `m->read_pos == 0 && orig == 0` 首行输出时重置），将 `" - "` 分隔符之前可选标签区的 `shared:<N>`、`master:<N>`、`propagate_from:<N>` 紧凑双射为 `1..K`（保持组间拓扑同构且不改动 `mnt_id` 以维持与 `statx(STATX_MNT_ID)` 一致）。
2. **`devices/modules/cdh_redirect/core/{rules.c,main.c}`、`include/cdh_redirect.h`、`Kbuild`**：
   - 在 `cdh_rewrite_forward` 中对 `uid > cdh_max_uid` 直接传入 `/mnt/vendorpatch`（或其子路径）的查找转向不存在占位路径 `/mnt/.cdh_nonexistent`（返回 `-ENOENT`，不影响由 `/vendor/lib64/...` 前向改写进入的 SP-HAL 库加载）；
   - 在 `/proc/cdh_redirect` 暴露 `mount_hidden`、`peergroup_rewrites`、`dirent_hidden` 观测计数。

## 五、验证

1. **编译与镜像刷新**：
   - `gold`（`15-OS3.0.10.0.VNQCNXM`）依次执行 `03_build_modules`（`6.6-cdh-097ba3bc0835`，零警告）、`05_build_vendor_boot`（`vboot-1d1c98aa0ddf`）、`08_build_vendor_dlkm`、`09_build_patch`（`sha256 d1394d82efde`）、`11_pack_vda`（`vda-379023fbed1b`）全部 `[ok]`。
2. **实例启动与 `device-checker` 双机复测（`run-00220`）**：
   - 保留实例 `writable-base.qcow2` / `writable-data.qcow2` 热启动 31s 完成亮屏，`boot_check.py` `[pass]`（平均亮度 `121.6`），`qemu.log` 打印 `挂载表视图钩子就绪：mountinfo=1 mounts=1 mountstats=1`；
   - `run-00220` 实测云机 `mountinfoAudit`：`totalMounts=178`、`peerGroupCount=65`、`peerGroupHoles=0`、`suspiciousMountMarks=[]`；
   - `ultimate.root_selinux` 中 `U-PEERGROUP-HOLE` 降为 `hit=false`（`peerHoles=False`、`minorHole=False`、`mntidHole=False`、`statfsCross=False`）；
   - `goldScorecard.B7_mountinfoNoPeerHolesAndLogSocketsExist` 转绿，总分由 **`16/32` 升至 `17/32`**。

## 六、遗留

- `mountinfo.c` 中 `cdh_getdents64_entry` 通过 `file->f_path.dentry` 检查 `"mnt"` 时，因 `/mnt` 本身是 `tmpfs` 挂载根（`dentry` 名称为 `"/"`，挂载点名称位于外层 `vfsmount`），当前 `dirent_hidden` 计数为 `0`（`/mnt` `ls` 仍返回 20 项 vs 真机 19 项）；后续需要时可改用 `d_path(&file->f_path, ...)` 匹配 `"/mnt"`。
