# #67 m22 `vendor.meizu.hardware.aw_light`：按原厂 `.so` 反解 AIDL 并自写实现（撤掉「只发标签的空挂」）

- 设备：m22（MEIZU 22 / Flyme 12.5.0.0A / Android 16 / QTI `sun`）
- 时间：2026-10-29
- 产物：`engine/vendor/hardware/vendor.meizu.hardware.aw_light/`（源码 + rc + VINTF + sepolicy）、
  `engine/vendor/build/modules/vendor.meizu.hardware.aw_light.json`、
  `out/vendor/android16/bin/hw/vendor.meizu.hardware.aw_light-service`（140,584 B / `1acb1aae…`）、
  `out/devices/flyme/m22/12.5.0.0A/img/vendor.img`（**206,295,040 B** / `237ebf61…`）
- 接续：#64（分层判据）、#65（prebuilt 通道）、#66（servicetracker 的逆向模板）

## 1. 问题

`manifest/flyme/m22/sepolicy/vendor_service_contexts` 第 19–22 行登记的 4 个 vendor HAL 名，
在本云 vendor 侧只有 SELinux 标签、没有实体：镜像里既没有 `/bin/hw/*-service`、没有
`etc/init/*.rc`、没有 VINTF 片段、也没有 `*_exec` 可执行标签（#65 已核过）。
`vendor.meizu.hardware.aw_light.IAwLight/default` 就是其中之一，Flyme 的灯效控制侧会
`getService` 这个名字，拿到空 → 灯效功能在云机上完全不存在。

## 2. 根因

- 引擎的 `sepolicySources()` 只遍历 `device.json` 声明的模块，没有模块宿主就没有片段落点，
  所以机型层只能留一段「标签 + find」的占位策略；`hal list` 里也没有这个模块。
- MEIZU 未公开 `aw_light` 的 AIDL 源码（AOSP/LineageOS 无对应 HAL），原厂只给了
  `lib64/vendor.meizu.hardware.aw_light-V1-ndk.so` + `bin/hw/…-service` 二进制。
- 因此过去只有两条路：下发原厂二进制（`kind=prebuilt`，如 #65），或继续空挂。
  #66 已证明第三条路可行——从 `-V1-ndk.so` 反解接口后自写实现。

## 3. 证据（逆向事实）

反汇编源：`out/tmp/unpack-vendor_a/flyme-m22/lib64/vendor.meizu.hardware.aw_light-V1-ndk.so`
（85,392 B），工具 `llvm-objdump -d --no-show-raw-insn` / `llvm-nm -D` + `c++filt` / `strings -a`。

| code | 方法 | 参数写序（transact 前） | 回包读序 | 推导签名 |
|---|---|---|---|---|
| 1 | `aw_color_set_phase` | Int32 ×3 | Header+Bool | `boolean (int,int,int)` |
| 2 | `aw_get_duration_time` | Int32 | Header+Int32 | `int (int)` |
| 3 | `aw_light_init` | — | Header+Bool | `boolean ()` |
| 4 | `aw_light_start` | — | Header+Bool | `boolean ()` |
| 5 | `aw_light_stop` | — | Header+Bool | `boolean ()` |
| 6 | `aw_light_switchEffect` | Int32 | Header+Bool | `boolean (int)` |
| 7 | `aw_music_phase` | Int32Array | Header+Bool | `boolean (in int[])` |
| 8 | `aw_set_Semaphore_letter` | Byte | Header+Bool | `boolean (byte)` |
| 9 | `aw_set_cl_brightness_phase` | Int32 | Header+Bool | `boolean (int)` |
| 10 | `aw_set_light_over_callback` | `IAwLightCallback::writeToParcel` | Header+Bool | `boolean (in IAwLightCallback)` |
| 11 | `aw_set_progress` | Int32 | Header+Bool | `boolean (int)` |
| — | `IAwLightCallback.onStopNotify` | Int32 | Header | `void (int)` |

- 全部 11 条 `AIBinder_transact` 的 `w4` 都是 `0x10000000`（同步），没有 oneway。
- 没有 out 参数、没有 parcelable，也没有跨包依赖：`aw_music_phase` 是裸 `int[]`，
  回调是接口类型（强 binder），因此不存在 structured parcelable 的字段序问题。
- 接口 hash：`strings -a` 取出 40 位常量 `85068d518464045ded39e579da9fca8b416eb2c5`，
  逐字写入 `aidl/vendor.meizu.hardware.aw_light/1/.hash`（不自造 hash）。
- 原厂策略侧的事实（`unpack-vendor_a/.../etc/selinux/`）：域 `hal_aw_light_default`、
  可执行标签 `hal_aw_light_default_exec`、服务名类型 `hal_aw_light_service`；
  `/dev/rgb_led`→`aw_light_device`、rgb_led sysfs→`aw_light_sysfs`（驱动真硬件才需要）。

## 4. 改动

