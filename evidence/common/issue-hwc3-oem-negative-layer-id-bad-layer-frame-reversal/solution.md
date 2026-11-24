# #324：HWC3 过滤 OEM 负 `layer` ID 私有哨兵命令根治 `presentOrValidate: BAD_LAYER` 与三槽位旧帧乒乓回放

- 日期：2026-11-24
- 设备 / 范围：跨机型通用（`android.hardware.graphics.composer3` 通用 HAL；首发于 Honor Magic7 RSR `honor/sun`，`sun_16-AQ3A.250214.001-20260904`，实例 `#1`）
- 状态：**已闭环**

## 一、问题

在 Honor Magic7 RSR（`honor/sun`，`1280×2800`，MagicOS / A16 `system` + A15 `vendor`）的 Web VNC 中按住向上滑动长列表（如 `Settings` → `Manage apps`）时，画面反复上下往返跳动（「来回滑动 / 滑不动」），且即使手指停住不动，屏幕仍在若干历史帧之间来回抖动。

前期触控全链路排查（Web noVNC → `cdha` WS 桥 → QEMU VNC/virtio-input → Guest `/dev/input/event1` → Android `InputReader` / `PointerLocation`）已证实触控坐标 **100% 严格单调无回跳**（111 次 `ABS_MT_POSITION_Y` 从 `37599` 单调递减至 `7200`，反向跳变 `0` 次），问题位于显示合成与帧缓冲扫描输出链路。

## 二、根因

1. **跨机型对照排除宿主 QEMU 与 `RE GLES` 共性问题**：
   - 在同一套宿主 QEMU（`virtio-gpu-gl-pci` + `cgl-headless`）上对比 `oriole_17-CP3A.261005.002.A1-20260905` `#1`（同样为 `RE GLES (Ganesh)` + `meow` + `max_frame_buffer_acquired_buffers=3` + `usesClientComposition=true`），实测 133 帧 **`Total reversals: 0`**；而 `sun #1` 复现出 **`Total reversals: 48~63`**，且按住停留期间呈现精确的周期 = 3 死循环回放（`dy = +37, -13, -24` 连续 4 轮重复），全屏 PNG MD5 证实回跳帧与数百毫秒前的旧帧字节级 100% 一致。
2. **`SurfaceFlinger` 日志与 `CdhComposerClient::executeCommands` 探针锁定直接元凶**：
   - 滑动期间 `sun #1` 的 `SurfaceFlinger` 每帧持续报错：
     ```text
     E HWComposer: getDeviceCompositionChanges: presentOrValidate failed for display 4614472116889401600: BAD_LAYER (3)
     E CompositionEngine: chooseCompositionStrategy failed for BOE0B1F: -2147483648 (Unknown error -2147483648)
     ```
   - 在 `CdhComposerClient::executeCommands` 中抓包发现，荣耀 MagicOS 定制 `SurfaceFlinger` 在滑动/交互期间每帧都会往标准 HWC3 `DisplayCommand.layers` 向量末尾追加一条 **负数 `layer` ID 的私有哨兵 `LayerCommand`**（实测捕获 **`L = -1`** 与 **`L = -3`**，单次 3 秒上滑触发 **+102 次**），用于向原厂高通/荣耀 HWC 传递私有扩展信号。
3. **上游 `drm_hwcomposer` 截断验证导致三槽位旧帧乒乓回放**：
   - 标准 HWC3 `createLayer` 仅分配非负 `layer` ID；上游 `drm_hwcomposer`（`hwc3/ComposerClient.cpp` 的 `DispatchLayerCommand()`）遍历到 `L = -1` / `-3` 时，`display->get_layer(command.layer)` 返回 `nullptr`，写入 `CommandError(EX_BAD_LAYER = 3)` 并将 `cmd_result_writer_->has_error_` 置 `true`；
   - 紧随其后的 `ExecutePresentOrValidateDisplay()` → `ExecuteValidateDisplay()` 在第 1478 行命中 `if (cmd_result_writer_->HasError()) return;`，直接丢弃所有 `DisplayChanges`（不回报 `CLIENT` 合成变更）；
   - `SurfaceFlinger` 因 `chooseCompositionStrategy()` 失败而跳过本帧 `RenderEngine` 合成，但后续 `FramebufferSurface` 仍继续轮转 3 个 `1280×2800` 槽位向 KMS 提交 `PresentDisplay()`，将未重绘的 3 个历史旧帧反复翻页回屏幕。

## 三、证据

