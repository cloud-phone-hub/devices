# v4l2_codec2：从崩溃循环到正确启动（两阶段）—— 里程碑证据

机型：xiaomi/gold（Redmi Note 13 5G）
清单：`gold_images_OS3.0.10.0.VNQCNXM_20260715.0000.00_15.0_cn_a9729c6694`
状态：**已闭环**（阶段一撤掉避让 → 阶段二用 abi-dir 正解启动，服务 running）
- 阶段一产物：patch.img sha256 `a94930d30a16…`（三处撤 + 超时放大）
- 阶段二产物：patch.img sha256 `3e0e69c2cf96`（15 库落盘 + 标签/通道授权）

证据文件：

| 文件 | 内容 |
|---|---|
| `phase1-log-fail.txt` | 崩溃循环、Watchdog half_watchdog、1340ms 黑屏 / 8477ms 出内容 |
| `phase1-log-pass.txt` | 撤掉后：`updatable_crashing=[]`、v4l2 退出 0 次、点击首帧 316ms |
| `phase1-settings-about.png` | 撤掉后「设置 → 全部参数与信息」正常出图（此前白屏十余秒） |
| `phase2-log-fail.txt` | 07 拼出的 hal 命令无 `--abi-dir`、`[err] v412 需要恰好两个` |
| `phase2-log-pass.txt` | 实例 35：服务 running、pid 565、零 execv 失败、零 avc |

前序：`../issue-boot-to-launcher/`、`../issue-boot-to-launcher-vulkan/`、`../issue-mnt-patch-vold-teardown/`

## 1. 问题

### 1.1 阶段一：点击延迟高、静置也在自愈

VNC 里点桌面图标后，先整屏黑约 1.3 秒，8.5 秒后才出内容；期间状态栏与桌面
偶发闪回，adb 时断时续。用户侧感受是「按钮和 VNC 点击延迟很高」。

对照组：完全不点击、静置 100 秒，system_server 仍从 pid A 换成 pid B —— 卡顿
与点击动作无关，是系统在周期性自愈（重启）。

### 1.2 阶段二：撤掉 v4l2 只是避让，被用户否决

阶段一把 v4l2_codec2 当噪声源撤掉了（rc 整体注释、vintf 清空、
`media_codecs_c2.xml` 三条 `c2.v4l2.*` 注释）。点击延迟恢复，但 Codec2 的
v4l2 硬件通道从系统里消失了。

用户明确否决这条路：「你不能关闭 android.hardware.media.v412 的启动作为方案，
应该是解决，让他正确启动」；随后又纠正库的来源：「别用 runtime_lib_dir 用
abi-dir 啊」—— ABI 必须从 lineage 底包就地研磨，不能读设备侧历史资产
`devices/xiaomi/gold-15/out/runtime_lib_dir/`。

## 2. 根因

### 2.1 阶段一：三条根因叠加

1. **v4l2_codec2 缺三层运行条件，进入崩溃-重启死循环。** patch 里
   `/vendor/bin/hw/android.hardware.media.c2@1.2-service-v4l2-64` 带的是厂商
   vendor.img 的标签 `hal_codec2_default_exec`，而 lineage 基座的
   `vendor_sepolicy.cil` 没定义该 type；同时还缺 15 个非 LLNDK 运行库。进程
   以 127 退出，init 判「4 分钟内崩 4 次」后置
   `sys.init.updatable_crashing=1`，`flags_health_check` 触发回滚并杀 zygote。
   实测 6 分钟 727 次退出、SSC=7。
2. **同一批文件标签问题还带崩另外三个 HAL**：`media.c2-mediatek`
   （`mtk_hal_c2_exec` 未定义）、`dolby.media.c2-default-service-dax`
   （`mediacodec` 域没有 system servicemanager 的 `add` 权限）、
   `vendor.cdh_gold_radio`（缺 `init → hal_radio_default` 的 transition，
   补上后又暴露 `/dev/hvc4` 的 `console_device` 读写被拒）。
