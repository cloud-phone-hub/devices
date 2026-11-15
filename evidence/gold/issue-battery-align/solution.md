# 电池信息与真机对齐（gold / Redmi Note 13 5G，Android 15）

## 问题

云机 App 侧 `BatteryManager` / `ACTION_BATTERY_CHANGED` 读到的电池信息与 gold 真机
不一致，device-checker `hardware.battery` 的字段 diff 不收敛，且**任何字段都不随
时间变化**：

| 字段 | 真机（run-00022 / run-00027） | 云机（对齐前） |
| :--- | :--- | :--- |
| `CYCLE_COUNT` | 753 | 0 |
| `technology` | Li-poly | Li-poly |
| `health` | 2（GOOD） | 2 |
| `charge_counter` | 4947 / 4962 | 4967（恒定） |
| `status` | 2(CHARGING) / 5(FULL) | 5（恒定） |
| `voltage` | 4450 / 4377 | 4389（恒定） |
| `temperature` | 380 / 320 | 290（恒定） |
| `seq` | 3007 / 3406（持续增长） | 3（几乎不增长） |

真机在「满电静置」与「补电」两个相位间交替；云机 health HAL 是自研的静态模拟，
只报一份设备无关的常量，既没有真机身份字段，也没有动态与周期推送。

## 根因

1. **健康 HAL 写死常量**：`engine/vendor/hardware/android.hardware.health/src/health_service.cpp`
   的 `BatteryProfile` 只有一份跨设备默认值（`cycleCount=0`、`voltage=4389`、
   `temperature=290`、`status=FULL`），与真机身份字段不符。
2. **没有动态模拟**：真机挂充时 status/voltage/temperature/charge_counter/current
   随补电与静置连续变化；静态实现只会被 framework 反复读到同一个值。
3. **没有周期推送**：BatteryService 只在 HAL 主动回调 `IHealthInfoCallback.healthInfoChanged`
   或 App 查询时更新。原实现从不调用 `update()`，`seq` 因而停在 3 附近。
4. **机型差异没有落点**：把 gold 的常量写进引擎会污染跨机型复用；机型口径应放
   设备集成层（`manifest/<vendor>/<brand>/vendor/`），随 07 装进 `/vendor/`。

## 证据

- 真机/云机字段对照基线：`.tmp/compare-run-00022.json`（`hardware.battery` 两段）。
- 真机 sysfs 全 `Permission denied`、`dumpsys android.hardware.health.IHealth/default`
  返回 `FAILED_TRANSACTION`，故真机以 device-checker（App 侧读 `BATTERY_CHANGED`）
  取证；云机以自建 AIDL 客户端直读 HAL。
- 云机 HAL 直读（`probe-healthinfo-cloud.txt`）：`batteryCycleCount: 753`、
  `batteryTechnology: Li-poly`、`batteryHealth: GOOD`、`batteryLevel: 100`。
- 22 次 30s 采样（`samples-cloud-600s.txt`）覆盖完整相位切换：
  `FULL 4383–4385mV / 310–376 / 4947–4970 / 0µA` →
  `CHARGING 4397→4449mV / 324→380 / 4952→4972 / 774333→254333µA` →
  回到 `FULL 4385mV / 375 / 4970 / 0µA`。
- 周期推送（`config-and-push-rhythm.txt`）：PowerCheckerService 在
  `21:26:28.099`、`21:26:58.099` 各收到一次 `onBatteryChanged`，间隔 30s，
  与 HAL 的 `kUpdateIntervalSeconds=30` 一致。
- 配置装载旁证：早期配置下限 4967 时运行态读到 `Charge counter: 4951`，
  证明 `/vendor/etc/cdh_battery.conf` 已生效（旧常量不会是 4951）。
- 真机最新基准（`real-device-battery-run-00027.json`，唤醒并前台运行 device-checker
  后下发）：`CYCLE_COUNT=753`、`charge_counter=4962`、`voltage=4377`、
  `temperature=320`、`status=5`、`seq=3406`。
- 双端同批对比（`compare-run-00028.json`，同一 `POST /api/run` 同时下发真机与云机）：
  两端均 `succeeded`，`CYCLE_COUNT=753`、`technology=Li-poly`、`health=2`、
  `level=100`、`plugged=1`、`status=5` 完全一致；运行类字段
  `charge_counter`（4957/4949）、`voltage`（4377/4384）、`temperature`（320/317）
  差值在模拟区间内。
- 云机 agent 上线时承载网络为 `ccmni2`（QEMU `net2`，`100.64.155.0/24`，未设
  `restrict`）；`mgmt0` 对应 `net0 restrict=on`，仅放行 `hostfwd 5911→5555`，
  这是「补默认路由仍 Connection refused」的根因。

## 改动

