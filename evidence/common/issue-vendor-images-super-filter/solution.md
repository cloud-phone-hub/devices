# `rom.vendor.images` 收窄到「清单点名」：三条包型的 `super/` 都跟着清单走

- 设备/机型：全机型（common）；实跑取样 `google/oriole`（Pixel 6，平铺工厂包）、`nothing/3a`（Nothing Phone 3a，OTA payload）
- 日期：2026-11-02
- 关联：m22 payload 筛选（`01_unpack` README「分区筛选」一节）、`#116`（vendor.files 白名单口径）

## 一、问题

`devices/manifest/*/*/baselines/*.json` 里 17 份基线只有 4 份写了 `rom.vendor.images`（m22 7+1、gold
`super+vendor_boot+cust`、ss9380 / popsicle `super.img`），其余 13 份是空数组 `[]`。空数组在
`setKeeps` 里被归成 nil，于是 01_unpack 对它们全部走默认集 `super.img` + `vendor_boot.img`：

- payload 机型（nothing/3a、oneplus 15r/ace3、oppo cph2797/pme110、realme rmx3823/5112/6688）：
  `super.img` 是容器语义 → **全部逻辑分区**落 `super/`，清单里说的那 7/15/16/17 个分区与目录实际内容
  没有对应关系；
- 平铺机型（oriole 四份基线）：平铺分支根本不看 `keeps`，包里有几个逻辑分区就落几个；
- `super.img` 机型（gold/ss9380/popsicle）：`SplitSuper` 全量拆，`super/` 里官方 super 有几个分区就有
  几个，10_pack_super 随后照着全打进新 super。

另外 `cdh unpack-vendor -list` 只打 zip 成员表，payload 机型的关键信息（到底有哪些分区、哪些是逻辑
分区、会落到哪个路径）完全看不到，改清单只能靠猜。

## 二、根因

1. `imageTarget` 的判定顺序是「super.img → 清单保留名单 → 取证辅助分区」，动态分区名（`system.img`
   / `vendor.img`…）一旦被清单点名就命中「保留名单」分支，落到**解包根**而不是 `super/<名>_a.img`；
   根目录的同名镜像又被 10_pack_super 以「根目录优先」当机型侧产物采用，与 05/07 读的
   `super/vendor_a.img` 打架。
2. 平铺分支 `extractImagesFromZip` 只判 `flatTarget(name)`，不看清单，`rom.vendor.images` 对它无效。
3. `finishVendor` 调 `SplitSuper(..., only=nil, ...)`，永远全量拆；拆完也不清理上一次留下的
   `super/*.img`（清单收窄后旧分区仍在，会被 `writeMarker` 登记成「本次产物」）。
4. `--list` 只遍历 zip 成员/ tar 头，没有解析 `payload.bin` 的 `DeltaArchiveManifest`。

## 三、证据

- 修前（m22 那一轮的教训）与修后对照见 `log-pass.txt`：同一份 Nothing 3a 包，修后日志逐条打印
  `- modem 185.14 MiB 清单未声明，跳过` / `+ odm 319.36 MiB 160 ops -> super/odm_a.img`。
- oriole 修后 `super/` 恰好 5 个文件（product 3054.86 / system 958.12 / system_ext 273.46 /
  vendor 559.57 / vendor_dlkm 46.79 MiB），加 `vendor_boot.img` 64 MiB 与 `firmware/` 三个取证件
  （boot / dtbo / vbmeta），目录 4.9 GiB；`.unpacked.json` `files=9`、`keeps=7`。
- nothing/3a 修后 `super/` 恰好 7 个文件（odm / product / system / system_dlkm / system_ext /
  vendor / vendor_dlkm），加 `vendor_boot.img` 96 MiB，**没有** `firmware/`，目录 5.9 GiB；
  `.unpacked.json` `files=8`、`keeps=9`（含自动补上的 `super.img`）。

## 四、改动

