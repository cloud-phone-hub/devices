# m22 NFC 空桩 90 s 自杀循环

> 设备：`devices/flyme/m22`（QTI 12.5.0.0A，Android 16）
> 关联：#53（NFC 空桩初建）、#55（HAL 可执行标签）、#56（hardware 名单对齐，
> 本条即其 6.3 遗留的落地）
> 日期：2026-10-29

## 1. 问题

`com.android.nfc` 在 m22 上**每 90 秒被 Watchdog 打死一次，无限重启**。

- 实例 36（13 min 32 s 窗口）：`Enabling NFC` / `HalOpenInternal: Open Error=OK` /
  `Watchdog triggered, aborting` / `JNI FatalError called: enableInternal` /
  `Fatal signal 6 (SIGABRT)` 各 **10 次**，间隔严格 90 s。
- `dumpsys nfc` 不可用（`FAILED_TRANSACTION`）；NFC 永远起不来。
- HAL 进程本身活着（`service list` 有 `android.hardware.nfc.INfc/default`），
  所以 #53/#55 的「服务在跑」判据看着全绿，问题只在**协议语义**上。

顺带暴露的第二个 bug：改完 HAL 源码后重跑 07，日志仍打
`[skip] android.hardware.nfc 已编译`，镜像里装的是旧二进制 —— 验证其实一直在
验旧件（见 `log-fail.txt` 第 E 节）。

## 2. 根因

### 2.1 主因：HAL 用「返回异常」表达失败，栈只认「`OPEN_CPLT` 事件」

`android.hardware.nfc.INfc` v1 的 `open()` 是同步 `void`，失败有两条路：

1. 返回 `ScopedAStatus::fromExceptionCode(...)`；
2. 通过 `INfcClientCallback.sendEvent(NfcEvent::OPEN_CPLT, NfcStatus::FAILED)` 通知。

栈里 `NfcAdaptation::HalOpen()` **只认第 2 条**。旧桩选了第 1 条
（`EX_UNSUPPORTED_OPERATION`），于是：

```
open() 4 ms 返回异常 → HalOpenInternal 只打一句 "Open Error=OK"（库文本，不代表成功）
                     → nfcManager_doInitialize 进 CondVar::wait 等 OPEN_CPLT
                     → 事件永远不来
                     → NfcService$WatchDogThread("enableInternal", 90 s) 超时
                     → nfcManager_doAbort → JNI FatalError → SIGABRT
```

`Open Error=OK` 这个「OK」是最容易看错的地方：它只是库里那句日志自己填的文本，
并不能证明 HAL 报过成功——真正能表达「开卡成功/失败」的只有 `OPEN_CPLT` 事件，
而旧桩一个都没发（证据 D 节）。修好之后这行在 logcat 里直接不再出现（见 §5.2 计数表）。

旧桩确实走了第 1 条：`open()` 直接 `return ScopedAStatus::fromExceptionCode(EX_UNSUPPORTED_OPERATION)`，
其余方法也一律异常（见 `log-fail.txt` 第 D 节）。

tombstone 里崩溃线程名叫 `enableInternal`，但那是 Watchdog 线程占了
`WatchdogThread("enableInternal", …)` 的名字；真正被卡住的是 `AsyncTask #1`
（`CondVar::wait → nfcManager_doInitialize+820 → enableInternal+410`），
见 `log-fail.txt` 第 C 节。

### 2.2 次因（构建层）：`needsBuild` 看不见源码

`07_build_vendor` 的复用判据当时是「编译产物存在 + vendor 静态树内容一致」，
**完全不看模块源码**。改 `.cpp` 后产物仍在 → 整模块 `[skip]` → 出图仍是旧件。
本轮两次「产物是旧件」（前一次是 `out/bin/cdh-vendor` 成了 `cdh` 副本，见 #56）
属于同一类：**判据只覆盖了它想得到的维度**。

## 3. 证据

完整日志见同目录 `log-fail.txt`（失败侧）/ `log-pass.txt`（修复后）/
`logcat-nfc-pass.txt`（实例 37 窗口内 NFC 相关原始行）。要点：

1. **90 s 循环**：`20:54:06` → `21:07:38` 共 10 次 abort，每次间隔 90 s，
   每次伴随 `Start proc … for restart com.android.nfc`。
2. **时序**：`Enabling NFC`（`.530`）→ `Open Error=OK`（`.535`），5 ms 就返回，
   之后 90 s 无进展 ⇒ 不是「open 慢」，是「事件不来」。
3. **tombstone 栈**：`#03 CondVar::wait` ← `#04 nfcManager_doInitialize+820`
   ← `#09 NfcService$EnableDisableTask.enableInternal+410`。
4. **AIDL 快照**（`engine/vendor/out/android16/aosp/aidl/nfc/android.hardware.nfc/1/`）：
   `INfcClientCallback` 只有 `sendData` / `sendEvent`，**没有** HIDL 的 `onOpenStatus`；
   `NfcEvent.OPEN_CPLT=0`、`NfcStatus.FAILED=1`。
