# Issue: popsicle 整线 01→13 在 08_build_hardware 秒级失败：android16 的 ABI/platform/AIDL/v412 依赖整代未落盘，08 只校验不拉取

- 设备：`xiaomi/popsicle`（Xiaomi 17 Pro Max / `canoe` / HyperOS 4 `16-OS4.0.5.0.XPBCN` / lineage-23.2 / kernel 6.12.81）
- 构建入口：`out/agent/dist/cdh-pipeline device build popsicle --manifest 16-OS4.0.5.0.XPBCN`
- 失败运行：`r_20261118_034335758`（2026-11-18 11:43:36 → 11:50:02，386046 ms，exitCode=1）
- 依赖根：`out/devices/hal/android16`（platform/AIDL）+ `out/devices/vendor/android16/abi`（链接期库）
- 状态: **已闭环**（2026-11-18）：补齐 android16 的 ABI（4+3 库）、platform-source（15 仓）、20 个 HAL 的 AIDL、v412（16 仓 + 219 HIDL 头 + system18/apex9）后，整线 01→13 全部通过（582909 ms），08 新编 20/20 模块

---

## 一、问题

整线构建跑到第 8 阶段 `08_build_hardware`，**开工 2 ms 就失败**，20 个 HAL 模块一个都没编：

```
==> [5/13] build_hardware  (08_build_hardware)
==> HAL 构建: android16，20 个模块
==> 产物根: /Volumes/work/cloud-phone-hub/out/devices/vendor/android16
[err] 阶段 build_hardware 失败: ABI 依赖库未就绪（.../out/devices/vendor/android16/abi/system/lib64）:
      libc++.so（缺失）; libbinder_ndk.so（缺失）; liblog.so（缺失）; libnativewindow.so（缺失）；
      先跑 cdh-pipeline vendor-deps --android 16 --abi
```

前序阶段 01 unpack（ok）、02 prepare（ok）、03 build_kernel（ok）、07 build_odm（ok）均正常；
04/05/06 与 09~13 显示 `skipped` 不是自身问题——04/05 在 DAG 第 3 波、06 在第 4 波，
08 在第 2 波失败后它们根本没被调度。

## 二、根因

1. **不是「丢了 4 个库」，而是 android16 这一代依赖缓存整体从未落盘**：失败时
   `out/devices/vendor/android16/`、`out/devices/hal/android16/` 两个目录都不存在；
   `out/devices/vendor/` 与 `out/devices/hal/` 下只有 `android14`、`android15` 两代缓存。
   ABI 只是 08 校验链上的**第一道关卡**，后面还压着 platform-source（15 仓）、
   20 个 HAL 的 AIDL 快照和 media-v412 的 16 个扩展仓 + hidl-gen + 219 个 HIDL 头 +
   `abi/media-v412/{system(18),apex(9)}`。
2. **08 阶段按设计只校验、不联网拉取**：`08_build_hardware/plan.go` 在循环前调用
   `deps.ABIProblems(v)`，缺库就直接返回错误；CLI 注释也写明「08 阶段只校验依赖是否就绪，不联网」。
   因此产物根被清空/迁移后，08 必然把失败暴露在自己这里，而不是由自己补齐。
3. **`vendor-deps` 是整线 DAG 之外的人工前置步骤**：`pipeline.Stages` 的 13 个阶段里没有
   vendor-deps 节点，`device build` 不会自动触发它；本工作区从 android14/15 时代沿用下来的
   依赖缓存又无法跨代复用（platform 仓按 AOSPTag `android-16.0.0_r2`、AIDL 按 android16 冻结版本、
   ABI 按 lineage-23.2 的 `/system/lib64` 与 `/vendor/lib64` 逐字节对齐），
   所以「01→13 一条命令跑到底」在 android16 首次构建时必然缺这一层。
4. **报错提示会误导修复范围**：错误只让人跑 `--abi`。只补 ABI 能过第一道校验，
   但紧接着会在 audio 模块的 AIDL（AIDL 未就绪）、platform-source（libxml2 静态库）、
   最后一个 media.v412（v412 platform/hidl/ABI）连续失败。popsicle 的 20 个模块把
   三种依赖全用上了。

## 三、证据

- 失败现场：同目录 `log-fail.txt`（13 阶段表 + 08 完整输出 + exitCode=1）。
- 失败前目录实况：`ls out/devices/vendor/android16/` 与 `ls out/devices/hal/android16/` 均
  `No such file or directory`；`out/devices/{vendor,hal}/` 只有 `android14`、`android15`。
- 时长判据：08 失败耗时 **2 ms**，是开工前硬校验（`deps.ABIProblems`）而不是模块编译错误。
- 历史同型：`../../m22/issue-m22-abi-deps-regenerate/`（#73）在 07 阶段遇过同一条 ABI 提示，
  当时的结论也是「`out/vendor/android16` 从空目录起步，deps 不会自动补」。
