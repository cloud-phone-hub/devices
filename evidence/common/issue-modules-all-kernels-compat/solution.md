# 议题 #157：`devices/modules/` 全部外部内核模块兼容 `devices/kernels/` 全版本（`5.10` / `6.1` / `6.6` / `6.12` / `6.18`）

- **设备/机型**：通用（`common/`，覆盖 `devices/modules/` 下全部 6 个外部模块与 `devices/kernels/` 下全部 5 棵内核树 `5.10` / `6.1` / `6.6` / `6.12` / `6.18`）
- **阶段**：`03_build_modules`、`devices/modules/*`、`internal/modmeta`
- **日期**：2026-11-04

## 一、问题

`devices/kernels/` 下维护了五棵内核源码树：`5.10`、`6.1`、`6.6`、`6.12`、`6.18`；而 `devices/modules/` 下的 6 个外部内核模块（`cdh_redirect`、`device_info`、`goodix_fingerprint`、`input_identity`、`kernelsu`、`virtio_media`）此前仅针对 `6.6`/`6.12`（部分含 `6.1`）开发和声明：
1. `goodix_fingerprint`、`input_identity`、`virtio_media` 的 `module.json` 仅声明 `"kernel": ["6.6", "6.12"]`，`kernelsu` 缺少 `"5.10"`，且全部 6 个模块均未包含 `"6.18"`；
2. 在 `5.10` / `6.1` / `6.18` 上实际编译 `virtio_media` 与 `kernelsu` 时直接报错或产生未导出未定义符号（见 `log-fail.txt`）。

## 二、根因

逐模块、逐内核版本实测暴露出三类跨版本不兼容：

1. **`virtio_media` 在 `5.10` / `6.1` 上的三处内核 API 差异**：
   - `virtio_media_driver.c:29` 无条件 `#include <media/frame_vector.h>`，而 `5.10` 中 `struct frame_vector` 定义在 `<linux/mm.h>`，`<media/frame_vector.h>` 自 Linux 5.15 起才独立拆出；
   - `scatterlist_builder.c:261` 调用三参 `vb2_create_framevec(userptr, length, write)`，而 `5.10` 与 `6.1` 的 `include/media/videobuf2-memops.h` 签名为双参 `vb2_create_framevec(unsigned long start, unsigned long length)`（第 3 参数 `bool write` 在 Linux 6.5 才恢复）；
   - `virtio_media_driver.c:960` 调用 `virtio_reset_device(virtio_dev)`，该封装在 Linux 5.19 才引入，`5.10` 中需直接调用 `virtio_dev->config->reset(virtio_dev)`。

2. **`kernelsu` 的 `0003-ksym-shim.patch` 在 `5.10` / `6.1` / `6.6` / `6.12` / `6.18` 间的未导出符号分流缺失**：
   - `source/kernel/infra/file_wrapper.c` 按内核版本分三路创建匿名安全 inode：
     - `>= 6.8`（`6.12` / `6.18`）调用已导出的 `anon_inode_create_getfile`；
     - `5.16 .. 6.7`（`6.1` / `6.6`）调用未导出的 `anon_inode_getfile_secure`；
     - `< 5.16`（`5.10`）自建 `ksu_anon_inode_make_secure_inode()` 并调用未导出的 `security_inode_init_security_anon`。
   - `source/kernel/policy/app_profile.c:188` 仅在 `LINUX_VERSION_CODE >= KERNEL_VERSION(5, 14, 0)` 时调用未导出的 `set_cred_ucounts`（`5.10` 内核无此符号）。
   - 原 `0003-ksym-shim.patch` 在 `ksu_ksym_table` 与 `ksym_tramp.S` 中写死了 `6.6` 的符号集（无条件查 `anon_inode_getfile_secure` 与 `set_cred_ucounts`，且漏掉 `security_inode_init_security_anon`），导致：
     - `5.10` 编译期 MODPOST 报 `WARNING: modpost: "security_inode_init_security_anon" [kernelsu.ko] undefined!`，且运行期 `ksu_ksym_init()` 会因 `vmlinux` 不存在 `anon_inode_getfile_secure` 和 `set_cred_ucounts` 而失败退出；
     - `6.12` / `6.18` 运行期 `ksu_ksym_init()` 会因 `vmlinux` 不存在 `anon_inode_getfile_secure`（已改名 `anon_inode_create_getfile`）而报 `ksym: unresolved: anon_inode_getfile_secure` 退出。

3. **`kernelsu` 在 `6.18` 下的 `KSU_KERNEL_DIR` 头文件路径错位**：
   - `6.18` 的 kbuild 在 `M=` 外部模块构建时导出 `$(srcroot)` 为外部模块顶层目录（`.../modules/kernelsu`）；
   - `source/kernel/Kbuild` 第 80 行写 `KSU_KERNEL_DIR := $(srcroot)`（假定 `M=` 直接指向 `source/kernel`），而本仓通过顶层 `kernelsu/Kbuild` 的 `obj-$(CONFIG_KSU) += source/kernel/` 递归进入，导致 `-I$(KSU_KERNEL_DIR)` 指向 `kernelsu/` 而非 `kernelsu/source/kernel/`，编译报 `fatal error: 'feature/sulog.h' file not found`。

