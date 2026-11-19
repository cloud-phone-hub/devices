# #268：Nothing Phone (3a) (`nothing/Asteroids`) Android 16 清单残留过期 `vendor-cil-rewrite` 规则致 `11_build_patch` 失败

- 编号：`#268`
- 分组：`asteroids`（Nothing Phone (3a)，`nothing/Asteroids`）
- 机型：`nothing/Asteroids` `14-UKQ1.250612.001-20260810`（`Asteroids_B4.1-260810-1153.zip`，`system=16` / `vendor=14 (volcano)`，`lineage-23.2` + `kernel 6.1.138-4k-cdh`）
- 状态：**已闭环**（2026-11-19）

## 1. 问题

在推进 `#16 nothing/Asteroids`（`14-UKQ1.250612.001-20260810`，`Asteroids-android16`）全流程构建时，前序 `01_unpack -> 02_prepare -> apply-profile -> 03_build_kernel -> 04_build_modules -> ... -> 10_pack_vendor_dlkm` 全部通过，但进入 `11_build_patch`（步骤 11.2）时硬失败退出：

```text
[err] 阶段 build_patch 失败: vendor cil 改写规则 #1 命中 0 次（期望 1）: (allow ot_rcp self (netlink_route_socket (nlmsg)))
```

## 2. 根因

1. `09_build_vendor` 新增的 `pruneUnsupportedPlatformPermsCIL`（[sepolicy_cil.go](../../../../agent/modules/pipeline/stages/09_build_vendor/sepolicy_cil.go#L1086)）已在策略装配阶段直接对比 `Asteroids` 官方 `BP2A`（QPR1）`plat_sepolicy.cil` / `system_ext_sepolicy.cil` / `product_sepolicy.cil` 所声明的 `class`/`permission` 集合，自动裁剪了 2 项平台未声明 SELinux 权限（`netlink_route_socket:nlmsg` 与 `system:firmware_load`）并移除了对应 4 条规则（`vendor-log.txt`：`裁剪平台未声明 SELinux 权限 2 项（移除规则 4 条）`）。
2. 而历史生成的 `devices/manifest/configs/nothing/Asteroids/Asteroids-android16/patch/config.json` 仍保留从早期 `m22` 抄入的 4 条 `vendor-cil-rewrite`（3 条 `ot_rcp` 与 1 条 `(allow kernel_34_0 vendor_firmware_file (system (firmware_load)))`），每条要求 `count: 1` 精确命中。当 `09_build_vendor` 已将这 4 条冲突规则剥离后，`11_build_patch` 回查 `vendor_sepolicy.cil` 命中数为 0，触发校验失败。

## 3. 证据

- 失败构建日志：[log-fail.txt](log-fail.txt)（`patch.vendor-cil-rewrite` 4 条规则下 `11_build_patch` 报规则 `#1` 命中 `0` 次）。
- 修复后构建与运行态验收日志：[log-pass.txt](log-pass.txt)（`09_build_vendor` 自动裁剪 2 项权限 / 4 条规则，`11_build_patch` 生成 `384 MiB` `patch.img` 回读 `760/760` 一致；guest `sys.boot_completed=1`、`sys.boot.reason=reboot,factory_reset`、`wm size=1080x2392`、`wm density=388`、`mActiveModeId=1`、`IQtiRadioStable/slot1: found`、`service list` 共 `323` 项、`/data/system/dropbox` 零崩溃）。
- 实机 VNC 截图：[pass-screen.png](pass-screen.png)（`1088x2392` Nothing OS 4.1 桌面完整渲染，`meanBrightness=110.69`）。

## 4. 改动

- 清空过期手动规则（[patch/config.json](../../../../devices/manifest/configs/nothing/Asteroids/Asteroids-android16/patch/config.json)）：
  - 将 `vendor-cil-rewrite` 置为空数组 `[]`，并在 `note` 中注明由 `09_build_vendor` 的 `pruneUnsupportedPlatformPermsCIL` 自动完成跨代/QPR 权限裁剪，不再由 `11_build_patch` 重复强匹配。

## 5. 验证

1. 执行 `TMPDIR=/Volumes/work/cloud-phone-hub/out/devices/tmp /Volumes/work/cloud-phone-hub/out/agent/dist/cdh-pipeline device build Asteroids --manifest 14-UKQ1.250612.001-20260810 --from build_patch`：
   - `11_build_patch` 输出 `patch.img`（`402653184 B`，`sha256 3bc5bbefdea4`，`e2fsck` 通过，760 条目回读一致）；
   - `12_pack_super`、`13_pack_vda` 全绿产出 `13.00 GiB` `vda.img`。
2. 通过 `cdha` 独占冷启动 `Asteroids_14-UKQ1.250612.001-20260810` 并运行 `verify_boot.py` 与 `hvc7` 复核：
   - `passed=true`、`bootCompleted=true`、`bootanim=stopped`；
   - `ro.system.build.version.release=16`、`ro.vendor.build.version.release=14`、`ro.build.display.id=B4.1-260810-1153`；
   - `mCurrentFocus=Window{... com.nothing.launcher/com.android.searchlauncher.SearchLauncher}`，人工调用 `read_file` 审阅 VNC 截图确认全屏渲染完整、无 `RescueParty`、无 dropbox 崩溃。

## 6. 遗留

- 同机 Android 14/15 GRF 版本（`14-UKQ1.241011.001-20251013`）将在 `02_prepare` GRF 系统代际自动对齐落地后统一按实测 `system_a.img` 版本验证，不再沿用旧 `patch/config.json` 静态规则。
