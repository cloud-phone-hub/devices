# m22 全量重建时 07 阶段因 ABI 依赖库缺失中断

- 设备：`flyme/m22`（MEIZU 22，QTI `sun`，清单 `manifest/flyme/m22/baselines/12.5.0.0A.json`）
- 阶段：`cdh device build m22` 第 7 阶段 `build_vendor`（`07_build_vendor`）
- 日期：2026-10-30

## 1. 问题

按 `./out/bin/cdh device build m22` 跑 11 阶段全量重建，前 6 阶段全过，第 7 阶段在
「步骤 3 编译 HAL 模块」的第一个模块（`android.hardware.audio`）就断：

```
==> 编译 HAL 模块 android.hardware.audio（变体 default，待落盘 bin/hw/android.hardware.audio.service, ... 等 4 项）
  + out/bin/cdh-vendor hal build android.hardware.audio --android 16 --out-dir .../out/vendor/android16
[err] ABI 依赖库未就绪: .../out/vendor/android16/abi/system/lib64（先跑 cdh-vendor deps --android 16 --abi）
[err] 阶段 build_vendor 失败: HAL 模块 android.hardware.audio 编译失败（exit status 1）
```

36 个模块一个都没编。此时 `out/vendor/` **整个目录不存在**，不只是 `abi/` 子目录缺失。

## 2. 根因

`cdh-vendor hal build` 链接期需要 NDK 侧 ABI 库（`libc++.so` / `libbinder_ndk.so` /
`liblog.so` / `libnativewindow.so` 等）落在 `out/vendor/android<ver>/abi/{system,vendor}/lib64`，
这批库不是编译产物，而是由 `cdh-vendor deps --android 16 --abi` 从 lineage 镜像里**抽取**出来的。

两处目录容易被混淆：

| 目录 | 内容 | 谁负责 |
| --- | --- | --- |
| `engine/vendor/out/android16`（1.2 G，10-23） | 依赖缓存：AIDL 源码、platform 八仓、codegen 工作区 | `deps` / `deps --platform` |
| `engine/out/vendor/android16` | HAL 编译产物 + ABI 库 + 模块静态树 | `hal build` / `deps --abi` |

本次 `vendor/out/android16` 完好（`--check` 报 `platform-source 齐全（8 仓）`），但
`out/vendor/android16` 在本次重建前已被清空。07 的步骤 3 只负责「编译并落盘」，不会自动补 ABI 库，
于是第一个模块就打在缺失的链接依赖上失败。

## 3. 证据

- 失败现场：同目录 `log-fail.txt`（前六阶段 ok 行、07 步骤 0–3 全文、`EXIT=1`）。
- 独立复现（只检查不下载）：`cdh-vendor deps --android 16 --abi --check`
  → `[err] ABI 依赖不完整: libc++.so（缺失）, libbinder_ndk.so（缺失）, liblog.so（缺失）, libnativewindow.so（缺失）`。
- 依赖根对照：`vendor/out/android16` 存在且 1.2 G；`deps --platform --check` 回 `[ok] platform-source 齐全（8 仓）`
  —— 证明缺的只是 ABI 库这一层，不是整个依赖体系。
- 目录差异：失败后 `ls out/vendor/` 直接 `No such file or directory`，说明是空目录起点，不是"部分缺失"。
- 修复与验证全过程：同目录 `log-pass.txt`。

## 4. 改动

**本次零代码改动**，只做运行态补齐：

```bash
cd engine
./out/bin/cdh-vendor deps --android 16 --abi      # 从 lineage-23.2 的 system_a/vendor_a 抽 ABI 库
./out/bin/cdh device build m22 --from build_vendor # 从失败阶段续跑 07→11
```

`deps --abi` 的实际输出：

```
[..] 从 lineage 镜像提取 ABI 库: /Volumes/work/cloud-phone-hub/lineage-rom/lineage-23.2/super/system_a.img
[..] 从 lineage vendor 提取模块专属运行库: /Volumes/work/cloud-phone-hub/lineage-rom/lineage-23.2/super/vendor_a.img
[ok] ABI 依赖就绪: .../out/vendor/android16/abi/system/lib64（4 库，专属 3 库）
```

