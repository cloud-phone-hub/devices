# 15R 云机：OplusWifi SmartAntenna 空 converter 导致 system_server 每秒级自杀循环

- **设备/机型**：OnePlus 15R（CPH2769 / ColorOS 16.0.10.500_EX01 / 骁龙 SM8750 `canoe` /
  Android 16 + lineage-23.2 底座）
- **固件基线**：`devices/manifest/oneplus/15r/baselines/CPH2769_16.0.10.500_EX01_A51_full.json`
- **首次暴露**：实例 7（2026-11-03 08:41 启动），`sys.boot_completed=1` 后每 ~80 秒崩一轮，共 4 轮
- **结论**：`ro.oplus.wifi.wsa=StbcMrc` 是官方 odm 顶层 `/build.prop` 写的；该 token 会加载
  `OplusWifiStbcMrcSolution`，而它的 converter 只在 `ro.boot.hardware` 以 `qcom`/`mt`/`ums`
  开头时创建。云机 `ro.boot.hardware=canoe` 判成「未知平台」→ converter 为 null →
  `handleBootCompleted()` NPE → system_server 自杀。修复 = 把顶层 `/odm/build.prop` 也纳入
  patch 覆盖，把该属性改 `none`（方案列表为空，所有 WSA 回调 no-op）。

---

## 一、问题

实例 7 能起过 `sys.boot_completed=1`，但随后每 ~80 秒 system_server 自杀一次，
`*** FATAL EXCEPTION IN SYSTEM PROCESS: OplusWifiCPUThread` 连续 4 次
（00:02:10 / 00:03:51 / 00:05:26 / 00:07:01），表现为「够到桌面 → 黑屏 → 重来」的循环：

```
java.lang.NullPointerException: Attempt to invoke virtual method
  'void com.oplus.server.wifi.antswap.OplusWifiTxRxCodecCoverter.handleBootCompleted()'
  on a null object reference
  at OplusWifiStbcMrcSolution.handleBootCompleted(OplusWifiStbcMrcSolution.java:824)
  at OplusWifiSmartAntenna.handleBootCompleted(OplusWifiSmartAntenna.java:112)
  at OplusWifiInjector.handleBootCompletedInternal(OplusWifiInjector.java:655)
```

此时 SELinux 已能现场编译（09 的 `vendor-cil-rewrite` 已生效）、boot_completed 已置位，
所以这不是启动链问题，而是**开机完成回调里的空指针**。

## 二、根因

三条链一起成立才出这个 NPE：

1. **属性值来自官方 odm 顶层 `/build.prop`。**
   `dump.erofs --cat --path=/build.prop odm_a.img` 第 55 行是 `ro.oplus.wifi.wsa=StbcMrc`，
   第 56 行是 `ro.oplus.wifi.wsa.disMrc=true`。全 12 个官方镜像的 `/build.prop` 与
   `/etc/build.prop` 里只有这一处设置它（system/system_ext/product/my_* 均为 0 命中），
   它的 `import /odm/etc/${ro.boot.prjname}/build.prop` 在云机展开失败
   （`property 'ro.boot.prjname' doesn't exist`），所以来源唯一。
2. **该 token 决定加载哪套 WSA 方案。**
   `OplusWifiSmartAntenna.<init>` 把属性按 `split("\\|")` 拆成 HashSet：
   含 `q`/`o` → `OplusWsaSolutionO`，`m` → `OplusWsaSolutionM`，`qcom_v2`、`icc` 各一套，
   **含 `StbcMrc` → `OplusWifiStbcMrcSolution`**。`handleBootCompleted()` 只在
   `mWsaSolutions` 非空时逐个回调。
3. **StbcMrc 方案的 converter 按平台创建，云机平台判定为 0。**
   `PlatformUtils.sIsQcomPlatform = get("ro.boot.hardware","oplus").toLowerCase().startsWith("qcom")`，
   MTK 看 `startsWith("mt")`、UNI 看 `ums`，其余 `getProductPlatform()==0`。
   `OplusWifiStbcMrcSolution.<init>` 只在 `isMtkPlatform()`/`isQcomPlatform()` 两个分支里
   创建 `mStbcCoverter`/`mMrcCoverter`；平台为 0 时两个都是 null →
   `handleBootCompleted()` 第 824 行直接调 null。

为什么「只改 06 的 `/odm/etc/build.prop`」不够，两条更细的原因：

- 那份覆盖件里的同名行**本来就被 SELinux 拒**：`init: Do not have permissions to set
  'ro.oplus.wifi.wsa' ... in property file '/odm/etc/build.prop'`，avc 的
  `tcontext=u:object_r:default_prop:s0`、`scontext=u:r:vendor_init:s0`——属性没有专门类型，
  vendor_init 对 `default_prop` 无 set 权限。所以它从来没有真正生效过。