- 修复前对照与抓包现场见同目录 [`log-fail.txt`](log-fail.txt)：
  - `sun #1` `human_drag` 48 次回跳及 3 槽位精确循环（`+37, -13, -24`）；
  - `oriole #1` 同环境 `slow_micro` 133 帧 `Total reversals: 0`；
  - `sun #1` 滑动期间 `HWComposer: getDeviceCompositionChanges: presentOrValidate failed ... BAD_LAYER (3)` 与 `CompositionEngine: chooseCompositionStrategy failed` 连续报错；
  - `CdhComposerClient` 诊断探针实录：`errs=[(idx=0,code=3)]` 每次均伴随 `layers` 末尾的 `{L=-1(DEAD!)}` 或 `{L=-3(DEAD!)}`（单次 `human_drag` `total_cmd_errors` 由 `72` 暴增至 `174`，增量 `+102`）。
- 修复后全量验证记录见同目录 [`log-pass.txt`](log-pass.txt)：
  - 完整保留原生 120Hz 高刷切换，单次 `human_drag`（79 帧，`cum_y = 0 → -2008px`）**`Total reversals (dy > +2px): 0`**，停留段 `dy = +0px` 纹丝不动；
  - `BAD_LAYER` / `presentOrValidate failed` / `chooseCompositionStrategy failed` 日志计数 **`0`**；`drm_hwcomposer` `Failed to commit frames` 增量 **`0`**。

## 四、改动

- **[`CdhComposer.cpp`](../../../hardware/android.hardware.graphics.composer3/src/CdhComposer.cpp)**（`CdhComposerClient::executeCommands`，零侵入上游 `drm_hwcomposer` 与官方系统镜像）：
  - 在将 `commands` 转发给上游 `inner_->executeCommands(commands, results)` 之前，扫描各条 `DisplayCommand::layers` 是否存在 `lc.layer < 0`；
  - 对不含负 `layer` ID 的原生 AOSP 机型零开销、零拷贝直接透传；
  - 对包含 `lc.layer < 0` 的 OEM 私有哨兵命令，原地通过 `std::remove_if` + `erase` 剔除负数 `layer` ID 条目（自动析构释放可能携带的资源，无 FD 泄漏），避免上游 `drm_hwcomposer` 误报 `EX_BAD_LAYER (3)` 打断整帧 `presentOrValidateDisplay`。
- 重编 `android.hardware.graphics.composer3`（`android15` `default` 与 `oplus` 双变体均通过），按流水线重跑 `09_build_vendor → 11_build_patch → 13_pack_vda` 刷新 `sun` 盘集（配方 `vda-08880d4bfd5b -> vda-d55e2f4d8341`，`patch.img` SHA256 `a535881030ef`），严守 `super.img` 只读直通红线。

## 五、验证

1. **多变体编译回归**：
   - `./out/agent/dist/pipeline build-hardware --android 15 --module android.hardware.graphics.composer3 --refresh`（`default`）通过；
   - `./out/agent/dist/pipeline build-hardware --android 15 --module android.hardware.graphics.composer3 --variant oplus --refresh`（`oplus`）通过。
2. **`sun #1` 真机态回归实测**（已清空临时刷新率设置，保持默认 60/120Hz 动态切刷）：
   - `human_drag`（从 `ManageApplicationsActivity` 顶部按住上滑 110 步 + 停留 0.5s + 松手）：79 帧全程严格单调（`cum_y: 0 → -1892px`，停留 `dy = +0px`，松手惯性减速至 `-2008px`），**`Total reversals (dy > +2px): 0`**；
   - `fast_hold`（35 帧上滑）与 `slow_micro`（123 帧慢速微滑）滑动及按住停留阶段均 **0 次回跳**（仅触底松手后出现 MagicOS 原生底部阻尼回弹）；
   - `logcat` 中 `BAD_LAYER` / `presentOrValidate failed` / `chooseCompositionStrategy failed` 计数 **`0`**；
   - 用户通过 Web VNC 实操确认滑动丝滑跟手、问题已彻底修复。
3. **对照机型基准**：
   - `oriole_17-CP3A.261005.002.A1-20260905` `#1` 在同宿主机 QEMU 环境下 `slow_micro` 133 帧 `Total reversals: 0`；修复路径在无 `layer < 0` 条目时为纯直通零改动，对非 OEM 机型无回归影响。

## 六、遗留

1. 其他同样使用 `android.hardware.graphics.composer3`（Android 15+）的荣耀/高通 OEM 机型在下次重打 `09_build_vendor → 11_build_patch` 时会自动继承本修复，无需额外机型级配置。
2. 如后续遇到使用 HIDL `composer@2.4`（Android 13/14）的荣耀机型出现同类负 `layer` ID 哨兵问题，可参考本条目及 `../issue-full-fleet-hal-backfill-and-health-audit/solution.md` 在 HIDL 侧做对等过滤。
