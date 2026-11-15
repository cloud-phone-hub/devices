# 云机 input 设备 id 序与真机不一致：descriptor 级已对齐，id 是动态 virtio 产物（全机型通用）

## 问题

gold `15-OS3.0.11.0.VNQCNXM` 配置化重构（`modules.input_identity` 数据驱动）重建后做完整
对比，用户报告标准检查 `hardware.inputs`（`InputDevice.getDeviceIds()` 遍历，逐设备采
`id / name / descriptor / sources`）云机与真机「对不上」。按 **id 逐行对齐**时设备发生错位
（如真机 id=2 是 `mtk-kpd`、云机 id=2 却是 `goodix_ts`），看上去像身份没对齐。

## 根因

「对不上」只发生在 **id** 这一列，`name / descriptor / sources` 实际 7/7 逐字一致。三层成因：

1. **id 不是身份字段，是运行时动态序号**。`InputDevice.getId()` 由 `InputManagerService`
   按 **异步 uevent 处理序**分配（`InputManagerService` 收到内核 `inputN` 的 add uevent 后
   递增编号），既不是内核 `inputN` 的注册序，也不参与设备身份。实证：真机内核 `inputN` 序
   （`mtk-pmic-keys=input0`、`mtk-kpd=input1`、`uinput-fpc=input2`）与其 InputManager id 序
   （`uinput-fpc=0`、`mtk-kpd=2`…）**本身就不一致**——id 由异步处理时序决定，与内核注册序无关。
2. **云机有两颗 virtio_input 设备抢早探针**。`goodix_ts`（virtio8）、`mtk-kpd`（virtio9）是
   QEMU `virtio-input` 设备，开机早探针注册；`input_identity` 模块只对它们做**能力对齐**
   （align handler），并不注册它们。其余 5 个设备（`uinput-fpc / fpc_irq@fingerprint / swtp /
   mt6833-mt6359 Headset Jack / mtk-pmic-keys`）才由 `input_identity` 按 `.devices` 文件顺序
   **同步**注册。virtio 早探针使 `goodix_ts` 抢到 id=2、`mtk-kpd` id=3，把自有设备往后挤；
   真机无 virtio，注册时序不同 → id 序结构性错位。
3. **descriptor / sources 由身份决定，与顺序无关**。`descriptor` 是身份哈希（由
   name/bus/vendor/product/version 等派生），`sources` 是源掩码——两者只取决于设备身份配置，
   配置化重构已把它们对齐到真机，故 7/7 逐字相同。

## 证据

- **run-00028**（两端 `hardware.inputs`，按 name 配对）：`descriptor` 与 `sources` **7/7 全匹配**，
  仅 id 异（log-pass.txt 第 1 段）：

  | name | descriptor | sources | id（真→云） |
  | :--- | :---: | :---: | :--- |
  | Virtual | ✓ | ✓ 0x301 | -1 → -1 |
  | uinput-fpc | ✓ | ✓ 0x501 | 0 → 0 |
  | mtk-kpd | ✓ | ✓ 0x101 | **2 → 3** |
  | fpc_irq@fingerprint | ✓ | ✓ 0x101 | **3 → 4** |
  | mtk-pmic-keys | ✓ | ✓ 0x101 | **4 → 6** |
  | mt6833-mt6359 Headset Jack | ✓ | ✓ 0x80000101 | 5 → 5 |
  | goodix_ts | ✓ | ✓ 0x1103 | **6 → 2** |

- **驱动归属**（云机 `/sys/class/input/inputN/device/driver` symlink）：`goodix_ts=virtio_input`
  （virtio8）、`mtk-kpd=virtio_input`（virtio9）；其余 5 设备无驱动 symlink（`input_identity`
  注册，非总线设备）。`Power Button`（button）存在但被 `getDeviceIds` 过滤。
- **跨重启确定性**：重启云机后 **run-00031** 的 id 序与 run-00028 **逐字一致**
  （`uinput-fpc=0 / goodix_ts=2 / mtk-kpd=3 / fpc_irq=4 / Headset Jack=5 / mtk-pmic-keys=6`，
  log-pass.txt 第 2 段）——id 序由固定启动时序决定，不是随机抖动。
- **真机内核序 ≠ InputManager 序**：真机 `inputN` 注册序与 `getId()` 序不一致，坐实 id 是
  异步 uevent 处理序而非内核注册序（log-fail.txt）。

## 改动

**无代码改动**——决策为「接受 descriptor 级对齐，不追 id 全对齐」。理由：

- id 是 `InputManagerService` 动态分配的运行时序号，**不是身份字段**；身份对齐的判据是
  `descriptor`（身份哈希）与 `sources`（源掩码），二者已 7/7 对齐，`aligner_xiaomi_gold.all`
  云 32/32 = 真 32/32，无回归。
- `.devices` 文件顺序**只能控制 input_identity 自有 5 设备的相对序**，无法移动 virtio 设备
  （`goodix_ts / mtk-kpd`）的位置；要把 virtio 挤到真机对应位次，需改 QEMU virtio 探针时序
  或 `InputManagerService` 编号逻辑——代价大、且对身份伪装无收益（descriptor 已一致）。
- 强行对齐 id 反而引入脆弱性：id 序依赖启动时序，跨内核/跨 QEMU 版本可能漂移，不应作为
  对齐目标固化。

## 验证

1. **完整对比无回归**：`aligner_xiaomi_gold.all` 云机 run-00024 = **32/32**、真机 run-00025
   = **32/32**；13 个 `ultimate.*` 单项探针 11 个命中计数完全一致，云机仅 2 处独有命中
   （`U-BINDER-TRANSACT` deadly = QEMU IPC 计时启发式假阳、`U-LOGCAT-ROOT` warn = 云策略
   magisk 上下文旁证），均与 input 身份无关。
2. **身份级对齐**：`hardware.inputs`（run-00028）name/descriptor/sources **7/7 逐字一致**。
3. **id 序确定性**：重启前后（run-00028 / run-00031）id 序逐字一致，证明差异是结构性而非抖动。

## 遗留

1. **id 序差异保留**（虚拟化产物，非配置问题）：`mtk-kpd 2→3`、`fpc_irq 3→4`、
   `mtk-pmic-keys 4→6`、`goodix_ts 6→2`。若未来某检测项硬比 `getId()` 数值，需另行评估
   （改 QEMU virtio 探针时序或 InputManagerService），当前不处理。
2. 云机另有两处虚拟化痕迹（非本条目范畴）：多 `QEMU Power Button`（被 `getDeviceIds` 过滤）、
   `mtk-kpd` 的 `location=virtio9/input0`（真机为空）。
3. 取证手法注意：云机 device-checker **不开机自启**，重启后需 `am start -n dev.checker/.MainActivity`
   拉起才会重连服务端；开机初 JS 桥未暖时 `hardware.inputs` 会「执行超时」，暖机后重发即正常。