3. **Watchdog 的 30 秒硬超时在本机太紧。** 系统没有 `ro.kernel.qemu`，
   `Build.isEmulator()` 为 false，AOSP 给 emulator 产品准备的
   `ro.hw_timeout_multiplier` 也没设；system_server 的 binder 线程池被一次长事务
   占住（`StorageManagerS → mediaserver` 卡 118 秒），
   `Watchdog$BinderThreadMonitor` 30 秒超时后直接自杀 system_server。
   `/data/anr` 的 `half_watchdog` 记录 main / ActivityManager / watchdog.monitor
   三处同时卡 30s。

另一处**必要条件**：rc 里声明了
`interface aidl android.hardware.media.c2.IComponentStore/default`，init 的
lazy HAL 语义会在客户端请求时按需拉起，**只加 `disabled` 拦不住**。

### 2.2 阶段二：服务起不来的三层缺陷

1. **运行期库闭包缺 15 个非 LLNDK 库。** 服务与插件的 ELF NEEDED 递归闭包共
   51 个：vendor 基底已有 27 个、LLNDK 白名单 9 个（其中 6 个基底就缺）、真正
   需要落 `/vendor/lib64` 的是 15 个（`libcodec2*`、`libchrome`、
   `libsfplugin_ccodec_utils`、`libstagefright_*bufferpool*` 等）。vendor 进程的
   linker namespace 看不到 `/system/lib64` 与 media-swcodec APEX，缺一个就是
   `CANNOT LINK EXECUTABLE` → exit 127 → 崩 4 次 → 回滚杀 zygote。
2. **执行文件标签不是 exec_type。** 07 装配兜底把 `bin/hw/` 下非 audio/radio
   的二进制一律标成 `vendor_file`；基线 `vendor_file_contexts` 同族规则只有三条
   （`media.c2-default-service`、`media.omx@1.0-service`、
   `dolby.media.c2-default-service-dax`），v4l2 路径不在表里，而 `vendor_file`
   不是 exec_type，init 的 entrypoint/execute 检查直接拒绝。
3. **AIDL store 走 system servicemanager。** 基线 mediacodec 域只被授予
   vndservicemanager 通道，`addService` 被拒后 `service.cpp` 直接 return 1。

工程约束：ABI 目录必须由 `--abi-dir` 显式给出（v412 恰好两个，先 system 后
apex），07 的 `halCommand` 原本只拼 service / plugin，不传该参数 ——
「手跑能过、流水线必挂」。

## 3. 证据

- **阶段一失败**（`phase1-log-fail.txt`）：v4l2 退出计数 727 次与 `SSC=7`；
  Watchdog `half_watchdog` 三处卡 30s；mediaserver 118s；三组 avc
  （unlabeled / service_manager add / hvc4）；点击 1340ms 黑屏、8477ms 出内容。
- **阶段一通过**（`phase1-log-pass.txt`）：`updatable_crashing=[]`、
  `starting service`=0 / `exited`=0、三个 HAL running、system_server pid 722
  稳定 20 分钟且零新增 ANR、点击首帧 316ms。
- **阶段二 ABI 来源**：`lineage-rom/lineage-22.2/super/system_a.img` 的
  `/system/lib64`（18 个 system 侧库）+ 其中
  `/system/apex/com.android.media.swcodec.capex` 就地三层解包
  （capex zip → `original_apex` → `apex_payload.img`，29671424 B → `/lib64`
  9 个 APEX 侧库）。gold 官方 `system_a.img` 里没有 swcodec APEX 的 5 个私有
  codec2 库，这条来源不可替换。
- **阶段二闭包求解**：`hal build media-v412` 用 `debug/elf` 递归 NEEDED，跳过
  LLNDK 与 vendor 已有（命中后仍递归其依赖），落盘 15 个，清单写入
  `engine/out/vendor/android15/v412-runtime-libs.txt`，与手工推导逐条一致。
- **阶段二标签真值**：基底 `vendor_file_contexts` 第 231/272/1436 行只有三条
  `mediacodec_exec` 同族规则；`vendor_sepolicy.cil` 里
  `(allow init_202404 mediacodec_exec ...)`、`(allow mediacodec mediacodec_exec
  ...)` 两条已存在，所以只需补「哪个路径打这个标签」。
