# issue: `android.hardware.health` 与 `android.hardware.graphics.composer3` 的欧加兼容层拆分为 `variants.oplus`，消除跨厂商二进制污染

机型：跨机型通用（`default` / `samsung` 全机型去污 + `oppo` / `oneplus` / `realme` 共 7 份清单切 `variant: "oplus"`）
阶段：`cdh-vendor`（`vendor/build` + `vendor/hardware`）& `07_build_vendor` 清单路由
时间：2026-11-12
改动类型：HAL 模块变体拆分 + 7 份欧加系机型配置清单同步 + 单元测试

## 1. 问题

在 `RMX6688`（见 `../../rmx6688/issue-rmx6688-android15-bringup/`）引入欧加（ColorOS / Realme UI）扩展服务兼容层时：
1. `android.hardware.health` 将 `CdhOplusCharger.{h,cpp}` 直接放在了 `src/` 顶层，且 `health_service.cpp` 无条件 `#include` 与调用 `RegisterOplusChargerIfDeclared()`；
2. `android.hardware.graphics.composer3` 同样将 `CdhOplusDisplayColor.{h,cpp}` 直接放在了 `src/` 顶层，并在 `service.cpp` 无条件调用 `RegisterOplusDisplayColorFeatureIfDeclared()`。

尽管运行期二者会先检查 `/odm` 或 `/vendor` VINTF 清单再决定是否注册 Binder 服务，但在二进制产物层面，欧加专属类名、VINTF 路径与接口描述符被无条件编译链接进了**所有机型**（包括小米 `gold`/`popsicle`、三星 `pa3q`、Google `oriole`、魅族 `m22`、荣耀 `sun` 等）的 `android.hardware.health-service` 与 `hwc3-service.drm` 中，造成跨厂商静态特征污染。

## 2. 根因

`cdh-vendor`（`halbuild`）的编译单元收集规则为：
`服务编译单元 = srcDir 顶层非 main 的 .cpp + extraSourceDirs + externalSources + 服务自身 main`。

因此任何直接放在 `src/` 顶层的 `.cpp` 都会进入 `default` 基础变体以及所有其它命名变体（如 `samsung`）。此前三星 OneUI 的 `ISehHealth` 扩展已按 `src/samsung/` + `variants.samsung`（`-DCDH_HEALTH_SAMSUNG=1`）模式做过源码与编译隔离（见 `../../ss9380/issue-health-hal-samsung-isehhealth/`），而欧加扩展未沿用这一变体边界。

此外，`vendor/build/internal/spec/spec.go` 的 `merge()` 对 JSON 数组采用**整体替换**语义，为 `android.hardware.graphics.composer3.json` 增加 `variants.oplus.compile.defines` 时必须同时保留基础配置的 3 个宏（`-DHWC2_INCLUDE_STRINGIFICATION`、`-DHWC2_USE_CPP11`、`-DUSE_IMAPPER4_METADATA_API`），不能只写增量单元素数组。

## 3. 证据

- `log-fail.txt`：拆分前 `strings` 实测统计——`out/vendor/android{14..17}/{,samsung/}bin/hw/android.hardware.health-service` 共 8 份非欧加二进制均命中 **44 条** `oplus` 字符串/符号（含 `vendor.oplus.hardware.charger.ICharger/default`、`/odm/etc/vintf/manifest/manifest_oplus_charger_aidl.xml`、`OplusChargerService`）；`out/vendor/android15/bin/hw/android.hardware.composer.hwc3-service.drm` 命中 **16 条** `oplus` 字符串/符号。
- `log-pass.txt`：拆分并重编 Android 14~17 全变体后 `strings` 复测统计——所有 `default` 与 `samsung` 二进制 `oplus_hits = 0`，`oplus` 变体二进制完整保留原有扩展实现且 `samsung_hits = 0`。

## 4. 改动

