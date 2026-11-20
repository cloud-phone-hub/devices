# `#283`：Android 16+ 大体量 `vendor` 触发 `11_build_patch` `mkfs.ext4` 溢出并实现 `patch.img` 自动扩容

- 编号：`#283`
- 分组：`common`（引擎级：`11_build_patch` 容器镜像容量自适配，影响全部 Android 16+ 摊平 APEX + 大型厂商闭源 GL 库机型）
- 触发机型与复验样本：`tcl/vinca`（`TCL-T517.zip`，`15-AP3A.240905.015.A2-20260512`，TCL 60 SE，Android 16 / `lineage-23.2`）
- 状态：**已闭环**（2026-11-19，`11_build_patch` 全量 Go 单测通过；`tcl/vinca` 自动扩容 `512 MiB` 完成 `patch.img` 打包与 1031 条目回读校验，干净盘冷启动 `passed=true` 并进入 `SetupWizardStartActivity`）

## 1. 问题

构建 `tcl/vinca`（Android 16 / `lineage-23.2` 底座）至 `11_build_patch` 步骤 `11.4 mkfs.ext4 -d` 时失败退出：
`__populate_fs: 无法为 ext2 文件系统分配块 写入文件 "libkeymaster_portable.so" 时`
`mkfs.ext4: 无法为 ext2 文件系统分配块 填充文件系统时`。

## 2. 根因

1. `11_build_patch` 原将未指定 `--size` 时的镜像容量硬编码为 `defaultSizeMiB = 384` MiB（依据早期 `gold` 仅 ~108 MiB 估算）；
2. `lineage-23.2`（Android 16）将图形与认证等 APEX 散件摊平进 `/vendor`，叠加官方 `libGLES_mali.so`（~55 MiB）与自编 HAL 二进制后，`vinca` 仅 `img/vendor.img` 单包就达到 `403,021,824 B`（384.35 MiB），加上 `vendor_dlkm.img`（`15.8 MiB`，且会在 `/vendor/lib/modules` 与 `/vendor_dlkm` 共写两份）与 `odm.img`（`8.4 MiB`），`patch/stage/root` 内容已达 `367 MiB`，叠加 ext4 元数据、日志块及 4 KiB 对齐开销后突破 `384 MiB` 上限。

## 3. 证据

- 失败现场日志：[log-fail.txt](log-fail.txt)（`vinca` `vendor.img=403021824 B`，`patch.work.img` `98304 块 = 384 MiB` 报 `无法为 ext2 文件系统分配块`）。
- 修复后实机通过日志：[log-pass.txt](log-pass.txt)（`patch.img` 自动扩容至 `512 MiB`、`1031/1031` 条目校验一致、`vinca` 冷启动 `passed=true`、`bootanim="stopped"`、`serviceCount="339"`、`meanBrightness=184.87`）。

## 4. 改动

1. [plan.go](../../../../agent/modules/pipeline/stages/11_build_patch/plan.go)（`L59-L65`、`L171-L246`、`L316-L369`、`L440-L467`）：将 `--size` 默认值语义设为 `0`（自动推导）；新增 `resolvePatchSizeMiB`，当未显式指定 `--size` 时，按 `vendorSize + dlkmSize*2 + odmSize + 80 MiB` 向上以 `64 MiB` 对齐并保底 `384 MiB`（小体积镜像保持 `384 MiB` 不破坏既有配方缓存，大体量 `vinca` 自动扩为 `512 MiB`）；显式传入 `> 0` 时按指定值执行。
2. [layout_test.go](../../../../agent/modules/pipeline/stages/11_build_patch/layout_test.go)（`L118-L132`）：新增 `TestResolvePatchSizeMiB` 覆盖显式大小、小镜像保底 `384 MiB`、A16 大体积自动扩至 `512 MiB` 及负数拒绝四类边界。

## 5. 验证

1. `cd /Volumes/work/cloud-phone-hub/agent && TMPDIR=/Volumes/work/cloud-phone-hub/out/devices/tmp GOCACHE=/Volumes/work/cloud-phone-hub/out/agent/gocache /opt/homebrew/bin/go test -count=1 ./modules/pipeline/stages/11_build_patch` 通过（`ok 1.239s`），并重编 `out/agent/dist/cdh-pipeline`；
2. `tcl/vinca` 自 `build_patch` 续跑完成 `patch.img`（`512 MiB`、`1031/1031` 校验一致）、`super.img`（`7.69 GiB`）与 `vda.img`（`13.00 GiB`）装配；
3. `verify_boot.py --match vinca/15-AP3A.240905.015.A2-20260512 --timeout 280` 干净冷启动 `passed=true`（首次扫描 `2.18 GiB preload_a + 1.01 GiB oem_a` 耗时 `~220s`，`serviceCount="339"`、`bootanim="stopped"`、`meanBrightness=184.87`、`SetupWizardStartActivity`），人工审阅 VNC PNG 截图完整显示 `欢迎 / TCL UI 9.0` 向导界面。

## 6. 遗留

- 无。
