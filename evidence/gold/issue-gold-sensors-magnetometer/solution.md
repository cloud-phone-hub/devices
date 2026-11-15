# 传感器清单对齐（磁力计 mmc5603/Memsic）：gold 云机 × 真机

- 设备：`xiaomi/gold`（Redmi Note 13 5G），云机实例 2，`c46c631991f2196b-cloud`；真机 `07cf5fc1784783d5`
- 日期：2026-10-31
- 结果：云机 `dumpsys sensorservice` 的 **29 个 sensor / 37 行摘录与真机逐字节一致**（`diff` 无输出）

## 一、问题

`device-checker` 的 `hardware.sensor` 在 handle=2 上双端不一致：

| 字段 | 真机 | 云机（修前） |
| :--- | :--- | :--- |
| `Name` | `mmc5603` | `qmc6308` |
| `Vendor` | `Memsic` | `QST` |
| `StringType` / `Type` / 量程 / 分辨率 / FIFO | `android.sensor.magnetic_field`、4912.05、0.15、4500/600 | 同左（本就一致） |

即只有「名字 + 厂商」两项错，其余 28 个传感器、以及 handle=2 的其它字段都不差。
但这足够让按字段比对的检查项判不一致，属"低成本可消除"的差异。

## 二、根因

本机型的虚拟 sensors HAL 是**清单驱动**的：`/vendor/etc/sensors_config.xml` 决定
`SensorManager` 能看到什么（见该文件头注释）。gold 这份清单是从另一批次的同型号样机抄来的，
那一批用 QST 的 qmc6308，而这台（`07cf5fc1784783d5`）是 Memsic 的 mmc5603。
所以差异不在 HAL 代码，而在设备集成层的清单取值。

## 三、证据

- `log-fail.txt`：`run-00034` 云机 handle=2 = `qmc6308`/`QST` vs 真机 `mmc5603`/`Memsic`。
- `log-pass.txt`：两侧 `dumpsys sensorservice` 清单直接 `diff` 无差异（37 行 vs 37 行），
  以及 `run-00039` 云机 29 个 sensor 的前三条（含 `mmc5603 | Memsic`）。
- 量程/分辨率/FIFO 等数值字段修前就一致，说明只是名称/厂商这一处抄错，不是拉错整份清单。

## 四、改动

`devices/manifest/xiaomi/gold/vendor/etc/sensors_config.xml`（handle=2）：

```xml
<!-- 磁力计（handle=2）2026-10-31 按真机 07cf5fc1784783d5 与云机 device-checker 对拍校正：
     该机为 mmc5603/Memsic（原值 qmc6308/QST 取自另一批次样机），其余字段本就逐项一致。 -->
<sensor handle="2" name="mmc5603" vendor="Memsic" version="1" type-id="2"
        type="android.sensor.magnetic_field" max-range="4912.05029296875"
        resolution="0.15000000596046448" power="0.001" min-delay="10000"
        fifo-reserved="600" fifo-max="4500" max-delay="200000"
        wake-up="false" reporting="0"/>
```

清单随 `07 build-vendor` 装进 `/vendor/etc/sensors_config.xml`，不需要重编 HAL；
`09 build-patch` / `11 pack-vda` 后发布到 `engine/out/dist/xiaomi/gold/OS3.0.10.0.VNQCNXM/`
（`patch.img` sha256 前缀 `304f3a9c9bd2`，`vda-recipe.txt` = `vda-d44b6b713de9`）。

## 五、验证

- 重启实例 2 后 `adb -s 127.0.0.1:5911 shell dumpsys sensorservice`：
  `0x00000002) mmc5603 | Memsic | ver: 1 | type: android.sensor.magnetic_field(2)`。
- 与真机（USB 直连）清单 `diff`：37 行完全一致，无输出。
- `device-checker run-00039`：云机 `hardware.sensor` 29 个 sensor 全字段与真机相同；
  该项在真机侧长期 `执行超时`（真机 sensorservice 列清单 >20 s），
  因此以 `dumpsys` 直比为准（两端都是同一份 `SensorInfo` 序列化结果）。

## 六、遗留

- `hardware.sensor` 这个功能点在**真机侧**仍 `执行超时`（超过服务端 20 s 上限），
  属于 `dev.checker` 该 check 在真机上的性能问题，与云机无关；要双端自动化对拍需先解决真机超时。
- 其余 28 个 sensor 的名称/厂商/量程本就是对的；若换批次样机（QST 那批），
  这份清单要按那台机改回去——清单是设备身份，不是通用默认值。
