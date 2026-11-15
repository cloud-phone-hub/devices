# 所有应用进程崩在 `Assertion failed: !gpuCount`（hwui 默认走 Vulkan）

- 机型：samsung / Galaxy S25 Ultra（`SM-S9380`，`ss9380`，OneUI Android 16）
- 发现时间：2026-10-30
- 盘集：build9 → build10
- 状态：已解决

## 一、问题

`sys.boot_completed=1` 之后画面始终全黑，只有右下角一个返回箭头。查进程发现
**所有 UI 进程启动即死**（`com.android.systemui`、`com.android.settings`、
`com.sec.android.app.SecSetupWizard`），crash buffer 里是同一句原生断言：

```
F native  : Assertion failed: !gpuCount
F libc    : Fatal signal 6 (SIGABRT), code -1 (SI_QUEUE)
```

## 二、根因

OneUI 的 `libhwui.so` 默认用 **Vulkan 后端**渲染，`RenderThread` 建上下文时走
`RenderThread::requireVkContext()` → `VulkanManager::initialize()`，最后在
`vkEnumeratePhysicalDevices` 返回 0 个物理设备时打 `LOG_ALWAYS_FATAL(!gpuCount)`。

本机 guest 侧 `/vendor/lib64/hw/` 里只有
`vulkan.virtio.so` / `vulkan.pastel.so` / `vulkan.lvp_mesa3d.so`，
而 **`/vendor/etc/vulkan/icd.d/` 整个目录不存在**，loader 枚举不到任何 ICD →
`gpuCount == 0`。`ro.hardware.vulkan` 是官方值 `adreno`，指向的驱动也不存在。

关键坑：**m22 用的 `ro.hwui.use_vulkan=false` 对 OneUI 无效**。反编译 OneUI 的
`libhwui.so` 里已经没有 `ro.hwui.use_vulkan` 这个字符串，只剩
`debug.hwui.renderer` 的 `skiavk` / `skiagl` 两个取值；写 `opengl` 会被静默忽略。

## 三、证据

- 失败态：`log-fail.txt`
  （`!gpuCount` 断言 + `strings libhwui.so` 无 `ro.hwui.use_vulkan` +
  运行中 `setprop debug.hwui.renderer opengl` 无效仍崩）
- 通过态：`log-pass.txt`
  （`debug.hwui.renderer=skiagl` 生效，`gpuCount` 崩溃计数归零，Settings 变 ResumedActivity）

## 四、改动

`devices/manifest/samsung/ss9380/baselines/SM-S9380_3_20260813083211_652iw49crl_fac.json`
的 `vendor.build-prop-sets` 增加：

```json
"debug.hwui.renderer": "skiagl"
```

> vendor 属性的真值在 baseline 的 `vendor.build-prop-sets`（由
> `engine/build/07_build_vendor/props.go` 落成 `vendor/build.prop`），
> 改 manifest 静态树里的 `vendor/build.prop` 不生效 —— 静态树只做整文件「身份层」覆盖。

## 五、验证

1. `07_build_vendor`→`11_pack_vda` 重跑通过（build10）；
2. 新实例上 `getprop debug.hwui.renderer` → `skiagl`；
3. `logcat -b crash -d | grep -c gpuCount` → `0`；
4. Settings 从「无法启动」变成正常 `ResumedActivity`，闪屏之后能出应用窗口。

## 六、遗留

- Vulkan 仍然不可用（没有 ICD），全部 UI 走 GL 后端；将来要跑 Vulkan 应用需要给
  guest 补 `vulkan.virtio.so` 的 `icd.d/*.json` 声明并把 `ro.hardware.vulkan` 调过去。
- 该属性只影响 hwui，不影响 SurfaceFlinger 自己的 RenderEngine（后者走 GL 正常）。
