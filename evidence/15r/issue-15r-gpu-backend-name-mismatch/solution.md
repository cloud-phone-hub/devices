# Issue: 15r 基线未锁 GPU 后端名，`ro.hardware.egl=adreno` 让 surfaceflinger 断言崩溃循环、`ro.hwui.use_vulkan=true` 让三个 UI 进程断言崩溃循环 → 起机黑屏

- 设备: OnePlus 15R（`15r`，CPH2769 / ColorOS 16.0.10.500_EX01 / 骁龙 `canoe` / Android 16，lineage-23.2 + kernel 6.12.81）
- 宿主: macOS (darwin, QEMU + hvf, virtio-gpu-gl + venus/mesa)
- 状态: **已解决**（2026-11-04）。基线 `vendor.build-prop-sets` 补两键
  （`ro.hardware.egl=mesa`、`ro.hwui.use_vulkan=false`），重跑 07→11 后从零起盘：
  52.25 s 出现 `init: processing action (sys.boot_completed=1)`、SF 崩溃 0 次、
  抓帧 1280x2800 平均亮度 63.9/峰值 255（ColorOS 桌面），**本轮验收通过**。

---

## 一、问题

本轮「依次构建 + 起机测试」的第 4 台。15r 的 11 阶段构建全绿（`out/dist/oneplus/15r/16.0.10.500_EX01/vda.img`
13.00 GiB），实例 `runtime/15r_16.0.10.500_EX01/1` 起来后分两层：

**run1（基线原样）**：`sys.boot_completed` 永不置位。qemu.log 里 surfaceflinger 每约 5 秒一次
`received signal 6`，共 94 行；init 每次 SF 死掉都顺手 `Sending signal 9 to service 'zygote'`，
连坐重启的还有 `vendor.audio-hal-aidl`/`netd`/`audioserver`/`wificond`/`mediaextractor`/
`media.swcodec`/`media`/`cameraserver`（各 14~15 次）。抓帧停在 EFI 控制台尺寸
（1024x768、平均亮度 0.1），从未进入安卓画面。

**run2（只补 `ro.hardware.egl=mesa`）**：引导层通了 —— 52.68 s 出现
`init: processing action (sys.boot_completed=1)`、SF 崩溃 0 次、`critical process` 0 次；
但屏幕全黑：连续三帧 1280x2800 **平均亮度 0.0 峰值 0**（一个非零像素都没有），
`boot_check.py` 判 `[fail] 引导完成但截图全黑`。guest 侧 `screencap` 直接挂死（命令下发后
不返回），SF 的 Scheduler 每帧报 `trackPendingFrame: Invalid present fence`，
composer 每轮只收到空图层集（`I/drmhwc: No layers to validate.` / `No layers to present.` 40 次）。
logcat 里 257 条同一个断言 `Abort message: 'Assertion failed: !gpuCount'`，
崩溃主体恰好是 `com.oplus.battery` 105 次、`com.android.launcher` 84 次、`com.android.systemui` 67 次。

## 二、根因

同一类根因的两段：**官方 vendor 件里的 GPU 后端名是物理 SoC 的，云机 vendor 树里没有同名实现件**，
而消费侧（libEGL / libhwui）都是「按属性拼库名，拼不到就 `__android_log_assert` 自杀」。

| 层 | 属性（官方 vendor build.prop） | 树里实际有什么 | 断言点 | 后果 |
| :--- | :--- | :--- | :--- | :--- |
| 1 | 第 128 行 `ro.hardware.egl=adreno` | `/vendor/lib64/egl/` 只有 `libEGL_mesa.so`、`libEGL_meow.so` | `libEGL.so!android::Loader::open+1856` → `SkiaGLRenderEngine::create+96`：`couldn't find an OpenGL ES implementation, make sure one of persist.graphics.egl, ro.hardware.egl and ro.board.platform is set` | SF 每 5 s SIGABRT，引导不动 |
| 2 | 第 45 行 `ro.hwui.use_vulkan=true`（配 `ro.hardware.vulkan=adreno`，第 127 行） | `/vendor/lib64/hw/` 只有 `vulkan.{lvp_mesa3d,mali,pastel,virtio}.so`，**无 `vulkan.adreno.so`** | `libhwui.so!renderthread::VulkanManager::initialize()`（`requireVkContext` ← `SkiaVulkanPipeline::setSurface`）：`Assertion failed: !gpuCount` | 每个用 HWUI 的进程 RenderThread 自杀；桌面/状态栏永远画不出图层 |