| 文件 | 要点 |
| :--- | :--- |
| `engine/vendor/hardware/android.hardware.health/src/health_service.cpp` | 新增 `/vendor/etc/cdh_battery.conf` 解析（key=value，缺省回落设备无关默认值）；按 `CLOCK_BOOTTIME` 在 600s 周期内模拟两相位：前 25% 补电（counter/voltage/temp 由下界插值到上界，current 900000→250000µA），其余满电静置（回落，current=0），电压叠加 ±1mV 抖动；`main()` 启动 detached 线程每 30s 调 `update()` 推送回调；各 getter 统一走 `sampleBattery()` |
| `devices/manifest/xiaomi/gold/vendor/etc/cdh_battery.conf` | gold 机型口径：`technology=Li-poly`、`cycle_count=753`、`charge_counter_min/max=4947/4972`、`voltage_full/charge_mv=4384/4450`、`temperature_full/charge_decic=310/380`、`cycle_seconds=600`；07 随机型静态树落到 `/vendor/etc/` |

## 验证

1. 全链路重编译并重打包（07 `build_vendor` → 09 `build_patch` → 11 `pack_vda`），
   实例 5 重启；dist `patch.img` sha256
   `54f51b88f0e1ac69db21f3b036fb9664a7a521aaa5b9db81311ba66bb3f0f497`。
2. 产物一致性（`config-and-push-rhythm.txt`）：
   `engine/out/vendor/android15/bin/hw/android.hardware.health-service` 与设备
   `/vendor/bin/hw/android.hardware.health-service` 的 sha256 同为
   `84229d30640a09e3283f5187eb2036b4553ebcd30b44ffe196d0036edbc651db`；
   设备 `/vendor/etc/cdh_battery.conf` 内容与机型静态树逐行一致。
3. HAL 直读（`probe-healthinfo-cloud.txt`）：`batteryCycleCount=753`、`Li-poly`、
   `GOOD`、`level=100`，与真机身份字段一致。
4. 动态性（`samples-cloud-600s.txt`）：22 次采样中 status 出现 `FULL→CHARGING→FULL`
   切换，voltage/temperature/charge_counter/current 按区间单调推进，静置时
   current=0，补电时 current 随相位下降。
5. 周期推送：PowerCheckerService 日志严格 30s 一条（`config-and-push-rhythm.txt`），
   说明 `update()` 回调链路生效，BatteryService 持续重发 `ACTION_BATTERY_CHANGED`。
6. 真机同口径复核（`real-device-battery-run-00027.json`）：`CYCLE_COUNT=753`
   与云机一致；`charge_counter=4962`、`temperature=320`、`voltage=4377`、
   `status=5` 均落在云机模拟区间内。
7. 双端同批正式对比（`compare-run-00028.json`）：唤醒真机并把 device-checker 切到前台，
   云机临时经 `ccmni2` 上线（`ip addr add 100.64.155.15/24 dev ccmni2`、
   `ip route add default via 100.64.155.2 dev ccmni2`、
   `ip rule add pref 10500 lookup main`），一次 `POST /api/run
   {"ids":["hardware.battery"],"devices":["07cf5fc1784783d5",
   "458ffdddb936e56b-cloud"]}` 返回两段 `succeeded`：

   | 字段 | 真机 | 云机 | 判定 |
   | :--- | :--- | :--- | :--- |
   | `CYCLE_COUNT` | 753 | 753 | 一致 |
   | `technology` | Li-poly | Li-poly | 一致 |
   | `health` | 2 | 2 | 一致 |
   | `level` / `scale` | 100 / 100 | 100 / 100 | 一致 |
   | `plugged` | 1 | 1 | 一致 |
   | `status` | 5 (FULL) | 5 (FULL) | 一致 |
   | `charge_counter` | 4957 | 4949 | 区间 4947–4972 内 |
   | `voltage` | 4377 | 4384 | 区间 4384–4450 下沿 |
   | `temperature` | 320 | 317 | 区间 310–380 内 |
   | `seq` | 3428 | 61 | 各自计数基数不同，均持续增长 |

   对比完成后已撤回云机临时网络改动（删 `ip rule 10500`、默认路由与 `ccmni2` 地址），
   恢复 `cph_netmgr` 设计状态（agent 离线属预期）；真机已 `goToSleep` 熄屏
   （`mWakefulness=Asleep`）。

## 遗留

1. **云机默认无外网（设计如此）**：实例 5 `net0 restrict=on` 只保留 `hostfwd 5911→5555`，
   `cph_netmgr` 亦不给 mgmt0 默认路由，故云机 device-checker 常态离线。需要整批对比时
   按验证 7 的 `ccmni2` 临时桥操作，用完即撤；不宜把临时路由固化进镜像。