1. **`android.hardware.health` 拆出 `variants.oplus`**：
   - 将 `src/CdhOplusCharger.{h,cpp}` 移至 `src/oplus/CdhOplusCharger.{h,cpp}`；
   - `src/health_service.cpp`：用 `#if CDH_HEALTH_OPLUS` 包裹 `#include "oplus/CdhOplusCharger.h"` 与 `RegisterOplusChargerIfDeclared(...)` 调用；
   - `vendor/build/modules/android.hardware.health.json`：在 `variants` 下新增 `"oplus"`（`extraSourceDirs: ["src/oplus"]`、`includeDirs: ["src", "src/oplus"]`、`compile.defines: ["-DCDH_HEALTH_OPLUS=1"]`）。
2. **`android.hardware.graphics.composer3` 拆出 `variants.oplus`**：
   - 将 `src/CdhOplusDisplayColor.{h,cpp}` 移至 `src/oplus/CdhOplusDisplayColor.{h,cpp}`；
   - `src/service.cpp`：用 `#if CDH_COMPOSER_OPLUS` 包裹 `#include "oplus/CdhOplusDisplayColor.h"` 及 `RegisterOplusDisplayColorFeatureIfDeclared()` 调用；
   - `vendor/build/modules/android.hardware.graphics.composer3.json`：新增 `variants.oplus`（`extraSourceDirs: ["src/oplus"]`，`compile.defines` 保留基础 3 宏并追加 `-DCDH_COMPOSER_OPLUS=1`）。
3. **欧加系 7 份机型配置清单统一对齐 `variant: "oplus"`**：
   - `manifest/configs/oneplus/CPH2767/CPH2767-android16.json`（`health` → `oplus`）
   - `manifest/configs/oneplus/PJE110/PJE110-android13.json`（`health` → `oplus`）
   - `manifest/configs/oppo/CPH2797/CPH2797-android16.json`（`health` → `oplus`）
   - `manifest/configs/oppo/PME110/PME110-android17.json`（`health` → `oplus`）
   - `manifest/configs/realme/RMX3823/RMX3823-android13.json`（`health` → `oplus`）
   - `manifest/configs/realme/RMX5112/RMX5112-android13.json`（`health` → `oplus`）
   - `manifest/configs/realme/RMX6688/RMX6688-android15.json`（`health` 与 `graphics.composer3` 均 → `oplus`）
4. **文档与单测**：
   - 同步更新 `vendor/build/README.md` 与 `vendor/hardware/android.hardware.graphics.composer3/README.md`；
   - 在 `vendor/build/internal/spec/spec_test.go` 新增 `TestRealHealthAndComposer3OplusVariants`，守护真实模块配置的变体隔离与编译宏完整性。

## 5. 验证

1. **单元测试**：
   - `cd engine/vendor/build && go vet ./... && go test ./...` 全绿（含 `TestRealHealthAndComposer3OplusVariants`）；
   - `cd engine/build && go test ./...` 全 20 个测试包全绿。
2. **全版本全变体实编与二进制符号复核**：
   - 重编 `health`（Android 14 / 15 / 16 / 17 × `default` / `samsung` / `oplus` 共 12 份产物）与 `graphics.composer3`（Android 15 × `default` / `oplus` 共 2 份产物）全部一次通过；
   - `strings` 校验：
     - `default` 产物（`android14..17` `health-service` + `android15` `hwc3-service.drm`）：**`oplus_hits = 0`、`samsung_hits = 0`**；
     - `samsung` 产物（`android14..17` `health-service`）：**`oplus_hits = 0`、`samsung_hits = 173`**；
     - `oplus` 产物（`android14..17` `health-service` + `android15` `hwc3-service.drm`）：**`oplus_hits = 44 / 16`、`samsung_hits = 0`**。

## 6. 遗留

- 已发布的现有各非欧加机型 `patch.img` 镜像在下一次执行 `07_build_vendor → 09_build_patch` 时会自动拾取重新编译后的纯净 `out/vendor/android<ver>/` 基础变体二进制；欧加系机型重打 `07_build_vendor` 时会自动路由至 `out/vendor/android<ver>/oplus/`。
