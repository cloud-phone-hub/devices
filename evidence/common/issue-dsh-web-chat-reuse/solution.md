# issue-dsh-web-chat-reuse：复用 DSH 官方 Web 聊天界面（iframe 页签）

## 问题

要把 cdha 自研聊天页逐步替换为 deepseek-harness（DSH）官方 Web 聊天界面，以减少自研 UI
缺陷并在 DSH 上游更新时直接复用。接入过程卡在三处：

1. 自包含运行时 `agent/hareness-sdk` 被认为缺 Web 包（`dsh-web-app` / `dsh-web-frontend`）；
2. `dsh web` 起来后界面提示「选择一个工作区开始」，工作区列表为空、「添加工作区」只能靠
   系统目录选择器（`directoryPicker/pick`，原生对话框），自动化/首次体验都卡死；
3. 发消息即「处理失败：`llm-deepseek: no API key for provider route "deepseek-official"`」
   （`MISSING_CREDENTIAL`）——web 会话默认模型路由没走我们的 `cdh-maas`。

## 根因

1. **闭包其实完整**：`pnpm deploy --filter dsh-python-runtime-closure` 的依赖闭包里
   `@deepseek-ai/dsh`（apps/cli）把 `dsh-web-app` 声明为 **dependencies**，`dsh-web-app`
   又依赖 `dsh-web-frontend`（含前端 dist）。它们都在 `hareness-sdk/node_modules/.pnpm`
   虚拟存储里、经 `.pnpm/@deepseek-ai+dsh@*/node_modules/@deepseek-ai/` 链接可达，
   只是没有提升到顶层 `node_modules/@deepseek-ai/`。bundle 解析走
   `createRequire(installAnchor).resolve.paths`，链路可达即可，无需扩闭包部署。
2. **工作区是 DSH 侧存储**：web 会话必须选定工作区（`DSH_HOME/storages/workspace.json`，
   home 级、跨 profile 共享）。空表时 UI 只给原生目录选择器；上游有幂等的
   `workspace/create` 网关端点（`{type:"client-request", method:"workspace/create",
   payload:{args:{request:{path}}}}`，响应 `{type:"server-response", result:{ok,value}}`），
   但没有任何预注册入口。
3. **默认模型路由写死**：composed profile 树里 `agent-default-model`
   （`@deepseek-ai/dsh-agent-default-model`）默认 `provider: deepseek-official,
   model: deepseek-flash`，cdha 的 patch 只注册了 `llm-pi-ai` 的 `cdh-maas` provider 与
   模型列表，没有覆盖默认路由；web/TUI 会话不显式选模型就必然走 deepseek-official。

## 证据

- `ls agent/hareness-sdk/node_modules/.pnpm/ | grep web` → 存在
  `@deepseek-ai+dsh-web-app@…`、`@deepseek-ai+dsh-web-frontend@…`（dist 齐全）、
  `@deepseek-ai+dsh-host-frontend-static@…`；
- `dsh web --port 0 --no-open`（自包含副本，`DSH_HOME=out/tmp/dsh-web-smoke`）启动打印
  `dsh web: http://127.0.0.1:55015/?token=…`；`?token=` 换 HttpOnly Cookie 后页面 200，
  无 token 401；
- `--profile web --dump-config`（带 `dsh-tools.patch.yml`）：`mcp-cdha-tools` 与
  `llm-pi-ai/cdh-maas` 均在树中；`agent-default-model` 段为 `deepseek-official`；
- 首次对话失败截图/日志见 `log-fail.txt`（`MISSING_CREDENTIAL` + 空工作区提示）；
- 修复后实测见 `log-pass.txt`（workspace/create 成功、助手回复、13 个 `mcp__cdha__*` 工具）。

## 改动

- `agent/internal/agent/dsh_web.go`（新增）：`Engine.StartDshWeb`/`CloseDshWeb` 管理
  `dsh web` 边车（`proc.StartSession`，单闸门约束）——按需启动、URL 复用、stdout/stderr
  扫描 token URL、60s 启动超时、退出回收；`ensureDshWebWorkspace` 用 token URL 换 Cookie
  后调 `workspace/create` 预注册 `engineRoot` 工作区（幂等，重复调用 `created:false`）。
- `agent/internal/agent/dsh_bridge.go`：`dshProviderPatch` 在有模型目录时追加
  `- id: agent-default-model` 覆盖（`provider: cdh-maas` + `model: dshDefaultModel()`），
  消灭 deepseek-official 默认路由。
- `agent/internal/server/server.go`：新增 `GET /api/dsh-web`（token 鉴权）返回
  `{"url": …}`；`agent/cmd/cdha/main.go` 退出时 `CloseDshWeb()`；
  `agent/internal/agent/loop.go`：Engine 增 `dshWebMu`/`dshWeb` 字段。
