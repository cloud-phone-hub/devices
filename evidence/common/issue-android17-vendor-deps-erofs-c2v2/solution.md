# #163 Android 17（`lineage-24.0`）`07_build_vendor` 因 `swcodec` APEX EROFS 格式及 Codec2 AIDL V2 升级导致构建失败

- **设备/机型**：跨机型（`common`，首发验证于 Google Pixel 6 `google/oriole` — `CP3A.261005.002.A1-20260905`，Android 17 / `lineage-24.0`）
- **阶段**：`07_build_vendor`、`engine/vendor/build`（`cdh-vendor deps` 与 `android.hardware.media.v412`）
- **状态**：已闭环（2026-11-05）

## 1. 问题

运行 `#4 Pixel 6 (google/oriole) — CP3A.261005.002.A1-20260905` 的 `07. 装配 vendor.img`（`07_build_vendor`）失败：

1. 初次执行 `07_build_vendor` 在步骤 3 编译 `android.hardware.audio` 时报错：
   `[err] ABI 依赖库未就绪: /Volumes/work/cloud-phone-hub/engine/out/vendor/android17/abi/system/lib64（先跑 cdh-vendor deps --android 17 --abi）`
2. 执行 `cdh-vendor deps --android 17 --v412` 拉取并提取 Android 17 ABI 依赖库时，在从 `lineage-24.0` 的 `/system/apex/com.android.media.swcodec.capex` 提取 `v412` 依赖库阶段报错：
   `[err] 从 swcodec APEX 提取 libcodec2_vndk.so 失败: debugfs: Bad magic number in super-block while trying to open .../lineage-24.0/super/swcodec-work/apex_payload.img`

## 2. 根因

1. **`lineage-24.0` 的 `swcodec` `apex_payload.img` 文件系统由 ext4 改为 EROFS**：
   - Android 14~16（`lineage-21.0` ~ `lineage-23.2`）中，`com.android.media.swcodec.capex` 内层解压出的 `apex_payload.img` 均为 ext4 镜像，`deps/abi.go` 的 `dumpFromImage` 固定调用 `debugfs -R "dump -p ..."` 提取文件。
   - 在 Android 17（`lineage-24.0`）中，`apex_payload.img` 偏移 `0x400`（1024）处的超级块魔数为 `0xE0F5E1E2`（小端字节序 `e2 e1 f5 e0`），即 **EROFS** 镜像，导致 `debugfs` 报 `Bad magic number in super-block`。
2. **Android 17 `swcodec` APEX 内置的 Codec2 AIDL 库由 V1 升级为 V2**：
   - 用 `fsck.erofs` 解包 `lineage-24.0` 的 `swcodec` `apex_payload.img` 后检查 `/lib64/`，原有的 `libcodec2_aidl.so` 与 `android.hardware.media.c2-V1-ndk.so` 在 Android 17 中已升级并重命名为 `libcodec2_aidl_V2.so` 与 `android.hardware.media.c2-V2-ndk.so`。
   - `deps/v412abi.go` 的 `V412ApexLibs` 与 `modules/android.hardware.media.v412.json` 此前写死 V1 库名与 `aidlPackageVersions.android.hardware.media.c2 = "1"`。
3. **`db-manifest` 物化路径缓存刷新**：
   - `build/internal/manifest/manifest.go` 与 `build/internal/env/env.go` 在收到 `<out>/tmp/db-manifest/...` 路径时，此前直接当外部静态文件返回而跳过了 `devices.db` 查询；需将 `db-manifest` 目录纳入“始终通过 `devices.db` 实时物化”范围，确保设备数据库为唯一真源。

## 3. 证据

- `CP3A.261005.002.A1-20260905` 初始 `07_build_vendor` 失败日志（见 `log-fail.txt`）：
  ```text
  ==> 编译 HAL 模块 android.hardware.audio（变体 default，待落盘 4 项）
    + out/bin/cdh-vendor hal build android.hardware.audio --android 17 --out-dir /Volumes/work/cloud-phone-hub/engine/out/vendor/android17
  [err] ABI 依赖库未就绪: /Volumes/work/cloud-phone-hub/engine/out/vendor/android17/abi/system/lib64（先跑 cdh-vendor deps --android 17 --abi）
  ```
