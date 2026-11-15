# Issue: roms 输入根从 engine/roms 迁到仓库根 roms

- 设备: 通用（构建流水线与 agent 全链路）
- 状态: **已闭环**（2026-11-15）

---

## 一、问题

`engine/roms`（293 GB）存官方固件包与 lineage UTM 底包，是纯只读输入，却与
`engine/out` 构建产物混在 engine 下。`devices/{manifest,hardware,kernels,modules,evidence}`
迁走后，roms 是最后一块仍挂在 engine 下的输入；engine 仍无法收敛为「只存 out/」。
目标：roms 移到仓库根 `roms/`，engine 只保留 `out/`。

## 二、根因

roms 最初作为 engine 的输入目录与构建系统一起放置，`RomsRoot()` 直接拼
`engineRoot/roms`；`engine.Init()` 也用 `<dir>/roms` 参与根判定。随着 `devices/`
成为设备侧资产根、`agent/` 成为控制面，把 ROM 输入留在 engine 下只是历史遗留。

额外风险：`engine/out/roms/**/.unpacked.json` 的 `package` 字段记录的是旧绝对路径，
完成标记判定用路径全等；若只搬目录，会让全部已完成机型被判为未解包并重解包。

## 三、证据

- 迁移前 `engine/` 顶层为 `out / roms`，`du -sh engine/roms` = 293 GB；
- `engine.RomsRoot()` 返回 `engineRoot/roms`；`isEngineRoot` 要求 `out/` + `roms/`；
- 直接拼接：`cluster.go:84,182`、`server/roms.go:55,118`、
  `manifestview/inspector.go:779`；
- 现有 marker：
  `{"package":"/Volumes/work/cloud-phone-hub/engine/roms/devices/.../gold_images_....tgz"}`；
  `resolveMarkerPackage` 对绝对路径原样返回，`readMarker` 只做路径全等。
- 迁移前后命令与输出见同目录 [log-fail.txt](log-fail.txt)、[log-pass.txt](log-pass.txt)。

## 四、改动

1. **目录迁移**：`engine/roms` → 仓库根 `roms/`（同盘 mv，293 GB 原样保留）；
   engine 顶层只剩 `out/`。
2. **路径访问器**：`engine.RomsRoot()` 改为 `<仓库根>/roms`。
3. **根判定**：`isEngineRoot(dir)` 改为 `out/` 存在且父目录是仓库根
   （`engine/` + `devices/manifest`），不再依赖 `roms/`；
   `searchUp` 从 `roms/`、`devices/`、仓库根执行均可命中。
4. **调用方**：
   - `cluster`：`EffectiveRomsDir` / local 节点 `RomsDir` 改为 `<engine>/../roms`；
   - `server/roms.go`：固件扫描根改用 `tools.RomsRoot(engineRoot)`，
     `engine/out/roms` 仍作为解包输出；
   - `manifestview`：输入 roms 根改为 `<engine>/../roms/devices`；
   - `tools` 新增 `WorkspaceRoot/RomsRoot`；
   - SSH 目录探测的 engine 标记改为 `out/ + ../devices/manifest`，ROM 标记兼容
     `roms/` 与 `../roms/`。
5. **完成标记兼容**：`01_unpack/official.go` 的 `readMarker` 在路径全等失败时，
   增加「同名 + 同大小 + kind」兜底，避免 roms 根搬家触发全量重解包；
   `prepare.go` 原有同类兜底保持。
6. **忽略与文档**：根目录 `.gitignore` 新增 `roms/`，`engine/.gitignore` 移除
   `roms/`；pipeline/env/01 README、AGENTS 第 5 条与证据树内导航路径同步为
   `roms/...`（历史 `log-*.txt` 保持事实快照）。

## 五、验证

- `cd agent && go build ./... && go vet ./... && go test ./...`：全绿；
- `cdh device build gold --dry-run`：厂商固件与 lineage 底包路径均为
  `/Volumes/work/cloud-phone-hub/roms/...`；
- 从仓库根与 `roms/` 目录执行 `cdh device list` 均正常（根定位不依赖 cwd）；
- `cdha /api/roms`：扫描到 42 个官方固件包（路径在 `roms/devices`）；
- `cdha /api/manifest/overview`：32 设备 / 36 模板 / 40 版本 / 42 ROM / 45 HAL；
- 旧 `.unpacked.json` 标记通过同名 + 大小兜底命中，不触发重解包。

## 六、遗留

- `engine/out/roms/**` 里旧标记仍保存迁移前的绝对路径，靠代码兜底兼容；
  下一次真正重解包会写成相对 `<仓库根>/roms` 的新路径；
- 历史 `log-*.txt` / `agent/out/harness` 中的旧路径不改写；
- 远端构建节点需把 roms 放在仓库根（或显式配置节点 `romsDir`）；
- `engine/out` 仍按本次范围保留在 engine 下。
