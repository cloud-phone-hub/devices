# Issue #251: `02_prepare` 解析 DSH 设备规格 JSON 时遇到浮点物理尺寸或字符串数值导致反序列化失败

- 设备：跨机型（`02_prepare` 真机规格采集链路）；本轮在 `google/oriole` 首次触发
- 阶段：`02_prepare` / `device apply-profile`
- 状态：**已闭环**（2026-11-18）

## 一、问题

在执行 `cdh-pipeline device apply-profile google/oriole --manifest 14-AP2A.240905.003.F1-20240815 --recollect` 通过 DSH Web 聊天界面采集 Pixel 6 真机规格时，DSH 会话（`session-b2aec904-021b-4fbe-a7ad-b03269d812d0`）正常输出了完整的 JSON 回复，但解析阶段报错退出：

`解析 DSH 聊天回复中的设备规格 JSON 失败: json: cannot unmarshal number 66.7 into Go struct field .display.widthMm of type int`

## 二、根因

`agent/modules/pipeline/stages/02_prepare/ai_profile.go` 中的 `AIDeviceHardwareSpec` 结构体对 `.display.widthMm`、`.display.heightMm`、`.display.edidProductId` 等大量字段使用原生 `int`，而 `parseAIDeviceProfileReply` 直接调用标准库 `json.Unmarshal([]byte(raw), &spec)`。当大模型在回复中给出带小数的物理尺寸（如 `66.7` mm）、浮点整数（`2400.0`）或字符串形式/十六进制数值（如 `"420"`、`"0x8b36"`）时，标准 JSON 解码器会直接拒绝，导致整轮 DSH 采集结果作废。

## 三、证据

- 失败日志原文：[log-fail.txt](log-fail.txt)
- 通过日志原文：[log-pass.txt](log-pass.txt)

## 四、改动

1. 在 `agent/modules/pipeline/stages/02_prepare/ai_profile.go` 中引入 `reflect`，新增 `unmarshalAIDeviceSpecLenient(raw []byte, dst *AIDeviceHardwareSpec) error` 与 `coerceJSONToType(val any, t reflect.Type) any`，并在 `parseAIDeviceProfileReply` 中替换原有的直接 `json.Unmarshal`：
   - 优先尝试标准 `json.Unmarshal(raw, dst)`，无类型偏差时零额外开销；
   - 仅当标准反序列化报错时，解为通用 `any`，按目标结构体 `reflect.Type` 递归归一化：`int`/`uint` 目标兼容 `float64`（`math.Round` 四舍五入）及十进制/十六进制字符串或浮点字符串；`float64` 目标兼容数字字符串；`bool` 目标兼容 `"true"/"false"/"1"/"0"` 与数值；`string` 目标兼容数字/布尔字面量；`struct`/`slice`/`map`/`ptr` 按 `json` tag 递归处理。
2. 在同目录 `agent/modules/pipeline/stages/02_prepare/ai_profile_test.go` 新增单元测试 `TestParseAIDeviceProfileReplyLenientTypes`，覆盖小数 `widthMm=66.7`、字符串整数、十六进制字符串和浮点数组元素等场景。

## 五、验证

- 单元测试：`cd agent && go test -count=1 ./modules/pipeline/stages/02_prepare/...` 与 `go test ./internal/...` 全量通过；
- 实机回归：重编 `out/agent/dist/cdh-pipeline` 后重跑 `cdh-pipeline device apply-profile google/oriole --manifest 14-AP2A.240905.003.F1-20240815 --recollect`，成功完成 `session-ced36e45-d244-4a35-8701-a47fb3e7d257` 采集，面板尺寸自动归一化为 `67x148mm` 并生成入库 `devices/manifest/devices/google/oriole/device-profile.{json,md}` 与反补模版配置。

## 六、遗留

- 非法非数值文本（如 `"N/A"`）落入整数字段时仍按失败拒绝，避免静默吞掉格式彻底损坏的响应。
