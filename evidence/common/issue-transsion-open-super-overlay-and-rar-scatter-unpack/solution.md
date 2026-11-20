# #286：传音散包 `Open/super.img` 增量叠写、`boot_a.img` 归一及多形态 `.rar` 解包

- **设备/机型**：跨机型 / 引擎级（`01_unpack` + `fsimg`；首发验证：`infinix/Infinix-X6861` `12-SP1A.210812.016-20251012`，同时惠及传音系双 `super.img` 散包如 `tecno/CM5` 及 `.rar` 散装线刷/`UPDATE.APP` 包）
- **状态**：已闭环（2026-11-20）

## 一、问题

在对 `Infinix-Zero-40-5G-X6861-15.zip` 执行 `01_unpack`（`unpack-official`）与后续 `02_prepare` 时，出现两类阻塞及同类潜在缺口：

1. 解出的 `super/vendor_a.img` 与 `super/system_a.img` 全零，`02_prepare` 报 `无法识别 厂商 vendor 的文件系统（既非 ext4 也非 erofs）`；
2. 随后预检发现 MTK `.rar` 散包（如 `Walton-Nexg-N10-Ultra.rar`）内既不含 `.pac`、启动镜像又以 `boot_a.img` / `vendor_boot_a.img` 命名，原 `extractRar` 仅假设 `.rar` 内是 `.pac` 且 `imageTarget` 不认 `_a.img` 根镜像。

## 二、根因

1. **zip 遍历字典序先命中子目录增量 `Open/super.img`**：传音散包（`X6861`、`CM5`）的根目录 `super.img` 含完整 `system*` / `vendor*` / `product*` / `odm*`，而 `Open/super.img` 是同尺寸 sparse 渠道增量包——其中标准分区全部为 `DONT_CARE` 空洞（`0xCAC3`），仅 `0x3000` liblp 元数据与 `tr_product_a` / `tr_preload_a` / `tr_region_a` 为 `RAW/FILL` 有效块；两者甚至不能简单按文件体积二选一（`CM5` 的 `Open/super.img` 6.89 GiB 大于根目录 `super.img` 5.52 GiB）。原 `extractImagesFromZip` 按 zip 目录顺序遍历并命中 `'O'` 排在前的 `Open/super.img` 后即标记 `found["super.img"] = true` 跳过根目录 `super.img`。
2. **`imageTarget` 与 `extractRar` 口径过窄**：`imageTarget` 仅精确匹配 `boot.img` / `vendor_boot.img` 而拒绝 `boot_a.img` / `vendor_boot_a.img`；`extractRar` 直接把 `bsdtar -xOf <rar>` 整输出当 `.pac` 流，遇到散装镜像归档、`UPDATE.APP` 归档或 RAR5 加密头都会失败或漏错报。

## 三、证据

- 失败与结构取证日志：[log-fail.txt](log-fail.txt)
- 单测与冷启动验收日志：[log-pass.txt](log-pass.txt)

## 四、改动

1. [`agent/modules/pipeline/internal/fsimg/liblp_write.go`](../../../../agent/modules/pipeline/internal/fsimg/liblp_write.go)：新增 `ApplySparseOverlay(dstPath, src, log)`，校验 `expectedSize == totalBlocks * blockSize` 后原地将增量 sparse 覆写到已转 raw 的 `dstPath`（`RAW`/`FILL` 写入、`DONT_CARE` `Seek` 跳过以原样保留基座扇区）。
2. [`agent/modules/pipeline/internal/fsimg/gpt_liblp_test.go`](../../../../agent/modules/pipeline/internal/fsimg/gpt_liblp_test.go)：新增 `TestApplySparseOverlay`，验证 `DONT_CARE` 保底、`RAW`/`FILL` 覆写与容量不匹配拒收。
3. [`agent/modules/pipeline/stages/01_unpack/official_detect.go`](../../../../agent/modules/pipeline/stages/01_unpack/official_detect.go)：`imageTarget` 对 `*_a.img` 回退匹配 `rootKeepImages` / `extraKeep` 基名（识别 `boot_a.img` / `vendor_boot_a.img`，仍拒绝 `init_boot_a.img` / `modem.img`）。
4. [`agent/modules/pipeline/stages/01_unpack/official_extract.go`](../../../../agent/modules/pipeline/stages/01_unpack/official_extract.go)：
   - `extractImagesFromZip` 新增 `selectZipTargets` 按目录深度最浅优先挑选各目标文件（同层取大），并识别位于 `Open/` 子目录中的次级 `super.img`；在基座 `super.img` 转 raw 后调用 `fsimg.ApplySparseOverlay` 依次叠写；
   - `extractRar` 改先通过 `bsdtar -tf` 明确报错或列举成员，按顺序分发 `.pac`、`UPDATE.APP`、`update.bin` 或散装/平铺镜像。
5. [`agent/modules/pipeline/stages/01_unpack/official_test.go`](../../../../agent/modules/pipeline/stages/01_unpack/official_test.go)：`TestImageTarget` 补充 `*_a.img` 断言，并新增 `TestExtractZipTranssionOpenSuperOverlay` 验证 `Open/super.img` 排在前的合并还原结果。

## 五、验证

1. 跑 `go test -count=1 ./modules/pipeline/internal/fsimg ./modules/pipeline/stages/01_unpack` 全部通过（`TestApplySparseOverlay`、`TestImageTarget`、`TestExtractZipTranssionOpenSuperOverlay` 均 PASS）。
2. 对 `Infinix-Zero-40-5G-X6861-15.zip` 重跑 `unpack-official -force`：日志确认先抽出 `super.img（8.95 GiB）` 转 raw（`11.57 GiB`），再将 `Open/super.img（1.89 GiB）` sparse 增量叠写，完整拆出 `system_a`、`vendor_a`、`tr_product_a`、`tr_preload_a` 等 13 个逻辑分区。
3. `infinix/Infinix-X6861`（`12-SP1A.210812.016-20251012`，`lineage-22.2` + `kernels/5.10`）13 阶段全量构建一次通过；`verify_boot.py --match Infinix-X6861/12-SP1A.210812.016-20251012 --timeout 280` 干净盘冷启动一次通过：`passed=true`、`bootCompleted=true`、`bootanim="stopped"`、`roHardwareEgl="meow"`、`roHardwareVulkan="mali"`、`serviceCount="322"`、`meanBrightness=127.71`、进入 `com.transsion.XOSLauncher/.uioverrides.QuickstepLauncher` 桌面，VNC PNG (`1088x2400`) 审阅确认无误。

## 六、遗留

- 无。验证后已删除 `out/devices/{roms,devices}/infinix/Infinix-X6861/12-SP1A.210812.016-20251012` 中间产物目录并留存 `out/devices/dist/infinix/Infinix-X6861/12-SP1A.210812.016-20251012/`。
