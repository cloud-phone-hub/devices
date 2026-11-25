# 分布式下发构建源码至 ROM 服务器制作云机并直传运行服务器启动闭环

## 1. 问题

需要将本机 `devices/` 下的内核源码（`kernels/<ver>` 及其 `patches/`）、外部内核模块（`modules/`）、HAL 硬件抽象层代码（`hardware/`、`hardware-hidl/`、`cuttlefish/` 及 `out/devices/hal/<android>/aosp` 依赖）按需增量下发到 Linux x86_64 ROM 构建服务器（`rom-main`，`192.168.31.164`），在 ROM 服务器上完整完成 13 阶段云机制作，再直传到 ARM64 Linux 运行服务器（`rom-main-runtime`，`192.168.31.207`，OrangePi 5 Plus Debian 12）上成功拉起运行。

在推进过程中先后暴露两类跨平台卡点：
1. **Linux x86_64 构建侧卡点**：
   - `halcore/toolchain` 与 `profile` 硬编码 macOS 路径（`darwin-x86_64`、`$HOME/Library/Android/sdk`），且 `ndkDefault` 误选非标准目录 `ondk-r26.0`；
   - `devices/hardware/common/libcxx-platform/__config_site` 仅适配 Clang 20+（NDK r29），在 ROM 机 NDK r28（Clang 19）下报 `_LIBCPP_ABI_FORCE_ITANIUM` 宏重定义与 `bits/alltypes.h` 缺失；
   - `android.service.daemon`（Rust）在无 `just` 环境下卡 `fetch-external`，且 Linux 上 `rquickjs-sys` bindgen 缺 `-isystem <ClangRT>/include` 报 `'stdbool.h' file not found`；
   - `rom-main` 无法直连 Google Git 拉取 `linux-x86/hidl-gen`；
   - `env` 未覆盖邻近目录 `/home/backup/cloud-phone-hub/roms/lineage` 及 `CDH_ROMS_DIRS` 平铺厂商包路径；
   - Linux 非 root 用户执行 `fsck.erofs --extract=<dir> --xattrs` 时因内核禁止写入 `security.selinux` 返回 `EPERM (-1)` 中断 `09_build_vendor`，且 Linux 无 macOS `xattr -lr` 命令读取解包树 SELinux 标签。
2. **Linux ARM64 运行侧（Debian 12 GLIBC 2.36）卡点**：
   - Ubuntu 26.04 容器编译出的 `secure_env`、`netsimd`、`adb`、`virgl_render_server`、`vhost-user-gpu` 依赖 `GLIBC_2.38..2.43` 与新版 Mesa GBM（`dri_gbm.so`）；
   - `daemon` 在 Linux 未配置 `CDH_MEDIA_V4L2_DECODER` 时硬报错阻断创建实例；
   - Linux 精简静态版 `qemu-system-aarch64` 编译时启用了 `--disable-vnc`，无条件传 `-vnc` 导致启动报错退出；
   - Linux 分支原仅挂 2D `virtio-gpu-pci`，需接入独立 `vhost-user-gpu`（`vhost-user-gpu-pci,chardev=stage-gpu,hostmem=256M` + `RENDER_SERVER_EXEC_PATH`）提供 3D virgl/Venus 能力。

## 2. 根因

1. 既有构建流水线与宿主守护进程主要在 macOS（Apple Silicon，`darwin` + APFS + HVF + VideoToolbox + 内嵌 virgl）开发验证，路径探测、xattr 权限模型与 QEMU/GPU 参数存在平台耦合。
2. `SyncWorkspace` 原先只同步 `manifest` 与部分 `hardware`，缺少对内核树（需保留 `.git` 供 `03_build_kernel` 执行 `git archive HEAD`）、`cuttlefish/source`（需保留 `.git` 供补丁工作区克隆）、外部模块、内核补丁与 HAL 预拉取依赖的分项下发机制。
3. 构建容器（Ubuntu 26.04，`GLIBC 2.43` + Mesa 26.0.3）高于运行节点宿主系统（Debian 12 Bookworm，`GLIBC 2.36` + Mesa 22.3），动态链接二进制直接裸跑必触发符号版本缺失。

## 3. 证据

- 失败日志详见同目录 [log-fail.txt](log-fail.txt)。
- 通过与运行实测日志详见同目录 [log-pass.txt](log-pass.txt)。

## 4. 改动

1. **构建源码按需下发能力（后端 + API + Agent 工具 + Web 界面）**：
   - 新增 `agent/core/internal/cluster/sync_sources.go` 与 `sync_sources_test.go`：实现 `ListLocalBuildSources` 与 `SyncBuildSources`，支持按需下发 `manifest`、`hardware`（含 `hardware-hidl` 与 `08_build_hardware/modules`）、`modules`、`kernel-config`、`cuttlefish`（保留 `.git`）、`kernel:<ver>`（保留 `.git`）、`hal-deps:<ver>`，并在下发前执行磁盘红线检查；
   - 在 `agent/core/internal/server/server.go`、`handlers_cluster.go` 暴露 `GET /api/cluster/source-catalog` 与 `POST /api/nodes/{id}/sync-sources`；在 `agent/core/internal/tools/cluster_tools.go` 注册 `cluster.sync_sources` 工具；在 `agent/core/web/src/templates/view-cluster.html` 与 `src/modules/cluster/index.ts` 新增可视化勾选下发面板。
