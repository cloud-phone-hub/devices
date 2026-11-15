# Issue: repo manifest 移除 core/engine 并更新 devices/kernels 声明

- 设备: 通用（仓库清单与目录结构）
- 状态: **已闭环**（2026-11-15）

---

## 一、问题

`manifest/default.xml` 仍把 `engine` 与 `core/{host/daemon,development/device-checker,downloader,downloader2}`
声明为 repo 项目，但这些内容已分别迁入 `agent/modules/`、`devices/` 与仓库根 `roms/`；
`manifest/kernel.xml` 仍把内核树与 KernelSU 源码声明在 `engine/kernels`、`engine/modules`。
继续保留会让 `repo sync` 把已删除/已迁走的目录重新拉回旧路径，并与现有结构冲突。

## 二、根因

前几轮只改了工作区目录与代码访问器，没有同步 repo manifest；manifest 里的
`<project path>` 是独立于代码的声明源，必须单独维护。

## 三、证据

- `default.xml` 声明 7 个项目，其中 `engine` 在磁盘上只剩 `.gitignore` 与 980 GB
  构建产物 `engine/out`，`core/host/daemon`、`core/development/device-checker` 已不存在；
- `kernel.xml` 的 8 个项目路径全部带 `engine/` 前缀，而内核树已在
  `devices/kernels/{5.10,5.15,6.1,6.6,6.12,6.18,patches}`，KernelSU 源码在
  `devices/modules/kernelsu/source`；
- 原始检查与迁移后校验见同目录 [log-fail.txt](log-fail.txt)、[log-pass.txt](log-pass.txt)。

## 四、改动

1. **default.xml**：删除 `engine`、`core/host/daemon`、`core/development/device-checker`、
   `core/development/downloader`、`core/development/downloader2` 项目声明；
   注释改为新布局说明（流水线/运行/校验收进 agent，设备输入收进 devices，ROM 在根 roms）。
2. **kernel.xml**：`engine/kernels` → `devices/kernels`、
   `engine/modules/kernelsu/source` → `devices/modules/kernelsu/source`；
   注释中的阶段号与 `.gitignore` 归属同步（03_build_kernel / 04_build_modules、
   `devices/.gitignore`）。
3. **目录处置**：删除 `core/`（14 GB downloader）；先把其中唯一一份未在 roms 的
   `OP_11_CPH2449_16.0.5.702_EX01.zip` 移到 `roms/devices/oneplus/CPH2449/` 再删；
   删除 `engine/.git` 与 `.gitignore`，保留 `engine/out` 作为构建产物工作区。
4. **devices/kernels 配置**：`patches/README.md`、`patches/6.6/README.md`、
   `devices/modules/kernelsu/README.md` 中的引擎路径、阶段号与构建命令同步。

## 五、验证

- 4 个 manifest XML 均可解析；`default.xml` 只剩 `skills`、`build`，
  `kernel.xml` 8 个项目全部在 `devices/` 下，无 `engine/` 或 `core/` 路径；
- `cd agent && go build ./...` 通过；
- `engine/out/bin/cdh device list` 正常（引擎根定位依赖 `engine/out`，未受影响）。

## 六、遗留

- `engine/out` 仍有 980 GB 构建产物（dist / roms / devices / tmp 等），本次**未删除**；
  若确定要移除 `engine` 目录本身，需要先决定这些产物迁到哪个输出根（当前代码
  `engine.OutRoot()` 固定为 `<仓库根>/engine/out`）。
- 其他已同步过 manifest 的机器需要重新 `repo sync` 才会按新路径清理旧项目目录。
- `core/development/downloader` 的源码未保留（按“core 不需要了”处理）；
  如需恢复，从 `cloud-phone-hub/downloader` 仓库重新拉取。
