# 01_unpack 解不了官方 OTA 包（只有 payload.bin，没有 super.img）

- 设备：flyme/m22（MEIZU 22，Flyme 12.5.0.0A / Android 16 / QTI `sun`）
- 组件：`engine/build/01_unpack`
- 输入：`roms/devices/flyme/m22/12.5.0.0A.zip`（8.24 GiB，SignApk 签名 OTA）
- 产物：`engine/out/roms/flyme/m22/12.5.0.0A/`（7 逻辑分区 + 33 物理分区 + `vendor_boot.img`）

## 问题

`01_unpack` 对 m22 的官方固件包直接失败：包内**没有** `super.img`，整机镜像全在
`payload.bin` 里（8,244,669,808 B）。引擎原有两条提取路径（tar / zip 找 `super.img`、
单镜像包）都不适配，下游 07–11 因此拿不到任何分区件。

## 根因

官方 A/B OTA 的产物是 Android OTA payload（`CrAU` v2），`super.img` 在出厂包里
从不存在——逻辑分区（system/vendor/product/odm/vendor_dlkm/system_ext/system_dlkm）
与物理分区（boot/dtbo/modem/xbl…）都以 install operation 的形式存在一个
`DeltaArchiveManifest` 里。引擎此前只认"整盘 super.img"这一种形态。

## 证据

`log-fail.txt`：`unzip -l` 列表里只有 `payload.bin` 与 `META-INF/*`，无 `super.img`；
`grep -rn "payload" engine/build/01_unpack` 当时无命中（payload 逻辑尚不存在）。

payload 头与 manifest 实测（Python 解析原始流）：

```
magic=CrAU version=2 manifest_size=296577 metadata_signature_size=267
block_size=4096  partitions=43  data_start=296868
file_size 8244669808 - data_bytes 8244372673 = 297135 = 24 + 296577 + 267 + 267
```

`dataStart = 24 + manifestSize + msig` 这个式子与文件大小差恰好对上，据此确定
v2 头是 **24 字节**（`u32` 签名长度在 manifest 之前），推翻了首版按 AOSP 20 字节
头解析得到的乱字段（`wt 3 at 13`）。op 类型分布：`REPLACE_XZ(8)` 8 个分区、
`REPLACE_BZ(1)` 1 个、`REPLACE(0)` 0 个，其余为 `ZERO(6)`。

## 改动

- 新增 `engine/build/01_unpack/payload.go`（约 595 行）：手写 protobuf 解析
  `DeltaArchiveManifest`（不引第三方依赖）、双头布局回退、`REPLACE`/`REPLACE_BZ`/
  `REPLACE_XZ`/`ZERO` 四种 op、`WriteAt` 乱序填盘、末尾 `Read` 校验长度精确。
  增量包的 `BSDIFF`/`SOURCE_*` 直接报错，不猜。
- 产物契约不变，下游无感：逻辑分区 → `super/<name>_a.img`；`vendor_boot.img` 单列；
  其余物理分区 → `firmware/<name>.img`（引擎暂不消费，留档）。
- `01_unpack/vendor.go`：`extractImages` 返回 `(payload bool, err)`；zip 内无
  `super.img` 时找 `payload.bin`；完成标记新增 `payload: true`，`alreadyUnpacked`
  据此核查登记过的 `super/*.img`、`firmware/*.img` 与 `vendor_boot.img`。
- 新增 `01_unpack/payload_test.go`：合成 payload 编码器 + 4 个用例（头解析、分区落点、
  拒绝不支持 op、`partitionTarget` 命名）。

## 验证

`log-pass.txt`：实跑 8.24 GiB 载荷，7 逻辑 + 33 物理（9.90 GiB）+ `vendor_boot.img`
96 MiB；并把解出的官方分区与出厂包内同一分区逐字节比对：

- `super/vendor_a.img` 的 `/build.prop` md5 = `4a012482c114a2f5694453f895716ed1`
- `super/odm_a.img` 的 `/etc/build.prop` md5 = `40415616a73d1ec1439dc6e50ec229fc`

两项都与 `dump.erofs` 直接读官方分区的结果一致。

## 遗留

- payload 只支持全量包（`REPLACE*` / `ZERO`）。遇到增量包（`SOURCE_*`/`BSDIFF`）
  会明确报错，需要时再实现"以旧分区为源"的补丁路径。
- `firmware/` 下的物理分区目前只留档，引擎不消费；若将来要做"整盘重建"，
  需要把 33 个物理分区与 GPT 偏移的对应关系补进来。