## 三、证据

- **失败日志**（[`log-fail.txt`](log-fail.txt)）：
  - `5.10` 编译 `virtio_media` 报 `fatal error: 'media/frame_vector.h' file not found`、`vb2_create_framevec` 参数过多、`implicit declaration of function 'virtio_reset_device'`；
  - `5.10` 编译 `kernelsu` 报 `WARNING: modpost: "security_inode_init_security_anon" [kernelsu.ko] undefined!`，且 `vmlinux` 符号表对账显示 `5.10` 无 `anon_inode_getfile_secure`/`set_cred_ucounts`、`6.12` 无 `anon_inode_getfile_secure`；
  - `6.18` 编译 `kernelsu` 报 `source/kernel/feature/sulog.h file not found`。
- **通过日志**（[`log-pass.txt`](log-pass.txt)）：
  - 修复后在 `5.10`（`5.10.270-4k-cdh`）、`6.1`（`6.1.168-4k-g9bb44cf2db60`）、`6.6`（`6.6.127-4k-cdh`）、`6.12`（`6.12.81-4k-cdh`）、`6.18`（`6.18.32-4k-g604b22fb250f`）五棵内核树上编译全部 6 个模块（30 个 `.ko` 组合）全部通过，`modmeta` 等单元测试全绿。

## 四、改动

1. **`devices/modules/virtio_media/`**：
   - `virtio_media_driver.c`：`#include <media/frame_vector.h>` 加 `#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 15, 0)` 门控；`virtio_media_remove()` 中 `< 5.19.0` 分支回退调用 `virtio_dev->config->reset(virtio_dev)`；
   - `scatterlist_builder.c`：引入 `<linux/version.h>`，`< 6.5.0` 分支调用双参 `vb2_create_framevec(userptr, length)`。
2. **`devices/modules/kernelsu/patches/0003-ksym-shim.patch`**：
   - `infra/ksym.c` 与 `infra/ksym_tramp.S` 引入 `<linux/version.h>`，按与调用方一致的宏条件编译分流三项版本相关符号：
     - `anon_inode_getfile_secure`：`5.16.0 <= LINUX_VERSION_CODE < 6.8.0`；
     - `security_inode_init_security_anon`：`LINUX_VERSION_CODE < 5.16.0`；
     - `set_cred_ucounts`：`LINUX_VERSION_CODE >= 5.14.0`。
   - `source/kernel/Kbuild`：将 `ifneq ($(srcroot),)` 分支下的 `KSU_KERNEL_DIR` 改为 `$(patsubst %/,%,$(dir $(abspath $(lastword $(MAKEFILE_LIST)))))`，兼容 `6.18` 顶层 `Kbuild` 递归构建。
3. **元数据、文档与单测**：
   - `devices/modules/{cdh_redirect,device_info,goodix_fingerprint,input_identity,kernelsu,virtio_media}/module.json` 的 `"kernel"` 统一扩充为 `["5.10", "6.1", "6.6", "6.12", "6.18"]`；
   - `devices/modules/README.md` 模块支持矩阵同步更新；
   - `engine/build/internal/modmeta/modmeta_test.go` 的 `TestEngineModulesCatalog` 断言全部 6 个模块通过 `5.10`、`6.1`、`6.6`、`6.12`、`6.18` 的 `CheckKernel` 校验。

## 五、验证

1. **五棵内核树 × 六个外部模块编译验证**：
   - `5.10`（构建卷 `cdh-kernel-5.10-cdh-77627c213cec`，含完整 `vmlinux` 与 `Module.symvers`）：6 个 `.ko` 全部编译 + MODPOST 零警告通过（`vermagic: 5.10.270-4k-cdh`）；
   - `6.6`（构建卷 `cdh-kernel-lineage-22-6.6-cdh-4f2152cbe2de`，含完整 `vmlinux` 与 `Module.symvers`）：6 个 `.ko` 全部编译 + MODPOST 零警告通过（`vermagic: 6.6.127-4k-cdh`）；
   - `6.12`（构建卷 `cdh-kernel-lineage-23-6.12-cdh-b4fe1c987b3a`，含完整 `vmlinux` 与 `Module.symvers`）：6 个 `.ko` 全部编译 + MODPOST 零警告通过（`vermagic: 6.12.81-4k-cdh`）；
   - `6.1` 与 `6.18`（挂载源码树 `modules_prepare`）：6 个 `.ko` 全部编译通过（`vermagic: 6.1.168-4k-g9bb44cf2db60` / `6.18.32-4k-g604b22fb250f`）。
2. **单元测试**：
   - `cd engine/build && go test -count=1 ./internal/modmeta ./02_build_kernel ./03_build_modules` 全绿通过。

## 六、遗留

- `6.18` 目前尚无绑定机型基线运行实例，后续接入 `6.18` 机型时需在真机/云机实例上复核 `kernelsu` 与 `device_info` 的运行态行为。