- `agent/web/`（embed 前端）：导航加「DSH」页签，`view-dsh` 用
  `iframe#dsh-web-frame` 嵌独立端口 URL（DSH `API_PATH='/api'` 硬编码站点根路径，
  无法反代子路径，只能独立 origin），附「新窗口打开」「重新连接」。
- 测试：`agent/internal/agent/dsh_web_test.go`（URL 正则、workspace/create 报文与
  Cookie 握手、`ok:false` 报错路径，httptest 回环）。

## 验证

- `go build ./...`、`go vet ./...`、`go test ./...` 全绿（50 包）；
- 重启 cdha 后浏览器实测：①「DSH」页签 iframe 加载、状态「已连接」；② 工作区 `engine`
  自动选中；③ 发送「你好，请用一句话回复」→ 助手回复「你好！有什么可以帮你的吗？」
  （模型 `deepseek-v4.1-flash-1017`，1 轮 1 步）；④ 让其列工具 → 13 个 `mcp__cdha__*`
  （bash_exec/cmd_run/dev_adb/ev_new/evidence_search/fs_*/img_probe/log_slice/manifest_show）；
- 旧「对话」页回归：`GET /api/chats` 200。

## 遗留

- 旧聊天代码（chat.go / chat_dsh.go / chat_dsh_pool.go 与 `/api/chats/*`）**暂留并存**，
  DSH Web 界面稳定运行一段时间后再删（用户确认的并存方案）；
- 旧聊天记录存 cdha 侧，DSH 会话存 `DSH_HOME/storages`，两边不互通、不做迁移；
- 工作区目前固定预注册 `engineRoot` 一个；如需 agent 根或其它目录，等 UI 侧出现需求再扩；
- `dsh web` 与聊天边车共用 `DSH_HOME`（`agent/out/harness/dsh`）但分属 `web`/`sdk`
  两个 profile，存储是 home 级共享的——DSH 上游更新后需复验两 profile 并发同 home 的兼容性。

## 后续：旧聊天代码清理（2026-11-15 完成）

DSH Web 运行稳定后按并存方案收口，删掉自研聊天链路，只保留 DSH 一条运行时。

### 删除

| 位置 | 内容 |
| --- | --- |
| `agent/internal/agent/chat.go` | 聊天类型、会话 CRUD、SSE 回合、提问/审批通道（LLM 配置段抽出为 `llm_settings.go`） |
| `agent/internal/agent/chat_dsh{,_pool}.go` | DSH 聊天回合与边车复用池 |
| `agent/internal/agent/chat_test.go` | 旧聊天单测 |
| `agent/internal/server/server.go` | `/api/chats`、`/api/chats/{id}`、`/api/chats/{id}/stream`、`/api/chat-questions/{qid}`、`/api/chat-approvals/{aid}` 路由与 6 个 handler |
| `agent/internal/agent/loop.go` | `chatQMu/pendingChatQ`、`chatAppMu/pendingChatApp`、`chatConnMu/chatConns` 与 `startChatDSHReaper()` |
| `agent/cmd/cdha/main.go` | 退出时的 `CloseChatConns()`（只留 `CloseDshWeb()`） |
| `agent/internal/store/store.go` | `ChatsDir()` 与 `harness/chats` 初始化目录 |
| `agent/web/` | 「对话」页签（`view-chat`）、工作台迷你聊天（`dpanel-chat`）、`chat-question-modal`、`chat-approval-modal`、`describeChatToolTrace` 等约 1200 行 JS、39 个聊天专用样式类（`style.css` 3633 → 3090 行） |

### 保留

- **模型配置**：`/api/llm/config`、`/api/llm/test`、`llm-config-modal` 与
  `applyLLMConfigUI`/`loadLLMConfig`；弹窗入口从旧「对话」页挪到「DSH」页顶部栏，
  密钥/默认模型状态也显示在该栏——DSH 边车的 patch 与默认模型路由都取自这里。
- **诊断 Run 的 DSH 执行路径**：`dsh_bridge.go` 的 `runDiagWithDSH`/`launchDSHClient`
  与 `internal/dshsdk` 整个 SDK 封装（流水线/构建诊断在用，非聊天代码）。
- 磁盘上的历史聊天 JSON（`agent/out/harness/chats/`）未删除，只是不再有入口。

### 验证

- `go build ./...`、`go vet ./...`、`go test ./...` 全绿（50 包）；
- 重启后路由核对：`GET /api/chats` → **404**，`GET /api/llm/config` → 200；
- 浏览器实测：左侧导航只剩「云机/DSH/流水线/对齐/节点/发布」；「DSH」iframe
  `#dsh-web-status` = 已连接；「模型配置」弹窗正常显示；「云机」「流水线」页签无空白；
  console 全程零报错。

### 遗留

- 旧聊天记录不再迁移（DSH 会话在 `DSH_HOME/storages`）；
- `internal/dshsdk` 中仅聊天用到的协议方法（`Handlers`/`Question`/`Approval` 通知等）
  暂留，诊断 Run 仍共用同一客户端。