1. **新建模块** `engine/vendor/hardware/vendor.meizu.hardware.aw_light/`
   - `aidl/vendor.meizu.hardware.aw_light/1/{.hash, vendor/meizu/hardware/aw_light/IAwLight.aidl,IAwLightCallback.aidl}`
     （头注释写明每条 code 与参数形状的反解依据）
   - `src/aw_light_service.cpp`（128 行）：设置类一律回 `true`（接受但无动作），
     `aw_get_duration_time` 回 0，回调只持有引用、从不触发 `onStopNotify`；
     不 open `/dev/rgb_led`、不写 sysfs。
   - `vendor/etc/init/vendor.meizu.hardware.aw_light-service.rc`：原厂 rc 逐字搬运，
     **新增** `interface aidl …IAwLight/default` 与 `seclabel u:r:hal_aw_light_default:s0`
     （原厂靠独立 file_contexts 换域，引擎侧由 6b 从 seclabel 派生 exec 标签）。
   - `vendor/etc/vintf/manifest/vendor.meizu.hardware.aw_light-service.xml`：原厂逐字
     （`<manifest version="1.0">` 是原厂写法，未抬到 9.0：`alignVintfSchema` 只降不升）。
   - `sepolicy/vendor_sepolicy.cil`：域 + exec 标签 + 服务名类型 + init 启动链 +
     `add/find` + system_server 双向 binder；`sepolicy/vendor_service_contexts`：
     `IAwLight/default → hal_aw_light_service`。
2. **spec**：`engine/vendor/build/modules/vendor.meizu.hardware.aw_light.json`（`kind=aidl`，
   `--version=1`、hash 来自 `.hash`、`workSubdir=aw-light-ndk-work`）。
3. **声明**：`manifest/flyme/m22/device.json` 的 `hardware` 加
   `vendor.meizu.hardware.aw_light@default`（32 → 33 项）。
4. **机型层下放**（同批，避免重复 `(type …)` 让 secilc `Failed to resolve`）：
   - `sepolicy/vendor_service_contexts` 删 `IAwLight/default` 行（4 → 3 个未实现名）；
   - `sepolicy/vendor_sepolicy.cil` 删 `hal_aw_light_service` 的 type / roletype /
     typeattributeset / `allow system_server … find` 四段，注释同步改写。

## 5. 验证

- `hal list` 出现 `vendor.meizu.hardware.aw_light aidl -> vendor.meizu.hardware.aw_light-service`。
- `hal build`：`AIDL 代码生成 … (v1, hash 85068d518464)` → 编 2+1 源文件 → 链接；
  产物 `strings` 含两个 descriptor + 实例名 + 同一个 hash；`llvm-readelf -d` 的 NEEDED
  只有 libc++/libbinder_ndk/liblog/libdl/libm/libc（不链接任何原厂 so）。
- **wire 回放**：把自研二进制再反汇编，与原厂 `.so` 按 code 逐条比对方法名——
  1..11 全部 `OK`（`getInterfaceHash`/`getInterfaceVersion` 也一致），即事务码↔方法映射与
  参数个数一一对应，客户端（Flyme framework）无需改动。
- 07（`cdh build-vendor --android 16 …`，exit=0）：
  - 步骤 3 编译该模块；步骤 4「`vendor.meizu.hardware.aw_light@default` 写入 3」；
  - 步骤 6 追加模块层 cil 2,897 B / service_contexts 530 B（marker 带 `@vendor.meizu.hardware.aw_light`），
    机型层块内已无 `hal_aw_light_service` 声明；
  - 步骤 6b 新增 `vendor.meizu.hardware.aw_light-service -> hal_aw_light_default_exec`；
  - 装配树 658 → **661**（文件 430 / 链接 199 / 目录 32），`{{vers}}` 已替换成 `202504`（残留 0）；
  - `vendor.img` 206,147,584 → **206,295,040 B**。
- 镜像回读（`debugfs`，`/opt/homebrew/opt/e2fsprogs/sbin/debugfs`）：
  - `bin/hw/…aw_light-service` = `1acb1aae…`、rc = `b3f929d3…`、VINTF = `cfefc9f2…`，
    三者与 `out/vendor/android16` 产物逐字节一致；
  - `ea_list` → `security.selinux = u:object_r:hal_aw_light_default_exec:s0`，mode 0755 / 0:2000；
  - `vendor_service_contexts` 的 `IAwLight/default` 只在模块 marker 块内出现一次，
    机型层块 0 命中；`vendor_file_contexts` 由 6b 派生 1 行（路径按原厂名全转义）。
- 判据说明：镜像 sha 仍不作一致性判据（mke2fs 非确定性），判据 = 装配树条目数 +
  逐文件 sha + 镜像 xattr。

## 6. 遗留

1. **运行期未复验**：需要在实机/模拟器上看 `init.svc.vendor.default.aw_light=running`、
   Flyme 灯效侧 `getService` 成功、`Failed to resolve` 与相关 avc denied 为 0。
   注意 `class late_start`，要等 boot 完成后才启动。
2. **语义是空实现**：`aw_get_duration_time` 恒 0 会让「呼吸/律动」在 framework 侧按
   立即结束处理；若 Flyme 有依赖「时长>0」的分支，需要补一个假时长（当前无从判断）。
3. **未搬原厂设备授权**：`/dev/rgb_led`（`aw_light_device`）与 rgb_led sysfs
   （`aw_light_sysfs`）一条未取——本实现不碰硬件。将来真接 LED 控制器时，
   路径绑本机 i2c 控制器，属板级事实，应留在机型层。
4. **剩余同类 HAL**：`meizu_charging`（11 方法 + 6 个枚举，本批继续）、
   `realitytap`（V3 接口 + 依赖 `android.hardware.vibrator` 的 `EffectStrength`/
   `IVibratorCallback`，跨包 import 需确认 aidl 搜索路径）、`diaghal`（服务由
   `/vendor/bin/diag-router` 注册，不是 bin/hw 独立服务）。
5. `patch.img` / `out/dist` 仍是旧版（09/11 未跑）。