- 也**不能靠 `/vendor/build.prop` 先占位**：init 读 build.prop 是「先汇总进 map、后读覆盖先读」
  （日志实证 `Overriding previous property 'bluetooth.device.class_of_device':'90,2,12'
  with new value '26,1,4'`），odm 在 vendor 之后读，`StbcMrc` 会把 vendor 侧的值盖掉。

## 三、证据

- `log-fail.txt`：实例 7 的 4 次 FATAL、NPE 栈、`/odm/etc/build.prop` 被拒的两行、
  官方 `odm_a.img` 顶层 build.prop 的 `wifi.wsa`，以及 jadx 反编译得到的三段判定逻辑
  （`/system_ext/framework/oplus-wifi-service.jar`）。
- `log-pass.txt`：实例 8 的 `getprop ro.oplus.wifi.wsa=none`、
  `OplusWifiSmartAntenna: invalid solution`、FATAL/StbcMrc/NPE 三项计数归零、
  `sys.boot_completed=1` 且 10 分钟后 system_server 仍是同一 PID。

## 四、改动

| # | 文件 | 改动 |
| :--- | :--- | :--- |
| 1 | `manifest/oneplus/15r/odm/build.prop`（新增） | 官方 odm **顶层** `/build.prop` 的覆盖件：123 行原样 + 头部补 `ro.odm.product.cpu.abilist` 段（与 `odm/etc/build.prop` 同口径），`ro.oplus.wifi.wsa` 由 `StbcMrc` 改 `none` |
| 2 | `manifest/oneplus/15r/odm/etc/build.prop` | 同名行同样改 `none`（两份保持一致，避免将来只改一份又漂移） |
| 3 | `manifest/oneplus/15r/kernel/cdh_redirect.rules` | 新增 `file /odm/build.prop /mnt/vendorpatch/odm/build.prop`，其余 `/odm` 路径仍读官方 1.99 GB 镜像 |
| 4 | 重建 | `build_odm` → `build_vendor_boot`（rules 进 vendor_ramdisk）→ `build_patch` → `pack_vda`；06 的 `syncOdmTree` 会自动把 `odm/build.prop` 落到版本树，09 的 `/odm` 子树取自 06 的 odm.img，无需改代码 |

语义说明：`none` 不命中任何 token，`mWsaSolutions` 为空 → `validateWsaSolution()` 返回 false，
`handleBootCompleted()` / `onRotationChange()` / `updateHeadSarState()` 等回调全部 no-op。
云机没有天线切换/STBC-MRC 的射频能力，这等于把该特性整体关掉。

## 五、验证

实例 8（2026-11-03 09:18 启动，`adb -s 127.0.0.1:5911`）：

| 判据 | 修复前（实例 7） | 修复后（实例 8） |
| :--- | :--- | :--- |
| `FATAL EXCEPTION IN SYSTEM PROCESS` | 4 | **0** |
| `OplusWifiStbcMrcSolution` | 8 | **0** |
| smart-antenna 回调 NPE | 6 | **0** |
| `getprop ro.oplus.wifi.wsa` | （StbcMrc 生效） | **none** |
| `OplusWifiSmartAntenna` 日志 | 无（方案非空） | **`invalid solution`**（预期分支） |
| `sys.boot_completed` | 1（但随后自杀） | **1** |
| system_server PID | 每轮更换（969/6013/10577/14140） | **956 稳定 ≥10 分钟** |

## 六、遗留

1. `android.hardware.power.stats-impl.oplus` 仍每 ~5 秒 SIGABRT
   （`Check failed: s == STATUS_OK` 159 次），与 cph2797 #80 同型（抢注同名 AIDL +
   后续 `add` 服务被 SELinux 拒）；本轮不阻塞开机，但需要按「机型兼容 rc 里 `stop` 官方服务」
   的同法收口。
2. `com.android.nfc` 反复 `NoSuchElementException`（栈在 `HwBinder.java`），以及
   `avc: denied { find } ... vendor.oplus.hardware.urcc.IUrcc/default`，均在 system_server
   存活期间出现，待下一轮定位。
3. `19395` 条属性 set 类 avc（`/odm/etc/build.prop` 里大量 `default_prop` 属性）仍未处理：
   它们目前只是「静默不生效」，若某个功能依赖这些属性，需要按属性逐条补 `vendor_property_contexts`
   或改为从顶层 `/build.prop`（init 主域）下发。
4. 已否决的做法：改 `ro.boot.hardware` 使 `PlatformUtils` 判成 qcom——`ro.hardware` 由它派生，
   会连带改变 init/HAL 的后缀查找（`init.<hardware>.rc`、`ueventd.<hardware>.rc`），
   风险远大于收益。
