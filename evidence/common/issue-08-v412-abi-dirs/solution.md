# 08_build_hardware 经流水线跑 v412 必然失败：`--abi-dir` 两头都没人给

## 1. 问题

`cdh-pipeline device build gold --manifest 15-OS3.0.11.0.VNQCNXM` 跑到 `08_build_hardware`，
前面 25 个模块都能编完，最后一个 `android.hardware.media.v412` 每次都以同一句报错退出：

```
--> android.hardware.media.v412（v412）
[err] 阶段 build_hardware 失败: 构建模块 android.hardware.media.v412 失败:
      v412 需要恰好两个 --abi-dir（先 system、后 apex），实际 0 个
```

即使先跑过 `cdh-pipeline vendor-deps --android 15 --v412`（日志里 `v412 ABI 就绪: system(18) + apex(9)`、
`out/devices/vendor/android15/abi/media-v412/{system,apex}` 都在位），重跑 08 仍是同一句 0 个。
也就是说缺的不是库，是**传参**。

## 2. 根因

`v412` 的构建流程（`core/halbuild/v412.go` 的 `buildV412`）要求恰好两个库目录：

- `abi/media-v412/system`（system 侧 18 个库）
- `abi/media-v412/apex`（media-swcodec APEX 侧 9 个库）

独立入口 `cdh-vendor hal build` 让调用方显式传 `--abi-dir`（`cmd/vendor/main.go` 的 `abiDirs`），
而**流水线入口 `08_build_hardware/plan.go` 的 `Run` 自己算 abiDirs 时只处理常规模块**：

```go
abiDirs := []string{}
if module.Kind != "v412" && module.Kind != "pure-c" {
    abiDirs = append(abiDirs, version.ABIDir())          // abi/system
    abiDirs = append(abiDirs, version.ABIVendorLibDir()) // abi/vendor
}
```

`v412` 被这个条件直接跳过 → 传进去的是空切片 → `buildV412` 的 `len(abiDirs) != 2` 立刻报错。
两条入口（独立 CLI / 流水线）对同一模块的入参口径没有对齐，就是全部原因；
`vendor-deps` 早就把 `V412ABISysDir()` / `V412ABIApexDir()` 指向的两个目录备好了。

## 3. 证据

- `log-fail.txt`：08 在 `android.hardware.media.v412` 处失败，报「实际 0 个」。
- 失败前的依赖准备是成功的：
  `vendor-deps --android 15 --v412` 输出 `[ok] v412 ABI 就绪: system(18) + apex(9)`，
  目录 `out/devices/vendor/android15/abi/media-v412/{system/lib64,apex/media-swcodec}` 均存在。
- `log-pass.txt`：同一台机器、同一份产物目录，改完 plan.go 后 v412 一次编过：
  `[ok] android.hardware.media.v412 构建完成`，`合并 vendor 静态树：新增 3`。
- 09 的装配摘要里 v412 条目由 0 变 46：
  `android.hardware.media.v412@default 46 项`。

## 4. 改动

`agent/modules/pipeline/stages/08_build_hardware/plan.go`（`Run` 里按模块 kind 分支）：

```go
abiDirs := []string{}
switch module.Kind {
case "v412":
    // buildV412 要求恰好两个目录：先 system、后 media-swcodec APEX。
    abiDirs = append(abiDirs, version.V412ABISysDir(), version.V412ABIApexDir())
case "pure-c":
    // 自带取库方式，不需要 ABI 快照。
default:
    abiDirs = append(abiDirs, version.ABIDir())
    if st, err := os.Stat(version.ABIVendorLibDir()); err == nil && st.IsDir() {
        abiDirs = append(abiDirs, version.ABIVendorLibDir())
    }
}
```

只动入参口径，不动 `buildV412` 的校验（「必须两个」是它的契约，保留）；
`pure-c` 维持原行为（不传）。

## 5. 验证

- `cd agent && gofmt -l` 干净，`go build ./...` 通过，
  `go test ./modules/pipeline/stages/08_build_hardware/...` 全绿（deps / halbuild / profile / spec / toolchain）。
- 重编 `cdh-pipeline` 后 `device build gold --manifest 15-OS3.0.11.0.VNQCNXM --from build_hardware`
  从 08 一路跑到 13 以 `[ok] 已完成` 结束，13 个阶段全通。
- 产物：`out/devices/dist/redmi/gold/15-OS3.0.11.0.VNQCNXM/{vda.img 14.21 GiB, patch.img 384 MiB,
  device.json, data.qcow2, vda-info.txt, vda-recipe.txt}`；`vda-log.txt` 里 12 个分区回读一致。

## 6. 遗留

- 这次先跑的是 `05_build_boot` 之后的整链；`01/02` 命中缓存（`[skip]`），未重解包。
- 流水线目前不给 v412 传 `VendorLib64`（`--vendor-lib64`）：v412 无法判断哪些库已随 vendor 基底就位，
  会把运行期闭包全部落盘。独立 CLI 支持该参数，流水线后续可按 `env.VendorDir` 解出 `vendor/lib64`
  清单再补上（不影响本次构建正确性，只影响是否多覆盖几个同名库）。
- 云机上 v412 解码器的真实起机验证未做（本条目只覆盖构建与打包链）。