- **阶段二同进程库授权**：基底 `same_process_hal_file` 的 allow 只有 kernel 域，
  mediacodec 一条都没有；插件 `libc2plugin_store.so` 的标签由官方基线定为
  `same_process_hal_file`，不补就是 dlopen 被拒。
- **阶段二失败**（`phase2-log-fail.txt`）：07 实际拼出的 hal 命令无 `--abi-dir`
  与 `[err] v412 需要恰好两个 --abi-dir（先 system、后 apex），实际 0 个`。
- **阶段二通过**（`phase2-log-pass.txt`）：实例 35，服务 running、pid 565、
  全文 `cannot execv` 0 次、`same_process_hal_file` 相关 avc 0 次；
  2026-10-27 在实例 4 上复核仍 running（pid 563）。

## 4. 改动

### 4.1 阶段一（其中「三处撤」已被阶段二恢复）

1. **停用 v4l2_codec2（三处同时撤，缺一不可）**：模块 rc 整体注释 service 段；
   模块 vintf 清空 `<hal>` 声明；`manifest/xiaomi/gold/vendor/etc/media_codecs_c2.xml`
   注释掉 3 条 `c2.v4l2.*`（avc/hevc decoder、avc encoder）。
2. **机型层 SELinux 片段（新建）** `devices/manifest/xiaomi/gold/sepolicy/vendor_sepolicy.cil`：
   定义 `mtk_hal_c2_exec`；给 `mediacodec` 补 system servicemanager 通道与
   `vendor_file` 执行；给 `hal_radio_default` 补 init transition、servicemanager、
   `vendor_file`、`vendor_default_prop`、`console_device`（hvc4）；顺带补
   `hal_wifi_default` 的 hvc9 与 `hal_thermal_default` 的 `proc_stat`。
   平台 type 一律用 `{{vers}}` 占位（`plat_sepolicy_vers.txt=202404`）。
3. **放大 Watchdog 超时**：`manifest/xiaomi/gold/vendor/build.prop` 增加
   `ro.hw_timeout_multiplier=10`（30s → 300s），**至今保留**。

### 4.2 阶段二（引擎侧，全部走 abi-dir）

1. `vendor/build/internal/profile/profile.go`：新增 `V412ABISysDir()` /
   `V412ABIApexDir()` / `V412ABIExtraDir()` / `V412SwcodecImage()`，落点
   `<artifact>/abi/media-v412/{system,apex/media-swcodec,extra}/lib64`。
2. `vendor/build/internal/deps/v412abi.go`（新建）：18+9 清单、
   `PrepareV412ABI`（幂等，`--v412-abi` 触发）、三层 zip 解包
   `ensureV412SwcodecPayload`、三镜像回退 `dumpLibFallback`、按需现捞
   `V412DumpLib`、LLNDK 真值 `V412LLNDKLibs`（读镜像内
   `/system/etc/llndk.libraries.txt`，读不到才用内置表兜底）。
3. `vendor/build/internal/halbuild/v412.go`：`publishRuntimeClosure` 求 NEEDED
   闭包并落盘、写 `v412-runtime-libs.txt`；`locateABILib` 顺序为
   abi system → abi apex → 现捞。
4. `vendor/build/main.go`：`deps --v412-abi`、`hal build --vendor-lib64`。

### 4.3 阶段二（装配侧 07）

5. `build/07_build_vendor/hal.go`：`halCommand` 对 `kind=v412` 自动追加两个
   `--abi-dir`（顺序固定）与 `--vendor-lib64 <unpack>/lib64`；`moduleTargets`
   读运行期清单把 15 个库纳入产物；`readModuleConfig` 抽出供 `Kind` 判定。
6. `build/07_build_vendor/plan.go`：`ModuleBuild.Kind` 字段与传递。

### 4.4 阶段二（声明与策略）

7. 恢复三处声明：模块 rc（service 定义 + `seclabel u:r:mediacodec:s0`）、vintf
   （`<fqname>IComponentStore/default</fqname>`）、`media_codecs_c2.xml` 的三条
   `c2.v4l2.*`（与 `c2.mtk.*` 并存）。
