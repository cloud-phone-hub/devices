# #257 跨机型：`ext4` 按需半截目录缓存污染 + `debugfs rdump` 遇既有目录静默中止致 `syncOfficialHandheldPermissions` 抹空 `vendor/etc/permissions`

## 1. 问题

`google/oriole` Android 16（`16-CP1A.260405.005-20260310`）构建完成后冷启动，抓帧瞬间或运行约 `46.88s` 时触发 `init: Received sys.powerctl='reboot,RescueParty' from pid: 774 (system_server)`，VNC 回落至卸载 APEX 时的 `fbcon` 控制台文本画面（亮度 `21.61`）。

实例内诊断发现：`pm list features | grep wifi` **为空**、`service list` 中**无 `wifi` 服务**、`/vendor/etc/permissions/` 里**仅剩 1 个文件 `handheld_core_hardware.xml`**（原本来自 `lineage-23.2` 基底的 `android.hardware.wifi.prebuilt.xml` 与步骤 9.4 写入的 `android.hardware.telephony.gsm.xml` 全部消失），导致 `com.google.android.connectivitythermalpowermanager`、`com.google.android.apps.scone`、`com.google.android.gms.persistent` 等进程因 `WifiManager == null` 抛出 `NullPointerException` 崩溃循环。

## 2. 根因

`google/oriole` 16 官方 `vendor_a.img` 是 **`ext4` 文件系统**（按需路径抽取，不整树解包），在 `09_build_vendor` 的权限同步链路上存在四环级联缺陷：

1. **环 1（`assemble.go:syncOfficialHandheldPermissions`）**：先调 `extractSource(src, plan, "etc/permissions/handheld_core_hardware.xml")` 探测官方是否存在该文件，使 `unpack_vendor/etc/permissions/` 被顺带建出且内部仅含 1 个文件；
2. **环 2（`officialunpack.go:unpackOfficialPath`）**：紧接着调 `extractSource(src, plan, "etc/permissions")` 想整目录抽取时，`unpackOfficialPath` 仅凭 `os.Lstat(dst)` 命中已存在的半截目录 `unpack_vendor/etc/permissions` 就短路复用返回；
3. **环 3（`files.go:officialFileSource`）**：`officialFileSource` 只要看到 `unpack_vendor` 目录存在（哪怕是 `ext4` 按需抽取单文件建出的残缺树），就把它塞进 `src.Trees`，令 `extractSource` 直接从残缺树返回；
4. **环 4（`internal/fsimg/vendorimg.go:extractExt4` 与 `assemble.go:replaceVendorDir`）**：即使进入 `debugfs -R "rdump /etc/permissions <base>"`，实测当宿主机目标同名目录已存在（哪怕只含 1 个文件）时，`e2fsprogs` 的 `debugfs rdump` 遇 `mkdir EEXIST` 会静默中止（实测 `1` 文件 vs 清空父目录后 `62` 文件）；随后 `syncOfficialHandheldPermissions` 执行于 `步骤 9.4` 之后并调用 `replaceVendorDir`（`os.RemoveAll(dst)`），直接把刚写进去的模块权限文件（如 `android.hardware.telephony.gsm.xml`）和基底文件一并删空。

## 3. 证据

- 失败日志见同目录 [log-fail.txt](log-fail.txt)：
  - `vendor-log.txt`：`[replace] vendor/etc/permissions <- 厂商 vendor/etc/permissions（检测到官方 handheld_core_hardware.xml，替换基底 tablet 权限目录，1 个文件）`；
  - 实例内 `/vendor/etc/permissions` 仅存 `handheld_core_hardware.xml`；
  - `dropbox` / `logcat`：`com.google.android.connectivitythermalpowermanager`、`com.google.android.apps.scone`、`com.google.android.gms.persistent` 均因 `WifiManager == null` NPE 循环崩溃，于 `46.88s` 触发 `sys.powerctl='reboot,RescueParty'`；
- 宿主实测复现：`debugfs -R "rdump /etc/permissions <preexisting_dir>"` 输出 `1` 个文件；先删既有同名子目录再 `rdump` 输出 `62` 个文件。

## 4. 改动

