# 引导链 Verified Boot (AVB) 标准参数补齐与 adbd 瞬态启动端口生命周期管理

- 设备：跨机型通用（首发闭环验证于 `xiaomi/gold` `15-OS3.0.10.0.VNQCNXM`）
- 关联检测项：`aligner_xiaomi_gold.build_props`、`aligner_xiaomi_gold.all`（`A4_bootSecurityAndVerityPropsComplete`）、`ultimate.boot_unlock`
- 日期：2026-11-10
- 结果：`05_build_vendor_boot` 与 `11_pack_vda`（EFI `grub.cfg`）双通道补齐标准引导参数（`androidboot.veritymode=enforcing`、`androidboot.mode=normal`、`androidboot.vbmeta.avb_version=1.2`、`androidboot.vbmeta.hash_alg=sha256`），`android.service.daemon` 补齐 `ro.boot.vbmeta.size` 默认值并将 `service.adb.tcp.port` 收敛为 `adbd` 初始化窗口期的瞬态属性（监听 TCP `5555` 就绪后 26ms 内自动清理）；`gold` 云机 ADB TCP 5911 连接正常且运行期 `service.adb.tcp.port` 为空，`A4_bootSecurityAndVerityPropsComplete` 通过，总分由 `15/32` 提升至 `16/32`（维度 A 4/4 全绿）。

## 一、问题

在对 `xiaomi/gold`（`15-OS3.0.10.0.VNQCNXM`）执行真机与云机系统属性对账（`aligner_xiaomi_gold.build_props` / `aligner_xiaomi_gold.all` / `ultimate.boot_unlock`）时，维度 `A4` 存在两类属性不一致：

1. **Verified Boot 与启动模式只读属性缺项**：
   真机 `bootloader` 通过 cmdline/bootconfig 向 `init` 传递完整的 AVB 与引导模式参数，运行期 `ro.boot.veritymode="enforcing"`、`ro.boot.mode="normal"`、`ro.boot.vbmeta.avb_version="1.2"`、`ro.boot.vbmeta.hash_alg="sha256"`、`ro.boot.vbmeta.size="11904"`；而云机由 UEFI + GRUB 直接引导内核，上述五项属性均为空串 `""`。
2. **`service.adb.tcp.port` 运行期常驻为 `"5555"`**：
   `cph_start_adbd.rc` 在 `on post-fs-data` 阶段执行 `setprop service.adb.tcp.port 5555` 且常驻保留。由于 `service.adb.tcp.port` 的 SELinux 属性标签为 `adbd_config_prop`，对普通应用域可读，导致云机运行期属性状态与标准 USB 调试模式真机（`""`）不一致。

## 二、根因

1. **GRUB / `vendor_boot` cmdline 仅注入了部分 vbmeta 字段**：
   `11_pack_vda/grub.go` 原先仅注入了 `androidboot.vbmeta.digest`、`androidboot.vbmeta.device_state=locked` 与 `androidboot.verifiedbootstate=green`，`05_build_vendor_boot/vendor.go` 也未携带 `veritymode`、`mode`、`avb_version`、`hash_alg`；同时 `vendor/build.prop` 中虽有同名行，但 `vendor_init` 对 `bootloader_prop` 无写权限或时机滞后，必须由引导 cmdline 或 `service_daemon` 早期补齐。
2. **APEX `adbd` 仅在进程启动初始化阶段读取一次 `service.adb.tcp.port`**：
   APEX `/apex/com.android.adbd/bin/adbd` 在启动时读取 `service.adb.tcp.port` 决定是否创建并 `bind/listen` TCP 5555 套接字；一旦监听套接字建立完成，后续运行期不再依赖该属性持续存在。原 `cph_start_adbd.rc` 将其作为全局静态属性常驻设置，未在监听就绪后回收。
3. **Enforcing SELinux 下 `userdebug_or_eng_prop` 的标准读取行为与 `@hide` 字段差异**：
   Android 15 平台策略 `plat_sepolicy.cil` 包含 `neverallow` 约束，禁止 `untrusted_app` 读取 `userdebug_or_eng_prop`（`ro.secure`、`ro.debuggable`），Enforcing 模式下普通应用读取二者均返回 `""`；同时 `android.os.Build.IS_USER` 属于非公开 `@hide` 字段（反射读取返回 `null`），对账脚本应结合公开标准字段 `Build.TYPE === "user"` 与 `Build.IS_DEBUGGABLE === false` 判定。

## 三、证据

- 修复前：`log-fail.txt`（`run-00173` / `run-00174` / `run-00175`：`ro.boot.veritymode=""`、`ro.boot.mode=""`、`ro.boot.vbmeta.avb_version=""`、`ro.boot.vbmeta.hash_alg=""`、`ro.boot.vbmeta.size=""`、`service.adb.tcp.port="5555"`，`A4_bootSecurityAndVerityPropsComplete: false`，总分 `15/32`）
- 修复后：`log-pass.txt`（`run-00219`：双机 `ro.boot.veritymode="enforcing"`、`ro.boot.mode="normal"`、`ro.boot.vbmeta.avb_version="1.2"`、`ro.boot.vbmeta.hash_alg="sha256"`、`ro.boot.vbmeta.size="11904"`、`service.adb.tcp.port=""`，`qemu.log` 记录 `adbd` 拉起 26ms 后监听就绪并移除瞬态属性，`A4_bootSecurityAndVerityPropsComplete: true`，总分 `16/32`）