- `apex_payload.img` 魔数与 `/lib64` 目录实测：
  ```text
  $ xxd -s 1024 -l 16 out/roms/lineage/lineage-24.0/super/swcodec-work/apex_payload.img
  00000400: e2e1 f5e0 0000 0000 0c00 0000 2800 0000  ............(...
  $ ls -1 out/tmp/swcodec-check/lib64/
  android.hardware.common-V2-ndk.so
  android.hardware.common.fmq-V1-ndk.so
  android.hardware.graphics.common-V3-ndk.so
  android.hardware.media.bufferpool2-V2-ndk.so
  android.hardware.media.c2-V2-ndk.so
  libcodec2_aidl_V2.so
  libcodec2_hal_common.so
  libcodec2_hidl@1.0.so
  libcodec2_hidl@1.1.so
  libcodec2_hidl@1.2.so
  libcodec2_hidl_plugin.so
  libcodec2_vndk.so
  libOrders.so
  libsfplugin_ccodec_utils.so
  libstagefright_bufferpool@2.0.1.so
  ```

## 4. 改动

1. **`engine/vendor/build/internal/deps/abi.go`**：
   - 新增 `isErofsImage(imgPath)`（检测偏移 1024 处小端魔数 `0xE0F5E1E2`）与 `findErofs()`（查找 `fsck.erofs`）。
   - `dumpFromImage` 在检测到 EROFS 镜像时改调 `fsck.erofs --extract=<tmpDir> --path=<inImgPath> <imgPath>` 提取单文件，ext4 镜像继续走 `debugfs`。
2. **`engine/vendor/build/internal/deps/v412abi.go`**：
   - 新增 `V412ApexLibsFor(v profile.Version)`，在 `v.Android >= 17` 时将 `libcodec2_aidl.so` / `android.hardware.media.c2-V1-ndk.so` 切换为 `libcodec2_aidl_V2.so` / `android.hardware.media.c2-V2-ndk.so`；`pullV412Abi` 与 `CheckV412` 均改调 `V412ApexLibsFor(v)`。
3. **`engine/vendor/build/modules/android17/android.hardware.media.v412.json`**：
   - 新增 Android 17 模块覆盖配置，声明 `aidlPackageVersions.android.hardware.media.c2: "2"` 与 V2 版的 `apexLinkLibs`。
4. **`engine/build/internal/manifest/manifest.go` & `engine/build/internal/env/env.go`**：
   - 在 `ResolvePath` 与 `ResolveManifest` 中识别 `<out>/tmp/db-manifest` 目录下的路径，统一经 `devices.db` 的 `FindVersion` + `MaterializeVersionWorkspace` 实时同步，杜绝使用 `devices/manifest` 或读取过期的临时物化文件。

## 5. 验证

1. **Android 17 依赖就位校验**：
   - 执行 `/Volumes/work/cloud-phone-hub/engine/out/bin/cdh-vendor deps --android 17 --v412` 与 `--check`：
     - `[ok] platform-source 13 个仓库就绪`
     - `[ok] ABI 库就绪（system 4 +bootstrap 3）`
     - `[ok] v412 依赖就绪（hidl 头 219 + ABI system(18)+apex(9)）`
     - `[ok] 全部 29 项 AIDL 快照就绪`
2. **`CP3A.261005.002.A1-20260905`（Android 17）`07_build_vendor` 及全流程构建验证**（见 `log-pass.txt`）：
   - 运行 `cdh device build oriole --manifest CP3A.261005.002.A1-20260905 --only 07_build_vendor`：18 个 HAL 模块在 `android17` 下全部编译通过，产出 `out/devices/google/oriole/CP3A.261005.002.A1-20260905/img/vendor.img`（`303915008` 字节，697 条目）。
   - 继续运行 `03_build_modules` 与 `08_build_vendor_dlkm` ~ `11_pack_vda` 全部通过，产出完整盘集 `out/dist/google/oriole/CP3A.261005.002.A1-20260905/vda.img`（13.00 GiB，5 个分区刷新）及 `patch.img`（sha256 `7d6879c4b733`）。
3. **单元测试**：
   - `engine/build` 与 `engine/vendor/build` 下 `go test ./...` 全部 PASS。

## 6. 遗留

- 无。