1. **`agent/modules/pipeline/internal/fsimg/vendorimg.go`（与同目录 `vendorimg_test.go`）**：
   - `extractExt4` 的 `isDir` 分支在调用 `debugfs rdump` 前，若宿主机 `dst` 已存在则先 `os.RemoveAll(dst)` 清除残缺目录；
   - 新增单测 `TestExtractExt4DirOverPreexistingPartialDir`。
2. **`agent/modules/pipeline/stages/09_build_vendor/officialunpack.go`（与 `officialunpack_test.go`）**：
   - 新增目录级完整抽取戳记 `officialDirUnpackStampName = ".cdh-dir-unpacked"`；
   - `unpackOfficialPath` 命中已有目录时，仅当整树 `erofs` 戳记新鲜或存在 `.cdh-dir-unpacked` 才复用，否则清掉半截目录重抽并在目录抽取完成后落戳；`countOfficialUnpack` 统计排除该戳记；
   - 新增单测 `TestExt4SingleFileUnpackDoesNotPoisonSyncHandheldPermissions`。
3. **`agent/modules/pipeline/stages/09_build_vendor/files.go`**：
   - `officialFileSource` 改为仅当 `officialUnpackFresh(dir, plan.OfficialVendorImg, "erofs")` 为真时才将 `unpack_vendor` 纳入 `src.Trees`，禁止未整树解包的 `ext4` 半截目录污染目录取件。
4. **`agent/modules/pipeline/stages/09_build_vendor/assemble.go`（与 `assemble_test.go`）**：
   - 将 `officialUnpackStampName` 与 `officialDirUnpackStampName` 纳入 `treeJunk`，禁止状态文件进入 `vendor/` 装配树与镜像；
   - 新增 `ensureEnabledModulePermissions` 并挂在 `prune-unsupported-permissions` 之后：对已启用 HAL 模块 `VendorTree` 中的 `etc/permissions/*.xml`（如 `android.hardware.telephony.gsm.xml`）缺则回补，且在启用 `android.hardware.wifi` 模块而缺少 `android.hardware.wifi*.xml`（不含 direct/aware/passpoint/rtt）时自动补齐标准 `android.hardware.wifi.xml`；
   - 新增单测 `TestEnsureEnabledModulePermissions`。

## 5. 验证

1. Go 单测与二进制重编（见 [log-pass.txt](log-pass.txt)）：
   - `go test -v -count=1 ./modules/pipeline/internal/fsimg/... ./modules/pipeline/stages/09_build_vendor/...` 全通过（`fsimg 1.285s`、`09_build_vendor 1.314s`，3 个新回归用例均 PASS）；
   - 重编 `out/agent/dist/cdh-pipeline`。
2. `google/oriole` `16-CP1A` 移除旧 `vendor.img` 缓存后执行 `--from build_vendor --refresh`：
   - `vendor-log.txt` 打印 `[replace] vendor/etc/permissions <- 厂商 vendor/etc/permissions（检测到官方 handheld_core_hardware.xml，替换基底 tablet 权限目录，62 个文件）`，且装配树中无任何 `.cdh-*` 戳记残留。
3. 联合 [#256](../../oriole/issue-oriole15-17-vendor-sepolicy-rild-rescueparty/solution.md) 补齐 A16 电话栈新域后冷启验证（见 [pass-screen.png](pass-screen.png) 与 [log-pass.txt](log-pass.txt)）：
   - 截图 `oriole_16_CP1A.png` 亮度 `86.75`（`peak=255`），完整呈现 Pixel Android 16 桌面；
   - `proc_uptime=91.69s` 稳态无重启，`Service wifi: found`、`Service phone: found`、`/vendor/etc/permissions` 实存 `62` 个文件、`qemu.log RescueParty=0`、`0x8d56=0`。

## 6. 遗留

- 此前在 `ext4` 官方镜像上受 `vendor.files` 整目录覆盖隐性波及的所有存量构筑目录，重跑 `09_build_vendor` 时会自动清掉无戳记的半截 `unpack_vendor` 子目录；对于已缓存 `vendor-recipe.txt` 但尚未跑过的旧目录，若需刷新须删 `vendor.img` 或带不命中缓存的条件重打。
