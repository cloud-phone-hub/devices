# Issue: popsicle `16-OS4.0.5.0.XPBCN` 冷启动卡开机 Logo 进不了桌面：`sphal` `common-V6-ndk` 代际错配、GKI 39-bit VA PAC 高位签名拦截 `vulkan.pastel.so` 与缺自包含 QTI Mapper

- 设备：`xiaomi/popsicle`（Xiaomi 17 Pro Max / `canoe` / HyperOS 4 `16-OS4.0.5.0.XPBCN` / Android 16 / `lineage-23.2` / kernel `6.12.81`）
- 前序条目：`../issue-android16-hal-deps-missing/`（#247 跑通 01→13 构建，遗留「未上机验证新盘集」）
- 验证盘集：`vda-d5f165d2f167` + `patch.img`（`sha256: ac51c5c3736e`）
- 状态：**已闭环**（2026-11-18，实例 #1 全新冷启动）：`sys.boot_completed=1`、`init.svc.bootanim=stopped`、首屏焦点 `com.miui.home/.launcher.Launcher`，`zygote64` / `surfaceflinger` / `system_server` / `com.miui.home` / `com.android.systemui` / `com.miui.miwallpaper` 全部首次拉起零重启，`native_F_DEBUG=0`、`sphal_blocked=0`

---

## 一、问题

承接 `../issue-android16-hal-deps-missing/`（#247）产出的 `popsicle_16-OS4.0.5.0.XPBCN` 盘集上机冷启动，画面一直停留在小米 / HyperOS 启动 Logo，`sys.boot_completed` 永不置位，无法进入桌面。逐层排障共暴露出**三道串联卡点**：

1. **卡点 #1（卡开机 Logo，`zygote64` 无限 `SIGSEGV`）**：`zygote64` 每次启动在 `eglGetDisplay+68` 触发 `SIGSEGV` 被 `init` 杀掉重拉；且 `libEGL_meow.so` 日志原本只打印 `dlopen 基座库失败: (null)`，真实链接器报错被吞掉。
2. **卡点 #2（过 Logo 后桌面起不来，`com.miui.home` 加载 `vulkan.pastel.so` 失败）**：`zygote64` 与 `system_server` 正常进系统后，`com.miui.home`（`uid=10147`）报 `dlopen failed: library "vulkan.pastel.so" not found`，无法创建桌面渲染管线。
3. **卡点 #3（桌面进程存活后，`SystemUI` 与 `miwallpaper` 循环 `SIGABRT`）**：`com.miui.miwallpaper`（`uid=10185`）与 `com.android.systemui`（`uid=10224`）反复崩溃，崩溃摘要均为 `Abort message: 'gralloc-mapper is missing'`。

---

## 二、根因

### 1. `gpu.go` 双重调用 `dlerror()` 吞掉真实链接器错误 + `common-V6-ndk.so` 重定向落空（卡点 #1）

- **日志截断根因**：`agent/modules/pipeline/internal/halcore/halbuild/gpu.go` 生成的 `libEGL_meow.so` 写成了 `dlerror() ? dlerror() : "?"`。POSIX / Bionic 的 `dlerror()` 首次调用返回错误字符串并**立即清空内部错误槽**，第二次调用必然返回 `NULL`，导致 `ALOGE("%s", NULL)` 永远打印 `(null)`。
- **`zygote64` 崩溃根因**：修好 `dlerror()` 后暴露出精确报错：
  `dlopen failed: library "android.hardware.graphics.common-V6-ndk.so" not found: needed by /vendor/lib64/android.hardware.graphics.allocator-V2-ndk.so in namespace sphal`。
  `zygote64`（`uid=0`）预加载图形栈时，`allocator-V2-ndk.so` 回退命中官方 `/vendor/lib64/android.hardware.graphics.allocator-V2-ndk.so`（Android 16 SDK 36 编译，依赖 `common-V6-ndk.so`）；而 `/vendor/lib64` 整目录重定向到了载体 `/mnt/vendorpatch/vendor/lib64`，`lineage-23.2` 基座已升级到 `common-V7-ndk.so`、载体里没有 `common-V6-ndk.so`，官方 `/vendor/lib64/android.hardware.graphics.common-V6-ndk.so` 被目录重定向遮蔽。
- **架构约束**：路径重定向严禁在 `cdh_redirect` C 代码或 Go 代码里写死库名列表，必须通过机型配置 `module/config.json` 的 `redirect` 声明式规则表达——在 `dir /vendor/lib64` 之前放置一条 `file` 自映射护栏规则即可让官方 `common-V6-ndk.so` 原地透出。

### 2. GKI 4K 内核 39-bit VA 下 ARMv8.3 PAC 指针认证高位未剥离（卡点 #2）