2. **`dumpsys android.hardware.health.IHealth/default` 仍 `FAILED_TRANSACTION`**：
   自研 AIDL 服务未实现 dump 通道；取证改用自建客户端
   （源码 `out/tmp/battery_probe/battery_probe.cpp`，按 HAL 同款 NDK 参数编译）。
3. **`seq` 基数与真机不同**：云机 `seq` 从 framework 启动计数（61），真机为 3428；
   两者都随 30s 推送增长，语义一致但绝对值不可比。
4. **相位起点随开机时刻变化**：动态模拟以 `CLOCK_BOOTTIME` 计算，重启后相位不从
   真机时刻续接；只保证两相位形态与区间，不追求与真机逐秒对齐。
5. **charge_counter 单位沿 android15 分支口径**：真机 intent extra 即 49xx 量级
   （非严格 µAh）；android16 分支仍用 µAh（5960000 量级），两分支配置各自独立。

---

## 后续变更（2026-10-28）：机型口径 `cdh_battery.conf` → `cdh_battery.xml`

**背景**：本 issue 当初把电池口径写成了 `key=value` 行式 `.conf` 并自写 30 行解析器，
是 engine/vendor 线上唯一的例外 —— 同线 audio/gnss/light/media/sensors/thermal/
vibrator/wifi 八个 HAL 全部用 `/vendor/etc/cdh_<模块>.xml` + 静态 libxml2 +
`CDH_<模块>_XML` 环境变量覆盖路径。health 的 conf 属历史遗留，不是有意设计，故统一。

**改动**：

| 文件 | 变更 |
| :--- | :--- |
| `engine/vendor/build/modules/android.hardware.health.json` | 加 `staticLibs: libxml2`（38 个 C 源静态编入，与 light 同款参数）；`extraLibs` 不变 |
| `engine/vendor/hardware/android.hardware.health/src/health_service.cpp` | `loadProfile()` 改 XML DOM 解析（`xmlReadFile` + `XML_PARSE_NONET`）；路径常量 `kDefaultXmlPath=/vendor/etc/cdh_battery.xml`，env 覆盖名改 `CDH_BATTERY_XML`；属性缺失/非法一律保留内置缺省；`counter_min > counter_max` 回退较大值并告警 |
| `devices/manifest/xiaomi/gold/vendor/etc/cdh_battery.xml` | 新建，五段结构：`<identity>`(7) / `<charger>`(6) / `<capacity>`(3) / `<cycle>`(5) / `<health_data>`(4)，取值与旧 conf 逐项等价 |
| `devices/manifest/xiaomi/gold/vendor/etc/cdh_battery.conf` | 删除（旧文件只被旧二进制读取） |
| `engine/vendor/build/README.md`、`devices/evidence/README.md` | 路径与说明同步 |

旧 9 键之外新增的全部字段（`health` / `level` / `capacity_level` / `charging_state` /
`ac_online`… / `full_charge_uah` / `state_of_health` / `part_status` 等）同样全部外置，
代码里只剩「没有该配置」时的通用兜底缺省。枚举属性写 AIDL 名字（`GOOD` / `FULL` /
`INVALID` / `UNSUPPORTED`），不写数字，以免 AIDL 版本间错位。

**验证**：

1. `engine/out/bin/cdh-vendor hal build health --android 15` → `[ok] 构建完成`，
   产物 `engine/out/vendor/android15/bin/hw/android.hardware.health-service` 647KB，
   静态编入 libxml2（`strings` 可见 `xmlReadFile` 与 `libxml2: out of memory`）。
2. `xmllint --noout cdh_battery.xml` → 语法 OK。
3. 属性集合双向比对（`.tmp/verify_battery_xml.py`）：代码读 26 个属性、XML 声明 25 个，
   「XML 有、代码不读」为空集，唯一「代码读、XML 未写」是 `serial_number`（真机无取证
   值，故意留空即不下发）。

**遗留**：

1. 本轮只做到编译与静态校验；实例 5 未运行，**运行时未复测**（需重打 vendor 镜像 +
   重启实例，看启动日志 `已加载 /vendor/etc/cdh_battery.xml（25 项）` 与 `dumpsys battery`）。
2. 过渡期「新 xml 配旧二进制」时，旧 HAL 只读 `.conf`，会回落内置缺省（`cycle_count=0`
   等），故镜像与 HAL 必须同批更新。
3. `engine/out/devices/.../vendor/etc/` 与 `patch/stage/root/vendor/etc/` 下还留着上一轮
   构建产生的 `cdh_battery.conf`；07/09 两步都是整树重建（先 `os.RemoveAll` 再铺底），
   下次重打镜像会自动消失，无需手工删。
4. `--android 16` 未编译验证（本机缺 `engine/out/vendor/android16/abi/system/lib64`，
   需 `cdh-vendor deps --android 16 --abi`）；本轮 16 侧只调了注释与兜底说明。
