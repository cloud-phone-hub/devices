# Issue: 修复 nodes.json 中 local 节点遗留的 romsDir

- 设备: 通用（运行态配置与 ROM 根口径）
- 状态: **已闭环**（2026-11-15）

---

## 一、问题

`out/agent/config/nodes.json` 的 `local.romsDir` 仍写着 `/Volumes/work/cloud-phone-hub/engine/roms`，
而 ROM 根在 #239 已迁到仓库根 `roms/`。`Cluster.Manager` 启动时加载该文件，`local` 节点因此
带着不存在的 ROM 目录：`EffectiveRomsDir()` 返回旧路径，跨机脚本的 `CDH_ROMS_ROOT` 与
ROM 扫描都会指向空目录。

## 二、根因

`NewManager` 只在内存里生成正确的 local 默认值（`<engine>/../roms`），但磁盘上的
`nodes.json` 一旦存在就整体覆盖默认值；#239 只改了代码默认值与环境变量，没有同步已持久化的
运行态配置。`romsDir` 非空时 `EffectiveRomsDir()` 直接采用它，不会回落到默认值。

## 三、证据

- 修复前 `nodes.json`：`"romsDir": "/Volumes/work/cloud-phone-hub/engine/roms"`，
  且 `engine/roms` 已不存在（`engine/` 下只剩 `out/`）；
- `cdha` 启动日志的 Agent/Engine 根正常，说明是配置数据过期而非代码路径错误；
- 原始检查与修复后记录见同目录 [log-fail.txt](log-fail.txt)、[log-pass.txt](log-pass.txt)。

## 四、改动

1. `out/agent/config/nodes.json`：`local.romsDir` →
   `/Volumes/work/cloud-phone-hub/roms`（远端节点 `node-cuimingqiang-pc` 的
   `/home/workspace/cloud-phone-hub/roms` 本就符合新口径，未改）。
2. 重启 `cdha serve`（同 token），让 `Cluster.Manager` 重新加载节点表；
   未改任何 Go 代码——默认值 `filepath.Join(engineRoot, "..", "roms")` 已正确。

## 五、验证

- `python3 -c "json.load(...)"` 通过，`local.romsDir` 与远端节点路径均指向各自仓库根的 `roms/`；
- `roms/` 目录存在性：`roms/`、`roms/devices`、`roms/lineage` 均在；
- 管理台重启后 `GET /api/nodes` 返回
  `"romsDir": "/Volumes/work/cloud-phone-hub/roms"`，`HTTP 200`；
- `cdha node list` 正常列出 `local` 与 `node-cuimingqiang-pc`。

## 六、遗留

- `nodes.json` 的其余字段（`distDir` / `deviceRoot` 指向 `engine/out/dist`、`runtimeRoot`
  指向 `out/core/daemon/runtime`）沿用各自模块口径，本次未动；
- 远端节点若也保存了旧版本路径，需要在该机器上单独核对。
