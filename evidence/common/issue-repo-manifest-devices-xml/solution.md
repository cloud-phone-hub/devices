# Issue: repo manifest 为 devices/ 建立 devices.xml 仓库声明

- 设备: 通用（仓库清单与目录结构）
- 状态: **已闭环**（2026-11-15）

---

## 一、问题

`devices/` 已成为设备侧输入的唯一落点（`manifest/` 机型三表、`hardware/` 自研 HAL 源码、
`modules/` 外部内核模块、`evidence/` 六段式证据），但 `manifest/` 下没有任何文件把
`devices/` 声明成 repo 项目：`default.xml` 只声明了 `skills`、`build`，`kernel.xml` 只覆盖
`devices/kernels/*` 与 `devices/modules/kernelsu/source`。新机器 `repo sync` 后拿不到
`devices/` 的主体内容，只剩内核树与 KernelSU 源码。

## 二、根因

manifest 是独立于磁盘目录的声明源，前几轮只搬了目录、同步了 `kernel.xml` 的既有点位，
没有为 `devices/` 主体新建声明文件。同时 `devices/kernels/*`、`devices/modules/kernelsu/source`
本身就是 `devices/` 下的独立上游项目，父项目声明必须与它们共存（路径嵌套），旧写法里
没有可参照的先例。

## 三、证据

- `default.xml` 的 include 列表只有 `qemu.xml` / `cuttlefish.xml` / `kernel.xml`，
  项目只有 `skills`、`build`；
- `manifest/` 目录只有 4 个 XML，无 `devices.xml`；
- 本机没有 `devices/.gitignore`，父项目若整体检出，`kernels/`（17 GB）与
  `modules/kernelsu/source/`（上游检出）都会被当成父仓库内容；
- 原始检查与验证记录见同目录 [log-fail.txt](log-fail.txt)、[log-pass.txt](log-pass.txt)。

## 四、改动

1. **新增 `manifest/devices.xml`**：一个 `path="devices" name="devices"` 的
   `cloud-phone-hub` 项目覆盖设备侧全部内容；文件头注明 `devices/kernels/*`、
   `devices/modules/kernelsu/source` 由 `kernel.xml` 嵌套声明，以及 `devices/.gitignore`
   的排除口径。
2. **`manifest/default.xml`**：include `devices.xml`（排在 `kernel.xml` 前，外层→内层），
   模块清单注释与「engine/core 已迁出」说明同步为 `devices`（见 devices.xml）。
3. **新增 `devices/.gitignore`**：排除 `kernels/`、`modules/kernelsu/source/`、
   `**/.DS_Store`，让父项目只跟踪设备侧自研内容，嵌套子项目所在目录对父仓库不可见。
4. 未改任何代码：`engine.DevicesRoot()` / `ManifestRoot()` 等访问器只看磁盘路径，与 repo 声明无关。

## 五、验证

- 5 个 manifest XML 均可解析，项目路径无重复；合并后共 43 个项目，
  `devices` 顶层项目存在，`kernel.xml` 的 8 个项目路径不变；
- 用真实 manifest 建 `repo init` 客户端，`repo manifest -o -` 合并清单同时列出
  `project name="devices"` 与 `devices/kernels/{5.10,5.15,6.1,6.6,6.12,6.18,patches}`、
  `devices/modules/kernelsu/source`，无「duplicate path」或嵌套报错；
- 同形态沙盒（父 `devices` + 子 `devices/kernels/5.10`，父 `.gitignore` 排除 `kernels/`）
  实跑 `repo sync` 成功，`repo status` 与父仓库 `git status` 均为空；
- `cd agent && go build ./...` 通过（本改动不触及代码）。
- **远端已落地（2026-11-15）**：在 GitHub 组织下创建 `cloud-phone-hub/devices`（public，
  默认分支 `main`），`devices/` 内 `git init` 后首次提交 `a2c882f` 推送成功；
  `git ls-remote origin` 与 `origin/main` 同步，远端 1589 个文件，`git clone --depth 1`
  拉回后 `evidence` / `hardware` / `manifest` / `modules` 四棵目录齐全、工作区干净。
  该仓库本地提交时显式设 `core.autocrlf=false`，避免证据日志里的 CRLF 被改写。

## 六、遗留

- 未做真实远端 `repo sync`：目前是「manifest 能解析 + 沙盒 sync + 真实远端可克隆」三件事分别
  验证过，还没有一台干净机器按 `repo init -u <manifest 仓> -m default.xml` 全量同步过
  `devices` 项目，首次换机时需补这一步；
- `devices/hardware`（522 MB，46 棵 HAL 树）单仓入库，GitHub 未告警（最大文件远低于 100 MB），
  但首次 clone 成本偏高，是否拆成独立仓库仍待定；
- `hardware/android.service.daemon/src/{resetprop/external,target}` 被该子树上游自带的
  `.gitignore` 排除，仓库里不含这两处 vendor 内容（nanopb、system_properties 本地仍在），
  换机后需按上游脚本重新拉取。