- `com.miui.home`（`uid=10147 > 10000`）受 `cdh_redirect` Tier-B 白名单约束：`vulkan.pastel.so` 仅在调用栈帧回溯命中 `vulkan.mali.so`（`caller_has_vulkan_interposer()`）时才对普通应用放行。
- 本机内核为 GKI 4K 页配置（`CONFIG_ARM64_VA_BITS=39`，用户态有效虚拟地址空间 `[38:0]`，`DEFAULT_MAP_WINDOW_64 = 1UL << 39`），且 Android 16 / `popsicle` 默认开启全套 ARMv8.3 PAC 指针认证（tombstone 实测 `pac_enabled_keys: 000000000000000f`）。函数压栈保存的 `LR`（`frame[1]`）在位 `[54:39]` 携带非零 PAC 签名。
- `devices/modules/cdh_redirect/core/rules.c` 原先硬编码了 48-bit 掩码 `0x0000ffffffffffffUL`，导致位 `[47:39]` 的 PAC 签名残留，传入 `find_vma(mm, pc)` 的地址大于 `0x8000000000UL` 查不到任何 VMA，栈回溯无法识别 `vulkan.mali.so` 调用者，从而将 `vulkan.pastel.so` 误判拦截。
- 注：GKI 6.12 `Module.symvers` 未导出 `vabits_actual` 符号，外部模块必须使用内核头编译期常量 `#define CDH_USER_VA_MASK (DEFAULT_MAP_WINDOW_64 - 1UL)`。

### 3. 缺自建 `android.hardware.graphics.allocator` + `lineage-23.2` `_upstream` 后缀未收口（卡点 #3）

- 官方 `/system/lib64/libui.so` 依赖 Xiaomi 定制属性读取 `@5.0/qti`（即 `/vendor/lib64/hw/mapper.qti.so`）。
- `popsicle-android16` 的 `hardware/config.json` 漏配了 `"android.hardware.graphics.allocator"`（对比 `gold-android15` 已配），导致 `09_build_vendor` 退回使用 `lineage-23.2` 基座摊平的 APEX 产物 `mapper.minigbm_upstream.so`。
- 一方面 `files_compat.go` 的 `alignVendorGrallocConfig` 之前只认无后缀的 `mapper.minigbm.so` / `mapper.minigbm.xml`，未处理 `lineage-23.2` 新增的 `_upstream` 变体名；另一方面基座 `mapper.minigbm_upstream.so` 动态依赖载体私有库 `libminigbm_gralloc_upstream.so`，`uid > 10000` 的 `SystemUI` 与 `miwallpaper` 在 `sphal` 命名空间无法加载私有载体库，直接触发 `Abort message: 'gralloc-mapper is missing'`。
- 启用本仓自研构建的 `"android.hardware.graphics.allocator"` 后，产出的 `mapper.minigbm.so` 将 minigbm 静态内聚（零 `libminigbm_gralloc*.so` 外部依赖），再由 `09_build_vendor` 对齐为 `mapper.qti.so` + `mapper.qti.xml` 并清理 `_upstream` 残留即可彻底闭环。

---

## 三、证据

- 失败现场：同目录 `log-fail.txt`（分阶段完整记录 `zygote64` `eglGetDisplay+68` `SIGSEGV`、`common-V6-ndk.so not found`、`vulkan.pastel.so not found` + `sphal_blocked: 4115`、以及 `miwallpaper` / `systemui` `pac_enabled_keys: 0xf` + `'gralloc-mapper is missing'`）。
- 通过现场：同目录 `log-pass.txt`（实例 #1 全新冷启动属性表、7 个关键进程首次拉起 PID、`topResumedActivity`、`mapper.qti.so` SELinux 上下文、`/proc/cdh_redirect` 11 条规则与 `sphal_blocked: 0`、零崩溃统计、以及 `go test ./modules/pipeline/...` 全绿输出）。
- 相关前序：`../issue-hyperos4-pastel-vulkan-and-wdt/`（#166，`vulkan.mali.so` -> `vulkan.pastel.so` 链路背景）与 `../issue-android16-hal-deps-missing/`（#247，Android 16 HAL 依赖补齐）。

---

## 四、改动

### 1. 声明式配置收口（零硬编码重定向表）

- `devices/manifest/configs/xiaomi/popsicle/popsicle-android16/module/config.json`：
  在 `dir /vendor/lib64 -> /mnt/vendorpatch/vendor/lib64` **之前**新增自映射护栏规则：
  ```json
  {
    "from": "/vendor/lib64/android.hardware.graphics.common-V6-ndk.so",
    "to": "/vendor/lib64/android.hardware.graphics.common-V6-ndk.so",
    "type": "file"
  }
  ```
- `devices/manifest/configs/xiaomi/popsicle/popsicle-android16/hardware/config.json` & `vendor/config.json`：
  对齐 `gold-android15` 增加自建 `"android.hardware.graphics.allocator"`（`version: "2.0"`, `variant: "default"`），并将 `ro.vendor.cdh.gpu.gl_extra_exts2` 设为 `"none"`（剔除 VirGL 不支持的 `GL_EXT_geometry_shader` / `GL_EXT_tessellation_shader` 伪造宣告）。

### 2. `02_prepare` 与 `09_build_vendor` 流水线通用对齐能力

