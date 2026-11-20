# #277：GLSL `#version 320 es -> 300 es` 补齐 `ldexp` 宏修复 Android 13/14 `SurfaceFlinger` `wangs_formula` 圆角着色器黑屏

- 编号：`#277`
- 分组：`common`（引擎级 / 跨机型，适用全部基于 ANGLE/ES 3.0 的 Skia/SurfaceFlinger 圆角与路径着色器场景）
- 触发机型与复验样本：`blu/G1130`（`13-UP1A.231005.007-20260511`）、`asus/ASUS_AI2205`（`13-TKQ1.220928.001-20240229`）
- 状态：**已闭环**（2026-11-19，C 宿主单测全绿 + `G1130` / `ASUS_AI2205` 亮屏 `passed=true`）

## 1. 问题

部分 Android 13/14 机型在系统进程起齐后 VNC 仍全黑（`meanBrightness=0.0`），logcat 中 Skia 反复打印着色器编译失败：
- `error: no function with name 'ldexp'`（位于 `wangs_formula` 第 62/65/68 行的 `floor(ldexp(idxInResolveLevel, ...))` 表达式）。

## 2. 根因

`android.hardware.gpu` 中的着色器降级补丁 [cdh_gl_shader_patch.h](../../../../devices/hardware/android.hardware.gpu/src/cdh_gl_shader_patch.h) 此前将 `#version 320 es` / `#version 310 es` 降级为 `#version 300 es` 并注入了 `fma()` 宏，但 Android 13/14 Skia 路径细分 / 圆角着色器（`wangs_formula`）同时调用了 GLSL ES 3.10+ 才引入的内置函数 `ldexp(x, exp)`；降级为 `300 es` 后下层编译器不存在 `ldexp`，导致 `SurfaceFlinger` 着色器链接失败并黑屏。

## 3. 证据

- 失败现场日志：[log-fail.txt](log-fail.txt)（摘录自 `verify_pass3/G1130_13-UP1A.231005.007-20260511.log` 与 `verify_pass3/ASUS_AI2205_13-TKQ1.220928.001-20240229.log` 中的 `wangs_formula` 源码与 `error: no function with name 'ldexp'` 编译报错）。
- 修复后实机通过日志：[log-pass.txt](log-pass.txt)（`G1130` 亮屏 `meanBrightness=59.2`、`passed=true`；`ASUS_AI2205` 亮屏 `passed=true`）。

## 4. 改动

1. [cdh_gl_shader_patch.h](../../../../devices/hardware/android.hardware.gpu/src/cdh_gl_shader_patch.h)（`L11-L17`）：在 `CDH_FMA_POLYFILL` 中补充 `#ifndef ldexp` / `#define ldexp(x, e) ((x) * exp2(float(e)))` / `#endif`，将 GLSL ES 3.10+ 的 `ldexp` 等价转换为 ES 3.00 原生支持的 `exp2(float(e))`。
2. [cdh_gl_strings_test.c](../../../../devices/hardware/android.hardware.gpu/src/cdh_gl_strings_test.c)（`L75`）：同步更新宿主单测断言，验证 `CDH_FMA_POLYFILL` 注入包含 `fma` 与 `ldexp` 两段宏定义。

## 5. 验证

1. 宿主 C 单测及 `android.hardware.gpu` 编译通过；
2. 实机冷启动验收：`blu/G1130` 消除 `ldexp` 报错并成功亮屏通过（`passed=true`、`meanBrightness=59.2`、`serviceCount="297"`）；`asus/ASUS_AI2205` 同步进入桌面（`passed=true`、`serviceCount="299"`）。

## 6. 遗留

- 无。
