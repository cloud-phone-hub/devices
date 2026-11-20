# #287：LZ4 `boot.img` 版本串字面量截断与 `UpsertVersion` 误生 `ro.vendor.boot_security_patch: null` 墓碑致 `keymint` `SIGABRT`

- **设备/机型**：跨机型 / 引擎级（`internal/kmod` + `internal/device`；首发验证：`tecno/TECNO-CM5` `16-BP2A.250605.031.A3-20260324`）
- **状态**：已闭环（2026-11-20）

## 一、问题

在适配 Batch C `#7 tecno/TECNO-CM5`（Android 16，`6.12` 内核）时先后暴露两项隐蔽引擎缺陷：

1. `02_prepare` 报 `[warn] 从 boot.img 提取内核版本失败: uname-version 缺少 #version 段: "Linux version 6.12.38-... (A<"`，导致生成的 `versions/.../kernel/config.json` 为空；
2. 补齐内核后再冷启动在 280s 超时，`vold` 每秒循环等待 `android.system.keystore2.IKeystoreService/default`，早期 `qemu.log` 显示 `vendor.keymint-default` 在 `[6.108s]` 抛出 `signal 6`（`SIGABRT`）。

## 二、根因

1. **LZ4 压缩内核段先扫明文命中截断字面量**：`internal/kmod/kernelver.go` 原先按 `明文 > gzip > lz4` 顺序搜索 `"Linux version"`，而 `CM5` 的 `boot.img` kernel 段（`02 21 4c 18` legacy LZ4）内部前部未压缩 literal 块恰好出现 `"Linux version 6.12.38-... (kleaf@build-host) "` 后紧跟控制令牌 `(A<`，提前命中残缺串并略过先解压的正确路径。
2. **`UpsertVersion` 不对称抽离反手写死 `ro.vendor.boot_security_patch: null` 墓碑**：`02_prepare` 对 `refreshVersionKernelFromBoot` 等场景调用 `db.UpsertVersion` 时，[`internal/device/config.go`](../../../../agent/modules/pipeline/internal/device/config.go) 的 `ExtractConfigAndSlimVersion` 把 `candCfg.BuildVendor.build-prop-sets["ro.vendor.boot_security_patch"]` 抽离到 `ver.SecurityPatch`，但 `mergeTemplateOverrides` 对 `baseCfg.BuildVendor`（内含模版写入的该键）与 `candCfg` 做 `diffJSONSection` 前未同步剥离同名键，遂把「抽离掉」误判为「版本显式删除」，落出 `versions/.../vendor/config.json` 中的 `{"ro.vendor.boot_security_patch": null}`；随后 `ResolveEffectiveVersion` 又把版本覆盖合并排在 `injectSecurityPatchToVendor` 之后，反手删光 `/vendor/build.prop` 的 `ro.vendor.boot_security_patch`，令 `RemoteKeyMint` `std::stoi("")` `SIGABRT`。

## 三、证据

- 失败日志与配置污染证据：[log-fail.txt](log-fail.txt)
- 回归单测与冷启动验收日志：[log-pass.txt](log-pass.txt)

## 四、改动

1. [`agent/modules/pipeline/internal/kmod/kernelver.go`](../../../../agent/modules/pipeline/internal/kmod/kernelver.go)：抽出 `plainKernelBytes`，将 `BannerFromKernel` 与 `ReleaseFromKernel` 顺序统一改为 `gunzip > IsLZ4Compressed/DecompressLZ4 > 明文`。
2. [`agent/modules/pipeline/internal/kmod/kernelver_test.go`](../../../../agent/modules/pipeline/internal/kmod/kernelver_test.go)：新增 `TestBannerFromBootDataLZ4LiteralPrefix`。
3. [`agent/modules/pipeline/internal/device/config.go`](../../../../agent/modules/pipeline/internal/device/config.go)：新增 `stripVendorBootSecurityPatch` 供 `mergeTemplateOverrides` 对齐比较口径；将 `ResolveEffectiveVersion` 顺序修正为先 `mergeSectionOverride(cfg.BuildVendor, overrides["buildVendor"])` 再兜底 `injectSecurityPatchToVendor`。
4. [`agent/modules/pipeline/internal/device/db_test.go`](../../../../agent/modules/pipeline/internal/device/db_test.go)：新增 `TestUpsertVersionDoesNotTombstoneBootSecurityPatch` 回归用例，并清理 `tecno/TECNO-CM5/16-BP2A.250605.031.A3-20260324/vendor` 污染覆盖文件。

## 五、验证

1. `go test -count=1 ./modules/pipeline/internal/device ./modules/pipeline/internal/kmod` 全部通过（`TestUpsertVersionDoesNotTombstoneBootSecurityPatch`、`TestBannerFromBootDataLZ4LiteralPrefix` 均 PASS）。
2. `prepare -manifest db://tecno/TECNO-CM5/16-BP2A.250605.031.A3-20260324` 自动将 `kernel/config.json` 回写为 `"uname": "Linux localhost 6.12.38-... #1 SMP PREEMPT ..."` 与 `"version": "6.12"`。
3. 增量重打包并发布 `TECNO-CM5` `patch.img`（`ro.vendor.boot_security_patch=2026-02-01` 确认在位）后，`verify_boot.py --match TECNO-CM5/16-BP2A.250605.031.A3-20260324 --timeout 280` 一次 `passed=true`：`bootanim="stopped"`、`roHardwareEgl="meow"`、`roHardwareVulkan="mali"`、`serviceCount="346"`、`meanBrightness=82.34`、`com.transsion.launcher3/.uioverrides.QuickstepLauncher` 正常点亮，HiOS 16 锁屏 `1088x2436` PNG 审阅无异常。

## 六、遗留

- 无。验证完成后已清理 `out/devices/{roms,devices}/tecno/TECNO-CM5/16-BP2A.250605.031.A3-20260324` 中间目录。
