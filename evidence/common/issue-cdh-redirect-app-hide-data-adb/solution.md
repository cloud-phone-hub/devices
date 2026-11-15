# #210：通过 `cdh_redirect` 对普通 App 隐藏 `/data/adb`（保留 KernelSU 能力）

## 问题

`gold_15-OS3.0.11.0.VNQCNXM` 首轮 `aligner.security_full` 检测发现，云机（`4503ac70a1c898f3-cloud`）在普通 App 视角下暴露出：

- `rootFiles.adbFs = ["/data/adb"]`
- `availability.rootHitTotal = 1`

云机需保留 KernelSU 供后台通道 / root 运维能力使用，但在普通应用（`untrusted_app`）视角必须呈现为无 root 痕迹的原厂未解锁态，否则 `/data/adb` 会直接命中风控 `adbFs` 扫描表。

## 根因

KernelSU 在内核运行期与 `ksud` 启动期间使用 `/data/adb`（权限 `0700 root:root`）。虽然普通 App 无法 `readdir` 目录内部，但 POSIX `stat("/data/adb")` / `faccessat`（扫描父目录 `/data` 元数据，SELinux 允许或通过 `File.exists()` 检测）仍可直接感知该目录存在。`cdh_redirect` 在 `cdh_rewrite_forward()` 内对 `cdh_should_skip_uid()`（即 `uid > cdh_max_uid`，`max_uid = 10000`）的进程仅隐藏了内核载体路径 `/mnt/vendorpatch`，未屏蔽 `/data/adb`。

## 证据

- 修复前：`4503ac70a1c898f3-cloud` `security.rootFiles.adbFs = ["/data/adb"]`、`rootHitTotal = 1`，见同目录 [`log-fail.txt`](log-fail.txt)。
- 修复后：shell（`uid=2000 <= 10000`）`/data/adb` 与 `/proc/cdh_redirect` 现场（`p0_cdh_redirect_pass.txt`）+ 实例 `2`（`4d5128bd8ca07462-cloud`）`aligner.security_full` 重采输出见同目录 [`log-pass.txt`](log-pass.txt)。

## 改动

1. **`devices/modules/cdh_redirect/core/rules.c`**：
   - 在 `cdh_rewrite_forward()` 的 `if (cdh_should_skip_uid())` 分支（`uid > cdh_max_uid`，即 `uid > 10000` 的普通应用进程）内，将对 `/data/adb` 或 `/data/adb/...` 的路径访问判断与 `/mnt/vendorpatch` 合并，统一就地改写为不存在路径 `"/mnt/.cdh_nonexistent"`，并递增 `cdh_mount_hidden` 计数、返回 `true`（内核路径查找返回 `-ENOENT`）；
   - `root`（`uid=0`）、`system`（`uid=1000`）及 `adb shell`（`uid=2000`）均满足 `uid <= cdh_max_uid`，不进入 `cdh_should_skip_uid()` 分支，因此 KernelSU 与运维通道不受任何影响。
2. **`devices/modules/cdh_redirect/README.md`**：
   - 同步补充普通 App 对 `/data/adb` 重定向屏蔽的说明。

## 验证

1. 重编 `03_build_modules` 并重跑 `08 → 09 → 11` 后，实例 `gold_15-OS3.0.11.0.VNQCNXM/2`（`4d5128bd8ca07462-cloud`）实测：
   - shell（`uid=2000 <= 10000`）执行 `ls -ld /data/adb` 与 `stat /data/adb` 正常见 `drwx------ 2 root root ... /data/adb`；
   - `cat /proc/cdh_redirect` 显示 `max_uid: 10000`、`mount_hidden: 45` 等指标正常运转；
   - App 侧 `aligner.security_full` 重采（扫描 46 条 `adbFs`）：云机 `rootFiles.adbFs = []`、`availability.rootHitTotal = 0`（已完全隐藏；`diff/security.json` 中仅真机 `07cf5fc1784783d5` 因装有 APatch 而命中 `"/data/adb"`，云机端清零）。

## 遗留

1. 若个别系统核心之外的高特权检测服务 UID 落在 `<= 10000` 区间并调用 `stat("/data/adb")`，`cdh_should_skip_uid()` 不会拦截；目前针对普通三方应用（`uid >= 10000` / `> max_uid`）已全部生效。
2. 其余机型待下一轮 `03 → 08 → 09 → 11` 自动带入新编 `cdh_redirect.ko`。