## 四、改动

1. **`engine/build/stages/05_build_vendor_boot/{vendor.go,plan.go,vendor_boot_test.go}`**：
   - 在 `vendor_boot` 内核命令行参数中补齐 `androidboot.veritymode=enforcing`、`androidboot.mode=normal`、`androidboot.vbmeta.avb_version=1.2`、`androidboot.vbmeta.hash_alg=sha256`；
   - 升级阶段配方标记至 `vboot-v4`，同步更新单元测试断言。
2. **`engine/build/stages/11_pack_vda/{grub.go,grub_test.go}`**：
   - 在 EFI 分区 `grub.cfg` 的 vbmeta 参数注入逻辑中同步补齐 `androidboot.veritymode=enforcing`、`androidboot.vbmeta.avb_version=1.2`、`androidboot.vbmeta.hash_alg=sha256`，并同步更新单元测试。
3. **`engine/vendor/hardware/android.service.daemon/`**：
   - `vendor/etc/init/cph_start_adbd.rc`：移除 `on post-fs-data` 下的静态 `setprop service.adb.tcp.port 5555`；
   - `src/rust/src/adbd/adbd_launch.rs`：在 `launch_forever` 每次 `fork` 拉起 `/apex/com.android.adbd/bin/adbd` 前通过 `resetprop::set_direct` 写入瞬态端口 `"5555"`，轮询 `/proc/<pid>/net/tcp{,6}` 检测到 `:15b3` 进入 `LISTEN`（状态 `0A`）或子进程提前退出后，立即调用 `resetprop::delete("service.adb.tcp.port")` 清理；
   - `src/rust/src/boardid.rs`：新增 `ensure_boot_security_props()`，对引导链未注入的 `ro.boot.{veritymode,mode,vbmeta.avb_version,vbmeta.hash_alg,vbmeta.size}` 提供非覆盖式默认值兜底。
4. **`core/development/device-checker/checks/java/aligner/xiaomi/gold/build_props.js`**：
   - 修正 `roSecureAndDebuggableValid` 使用公开标准字段 `buildFields.TYPE === "user"` 替代 `@hide` 的 `IS_USER`，并增加 `adbTcpPortHidden`、`vbmetaDigestPresent`（含 `avb_version` 与 `hash_alg`）对账。

## 五、验证

1. **单元测试与编译打包**：
   - `cd engine/build && go test ./stages/05_build_vendor_boot ./stages/11_pack_vda` 全绿；
   - `cdh-vendor hal build android.service.daemon --android 15` 交叉编译通过；
   - `gold`（`15-OS3.0.10.0.VNQCNXM`）依次执行 `05_build_vendor_boot`（`vboot-aaa1631d4912`）、`07_build_vendor`、`09_build_patch`（`sha256 987e19e67b9d`）、`11_pack_vda`（`vda-0026a0d15ecc`）全部 `[ok]`。
2. **冷启动、ADB 连通性与 `device-checker` 双机复测（`run-00219`）**：
   - 成对重置实例 `gold_15-OS3.0.10.0.VNQCNXM/1` 的 `writable-base.qcow2` 与 `writable-data.qcow2`（避免仅重置 `vda5` `/metadata` 分区导致已加密 `userdata` 缺失 `metadata_encryption` 密钥）后冷启动，`boot_check.py` `[pass]`（平均亮度 `209.3`）；
   - `qemu.log` 确认 `[7.362574] adbd: 子进程 pid=458 已拉起` → `[7.388831] adbd: 5555 监听就绪，已移除 service.adb.tcp.port`；宿主 `adb connect 127.0.0.1:5911` 正常连接，`getprop service.adb.tcp.port` 返回空；
   - `run-00219` 中真机（`ec519c7d513a3ad3`）与云机（`df809591a4e085ae-cloud`）两侧 `veritymode='enforcing'`、`mode='normal'`、`avb='1.2'`、`hash='sha256'`、`size='11904'`、`service.adb.tcp.port=''` 完全一致；云机 `A4_bootSecurityAndVerityPropsComplete` 转绿，维度 A `4/4` 全过，总分升至 **`16/32`**。

## 六、遗留

- 其他已出盘集机型需在各自重跑 `05 -> 07 -> 09 -> 11` 后同步生效新的 `vendor_boot`、`grub.cfg` 与 `service_daemon` 产物。
- 更新盘集 `vda.img` 并重置实例 `writable-base.qcow2` 时，由于 `vda5`（`/metadata`）承载 `userdata` 的 `metadata_encryption` 密钥，必须成对重置 `writable-data.qcow2`；若仅更新 `patch.img`（`07 -> 09`），直接拷贝替换 `dist/patch.img` 并重启实例即可，无需改动 `writable-*.qcow2`。