8. 模块自带两个策略片段：
   `vendor/hardware/android.hardware.media.v412/sepolicy/vendor_file_contexts`
   （路径 → `mediacodec_exec`）与同目录 `vendor_sepolicy.cil`
   （`(allow mediacodec same_process_hal_file_{{vers}} (file (read getattr map
   open execute)))`）。

## 5. 验证

### 5.1 阶段一（实例 34，撤掉后的基线）

- `ro.hw_timeout_multiplier=10`、`sys.boot_completed=1`、
  `sys.init.updatable_crashing=[]`；
- v4l2：`dmesg` 只剩一条 `Parsing file …v4l2-64.rc`，`starting service`=0、
  `exited`=0（修复前 727 次）；
- 三个 HAL 全部 running：`media-c2-hal`、`dolby-c2-hal`、`cdh-radio`；
- 稳定性：system_server pid 722、mediaserver pid 674，21:01→21:21 未变，
  零新增 ANR；
- 点击首帧：导航栏返回 316ms、最近任务 1512ms、桌面图标 + app 冷启动 3401ms
  （修复前 1340ms 才黑屏、8477ms 才出内容）；
- 截图 `phase1-settings-about.png`（设置 → 全部参数与信息，此前白屏十余秒）。

### 5.2 阶段二（镜像层，patch.img → vendor.img）

- 15 个运行库全部在 `/lib64`，`stat` 均 0644、root:root；
- `/bin/hw/android.hardware.media.c2@1.2-service-v4l2-64` 的 xattr 是
  `u:object_r:mediacodec_exec:s0`，插件是 `same_process_hal_file`，运行库是
  `vendor_file`；
- 镜像内 `vendor_file_contexts` 含 v4l2 行，`vendor_sepolicy.cil` 含
  `same_process_hal_file_202404` 授权。

### 5.3 阶段二（运行层，实例 35）

- `init.svc.android-hardware-media-c2-v4l2-hal-1-2 = running`；
- `init: started service ... has pid 565` 只出现一次，全程无重启；
- 服务日志：`Service starting...` + `Codec2's AIDL IComponentStore service created.`；
- `servicemanager: Caller(pid=565,uid=1013,sid=u:r:mediacodec:s0) Found
  android.hardware.media.c2.IComponentStore/default in device VINTF manifest.`；
- 全文 `cannot execv` 0 次、`same_process_hal_file` 相关 avc 0 次。

## 6. 遗留

1. **guest 没有 `/dev/video*`**：QEMU 11.0.4 的 `-device help` 里没有
   virtio-video / v4l2 设备，daemon 只给了 `virtio-gpu-gl-pci,venus=on`（那是
   Vulkan 的 venus）。因此 `V4L2ComponentStore` 枚举到 0 个组件，
   `dumpsys media.codec | grep -c c2.v4l2` 为 0。服务链路已通，剩下的是给 QEMU
   加 virtio-video 后端（或改走 v4l2-proxy），不在本里程碑范围。
2. **VNC 点击的坐标跳变**：脚本只发 down/up 时，down 携带的坐标跳变会被 guest
   触摸栈当作滑动起手，点图标变成翻页。已加 `out/tmp/vnc_click_move.py`
   （先发 mask=0 的 pointer 移动再 down/up）；宿主侧 daemon/QEMU 是否要把首次
   down 也补一次 move 事件，待评估。
3. **app 冷启动仍偏慢**（超级小爱 3.4s），与 AOT 只做了 18MB verify-only 有关，
   后续可评估 `dex2oat --compiler-filter=speed` 固化。
4. **服务运行期的属性读取被 avc 拒**：对 `system_prop` / `vendor_audio_prop` /
   `default_prop` 的读取 permissive=0 被拒，未影响启动与注册，属可收的噪声。
5. **adb offline 与本主题无关**：全新 data 种子下宿主公钥未注入
   `/data/misc/adb/adb_keys`（`post-fs-data` 的 mkdir 被拒），本轮验证曾改走
   daemon 的 hvc7 shell 通道。2026-10-27 实例 4 的 5911 已可正常 adb
   （见 `../issue-cmd-binder-transfer-fd/`）。