2. **`pipeline` Linux x86_64 跨平台构建与纯 Go EROFS 标签直读**：
   - `internal/halcore/toolchain/toolchain.go`、`profile/profile.go`、`profile/profile_paths.go`、`deps/v412.go`、`halbuild/hidl.go`、`halbuild/cargo.go`：按 `runtime.GOOS/GOARCH` 动态发现 NDK prebuilt、Android SDK、`cargo`、ClangRT `include` 与离线缓存 `linux-x86/hidl-gen`；
   - `devices/hardware/common/libcxx-platform/__config_site`：按 `__clang_major__ < 20` 兼容 NDK r28 与 r29；
   - `internal/env/env.go` & `profile.go`：支持 `<dirname(workspaceRoot)>/roms/lineage` 及 `CDH_ROMS_DIRS` 多目录精确匹配；
   - 新增 `internal/fsimg/erofs_selinux.go`（及 `erofs_selinux_test.go`）：纯 Go 直接解析 EROFS 磁盘结构（Compact/Extended inode、shared/inline xattr、FLAT_PLAIN/FLAT_INLINE 目录）读取全部条目的 `security.selinux`，并在 `09_build_vendor/officialunpack.go`、`setxattr.go`、`02_prepare/prop.go` 中对非 `darwin` 宿主省略 `--xattrs` 解包。
3. **Linux ARM64 运行栈自包含封装与 `daemon` 适配**：
   - 新增 `build/qemu/bundle-linux-glibc-runtime.sh`（并接入 `build-qemu-linux` / `build-cuttlefish-linux`）：从构建镜像提取自包含 `ld-linux-aarch64.so.1` + glibc + Mesa 26.0.3（`dri_gbm.so`、`panthor_dri.so`、`swrast_dri.so`、`libgallium` 等）并生成保持 PID/FD 透传的 `exec` 包装器；
   - `agent/modules/daemon/backend/media.go` & `run/hosts.go`：Linux 未配置 `CDH_MEDIA_V4L2_DECODER/ENCODER` 时优雅跳过硬件媒体透传；
   - 新增 `agent/modules/daemon/backend/gpu.go` 并更新 `run/qemu.go`：探测 QEMU `-vnc` 支持以兼容 `--disable-vnc` 静态构建；Linux 下自动拉起独立 `vhost-user-gpu`（注入 `RENDER_SERVER_EXEC_PATH`）并挂载 `vhost-user-gpu-pci,chardev=stage-gpu,hostmem=256M` + `memory-backend-memfd`。

## 5. 验证

1. **源码下发与 `rom-main` 13 阶段全量制作**：
   - 将 `manifest`、`hardware`、`modules`、`kernel:6.1`、`cuttlefish`、`hal-deps:android14` 下发至 `rom-main:/home/backup/cloud-phone-hub/runtime`；
   - 在 `rom-main` 实地执行 `pipeline device build aito --manifest 16-W1UCS36H.96-35-8-9-20260801`，13/13 阶段全绿通过（耗时 276s）：编出 `6.1.138-4k-cdh` 内核、13 个 `.ko` 模块、23 个 Android 14 HAL 模块，直读 4183 条官方 EROFS SELinux 标签真值，产出 `vda.img`（23.00 GiB）、`patch.img`（384.0 MiB，`sha256 d27ab79fb84a`）、`data.qcow2`、`device.json`。
2. **跨机免密直传与 `rom-main-runtime` 实机拉起**：
   - 经 `POST /api/nodes/sync` 以 `remote-direct-rsync` 免密直传 25.1 GB 盘集至 `rom-main-runtime`（实传 8.21 GB，加速比 3.06）；
   - 在 OrangePi 5 Plus（Debian 12，`GLIBC 2.36`）实测 `vhost-user-gpu --probe-gpu` 返回 `gl_renderer: "Mali-G610 MC4 (Panfrost)"`, `hardware: true`, `PROBE_EXIT=0`；
   - 成功创建并拉起 KVM 实例（`aito_16-W1UCS36H.96-35-8-9-20260801/1`，`adb=5911`），`adb devices -l` 在线识别 `model:motorola_razr_50 device:aito`（Android 16），`uname -a` 生效 `6.1.141-android14-11-...`，`lsmod` 全部 13 个模块在位，`cdh_redirect` 21/32 规则命中 `62244` 次，`keymint`、`service_daemon`、`wifi`、`bluetooth`、`composer`、`allocator`、`camera`、`audio`、`radio` 等 16 个自编译 HAL/守护服务全部正常运行。
3. **单元测试全绿**：
   - `agent/core`（`internal/cluster`、`internal/server`、`internal/tools`）、`agent/modules/pipeline`（`env`、`fsimg`、`halcore/*`、`02_prepare`、`08_build_hardware`、`09_build_vendor`）、`agent/modules/daemon`（`backend/...`、`run/...`）全部单测 PASS。

## 6. 遗留

- 首轮 `vhost-user-gpu-pci` 未带 `,hostmem=256M` 参数导致 guest `virtio_gpu` 协商缺少 `RESOURCE_BLOB`/`CONTEXT_INIT` 并报 `timed out waiting for cap set 2`（使 `SurfaceFlinger` `eglInitialize` 未完成）；代码与 `out/agent/dist/daemon-linux-arm64/daemon` 已补齐 `,hostmem=256M` 与 `RENDER_SERVER_EXEC_PATH`，因收尾时 `192.168.31.207` 节点物理断电离线（ARP `INCOMPLETE`），待该节点重新上电后重推一次 `daemon` 重启实例即可复核桌面渲染。
