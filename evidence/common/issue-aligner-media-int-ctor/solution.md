# aligner `media_full` 整域空采（桥 `newInstance` 不做 Double→int）+ 慢机超时

- 范围：`core/development/device-checker/checks/java/aligner/`（设备无关采集套件），跨机型成立
- 实测设备：gold 真机 `07cf5fc1784783d5`（Redmi Note 13 5G，OS3.0.11.0.VNQCNXM / Android 15）、
  oriole 真机 `b8f75582353b320a`（Pixel 6，Android 14）
- 日期：2026-11-13；全量采集报告：[`report/aligner/20261113/`](../../../../report/aligner/20261113/README.md)；完整性报告：[`completeness.md`](../../../../report/aligner/20261113/completeness.md)

## 一、问题

19 域全量采集中，`aligner.media_full` 在两台真机上都「成功」返回，但内容为空：
`error=MediaCodecList_unavailable`、`decoderCount=0`、`encoderCount=0`，耗时只有 35ms / 4ms
（`run-00054`，见 `log-fail.txt`）。状态是 `succeeded`，不看内容就会误以为编解码域已对齐。

另外 gold 上 `camera_full`（35s）、`proc_full`（30s）在仓库 manifest 的超时内跑不完，服务端回
`执行超时`，结果整条丢失。

## 二、根因

1. `media_full.js` 用 `J("android.media.MediaCodecList").newInstance([0])` 构造。宿主桥的
   `newInstance` 只按参数个数挑构造器后直接 `con.newInstance(...)`，**不走** `call` 路径里的数值
   强转；Rhino 数字字面量是 `Double`，于是抛
   `IllegalArgumentException: ... argument 1 has type int, got java.lang.Double`，被 `safeCall`
   吞掉 → `mcl=null` → 空快照。旧 `media/mediacodec.js` 同样写法、同样失效（`dev/test_media.js`
   早已用 `Integer.valueOf` 绕开，但没同步到 aligner）。
2. 注释「0=ALL_CODECS」写反：平台常量 `REGULAR_CODECS=0`、`ALL_CODECS=1`。
3. 超时：gold 的 Rhino 执行比 oriole 慢 5~10 倍（同脚本 camera 41.1s vs 7.9s、proc 35.3s vs 4.6s、
   native_env 106.9s vs 15.7s），manifest 的 `timeoutMs` 是按快机定的。

## 三、证据

- `log-fail.txt`：`run-00054` 两机空快照原文 + 调试脚本 `dbg.media` 的异常原文
  （`newInstance([0])` → `got java.lang.Double`；`newInstance(0)` → `无匹配构造器`）。
- `log-pass.txt`：`run-00067` 修复后两机结果（gold 59 解码 / 25 编码 / 22 硬解；oriole 45 / 27 / 11）。
- 修正方案先在 oriole 上用 `dbg.media` 验证：`newInstance([jInt(0)])` → 68（REGULAR）、
  `newInstance([jInt(1)])` → 72（ALL）、`getCodecCount()` = 68，常量含义与 SDK 一致。
- 超时：`engine/out/tmp/aligner-audit/run1.log` 中 gold `camera` 三次 `执行超时`、`build_props`
  熄屏态超时；放宽超时后同脚本 gold camera 实测 `elapsedMs=41098`、proc `35321`。

## 四、改动

- `aligner/media_full.js`：构造参数改 `jInt(listKind)`；`listKind` 运行时反射读
  `MediaCodecList.ALL_CODECS`（取全集，REGULAR 是其子集）；异常文本写进 `error`
  （`MediaCodecList_unavailable: <原因>`），不再被吞；新增 `codecListKind`、`regularCodecCount`
  （`getCodecCount()`，REGULAR 数）两个字段；修正头部注释与 manifest desc。
- `aligner/manifest.json`：`camera_full` 35000→90000、`proc_full` 30000→90000、
  `sensors_full` 30000→60000、`inputs_full` 45000→60000（≥ gold 实测的 2 倍）；
  `sensors_full.desc` 删去未实现的「sensorSample 动态采样可用性」（该能力在 `ids_full.sensorFingerprint`）。
- 同批顺带（审计 P0，防整域丢失）：
  - `aligner/_common.js` `roundNum`：NaN / ±Infinity 改返回字符串 `"NaN"` / `"Infinity"`；
    原样回传会被 `JsJson` 写成裸 `NaN`，服务端 serde 解析失败直接丢包、run 超时。
  - `aligner/ids_full.js`：`collectSensorFingerprint()` 外包 try（指纹段异常不再连带整个 ids 快照失败），
    `toFixed` 前加 `fx()` 守卫（null/非有限数不抛 TypeError）。有效数值下 `fx()` 与 `toFixed` 输出相同：
    gold 修复前后 `sensorFingerprint.hash` 同为 `609e718665025618`；oriole 前后不同
    （`b4a593b3…` → `f6a2dcca…`），逐行比对差在 `aq:` 段——加速度计 Y 轴 `minDiff` 两次采样分别是
    `0.000598` / `0.004786`，是采样量化本身的波动，与本次改动无关（见遗留）。

  - `aligner/ids_full.js`（追加，20:25）：新增 `idOk()`。宿主桥失败时返回 `"Error: …"` 字符串，原来 `availability` 用 `!!值` 判断，会把错误串记成 true；改用 `idOk()` 后 `run-00084/85` gold `gaid`/`soterDeviceId`、oriole `ifaaProcessCmd`/`soterDeviceId` 正确记为 false。

## 五、验证

- 本地：19 个 set 展开 `//@include` 后 `node --check` 全过；`compare_align.py --self-test` 19/19。
- 设备：修复版经 `push_checks.py` 上传，`run-00067` 两机 `media_full` 均有完整清单
  （gold `OMX.MTK.VIDEO.DECODER.*` 硬解在列）；修复版 `ids_full` 复采 `run-00083` 两机 succeeded；
  19 域 × 2 台全部 `succeeded`，字段全量写入 `report/aligner/20261113/`。
- 服务端已恢复为仓库版脚本（`aligner.camera_full` / `proc_full` 读回 `timeoutMs=90000`），调试功能点 `dbg.*` 已删除。

## 六、遗留

- 未做真机 vs 云机分级对比：本轮服务端在线的只有两台真机（无 `-cloud` 设备），`compare_align.py` 未跑。
- 审计（`report/aligner/20261113/audit.md`）列出的其余缺口未改：对比器对齐键错（camera `cameraId`、
  telephony `slotIndex`、native_env `nativeModules.files`）、带 `[...]` 路径的策略不生效、未排序数组、
  `__unreadable__` 未判、vendorTree 3000 上限（gold 实测 `truncated:true`）、camera 值 300 字符截断、
  ultimate 约七成信号未迁入、native 通道缺失等。
- gold 上的 App 是 2026-10-08 版本，无 WS keep-alive；且 `server/src/adapter.rs` 的 `handle_device` 断开时
  按 android_id **无条件** `remove`（旧连接晚于新连接关闭会把新注册删掉），`Devices::insert/remove`
  用 `try_lock` 争用时静默失败——本轮靠「仅不在线时 `am start`、不 force-stop」规避，服务端未改。
- HyperOS 熄屏态下 gold 无法用 `input keyevent` 唤醒（缺 `INJECT_EVENTS`），慢机超时只能靠放宽 `timeoutMs`。
- `ids_full.sensorFingerprint` 的「稳定层」哈希把加速度计 `minDiff`（`aq:`）算进去了，oriole 两次采集
  Y 轴 `minDiff` 不同导致哈希变化——该段不适合放进稳定层，待评估。
