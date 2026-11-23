# #305 全机型通用：NFC / Radio / HAL `dumpsys` 报 `FAILED_TRANSACTION`（`pioneer` SELinux 反向授权缺失）+ 管理台高级配置折叠状态持久化

## 1. 问题

1. **NFC 异常**：管理台「网络与外设」面板中 NFC 卡片显示 `服务异常 (FAILED_TRANSACTION)`；Guest 内执行 `dumpsys nfc` 报 `Error with service 'nfc' while dumping: FAILED_TRANSACTION`，`cmd nfc status` 报 `Failure calling service nfc: Failed transaction (2147483646)`；进一步排查发现 `dumpsys phone`、`dumpsys isub`、`dumpsys android.hardware.nfc.INfc/default` 同样全线报 `FAILED_TRANSACTION`。
2. **双卡双待与基带高级配置自动折叠**：在管理台展开「双卡双待与基带高级配置」后，每隔约 8 秒（后台定时刷新）会自动折叠回去。

## 2. 根因

1. **SELinux `pioneer` 反向授权仅覆盖 `system_server`，遗漏独立系统服务与 HAL 域**：
   - 本仓库 `adbd` / `service_daemon` 运行在自定义 permissive 域 `u:r:pioneer:s0`，但被调用的 `com.google.android.nfc`（`u:r:nfc:s0`）、`com.android.phone`（`u:r:radio:s0`）、`bluetooth` 与各 AIDL HAL（`halserverdomain`）均处于 **enforcing** 域；
   - `devices/hardware/android.service.daemon/sepolicy/vendor_sepolicy.cil` 此前仅给 `system_server_{{vers}}` 配置了对 `pioneer` 的 `fd/fifo_file/binder/unix_stream_socket` 反向授权；
   - 当 `adb shell`（`pioneer`）执行 `dumpsys nfc` / `cmd nfc` / `dumpsys phone` / `dumpsys isub` 时，Binder 驱动在接收端做 `selinux_binder_transfer_file` 校验触发硬拒绝：
     `avc: denied { use } for comm="dumpsys" path="pipe:[35366]" scontext=u:r:nfc:s0 tcontext=u:r:pioneer:s0 tclass=fd permissive=0`
     返回 `BR_FAILED_REPLY` → `FAILED_TRANSACTION (2147483646)`，导致实际已正常初始化并连接宿主虚拟 NFCC（`hvc11`）的 NFC 被管理台误判为服务异常。
2. **前端轮询重绘未持久化 `<details>` 展开状态与表单草稿**：
   - `agent/web/src/modules/daemon/config-panel.ts` 每 8 秒静默调用 `refreshInstanceConfig(true) -> renderConfigPanel(snap)`，整体覆写 `#vnc-cfg-body` 的 `innerHTML`；
   - `<details class="cfg-details">`（「双卡双待与基带高级配置」和「宿主虚拟 AP 配置」）未记录用户展开/折叠状态，重绘后立即恢复默认折叠，同时未保存的卡槽/协议下拉框与滚动条位置也会被冲掉。

## 3. 证据

- 修复前失败日志与 AVC 拒绝记录：[log-fail.txt](log-fail.txt)
- 修复后冷启实机与 API 验收日志：[log-pass.txt](log-pass.txt)

## 4. 改动

1. **补齐独立系统服务与 HAL 对 `pioneer` 的 SELinux 反向授权**：
   - 修改 [vendor_sepolicy.cil](../../../hardware/android.service.daemon/sepolicy/vendor_sepolicy.cil)：为 `nfc_{{vers}}`、`radio_{{vers}}`、`bluetooth_{{vers}}` 与 `halserverdomain` 补齐与 `system_server_{{vers}}` 同口径的四条反向规则（`fd { use }`、`fifo_file { read write getattr }`、`binder { call transfer }`、`unix_stream_socket { getattr getopt ioctl read setopt shutdown write }`）；
   - 重跑 Pixel 6（`oriole` A17）`08_build_hardware → 09_build_vendor → 11_build_patch → 13_pack_vda`（`patch.img` `047d804bccec`，`vda-a7581225ec43`）。
2. **前端面板折叠状态、滚动位置与表单交互保护**：
   - 修改 [config-panel.ts](../../../../agent/web/src/modules/daemon/config-panel.ts)：
     - 持久化 `dsdsDetailsOpen`（「双卡双待与基带高级配置」）与 `wifiApsDetailsOpen`（「宿主虚拟 AP 配置」）展开状态，重绘前先回读当前 DOM 状态并通过 `toggle` 事件实时同步，切换实例时重置；
     - 持久化 `simDraftDirty` / `simDraft`（默认语音/短信/数据与 CDRM 协议草稿），保存或切预设后清空；
     - 静默定时刷新（`silent === true`）时若用户焦点在面板内 `<input>` / `<select>` / `<textarea>`，跳过本次 DOM 覆写避免打断编辑或收起下拉列表；
     - 重绘前后保留 `#vnc-cfg-body` 滚动条 `scrollTop`；
   - 重编前端（`npm run build` → `index-B2ornQbe.js`）与 `cdha` 二进制并保留原 Token 重启 `cdha serve`。

## 5. 验证

1. **Pixel 6 `#5` 冷启实机验证**：
   - `dumpsys nfc` 正常输出 `mState=on`、`mScreenState=ON_UNLOCKED`、`pollTech=0x2f`；`cmd nfc status` 返回 `Nfc is enabled`；
   - `dumpsys phone`、`dumpsys isub`、`dumpsys android.hardware.nfc.INfc/default` 全部正常返回、零 `FAILED_TRANSACTION`；
   - `/api/daemon/instance/config` 返回 `nfc: { supported: true, enabled: true, guestState: "ON", hostAttached: true, hostStatus: { connected: true, initialized: true, discovering: true } }`；
   - 实测 `POST /api/daemon/nfc/tap` 与 `remove` 正常切换在卡/移卡状态；
   - `go test ./modules/daemon/run ./modules/daemon/backend/nfc` 全绿。
2. **前端构建与服务验证**：
   - `tsc --noEmit && vite build` 0 错误；重启 `cdha serve` 后确认首页已下发新 bundle `index-B2ornQbe.js`。

## 6. 遗留

- `vendor_sepolicy.cil` 位于公共模块 `android.service.daemon`，后续各机型重跑 `08 → 09 → 11 → 13` 时会自动同步获得此修复；本次已率先在 Pixel 6 A17（`oriole`）盘集重建并实机冷启验证通过。