- `agent/modules/pipeline/stages/02_prepare/firmware_measured_extra.go` & `apply_profile_configs.go`：
  测量官方 `/vendor/lib64/android.hardware.graphics.common-V*-ndk.so` 存在性；对 Android 16+ 且官方自带 `common-V6-ndk.so` 的机型，在生成 `module/config.json` 时自动将自映射 `file` 规则插在父目录 `dir /vendor/lib64` 之前；同时在 `apply_profile.go` 中通过 `sanitizeVirglSafeGLExtraExts` 过滤 VirGL 不安全扩展。
- `agent/modules/pipeline/stages/09_build_vendor/files_compat.go`（`alignVendorGrallocConfig`）：
  兼容 `lineage-23.2` APEX 摊平的 `_upstream` 后缀——删除冗余 `mapper.minigbm_upstream.so`、将自建自包含的 `mapper.minigbm.so` 重命名为 `mapper.qti.so`、将 `mapper.minigbm_upstream.xml` 对齐为 `mapper.qti.xml`、将 `allocator-service.minigbm_upstream`（及 `.rc` 引用）统一收敛为 `allocator-service.minigbm`，并对 `vendor_service_contexts` 去重保留唯一 `mapper/qti`。
- 配套单测：`02_prepare`、`09_build_vendor`、`halbuild` 同目录 `*_test.go` 同步补齐断言。

### 3. GPU 拦截层错误日志与 `cdh_redirect` 39-bit VA PAC 掩码修复

- `agent/modules/pipeline/internal/halcore/halbuild/gpu.go`：
  将 `dlerror() ? dlerror() : "?"` 改为单次缓存 `const char *err = dlerror();`，消除双重调用清空错误槽导致的 `(null)`。
- `devices/modules/cdh_redirect/core/rules.c`：
  严守不在 C 代码里硬编码任何库路径表的前提下，仅将栈回溯地址掩码从硬编码 48-bit `0x0000ffffffffffffUL` 修正为 `#define CDH_USER_VA_MASK (DEFAULT_MAP_WINDOW_64 - 1UL)`，在 39-bit VA GKI 内核上准确剥离 ARMv8.3 PAC `[54:39]` 签名位。

---

## 五、验证

| 核验项 | 结果 |
| :--- | :--- |
| 构建与单测 | `cdh-pipeline build-hardware --android 16 --module android.hardware.graphics.allocator --refresh` 通过；`cdh-pipeline device build popsicle --manifest 16-OS4.0.5.0.XPBCN` 产出 `vda-d5f165d2f167` + `patch.img`（`ac51c5c3736e`）；`go test ./modules/pipeline/...` 全包通过 |
| 冷启动耗时与状态 | 实例 #1 `14:18:17` 拉起 QEMU，首次开机 `IArtd` dexopt 后于 `14:20:35` 完成启动：`sys.boot_completed=1`、`init.svc.bootanim=stopped` |
| 首屏桌面焦点 | `topResumedActivity=ActivityRecord{94360679 u0 com.miui.home/.launcher.Launcher t10}`；QEMU `cgl-headless` 稳定输出 `1200x2608` 桌面帧（单帧 ~1.8 ms） |
| 关键进程稳定性 | `zygote64`（`649`）、`surfaceflinger`（`710`）、`allocator-service.minigbm`（`752`）、`system_server`（`759`）、`com.miui.miwallpaper`（`2891`）、`com.android.systemui`（`3184`）、`com.miui.home`（`3745`）全部为首次拉起 PID，零重启 |
| QTI Mapper 落盘 | `/vendor/lib64/hw/mapper.qti.so`（`368096 B`，`u:object_r:same_process_hal_file:s0`）+ `/mnt/vendorpatch/vendor/etc/vintf/manifest/mapper.qti.xml` |
| 声明式重定向表 | `/proc/cdh_redirect` 显示 `rules: 11/32`，`file /vendor/lib64/android.hardware.graphics.common-V6-ndk.so` 位于 `dir /vendor/lib64` 之前；`forward_missed: 0`、`sphal_blocked: 0` |
| 崩溃与图形错误计数 | `native_F_DEBUG: 0`、`gralloc_mapper_missing: 0`、`zygote_dlopen_fail: 0`、`pastel_dlopen_fail: 0` |

---

## 六、遗留

1. **`.qtidataservices` Java 层 `UnsatisfiedLinkError`**：`logcat -b crash` 中存在后台电话扩展进程 `.qtidataservices` 查找 `Java_vendor_qti_iwlan_JniHelper_getAllQualifiedNetworks` 的非致命 Java 异常（不影响 `system_server`、桌面、网络与图形渲染），后续可在 IWLAN / Radio 专项收口。
2. **其它 Android 16 新机型复用检查**：`oneplus/15r` 等同属 `lineage-23.2`（Android 16）的新机型接入时，直接通过 `02_prepare` 自动测量生成 `common-V6-ndk.so` 自映射配置并确保 `hardware/config.json` 包含 `"android.hardware.graphics.allocator"` 即可复用本套闭环。
