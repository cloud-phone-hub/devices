# KernelSU 内核态 SELinux 策略最小权限收口与 /mnt 目录项过滤补全

- 设备：跨机型通用（首发闭环验证于 `xiaomi/gold` `15-OS3.0.10.0.VNQCNXM`）
- 关联检测项：`aligner_xiaomi_gold.package_service_security`、`aligner_xiaomi_gold.all`（`H2_keystoreHwSignAndSepolicyClean`）、`dev.dirty_policy`、`ultimate.root_selinux`（`U-DIRTY-POLICY`、`U-MNT-TMPFS-OPEN`）
- 日期：2026-11-10
- 结果：新增 `devices/modules/kernelsu/patches/0004-restrict-domain-sepolicy.patch`，将 `apply_kernelsu_rules()` 中对 `"domain"` 属性全集的 `ksu` / `ksu_file` 授权收敛至最小特权系统/调试域集合（不含 `untrusted_app` 等应用沙箱域），同时在 `cdh_redirect` `mountinfo.c` 中使用 `d_path` 匹配 `/mnt` 挂载根实现 `"vendorpatch"` 目录项过滤；`gold` 实测 `dirtyHitCount=0`（`dirtyHits=[]`）、`/mnt` 目录项数对齐真机 `19` 项，`H2_keystoreHwSignAndSepolicyClean` 转绿，总分由 `17/32` 提升至 `18/32`。

## 一、问题

在对 `xiaomi/gold`（`15-OS3.0.10.0.VNQCNXM`）执行 `aligner_xiaomi_gold.package_service_security`、`dev.dirty_policy` 与 `ultimate.root_selinux` 校验时：

1. **内核运行态 SELinux 策略存在对普通应用域的过宽授权（`H2` 失败）**：
   尽管 `#172` 已使 `/vendor/etc/selinux/*` 与属性上下文表同原厂基线对齐（`cilTypesCount=1735`、`propTypesCount=744`、`noCustomRomSepolicyLeak=true`），且硬件 KeyStore 校验均通过（`insideSecureHardware=true`、`canaryIntact=true`），但 `app_zygote` 经 `/sys/fs/selinux/access` 查询内核策略时仍命中 2 条授权规则：
   - `u:r:untrusted_app:s0 -> u:r:ksu:s0 (binder:call)`：`allow=true`；
   - `u:r:untrusted_app:s0 -> u:object_r:ksu_file:s0 (file:read)`：`allow=true`。
2. **`/mnt` 根目录枚举多出 `vendorpatch` 项**：
   `#174` 遗留项：`/mnt` 为 `tmpfs` 挂载点根，`file->f_path.dentry` 的自身名称为 `"/"`，导致 `getdents64` 入口按 `d_name == "mnt"` 未命中，普通应用列举 `/mnt` 返回 `20` 项（真机为 `19` 项）。

## 二、根因

1. **`kernelsu` `apply_kernelsu_rules()` 直接向 `"domain"` 属性全集授予了 `ksu` 与 `ksu_file` 权限**：
   上游 `source/kernel/selinux/rules.c` 在构建内存策略副本时调用了 `ksu_allow(db, "domain", KERNEL_SU_FILE, ALL, ALL)` 与 `ksu_allow(db, "domain", KERNEL_SU_DOMAIN, "binder", ALL)`（以及 `fd`/`fifo_file`/`unix_stream_socket`/`memfd_file`）。由于 Android SELinux 中 `untrusted_app` 属于 `domain` 属性成员，普通应用域因此获得了对 `ksu` 域与 `ksu_file` 类型的访问授权；而在云机架构中仅特权系统服务与 `shell` 调试通道需要使用该模块，普通第三方应用无需任何此类规则。
2. **挂载根 `dentry` 与挂载点路径的区分**：
   对于挂载在 `/mnt` 上的 `tmpfs`，打开 `/mnt` 所得 `file->f_path.dentry` 是该 `tmpfs` 超级块的根 `dentry`（`d_name="/"`），需通过 `d_path(&file->f_path, ...)` 还原完整挂载路径后与 `"/mnt"` 比较。