- `01_unpack/vendor.go`
  - `imageTarget`：动态分区名（`flatPartitions`）一律返回 false，落点交给平铺分支，杜绝「清单点名就
    落解包根」；
  - 新增 `keepFlatPartition`（平铺路线按清单筛，声明 `super.img` 或未声明时全收）与
    `splitOnly`（super.img 路线只拆点名的逻辑分区，点名 `super.img` 时仍全量）；
  - 新增 `pruneSuperDir`：拆分后清掉本次没产出的 `super/*.img`；
  - 平铺分支把产物同时登记到清单名（`found["system.img"]`），消掉 `warnKeeps` 的假告警。
- `01_unpack/payload.go`：新增 `readPayloadManifestFrom`（流式只读头 + manifest，不看数据段）与
  `describePayloadManifest`（分区类型 + 落点），`--list` 直接可用。
- 13 份基线补齐 `rom.vendor.images`（分区集全部由 `--list` 实读得出，不靠猜）：
  - oriole ×4：`system, system_ext, product, vendor, vendor_dlkm` + `vendor_boot`；
  - nothing/3a ×2：上述 7 个 + `odm` + `vendor_boot`；
  - oneplus CPH2769 / PJE110：7 个 + `my_product, my_engineering, my_stock, my_heytap, my_carrier,
    my_region, my_bigball, my_manifest` + `vendor_boot`（16 项）；
  - oppo cph2797 / PME110：再加 `odm_dlkm`（17 项）；
  - realme rmx5112 / rmx6688：17 项；rmx3823 再加 `my_company, my_preload`（18 项）。
- 测试：`TestExtractZipImagesFlatHonorsKeeps`、`TestSplitOnlyAndFlatKeep`、
  `TestPruneSuperDirDropsStale`（与代码同目录 `01_unpack/vendor_test.go`）。
- 文档：`01_unpack/README.md`「分区筛选」补三条包型落点、`--list` 用法、平铺实跑数字；遗留项把
  「平铺路线不筛分区」换成「tar 流里的平铺分区不筛也不抽」。

## 五、验证

- `go build ./...` / `go vet ./...` / `go test ./...` 全绿（含新增 3 条用例）。
- oriole `AP2A.240905.003.F1`、nothing/3a `B4.1-260810-1153` 均 `--force` 重解，`EXIT=0`，
  `super/` 文件集与清单逐项一致（见上）。
- 启动测试（既有 dist，均为改动后清单口径）：oriole 实例（5911）`boot_completed=1`、NexusLauncher
  前台、`ro.hardware.egl=mesa`；cph2797 `boot_completed=1`、`com.android.launcher` 为
  `topResumedActivity`；ss9380 `boot_completed=1`（init 处理 `sys.boot_completed=1` 动作）、
  SetupWizard 前台（该机 adb offline，走 daemon guest 通道读 `getprop`：`SM-S9380 / 16`）。
- 三台 `screencap` 均 6 s 超时（0 字节），与 m22 同现象，gold 正常（2.79 MB PNG）→ 见遗留。

## 六、遗留

- **tar 流里的平铺分区不筛也不抽**：`extractTarStream` 没有平铺分支；现有 tar 机型都带 `super.img`，
  未受影响。若出现「tar 里给平铺 system/vendor」的包，要补落点映射并重验。
- **无 dist 的 7 台没做整线构建/启动测试**：其中 oneplus 15r、ace3、oppo pme110 缺
  `manifest/<vendor>/<dev>/device.json`（运行参数待补），pme110 与 oriole-cp3a 走 lineage-24.0 而
  `kernels/` 下没有 24 的源码；3a（两份）、realme ×3、oriole bp1a/cp1a 可构建，但 02 会因
  `rom.vendor.kernel.uname-version` 未实测报「无法确定内核源码」，需先按 `AGENTS.md` 第 5 条
  从固件实测内核口径。
- **`screencap` 多机型超时**：oriole / cph2797 / m22 都卡住（guest 进程 `futex_wait_queue`），gold
  正常；`dumpsys SurfaceFlinger` 在 m22 报 `FAILED_TRANSACTION`。与本次改动无关，需单列一条
  issue 追读回通路。
