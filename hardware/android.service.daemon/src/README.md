# fishbridge 功能梳理

fishbridge 是运行在 Android 云手机 guest 内、用于与宿主侧（cloud-droid-hub 的
guestcontrol / controller）交互的**设备控制桥**。它提供两套入口：

- **CLI 入口**：宿主通过 `adb shell fishbridge <command>` 调用
- **RPC 入口**：`fishbridge rpc`，宿主通过 ADB 传输层运行 guestcontrol 协议，处理结构化请求

## 通用状态文件（/data/local/tmp）

| 路径 | 用途 |
| --- | --- |
| `fishbridge-proxy-state.txt` | 系统代理状态 |
| `fishbridge-sensor-state.txt` | 传感器状态 |
| `fishbridge-heartbeat-state.txt` / `.pid` | 心跳计数 / 守护进程 pid |
| `fishbridge-screenrecord-process.json` | 录屏进程状态 |
| `fishbridge-screenrecord.mp4` / `fishbridge-screenrecord-screenshot.png` | 录屏 / 截图默认输出 |
| `fishbridge-event-mouse-state.txt` | 鼠标事件状态 |

常量定义见 `cmd/fishbridge/bridge.go`；`sensor`、`geo` 还依赖
`/vendor/bin/cuttlefish_sensor_injection` 与 `/proc/bus/input/devices`。

---

## 一、CLI 子命令

| 子命令 | 功能 |
| --- | --- |
| `version` | 输出版本号 `fishbridge 0.1` |
| `ping` | 存活探测，输出 `OK` |
| `cpuinfo` | 读取并输出 CPU 信息（含 CPU 亲和性配置，linux/arm64、amd64 分支） |
| `meminfo` | 输出 `/proc/meminfo` 内存信息 |
| `phonenumber` | 获取/处理手机号码相关逻辑 |
| `virtgpu` | 探测 GPU 节点、特性及后端类型（mesa / swiftshader） |

### 1. display — 显示控制（display.go）

- `rotate`：旋转屏幕（`cmd window user-rotation lock`，0-3 循环）
- `resize`：调整分辨率/密度（`wm size` / `wm density`）

### 2. proxy — 系统代理（proxy.go）

- `set-hex <hex>`：解码 HEX 代理地址 → `settings put global http_proxy`，状态写入
  `fishbridge-proxy-state.txt`
- `clear`：清除代理

### 3. event — 输入注入（event.go）

- `send ...`：发送规范化 input event（type/code/value 三元组）
- `text-hex <hex>`：HEX 解码文本后 `input text` 注入
- `mouse`：鼠标移动/点击事件

### 4. finger — 指纹（finger.go）

- `touch` / `remove`：模拟指纹录入 / 移除指纹事件

### 5. geo — 位置注入（geo.go）

- `fix <lng> <lat> <alt> <satellites> <velocity>`：注入经纬度/海拔/卫星数/速度，写
  `/data/vendor/gps/fix`
- `nmea` / `nmea-hex` / `gnss` / `gnss-hex`：注入 NMEA 语句或 GNSS 控制数据

### 6. gsm — 蜂窝网络（gsm.go）

- 处理 GSM 状态/呼叫：当前**无后端实现**，返回错误引导走 Radio controller
  （对应 opcode 0x0300-0x030a）

### 7. heartbeat — 心跳（heartbeat.go）

- `ensure`：拉起后台 daemon，每秒更新心跳状态文件
- `get`：读取当前心跳计数

### 8. multiapp — 应用分身/多用户（multiapp.go）

- `status <package>`：查询某包在各分身用户下的安装/uid/activity 状态
- `enable` / `disable`：为指定 clone 用户安装/卸载应用（`cmd user list`、
  `cmd package install-existing/uninstall`）

### 9. rpc — 启动 RPC 服务（见第二部分）

### 10. screenrecord — 录屏/截图（screenrecord.go）

- `start`：用 `screenrecord` 启动录屏，进程状态写入 json 文件
- `stop`：停止录屏，读 MP4 输出 HEX 返回
- `screenshot`：`screencap` 截图返回 PNG HEX