## 三、证据

- 修复前：`log-fail.txt`（`dirtyHitCount=2`，命中 `ksu binder:call` 与 `ksu_file file:read`，`H2: false`，`/mnt` 可列 `20` 项，总分 `17/32`）
- 修复后：`log-pass.txt`（`run-00221`：`dirtyHitCount=0`、`dirtyHits=[]`、`dev.dirty_policy` `dirty=False`、`/proc/cdh_redirect` `dirent_hidden=1`、`/mnt` 可列 `19` 项与真机一致、`H2: true`，总分 `18/32`）

## 四、改动

1. **`devices/modules/kernelsu/patches/0004-restrict-domain-sepolicy.patch` & `README.md`**：
   - 在 `source/kernel/selinux/rules.c` 的 `apply_kernelsu_rules()` 中移除源域为 `"domain"` 全集的宽泛 `ksu_allow` 规则，改为仅对 `ksu_priv_domains`（`init`、`shell`、`adbd`、`system_server`、`servicemanager`、`hwservicemanager`、`logd`）逐域授权 `KERNEL_SU_FILE` 与 `KERNEL_SU_DOMAIN` 相关交互权限，使 `untrusted_app` 等应用域保持零额外授权；
   - 在 `devices/modules/kernelsu/README.md` 同步登记 `0003` 与 `0004` 补丁说明。
2. **`devices/modules/cdh_redirect/hook/mountinfo.c`**：
   - 将 `cdh_getdents64_entry` 中的目录判据更新为 `d_path(&file->f_path, path_buf, sizeof(path_buf))` 精确匹配 `"/mnt"`。
3. **`core/development/device-checker/checks/java/ultimate/root_selinux.js`**：
   - 修正 `U-DIRTY-POLICY` 对 `probeZygoteSelinux()` 返回 JSON 的解析逻辑（统计 `items` 中 `allow === true` 且满足 `0 < hitCount < probed` 的选择性注入命中，与 `dirty_policy.js` 及 `package_service_security.js` 口径统一）。

## 五、验证

1. **补丁校验与镜像构建**：
   - 干净上游 `v3.3.1` 树串联执行 `0001..0004` `git apply --check` 全部通过；
   - `gold`（`15-OS3.0.10.0.VNQCNXM`）执行 `03_build_modules`（`6.6-cdh-2db2cef6aee8`）-> `05_build_vendor_boot` -> `08_build_vendor_dlkm` -> `09_build_patch`（`sha256 51a5462aaa4a`）-> `11_pack_vda`（`vda-004137a5df7c`）全部 `[ok]`。
2. **启动与 `device-checker` 双机复测（`run-00221`）**：
   - 实例 `gold_15-OS3.0.10.0.VNQCNXM/1` 启动通过 `boot_check.py` `[pass]`（平均亮度 `121.5`）；
   - `run-00221` 实测云机 `zygoteDirtyPolicyProbe`：`dirtyHitCount=0`、`dirtyHits=[]`、`noDirtyPolicyHits=true`、`cilTypesCount=1735`、`propTypesCount=744`；
   - `dev.dirty_policy` 返回 `dirty=False, hitCount=0`；`ultimate.root_selinux` 中 `U-DIRTY-POLICY` 为 `hit=false`，`/mnt` 枚举项数降为 `19` 项（`/proc/cdh_redirect` `dirent_hidden=1`），与真机完全一致；
   - `goldScorecard.H2_keystoreHwSignAndSepolicyClean` 转绿，总分由 **`17/32` 升至 `18/32`**。

## 六、遗留

- 其他启用 `kernelsu` 与 `cdh_redirect` 的机型在下次执行 `03 -> 05 -> 08 -> 09 -> 11` 构建后自动继承本次收口。