两层是串联的，不是同一件事：第 1 层修完才看得见第 2 层。SF 自己走 GL（`SkiaGLRenderEngine`），
所以第 1 层修好后 SF 稳定；但 app 侧的 RenderThread 由 `ro.hwui.use_vulkan` 决定管线，
ColorOS 这台机选的是 Vulkan，而云机没有 Vulkan 驱动 —— 于是「引导完成 + 全黑 + screencap 挂死」
这个组合：SF 活着但每帧拿不到任何图层，present fence 无效。

`ro.hardware.vulkan` 保持官方 `adreno` 未动，改在**消费侧**关掉 Vulkan 管线：把同一份 vendor build.prop 第 45 行的
`ro.hwui.use_vulkan` 置 `false`。这不是新发明——`m22`、`gold`、`cph2797`、`ss9380`、`popsicle` 的基线
早就都锁了 `ro.hwui.use_vulkan=false`（官方件一律是 `true`），15r 基线只是漏登记这一键，与 #149
「oriole AP2A 基线漏六键」是同一形状的问题。至于不把 `ro.hardware.vulkan` 锁成 `virtio` 让 Vulkan 真起来：
venus→MoltenVK 的能力集在本宿主从未验证，而 GL 路径是本轮实测已经出画的（见五、验证与 log-fail.txt 的免重编旁证）。

## 三、证据

`log-fail.txt`（run1 + run2 原始日志）与 `log-pass.txt`（run3 通过面），要点：

1. run1 崩溃主体与断言原文：`15r-logcat.txt` 第 515~549 行（`>>> /system/bin/surfaceflinger <<<`、
   `Loader::open+1856`、`SkiaGLRenderEngine::create+96`）；qemu.log 时序块见 `15r-fail-qemu.log`
   第 3293/3553/3865/3952 行。
2. run2 断言原文与计数：`15r-logcat-pass.txt` 里 `!gpuCount` 257 次，按主体 105/84/67 分布；
   backtrace 第 04~07 帧为 `VulkanManager::initialize → __call_once → requireVkContext →
   SkiaVulkanPipeline::setSurface`。
3. run2 的「全黑」是宿主侧 websocket 抓帧（5913 口）连续三帧 0.0/0，附图
   `desktop-black-run2.png`；run3 同口径 63.9/255，附图 `desktop-pass.png`。
4. 免重编旁证（决定性）：同一 run2 实例上 `setprop debug.hwui.renderer skiagl` + 重拉 launcher，
   抓帧立刻从 `0.0/峰值 0` 变成 `64.2/峰值 255` —— 说明除管线选择外，SF/mesa/合成链都是好的。
5. 属性与实现件对照：官方件 `out/roms/oneplus/15r/16.0.10.500_EX01/super/unpack_vendor/build.prop`
   第 45/127/128 行；树内实现件 `out/devices/oneplus/15r/16.0.10.500_EX01/vendor/lib64/egl/`、
   `.../vendor/lib64/hw/`。
