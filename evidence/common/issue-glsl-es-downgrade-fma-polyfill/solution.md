# #211：GLSL ES 3.10/3.20 降级 3.00 时注入 `fma()` 兼容宏修复权限弹窗透明

## 问题

`gold_15-OS3.0.11.0.VNQCNXM` 云机在触发 MIUI / HyperOS 运行时权限弹窗（`com.lbe.security.miui:ui` `GrantPermissionsActivity`）及部分 SystemUI 圆角浮层（`com.android.systemui`）时，弹窗白色圆角底板与底部选项按钮背景完全透明，仅悬浮文字与位置示意方图，如高德地图定位权限弹窗截图 [`popup-before.jpg`](popup-before.jpg) 所示。

## 根因

宿主桌面 Mesa GLSL ES 编译器仅支持 GLSL ES 3.00，因此 `engine/vendor/hardware/android.hardware.gpu/src/cdh_gl_strings.c` 的 `glShaderSource()` 会将 Skia/HWUI 下发的 `#version 320 es` / `#version 310 es` 着色器原地改写为 `#version 300 es`。但原实现直接原地改版本号字符（`m[10] = '0'`），**未补充 `fma(a, b, c)` 定义**：

1. GLSL ES 3.10/3.20 内置 `fma()`，而 GLSL ES 3.00 无此内置函数；
2. Skia 绘制 `GrantPermissionsActivity` / SystemUI 的 Elliptical RRect（椭圆圆角矩形）片元着色器使用了 `fma()`，降级成 `#version 300 es` 后在 Mesa 侧报 `error: no function with name 'fma'`（`0:17(6)` / `0:22(6)`），Skia 编译失败后跳过圆角背景与按钮绘制；
3. 若直接扩大源码字符串长度且调用方传入了显式 `length[]` 数组，未同步增广 `length[i]` 会导致着色器源码尾部被截断。

## 证据

- 修复前：高德地图定位权限弹窗透明截图 [`popup-before.jpg`](popup-before.jpg) 与 Skia 报错摘录见同目录 [`log-fail.txt`](log-fail.txt)。
- 修复后：高德地图定位权限弹窗恢复实心白底截图 [`popup-after.png`](popup-after.png)、宿主单测结果与实机 `logcat` 零报错见同目录 [`log-pass.txt`](log-pass.txt)。

## 改动

1. **新增 [`cdh_gl_shader_patch.h`](../../../vendor/hardware/android.hardware.gpu/src/cdh_gl_shader_patch.h)**：
   - 定义 `CDH_FMA_POLYFILL`（`#ifndef fma / #define fma(a, b, c) ((a) * (b) + (c)) / #endif`）；
   - 实现 `cdh_find_glsl_310_or_320_es()`、`cdh_patch_glsl_es_chunk()` 与 `cdh_prepare_patched_sources()` / `cdh_free_patched_sources()`：在首个被降级的 `#version 310/320 es` 行结束换行符之后（先于任何 `#extension` 指令）插入 `CDH_FMA_POLYFILL`，将所有 `#version 310/320 es` 改写为 `#version 300 es`，并在 `length != NULL` 时同步更新对应分片的 `lens[i]` 避免截断。
2. **更新 [`cdh_gl_strings.c`](../../../vendor/hardware/android.hardware.gpu/src/cdh_gl_strings.c)**：
   - `glShaderSource()` 改用 `cdh_prepare_patched_sources(count, string, length, &patched)` 并将 `length ? patched.lens : NULL` 透传给底层 `real_fn`。
3. **新增 [`cdh_gl_strings_test.c`](../../../vendor/hardware/android.hardware.gpu/src/cdh_gl_strings_test.c)**：
   - 覆盖单片 `#version 320 es + #extension`、双片显式 `length[]` 截断防护、无换行头分片及 `#version 300 es` 原样透传 4 项宿主 C 单元测试。

## 验证

1. **宿主 C 单元测试**（2026-11-14 复测）：
   - 执行 `cc -std=c11 -Wall -Wextra engine/vendor/hardware/android.hardware.gpu/src/cdh_gl_strings_test.c -o $TMPDIR/cdh_gl_strings_test && $TMPDIR/cdh_gl_strings_test` 输出 `[pass] cdh_gl_strings_test: 4/4 passed`。
2. **`gold_15-OS3.0.11.0.VNQCNXM` 实例 2（`adb 127.0.0.1:5911`）实机复测**：
   - 重建 GPU HAL 并从 `07_build_vendor` 重打包冷启动后，拉起高德地图（`com.autonavi.minimap`）触发 `com.lbe.security.miui/...GrantPermissionsActivity` 定位权限弹窗：实拍截图 [`popup-after.png`](popup-after.png) 圆角白色底板与按钮恢复实体填充；
   - `logcat -d` 过滤 `"no function with name 'fma'|SkSL Error"` 真实编译报错为 `0` 条。

## 遗留

1. `fma(a, b, c)` 当前采用预处理宏 `((a) * (b) + (c))` 展开；若未来遇到带副作用参数或依赖单指令 FMA 舍入精度的自定义着色器，可改为在头部注入重载内联函数；另外 GLES 扩展集合对齐宿主/Mali 白名单问题仍在 `display` 域后续 P1 待办中追踪。