- 修复后复核：`vendor-deps --android 16 --check` 回
  `platform-source 齐全（15 仓）`、`ABI 依赖齐全（4 库）`，20+ HAL 表项全绿。
- 通过现场：同目录 `log-pass.txt`（四条 deps 命令的完整输出 + 13 阶段全 ok + 产物清单）。

## 四、改动

**本次零代码改动，只补运行态依赖**（顺序不能颠倒，`--v412` 还要在 platform 之后）：

```bash
cd /Volumes/work/cloud-phone-hub

# 1) 链接期 ABI：从 lineage-23.2 的 system_a.img / vendor_a.img 就地提取（离线，0.14 s）
out/agent/dist/cdh-pipeline vendor-deps --android 16 --abi

# 2) platform-source（15 仓）+ 20 个 HAL 的 AIDL 快照（联网）
out/agent/dist/cdh-pipeline vendor-deps --android 16

# 3) media-v412：16 个扩展仓 + hidl-gen + 219 个 HIDL 头 + v412 ABI（联网）
out/agent/dist/cdh-pipeline vendor-deps --android 16 --v412

# 4) 只校验不下载，确认全绿
out/agent/dist/cdh-pipeline vendor-deps --android 16 --check

# 5) 整线重跑（cached 阶段会秒过）
out/agent/dist/cdh-pipeline device build popsicle --manifest 16-OS4.0.5.0.XPBCN \
  --report out/devices/tmp/popsicle-16-OS4.0.5.0.XPBCN-build-report.json
```

依赖落点：
- `out/devices/vendor/android16/abi/system/lib64`（4 库）+ `abi/vendor/lib64`（3 库）+ `.abi-complete`
- `out/devices/hal/android16/aosp/platform`（15 仓，标记 `.platform-complete`）
- `out/devices/hal/android16/aosp/aidl/<hal>`（各 HAL 版本包 + `.hash`）
- `out/devices/vendor/android16/abi/media-v412/{system,apex}`（18 + 9 库）

## 五、验证

| 项 | 结果 |
| :--- | :--- |
| 依赖校验 | `platform-source 齐全（15 仓）`、`ABI 依赖齐全（4 库）`、20+ HAL AIDL 全绿 |
| 08 编译 | `HAL 构建完成: 新编 20，复用 0，总计 20 个模块`（含 `android.hardware.media.v412`），384881 ms |
| 整线 | **13/13 阶段通过**：04 ok 62920 / 05 ok 7477 / 06 ok 8150 / 09 ok 16059 / 10 ok 7684 / 11 ok 11166 / 12 ok 14299 / 13 ok 66630；`report.ok=true`，`[ok] 已完成`（582909 ms） |
| 04 模块 | `[ok] 模块产物已就绪: 11 个 .ko`；10 依赖闭包成立（11 模块 / 内核内建 686 项） |
| vendor.img | 303128576 B；`e2fsck -fn` 干净（703/768 文件） |
| vendor_dlkm.img | 15831040 B，sha256 `c94350e57418`；`e2fsck -fn` 干净（36/144 文件） |
| patch.img | 402653184 B，sha256 `b87a51125b03`；`e2fsck -fn` 干净（755/98304 文件），回读 744/744 条目一致 |
| odm.img | 8388608 B；`e2fsck -fn` 干净（14/512 文件） |
| super.img | 13421772800 B（12.50 GiB，`mode: copy`，官方镜像原样直通，符合 AGENTS.md 第 8 条） |
| 盘集 | `vda.img` 15.67 GiB（super/boot_a/boot_b/vendor_boot_a/vendor_boot_b/cust 六区已刷新）+ `patch.img` 384 MiB + `data.qcow2` 193.7 KiB（虚拟 110.27 GiB）+ `device.json` |

> 说明：04 本轮产出 11 个 `.ko`（4 个外部模块 + 7 个公共 in-tree），与 #136 记录的 13 个差异来自
> 本轮清单/配方口径，不在本条范围内，未作改动。

## 六、遗留

1. **08 仍不会自动补依赖**：`pipeline.Stages` 没有 vendor-deps 节点，android17 或下一个
   新代际首次构建还会以同样的 2 ms 硬失败开场。可选收口：把 vendor-deps 作为 08 的 DAG 前驱
   （联网阶段需可关闭），或让 08 在缺依赖时给出「platform/AIDL/v412 也缺」的完整清单。
2. **错误提示只提 `--abi`**：本次按提示补 ABI 后仍会在 AIDL/platform/v412 连撞三关；
   建议在 08 的报错里按模块 kind 汇总缺项（AIDL 未就绪 X 个、platform 缺失、v412 缺失）。
3. **依赖体积**：`out/devices/hal/android16` 约 372 MB、`out/devices/vendor/android16/abi` 约 7 MB，
   清理 out 树时若只清 `vendor/android*` 会留下「platform 在、ABI 不在」的半套状态；
   建议把 `.abi-complete`/`.platform-complete` 与目录一起清。
4. 本轮未上机验证新盘集（只做到 13_pack_vda 产物与 ext4 只读校验）。
