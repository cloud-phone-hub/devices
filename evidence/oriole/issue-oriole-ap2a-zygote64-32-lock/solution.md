# Issue: oriole AP2A 基线缺「纯 64 位锁」，init 按 vendor 的 `zygote64_32` 起 32 位 zygote → `InitFatalReboot` 无限重启

- 设备: Google Pixel 6（oriole，lineage-21.0 底包 + 官方固件 AP2A.240905.003.F1）
- 宿主: macOS (darwin, QEMU + hvf)
- 状态: **该层已解决**（2026-11-04）：基线 `vendor.build-prop-sets` 补齐与本机 cp1a 基线同口径的六键
  （`ro.zygote=zygote64`、`ro.bionic.2nd_arch=`、`ro.bionic.2nd_cpu_variant=`、
  `ro.vendor.product.cpu.abilist=arm64-v8a`、`abilist32=`、`abilist64=arm64-v8a`），重跑 07→11 后
  init 只解析 `init.zygote64.rc`、`app_process32` 相关日志从 54 行归零、primary zygote 从「起 2 ms 即被杀」
  变成活满 16.4 s 并跑起 system_server。**整机验收本轮仍判不通过**：暴露出下一层堵点
  （system_server 每 4~5 s 崩溃 → zygote 作为 critical process 死满 4 次 → fatal reboot → RescueParty），
  另立 [#150 issue-oriole-system-server-crash-loop](../issue-oriole-system-server-crash-loop/solution.md) 跟踪。

---

## 一、问题

本轮「依次构建 + 起机测试」的第 3 台。oriole 的 11 阶段构建（含 #148 修好的顶层 `kernel=6.1`）全绿，
`out/dist/google/oriole/AP2A.240905.003.F1/vda.img` 13.00 GiB 落盘；实例
`runtime/oriole_AP2A.240905.003.F1/1` 起来后：

- 观察 6 分钟（06:22:56 起、06:29 停机取证，未满 10 分钟上限，但故障已是确定性循环） qemu.log
  **始终没有** `init: processing action (sys.boot_completed=1)`；
- guest 每约 55 秒整机重启一次，这段观察里共 3 轮，日志判据是内核态的 fatal reboot；
- VNC（websocket 口 5913）抓帧 1088x2400、**平均亮度 0.6**（开机动画从未稳定出现）。

日志里的因果链（完整取证见 `log-fail.txt`）：

```
[0.394710] init: Overriding previous property 'ro.zygote':'zygote64' with new value 'zygote64_32'
[11.086510] init: cannot execv('/system/bin/app_process32') … Exec format error
[11.087678] init: Service 'zygote_secondary' (pid 697) exited with status 127
[11.088594] init: Sending signal 9 to service 'zygote' (pid 693) process group...
[11.099049] init: Service 'zygote' (pid 693) received signal 9
...（每 5 s 一轮，共 12 轮）
[55.185165] init: critical process 'zygote' exited 4 times before boot completed
[55.186402] init: InitFatalReboot: signal 6
[55.826845] init:   reboot: Restarting system with command 'zygote-fatal'
```

## 二、根因

| # | 事实 | 结论 |
|---|---|---|
| 1 | 07 步骤 6 的打印是「厂商 build.prop（取自步骤 1b 解包树）-> 装配树」，装配树 `vendor/build.prop` 的基料是**官方**件，不是 lineage 基座件 | 官方 Pixel 6 A14 `vendor_a.img:/build.prop` 第 83 行 `ro.zygote=zygote64_32`、第 10~12 行 abilist32=`armeabi-v7a,armeabi`、第 33~34 行 `ro.bionic.2nd_arch=arm` 被原样带进云机盘 |
| 2 | init 装载顺序 system→system_ext→product→vendor→vendor_dlkm→odm，后装载者覆盖先装载者（`Overriding previous property`  printk 形态） | 基座 system 的 `zygote64` 被 vendor 的 `zygote64_32` 覆盖，**在服务启动前就已定局**（0.394 s） |
| 3 | `ro.zygote` 决定 init 是否解析 `init.zygote64_32.rc` 并起 `zygote_secondary`（本机日志第 599~614 行确有该 rc 进 import 列表） | 云机盘是 arm64-only：`/system/bin/app_process32` 与 `mediaserver`/`drmserver` 都是不可 exec 的占位件（`Exec format error`，本轮共 140 行、只这三件），secondary 每次 `status 127` |
| 4 | secondary 退出后 2 ms 内 init 对 primary 发信号 9，并跑 primary 的 `onrestart` 链（`<Service 'zygote' onrestart>:2/:7`） | **连坐**：32 位 zygote 起不来会反复杀掉正常工作的 64 位 zygote，`zygote` 作为 critical process 在 boot completed 前死满 4 次 |
| 5 | `critical process 'zygote' exited 4 times before boot completed` → `InitFatalReboot: signal 6` → `Restarting system with command 'zygote-fatal'` | 引导永远不可能完成，且是**内核级重启**，与显示/HAL 无关；本轮 3 轮循环 |
| 6 | AP2A 基线的 `build-prop-sets` 只有 20 项（身份 / GPU 三键 / soc / 板级 / 密度），六键全缺；同机型**较新**的 cp1a 基线已带这六键，其 note 明写「按基座只有 64 位内容这一事实锁 64-only」；`Template.md` 第 66 行也把该口径写成迁移必做项 | AP2A 早于该规则迁移，属于**基线数据欠账**，不是引擎缺陷。gold 的 `issue-zygote-secondary-missing` 是同族问题在 framework 侧的形态（odm abilist 声明 32 位 → `ZygoteProcess.bootCompleted` 硬连 secondary socket）；本机 odm 段早已锁成纯 64，缺口只在 vendor 段 |

判定要点：`ro.*` 一旦设置即锁定（先到者胜），正解是在 **vendor build.prop 装载时**就把六键写对，
而不是补 32 位件、也不是在 rc 里 setprop（晚于 zygote-start，来不及）。

## 三、证据

- `log-fail.txt`：判定行（窗口内 0 次 `sys.boot_completed=1` + 抓帧平均亮度 0.6）、fatal 链完整时序、
  官方解包件与修复前装配树两侧同六行、cp1a/AP2A 两基线键集对照。
- `log-pass.txt`：修复后装配树 `vendor/build.prop` 六键实值、07→11 重跑 EXIT=0、init 只解析
  `init.zygote64.rc`、`app_process32` 归零、primary zygote 的 9.018→25.447 s 存活窗口、
  system_server 已在其中运行（`from pid: 835 (system_server)`），以及本轮整机仍不通过的计数
  （`sys.boot_completed=1` 0 次、6 轮引导、5 次 RescueParty、抓帧平均亮度 3.9）。
- 原始件：`engine/out/tmp/oriole-fail-qemu.log`（49230 行，故障轮全量）、
  `engine/out/tmp/oriole-bootloop.png`（故障轮抓帧）、
  `engine/out/tmp/oriole-build.log` / `oriole-build2.log`（修复前后两轮构建日志）。

## 四、改动

**只有基线数据，无代码改动**（07 的属性收口 `props.go` 本来就支持空值覆盖）：

`devices/manifest/google/oriole/baselines/oriole-ap2a.240905.003.f1-factory-655c44e7.json`
的 `vendor.build-prop-sets` 追加六键（与 cp1a 基线逐字相同）：

```json
"ro.zygote": "zygote64",
"ro.bionic.2nd_arch": "",
"ro.bionic.2nd_cpu_variant": "",
"ro.vendor.product.cpu.abilist": "arm64-v8a",
"ro.vendor.product.cpu.abilist32": "",
"ro.vendor.product.cpu.abilist64": "arm64-v8a"
```

同文件 `vendor.note` 末尾补「纯 64 位锁」段，写清：键来自官方件、覆盖顺序、`app_process32`
占位件、连坐时序、fatal 判据，以及为何不处理 `dalvik.vm.isa.arm.*`。

重跑 `cdh device build oriole --manifest oriole-ap2a…json --from build_vendor`
（阶段 5：`build_vendor, build_vendor_dlkm, build_patch, pack_super, pack_vda`，EXIT=0）。

## 五、验证

锁本身生效（`log-pass.txt` §1~§4）；**整机仍不通过**（§6~§7），失败点已下移到 system_server，
归 #150。本轮观察 6 轮引导后停机，并用 `/api/delete` 移除实例目录（RescueParty 已把 guest
带进 `recovery --prompt_and_wipe_data`，`/data` 不干净，下次复测必须从零起盘）。

## 六、遗留

1. **同类欠账面**：静态对照七台目标机型，基线里带 `ro.zygote` 键的只有 gold、popsicle（和本轮补上的
   oriole）；m22 / 15r / cph2797 / ss9380 不带该键。m22 本轮实测起机通过，说明它的官方 vendor 基料
   本身就是 `zygote64`——**缺键不等于有 bug，但缺键=没有防线**。后续每台的 01 解包完成后应 grep 一次
   `out/roms/<厂商>/<机型>/<build-id>/super/unpack_vendor/build.prop` 的 `ro.zygote` /
   `ro.vendor.product.cpu.abilist32`，ss9380（同样无锁）是下一个待确认对象。
2. **引擎侧无一致性检查**：07 步骤 6 把官方件当基料，却没有任何「基料声明 vs 基座实际内容」的体检
   （`ro.zygote` 指向 `_32` 而 system 里没有可 exec 的 `app_process32` 就是必然起机失败）。可以在 07
   加一条告警：装配树 vendor build.prop 出现 `ro.zygote=zygote*_*` 且基座 `system/bin/app_process32`
   不可执行 → 直接停下并提示基线补锁。本轮未做（只补数据，避免在验证窗口内动引擎）。
3. **两条 oriole 基线并存**（AP2A / cp1a）：同一条规则维护两处，若再来 A15/A16 基线仍需逐条搬。
   `Template.md` 已写该口径，但只写 abilist 两键（gold 场景），未写 `ro.zygote` 与 `ro.bionic.2nd_*`；
   建议下次改模板时补全六键。
4. **`dalvik.vm.isa.arm.variant/features`（装配树第 38~39 行）未清**：只影响 32 位 dex2oat 的目标 ISA，
   在纯 64 盘上无引用路径；与 cp1a 基线保持一致不动，若后续出现 `dex2oat32` 相关噪音再收。