5. **07 复用旧件**：round6 日志 31 个模块全 `[skip]`，盘上 `android.hardware.nfc-service`
   是 154,584 B / `13:23` / md5 `493cdcf6…`，而当轮新编的是 186,624 B。

## 4. 改动

### 4.1 `engine/vendor/hardware/android.hardware.nfc/src/nfc_service.cpp`（重写）

按「真机控制器上电失败」的语义应答，全部方法都回 CPLT 事件：

| 方法 | 行为 |
| :--- | :--- |
| `open(callback)` | 保存回调、`return ok()`，随后异步（延迟 20 ms、detached 线程）回 `sendEvent(OPEN_CPLT, FAILED)` |
| `close(type)` | 取走回调，异步回 `sendEvent(CLOSE_CPLT, OK)` |
| `coreInitialized()` | 异步回 `POST_INIT_CPLT` + `FAILED` |
| `powerCycle()` | 异步回 `OPEN_CPLT` + `FAILED` |
| `preDiscover()` | 异步回 `PRE_DISCOVER_CPLT` + `FAILED` |
| `getConfig()` | 返回全 0 `NfcConfig`（无 NCI 版本、无厂商能力） |
| `write()` | `EX_UNSUPPORTED_OPERATION`（没有控制器可写，这条栈能接受） |

为什么延迟 20 ms：`open()` 返回后到调用方进 `CondVar::wait` 之间有空档，
事件发太早会被丢掉。`ALOGI`/`ALOGE` 覆盖每个事件，便于下次直接读串口定因。

文件头注释写清了「为什么不能只返回异常」，并引用实例 36 的实测数字。

### 4.2 `engine/build/07_build_vendor/hal.go` —— 复用判据加入源码指纹

- `needsBuild` 签名改为 `(plan *Plan, build ModuleBuild, refresh bool) (bool, error)`，
  判据变成：`--refresh` → 产物/vendor 静态树缺失 → **源码指纹不一致**。
- `moduleSourceDigest`：`<HardwareDir>/<模块>/**` 全部普通文件（跳过 `.git` 与
  `vendorTreeIgnore`）按相对路径混入 sha256，再叠加 `modules/<模块>.json` 与
  `modules/android<版本>/<模块>.json` 的内容。
- 指纹落 `<VersionDir>/hal-src/android<版本>/<模块>@<变体>.sha256`（**不落**
  `plan.HalRoot`，那里整棵会并进 vendor.img）。
- 指纹文件缺失 ⇒ 视为已变更（多编一次是安全方向），编译成功后
  `recordSourceDigest` 写入；写失败只 `[warn]`，不中断构建。
- 报告行同步：复用行加「源码指纹一致」，重编 note 新增
  「模块源码或配置已变（指纹不一致）」。

### 4.3 `plan.go` 与 `hal_test.go`

- `moduleState(plan, build)`、`dryRunCommands` 的调用点改用新签名；指纹算不出来时
  干跑仍列出命令（宁可多列，不隐藏待执行项）。
- 新增 `newNeedsBuildFixture` + `TestNeedsBuildFollowsSourceDigest`：
  源码改动触发重编、落指纹后复用、配置改动触发重编、`--refresh` 无条件重编。

## 5. 验证

完整日志见 `log-pass.txt`（A~G 段）。

### 5.1 构建侧：改过的源码真的进了镜像

| 判据 | 旧件（round6） | 本轮（round7） |
| :--- | :--- | :--- |
| 07 的判定 | 31 个 `[skip]` | 31 个「模块源码或配置已变（指纹不一致）」、0 skip |
| `bin/hw/android.hardware.nfc-service` | 154,584 B / md5 `493cdcf6…` | 186,624 B / md5 `fc8d0d03eca7a856c5c392a69c9c06a4` |
| 指纹 | 无 | 31 个落 `<VersionDir>/hal-src/android16/`，nfc 为 `1512ea43…` |
| HalRoot 内的 `.sha256` | — | 0 个（如设计，不往 vendor.img 里塞） |

- 随后 `cdh device build m22 --only build_vendor` 复跑：31 个 `[skip]`、
  `grep -c cdh-vendor` 只有配置行 1 处（无实际编译），增量判据没有被写成「永远重编」。
- `go build ./... && go vet && go test ./07_build_vendor/...` → `ok … 16.460s`，
  新增的 `TestNeedsBuildFollowsSourceDigest` 覆盖「改源码 / 落指纹 / 再改 / 改配置 / `--refresh`」五条路径。
- 镜像：`patch.img` 402,653,184 B / sha256 `d691deed587bc322b636cd64c6831133dde9fe2a67f5b4b0b0b1f5622b159e63`，
  `vendor.img` 206,680,064 B / sha256 `b429f472ee0b96…`，`boot.img` `414801a6d238ae…`。

### 5.2 实例侧：90 s 循环消失