### 11. sensor — 传感器（sensor.go）

- `status` / `get` / `set`：通过 `cuttlefish_sensor_injection` 注入/查询加速度计、
  陀螺仪、磁场等传感器数据

### 12. sms — 短信注入（sms.go）

- `send-hex` / `pdu-hex`：解析短信 PDU/内容，当前**无后端实现**（引导走 Radio
  controller，对应 opcode 0x0400/0x0401）

### 13. validation — 设备自校验（validation.go）

- `prepare`：准备校验请求到请求文件
- `run`：执行校验并恢复系统状态

---

## 二、RPC 服务（rpc.go + guestcontrol 协议）

`fishbridge rpc` 通过 stdin/stdout 跑帧协议（hello → session → request/response/stream），
携带 **executable sha256 校验**、会话 ID、相对 deadline，按能力位（capability）分发。
全量 opcode 见 `internal/guestcontrol/request.go`：

| 能力 | opcode | 功能说明 |
| --- | --- | --- |
| SystemManifest | 0x0002 | 扫描 `/` 挂载点（system/system_ext/product/vendor/odm/apex 等），生成带 Merkle root 的系统清单（root 权限专属） |
| Screenshot | 0x1000 | 截图流式返回（PNG） |
| GPS | 0x1200 | 注入 GPS 位置 |
| Volume | 0x1400/0x1401 | 音量 get/set |
| Vibration | 0x1500/0x1501 | 振动 play/stop |
| Clock | 0x1610-0x1612 | 设时钟 / 同步宿主时间 / 自动校时 |
| BootTime | 0x1620/0x1621 | 设置/读取首次开机时间 |
| Battery | 0x1700-0x1705 | 设置电量 / 显示 / 重置 / 放电开始 / 充电开始 / 过渡停止（cdg_battery.go） |
| Lock | 0x1800-0x1803 | 设/清锁屏密码、锁屏、解锁（cdg_lock.go） |
| File | 0x1900/0x1901 | 文件 push（流式）/ download（cdg_file.go） |
| MockData | 0x1a00/0x1a01 | 模拟联系人 / 模拟短信（cdg_mockdata.go） |
| DisplayRefresh | 0x1b00-0x1b02 | 列出/获取/设置屏幕刷新率（cdg_display_refresh.go） |
| MultiApp | 0x1c00-0x1c03 | 分身 profiles/status/enable/disable |
| TouchPlay | 0x1d00 | 触摸播放（协议已定义，fishbridge 未挂接回调） |
| AppDisplay | 0x1e00-0x1e02 | 虚拟显示器 list/create/delete（cdg_app_display.go） |
| Fold | 0x1f00/0x1f01 | 折叠屏 get/set（cdg_fold_*.go） |
| Region | 0x1600/0x1601 | 区域/时区 apply/show（cdg_region.go） |

### RPC 信封类 opcode（仅宿主侧 guestcontrol 控制器 / ADB layer 处理）

`0x0001 Hello`、`0x0003 RestoreState`、`0x0004 Quiesce`、`0x0005 Resume`、
`0x0006 AuditStatus`、`0x1100-0x1103 PacketXxx`、`0x1300-0x1303 SnapshotXxx`——
fishbridge 本身不直接实现。

---

## 三、随附静态资产（cmd/fishbridge/assets/）

下拉式 assets 注入的 dex 工具，用于部分 RPC 能力落地：

- `display/classes.dex`、`lock/classes.dex`、`region/classes.dex`、
  `mockdata/classes.dex` —— 分别对应显示、锁屏、区域、模拟数据的系统级辅助。

---

## 小结

fishbridge = **guest 侧的设备模拟控制中心**，覆盖：输入注入、显示/刷新率、位置/GPS、
传感器、指纹、音量、振动、时间/开机时间、电池、锁屏、区域/时区、应用分身、虚拟显示器、
折叠屏、录屏截图、文件传输及系统清单一致性校验。GSM/SMS 两类仅预留协议接口，实际由
Radio controller 承接。