# issue-dsh-only-runtime-slimdown：DSH 成为唯一运行时后的死代码清理

## 问题

cdha 的对话与诊断都已统一走 DSH（`dsh --profile sdk` / `dsh web`），但 `agent/internal`
里仍留着旧自研 Agent 运行时留下的三块代码，没有任何生产调用者：

1. `internal/dshsdk` 里聊天专用的协议面（回调切换、流式提问、恢复会话、子智能体通知）；
2. `internal/llm` 里旧 Agent 的模型调用栈（SSE 流式、按角色路由 + 输入哈希缓存、function
   tool_calls 编解码）——现在这个包只剩「模型列表展示」与「连通性测试」两个用途；
3. `internal/store` 里仅服务于上述 llm 缓存的 `GetCache`/`PutCache`。

另有一处真实缺陷：`runDiagWithDSH` 的 `InitializeParams.Capabilities.UserQuestions` 声明为
`true`，但诊断路径的 `Handlers` 里并没有 `OnUserQuestion`，模型一旦调用提问工具就会收到
`-32601 host does not handle user-question/ask`。

## 根因

这些代码是「DSH 接管前」的历史分层：

- `dshsdk` 的 `SetHandlers` 是为「聊天边车常驻复用」加的（按回合换回调），消息类型与
  `OnStatus`/`OnQueryCompleted`/`OnSubagent*` 回调是为 SPA 前端展示事件加的；
- `llm` 的流式与角色路由是旧聊天 SSE 与旧工具调用闭环的底座，缓存是旧 Agent 的
  「角色 + 输入哈希」去重；DSH 自带会话日志、上下文管理与工具编排，这些全部失去调用者；
- store 缓存目录 `harness/cache` 随之只剩写入方没有读取方。

## 证据

- 全仓引用统计（`grep -rn "dshsdk.<符号>"`，排除定义包）：`Initialize/Query/Wait/Close/
  StderrTail/Launch` 各 ≥1，`SetHandlers/Done/Pid/Prompt/Resume` 全为 0；
- `llm.Complete/CompleteStream/CompleteModelStream/CompleteForRole/SetRole/RegisterProvider/
  ToolCall/ToolDef` 外部引用全为 0；`internal/eval` 不直接调用 llm，评测也是跑
  `agent.Start` → DSH；
- store 的 `GetCache/PutCache` 除自身单测外无调用方；
- 清理前后规模：`internal/llm` 1267 → 773 行（llm.go 115→55、openai.go 566→298、
  router.go 397→287、llm_test.go 189→133），`internal/dshsdk` 732 → 623 行
  （client.go 404→308、protocol.go 173→123），`internal/store` 删 21 行 + 1 个 helper；
- 缺陷旁证：旧 `chat_dsh_pool.go` 曾用 `Capabilities{UserQuestions:true}` 配
  `OnUserQuestion`，该文件在上一轮（#233 后续）已删除，诊断路径只剩下半截声明。

## 改动

| 位置 | 内容 |
| --- | --- |
| `internal/dshsdk/client.go` | 删 `SetHandlers`/`currentHandlers`/`Done`/`Pid`/`Prompt`/`Resume`；`Handlers` 只留 `OnEvent`/`OnApproval`；`handleNotification` 只认 `session.event`，`handleServerRequest` 只认 `approval/request`；去掉 `handlersMu` 读写锁 |
| `internal/dshsdk/protocol.go` | 删 `StatusNotification`/`QueryCompletedNotification`/`SubagentStartedNotification`/`SubagentFinishedNotification`/`UserQuestionRequest`/`UserQuestionCancelled`/`Capabilities`；`InitializeParams` 去掉 `Capabilities` 字段 |
| `internal/agent/dsh_bridge.go` | `Initialize` 不再声明 `Capabilities{UserQuestions:true}`（与诊断路径无提问 handler 的事实对齐） |
| `internal/llm/llm.go` | 删 `StreamDelta`/`ToolCall`/`ToolDef`/`StreamProvider`/`ExtractDescriptionAndSchema`；`CacheBacking` 收窄为 `SecretSource`（只留 `GetSecret`）；`Request` 去掉 `Tools`/`DisableCache`，`Message` 去掉工具字段，`Response` 去掉 `ToolCalls`/`Cached` |
| `internal/llm/openai.go` | 删 `CompleteStream`（约 190 行 SSE 聚合）；`buildChatPayload` 去掉 `stream` 参数与 tools/tool_calls 编解码；响应解析去掉 tool_calls |
| `internal/llm/router.go` | 删 `CompleteModelStream`/`CompleteForRole`/`RegisterProvider`/`SetRole`/`ScriptedProvider.CompleteStream`；`cache` 字段改 `secrets SecretSource` |
| `internal/store/store.go` | 删 `GetCache`/`PutCache`/`cacheKey` |
| 测试 | `llm_test.go` 重写为 `TestPlaintextKeyRejectedAndComplete` + `TestStoreSecretRef`；`store_test.go` 删缓存断言；`agent`/`server` 测试去掉 `RegisterProvider`/`SetRole` 样板 |

## 验证

- `go build ./...`、`go vet ./...`、go test `./...` **50 包全绿**；
- `POST /api/llm/test`（`deepseek-v4.1-flash-1017`）→ `{"latencyMs":1140,"reply":"模型连通正常。"}`；
- `cdha run --goal "SDK 链路自检" --max-steps 2` 实跑一次诊断 Run：`[completed]`，
  输出「DSH 链路正常」——证明精简后的 `Initialize → Query → Wait → Close` 与
  `session.event` 事件映射链路完好；
- 浏览器：DSH 页 `#dsh-web-status=已连接`，模型配置弹窗模型列表 3 项，console 零报错。

## 遗留

- `internal/agent` 与 `internal/checker` 各有一份 3 行 `truncateStr`，`cluster`/`manifestview`/
  `cmd` 各有一份 `firstNonEmpty`——属于跨包微型重复，合并需新建共享小包，收益低于依赖成本，
  本次不动；
- `internal/tool`、`internal/tools`、`internal/mcp` 的导出符号均仍有包内或跨包调用者
  （`ParseRisk`、`ConfigOverview` 等由 `RegisterDefaults` 链路间接使用），无整块可删项。