6. 同族口径（本轮逐台复核）：`m22`、`cph2797`、`ss9380`、`popsicle` 的基线都锁了 `ro.hardware.egl=mesa` **且** `ro.hwui.use_vulkan=false`；
   `oriole`(cp1a/AP2A)、`oppo/pme110`、`nothing/3a`(Asteroids B4.1) 锁 `egl=mesa + vulkan=virtio + gralloc=minigbm`；
   `gold` 锁 `egl=meow + vulkan=mali`（它的树里确实有 `libGLES_mali.so`/`vulkan.mali.so`）+ `use_vulkan=false`。
   **15r 原先两条都没登记**，所以官方值原样透传；`oriole` 的 ap2a/cp1a 基线也没有 `ro.hwui.use_vulkan`，
   见 [#150](../../oriole/issue-oriole-system-server-crash-loop/solution.md) 遗留里补的这条线索。

## 四、改动

只改数据，不改代码：`manifest/oneplus/15r/baselines/CPH2769_16.0.10.500_EX01_A51_full.json`
的 `vendor.build-prop-sets` 从 2 键补到 4 键。

```json
"build-prop-sets": {
  "ro.vendor.boot_security_patch": "2026-08-01",
  "ro.soc.model": "vsoc_arm64",
  "ro.hardware.egl": "mesa",
  "ro.hwui.use_vulkan": "false"
}
```

`vendor.note` 同步补了这两键的实测出处、断言原文、计数、免重编旁证，以及
「为什么不锁 `ro.hardware.vulkan=virtio`」「为什么不锁 `ro.hardware.gralloc`」两条否决理由
（本树没有 `gralloc.minigbm.so`，只有 `gralloc.default.so` + AIDL 服务
`vendor.graphics.allocator.minigbm_upstream`，实测 `init.svc=running`，套 oriole 口径会打断 legacy get_module）。

生效链：07 `build_vendor` 的基料是**官方** vendor `build.prop`（`out/roms/.../super/unpack_vendor/build.prop`），
`build/vendor/build/props.py` 把 `build-prop-sets` 覆盖上去 → `vendor.img` → `super.img` → `vda.img`。
重跑 `cdh device build 15r --from build_vendor`（07→11），并从 07 产物
`out/devices/oneplus/15r/16.0.10.500_EX01/vendor/build.prop` 第 45/128 行复核实值。

## 五、验证

run3 从零起盘（先 `/api/stop` 再 `/api/delete`，不复用旧 overlay）：

- `boot_check.py`：`[pass] 引导完成（日志 8462 行）+ 截图非黑（平均亮度=3.8 峰值=102）`；
- qemu.log（13294 行）：52.254431 s `init: processing action (sys.boot_completed=1)`，
  SF `received signal` **0** 次（run1 为 94），`critical process` 0 次；
- VM 内实测：`ro.hwui.use_vulkan=false`、`ro.hardware.egl=mesa`、`sys.boot_completed=1`，
  `com.android.launcher` pid 3420、`com.android.systemui` pid 2776 存活，
  `mFocusedApp=com.android.launcher/.Launcher`；
- 抓帧（websocket 5913）：`1280x2800 平均亮度=63.9 峰值=255`，`desktop-pass.png` 里是完整
  ColorOS 桌面（状态栏、时钟组件、Phone Manager/Photos/Notes/Gemini 图标、充电上限弹窗）；
- run3 logcat 残留：`Fatal signal` 1 次（ColorOS 自带 `minidumpreader` SIGSEGV，run1/run2 同样存在，
  不在引导链上）、`gpuCount` 0 次（run2 为 257）、`No layers to present` 0 次（run2 为 40）。

## 六、遗留

1. **Vulkan 面仍缺**：`ro.hardware.vulkan=adreno` 未锁，本树可用的是 `vulkan.virtio.so`。
   现在只是让 hwui 不去要 Vulkan；直接调 Vulkan 的 app（游戏、`dEQP`、部分相机后处理）仍然起不来。
   要补的话得先在宿主上实测 venus 的能力集，再决定「锁 virtio」还是「继续关消费侧」。
2. **同族机型的预检口径**：凡官方 vendor build.prop 里 `ro.hardware.egl`/`ro.hardware.vulkan`
   指向物理 GPU 名（adreno/mali 等）的机型，起机前都该按本条方法对一遍「属性名 ↔ 树内实现件」，
   而不是等 SF 崩起来才看见。`cph2797`/`ss9380`/`popsicle` 已锁 egl，但 `ro.hwui.use_vulkan`
   是否需要同样处理要看各自官方件与树内 `vulkan.*.so`，本轮逐台复核。
3. `minidumpreader` 的 SIGSEGV 是 ColorOS 自带件在云机上的独立问题（它依赖
   `vendor.oplus.hardware.stability.oplus_project.IOplusProject/default`，qemu.log 里 `find` 被 denial 79 次），
   不影响引导，未单列条目。