实例 37（m22，console 5918 / adb 5919 / vnc 5920）。起机后 NFC 开关是 OFF、
进程 `com.android.nfc`（pid 1326）静默待着，整段缓冲里 `Watchdog triggered` / `JNI FatalError` 都是 0。
20:55 在设置里把开关拨到 ON（此刻 framework 侧还没初始化完，回了 `enable: NFC is not initialized yet`；
这一行已随环形缓冲滚出，此处只作因果说明，**不当判据**），
`settings global nfc_on` 于是留下 1，进程重启后会走一遍 `enableInternal` —— 正好复现旧件的触发路径。

用 root 通道 `kill -9 1326` 让 persistent 进程重启，抓到的链路：

```
20:59:05.365 NfcService: Starting NFC service
20:59:05.393 NfcService: Enabling NFC
20:59:05.397  569  569 I  open: no nfc controller, reply OPEN_CPLT(FAILED) asynchronously
20:59:05.419 libnfc_nci: nfc_free_conn_cb: invalid conn_id
20:59:05.419 libnfc_nci: nfcManager_doInitialize: fail nfa enable; error=0x0
20:59:05.428 NfcService: enableInternal: Error enabling NFC
```

- 第 3 行的空 tag 来自 **HAL 进程 569**（`ps -A` → `569 1 android.hardware.nfc-service`），
  即新写的桩；ALOGI 没设 `LOG_TAG`，所以 tag 列是空的（要不要补 tag 记进遗留）。
- 距 `Enabling NFC` 仅 4 ms 就把「开不了」报出来；22 ms 后栈侧收到 `OPEN_CPLT=FAILED`
  并立刻返回 —— 与失败侧「5 ms 返回 `Open Error=OK` 然后静默 90 s」形成完整对照。

长稳窗口（20:59:05.300 → 21:07:18.149，**8 min 13 s**，其间不再做任何操作）：

| 关键字 | 次数 |
| :--- | :--- |
| `Enabling NFC` | 1 |
| `open: no nfc controller` | 1 |
| `enableInternal: Error enabling NFC` | 1 |
| `Watchdog triggered, aborting` | **0** |
| `JNI FatalError` | **0** |
| `Abort message` / `nfcManager_doAbort` / `Fatal signal` | **0** |
| `HalOpenInternal: Open Error=` | **0** |

旧件在同一长度里应出 5~6 次 abort，实测 0；`pidof com.android.nfc` 仍是 6839
（8 分钟内没换过），`init.svc.vendor.nfc-default=running`。窗口内 NFC 相关原始行存为同目录
`logcat-nfc-pass.txt`（76 行）。

### 5.3 两个容易看错的点（顺手澄清）

- 本实例唯一一份 tombstone 是 `svc nfc enable` **一次性命令进程**（pid 6343 /
  RenderThread / SIGABRT / Choreographer 初始化失败），不是 `com.android.nfc`。
- `dumpsys nfc` 依旧 `FAILED_TRANSACTION`，但这条**不能当判据**（vendor 域 fd `use`
  被拒，见 #55），所以本条一律用 `logcat` 与 `pidof`/`init.svc` 判。

## 6. 遗留

1. **NFC 仍不可用**（设计如此）：本桩只让 `com.android.nfc` 干净地停在 OFF。
   要真的能用需要 NCI 控制器模拟（native `libnfc-nci` + HCI over socket），
   成本与语义都超出「云端机型补空桩」的范围。
2. **`dumpsys nfc` 仍可能 `FAILED_TRANSACTION`**：那是 vendor 域 fd `use` 被拒
   的既有现象（见 #55 遗留），与 HAL 桩无关；本条不以它为判据。
3. **首次启用指纹会全量重编一次**（31 个模块），之后按模块增量。
4. **怎么手工触发 enable**（本轮踩过的三条死路，给下一次省时间）：
   - `svc nfc enable`：`Initialization of Choreographer failed` → 命令进程自己 SIGABRT；
   - `cmd nfc` / `dumpsys nfc`：`FAILED_TRANSACTION`（见遗留 2）；
   - 设置 UI（`am start -a android.settings.NFC_SETTINGS` 落到 Flyme「更多连接方式」→ 点 NFC 行）：
     开关能拨，但 framework 未初始化完时只回 `enable: NFC is not initialized yet`；
   - **可用**：root 通道 `kill -9 <com.android.nfc pid>`，让 persistent 进程重启并在启动流程里自动 enable。
5. **开关显示 ON、adapter 实际 OFF**：enable 失败后 NfcService 把 adapter 置回 OFF，
   但 `settings global nfc_on` 保留 1。这是「失败但不假装成功」的正常形态；
   若以后要真开 NFC，得换 5.2 之外的控制面（`cmd nfc` 那条 binder 先修通）。
6. **HAL 的 ALOGI 没有 `LOG_TAG`**，在 logcat 里 tag 列为空，只靠 pid 认进程；
   下次动这个文件时应补 `#define LOG_TAG "android.hardware.nfc-service"`。
