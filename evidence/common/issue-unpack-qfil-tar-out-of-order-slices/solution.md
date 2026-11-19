# #261 `01_unpack` QFIL `.tar.gz` 固件包中 `super_<N>.img` 排在 `super_1.img` 之前被静默丢弃

- **编号**：#261
- **设备 / 机型**：`common/`（首发于 `vivo/PD2454` `15-AP3A.240905.015.A2-20250416` QFIL 分片线刷包）
- **固件包**：`roms/devices/vivo/（X200 Ultra）PD2454_MA_15.0.10.24.W10.V000L1.zip`
- **状态**：已闭环（2026-11-19）

## 1. 问题

对 vivo X200 Ultra (`vivo/PD2454`) 执行 `01_unpack` 后，阶段正常退出但解包树 `super/` 中仅产出 `odm_a.img`、`oem_a.img`、`system_dlkm_a.img`、`system_ext_a.img`、`vendor_a.img`、`vgc_a.img` 6 个逻辑分区（共计 `4.95 GiB`），最核心的 **`system_a.img`（7.47 GiB）、`product_a.img`、`vendor_dlkm_a.img` 全部缺失**。

## 2. 根因

`cdh-pipeline unpack-official --rom ... --list` 检查原始 tar 流成员顺序，发现打包目录遍历顺序无序：

- `super_10.img`、`super_4.img`（`product_a`）、`super_9.img`（`vendor_dlkm_a`）、`super_5.img`（7475.01 MiB，`system_a`）全部排在 **`super_1.img`**（Android LP 元数据表 `0x414c5030`）之前；
- `super_6.img`、`super_8.img`、`super_7.img`、`super_2.img`、`super_3.img` 排在 `super_1.img` 之后。

[`agent/modules/pipeline/stages/01_unpack/official_extract.go`](../../../../agent/modules/pipeline/stages/01_unpack/official_extract.go) 中的 `extractTarStream()` 在遍历顺序流时，仅在读到 `super_1.img` 后调用 `parseLPSuperSliceAll` 构建 `sliceToTarget` 映射；而排在 `super_1.img` 之前读到的 `super_<N>.img`（`N >= 2`）此时因 `len(sliceToTarget) == 0` 跳过 LP 分片分支，后续也不命中 `flatTarget` 或 `imageTarget`，导致全部排在前的分区被静默丢弃。

## 3. 证据

- **失败现场**（`log-fail.txt`）：仅产出 6 个逻辑分区（`4.95 GiB`），`super_4.img` / `super_5.img` / `super_9.img` / `super_10.img` 全程未出现在抽取输出中；
- **成员顺序实测**：`unpack-official --list` 显示 `super_10.img` -> `super_4.img` -> `super_9.img` -> `super_5.img` -> `super_1.img` -> `super_6.img` -> `super_8.img` -> `super_7.img` -> `super_2.img` -> `super_3.img`。

## 4. 改动

1. **[`agent/modules/pipeline/stages/01_unpack/official_extract.go`](../../../../agent/modules/pipeline/stages/01_unpack/official_extract.go)**：
   - 新增 `isLPSuperSliceMember`（匹配 `super_<N>.img`, `N >= 2`）与公共落盘/unsparse 方法 `applyLPSuperSlice`；
   - 在 `extractTarStream` 中，当 `len(sliceToTarget) == 0` 时，将早于 `super_1.img` 出现的 `super_<N>.img`（支持 `.lz4` 后缀）流式写入同目录非 `.img` 暂存路径 `super/.lp_pending_<baseLower>.part` 并登记到 `pendingSlices`（配 `defer` 异常清理保障不残留）；
   - 当解析到 `super_1.img` 生成 `sliceToTarget` 时，立即遍历 `pendingSlices` 经 `applyLPSuperSlice` 执行 `os.Rename` 零拷贝归位与 `unsparse`，不在映射内或已抽取的暂存分片立即删除。
2. **[`agent/modules/pipeline/stages/01_unpack/official_test.go`](../../../../agent/modules/pipeline/stages/01_unpack/official_test.go)**：
   - 新增 `buildLPMetadataBytes` 与 `TestExtractTarLPSuperSlicesBeforeMetadata`，故意把 `super_2.img`（sparse `system_a`）与 `super_3.img`（`product_a`）排在 `super_1.img` 之前、`super_4.img` 排在之后，验证全部 3 分区落盘内容准确且无 `.lp_pending_*` 残留。

## 5. 验证

1. `cd agent && TMPDIR=/Volumes/work/cloud-phone-hub/out/agent/tmp go test ./modules/pipeline/stages/01_unpack/...` -> 全部通过（含 `TestExtractTarLPSuperSlicesBeforeMetadata`）；
2. 重编 `cdh-pipeline` 后清理不完整解包目录，对 `vivo/PD2454` 重跑 `device build PD2454 -manifest 15-AP3A.240905.015.A2-20250416 --only unpack --refresh`（详见 `log-pass.txt`）：
   - `super_4.img -> product_a.img`、`super_9.img -> vendor_dlkm_a.img`、`super_5.img -> system_a.img`（7.30 GiB）全部先暂存后成功结算；
   - 最终还原 **9 个逻辑分区 + 1 个物理分区（12.60 GiB）+ `vendor_boot.img`（96.00 MiB）**，`super/` 含完整 9 分区与 `.super-layout.json`，无任何 `.lp_pending_*` 残留。

## 6. 遗留

- 无。对 `.zip` QFIL 包尚未观测到乱序分片缺失（`.zip` 可按索引随机读取，未出现 tar 流单向顺序问题）。