注意它取的是仓库根 `lineage-rom/lineage-23.2/`（下载中转），不是 `engine/out/roms/lineage/lineage-23.2/`。

> 2026-11-03 更正：`profile.LineageDir()` 已改为默认取 `engine/out/roms/lineage/<版本>`
> （父目录可用 `CDH_LINEAGE_BASES` 覆盖），仓库根 `lineage-rom/` 不再被代码引用；
> 上面日志里的路径是当时的软链写法，现在同一条命令会打印 `engine/out/roms/lineage/lineage-23.2/…`。

## 5. 验证

| 项 | 结果 |
| --- | --- |
| 07 步骤 3 | 36/36 模块全部编译通过，装配树 654 条目（文件 423 / 链接 199 / 目录 32） |
| `vendor.img` | 201,883,648 B；`e2fsck -fn` 干净（665/768 文件） |
| `vendor_dlkm.img` | 15,831,040 B，sha256 `5af7dd961cd6`；e2fsck 干净 |
| `patch.img` | 402,653,184 B，sha256 `6fe7ff700faf`；回读 vendor_dlkm 197 条目；e2fsck 干净 |
| `super.img` | 9.39 GiB，sha256 `3eb133bbc4af` |
| 盘集 | `vda.img` 13,958,643,712 B + `patch.img` + `data.qcow2` + `device.json` |
| 镜像回读 | `/etc/init/hw` 4 件（`init.qcom.rc` 2164 B）、`cph_logcat_dump.rc` 1981 B / 0644 / 0:0、顶层白名单齐 |
| 机型层最新改动 | `etc/init` 57 件且 **无** early 版（#71 生效）；`init.qcom.rc` 2164 B（#70 生效） |
| 全流程 | `EXIT=0` |

## 6. 遗留

1. **`build_vendor` 不会自动补 ABI 库**：`out/vendor/android16` 被清空时，07 会一路跑到
   步骤 3 才失败（前面步骤 1/2/2b 白做）。可在 07 步骤 0 增加一次
   `deps --abi --check` 的前置校验，把失败提前到秒级 —— 本次未改代码，留待决定。
2. **装配树条目 654 与历史记录的 669 有差**（文件 423 vs 438，链接/目录数一致）。
   已核对四层无缺失：`bin/hw` 44 个服务（= 36 模块产物）、`etc/init` 57、`etc/vintf/manifest` 40、
   `etc/selinux` 14，APEX 摊平与机型静态树均正常；`out/vendor/android16` 本轮从零编译，
   产物根内没有超出声明模块的文件（`bin/hw` 42 + 基底 2 = 44）。差异更可能是此前多次实验在
   产物根/装配树上留下的残留文件（本次全新建树后自然消失），但上一轮结束时的树已被覆盖，
   无法做逐文件 diff 定论 —— 下次重建时若要闭环，应在构建前 `cp -R` 一份旧树。
   **已闭环（2026-10-30，见 #74 `issue-m22-sepolicy-domain-attributes`）**：差的就是 v412 的
   15 个运行期库（文件 438−423=15，含 `libchrome.so`）。成因是 `hal.go:409` 的
   `v412RuntimeLibs()` 在步骤 0 读清单，而清单由步骤 3 编译后写出 —— 产物根被清空的第一趟
   必然读到空集。本轮重跑一趟 07 后装配树回到 **669**、`vendor.img` 回到 **206,700,544 B**，
   与 #71 逐字一致；不是「历史残留」，也不再是「无法定论」。
3. **`平台覆盖层: qcom（产物根下暂无该目录，跳过）`** 是本机历史一贯状态
   （见 `issue-m22-idc-dead-config-removal/log-pass.txt:29`），非本次引入；m22 的平台覆盖层
   从来没有落地过，是否要用由 `manifest/README.md:91` 的策展口径决定。
4. `out/tmp/` 在本轮开始前是空目录（上一轮构建日志已不存在），本轮日志为
   `out/tmp/m22-rebuild-full.log` 与 `out/tmp/m22-rebuild-from-vendor.log`。
