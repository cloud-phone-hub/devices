# Widevine HAL 在云机不可用（gold / Redmi Note 13 5G，Android 15）

- 设备：xiaomi/gold（gold-15，`gold_cn_OS3.0.10.0.VNQCNXM`）
- 实例：5
- 组件：`com.google.android.widevine` APEX（versionCode 190240725，nonupdatable）
- 产物：`engine/out/dist/xiaomi/gold/OS3.0.10.0.VNQCNXM/patch.img`
  sha256 `027fb407645096fc1ad54b172f407f0e55bc631e3786a007916ad5816ba80a1e`

## 问题

云机里 Widevine 完全不可用，但表面上"什么都不缺"：

- APEX 已预装且激活：`/vendor/apex/com.google.android.widevine.nonupdatable.apex`，
  `isActive="true"`；
- APEX 自带 `bin/hw/android.hardware.drm-service.widevine`、
  `etc/com.google.android.widevine.rc`（服务名 `vendor.drm-widevine-hal`）、
  `etc/vintf/com.google.android.widevine.xml`；
- 但 `init.svc.vendor.drm-widevine-hal` 为空，`service list` 里没有任何
  `android.hardware.drm.IDrmFactory/*`，框架只能拿到 `drm.drmManager` 空壳。

## 根因

两层，必须同时补齐：

1. **基座 vendor 策略没有 `hal_drm_widevine` / `hal_drm_widevine_exec` 类型。**
   APEX 镜像里 inode 的 SELinux xattr 是烘好的
   `u:object_r:hal_drm_widevine_exec:s0`，但策略未声明该类型，内核把它当
   unmapped（avc `tctx=unlabeled`、`trawcon` 指向正确标签），init 因此报
   "incorrect label or no domain transition"，拒绝启动服务。
   同平台真机（MTK Android 15，plat vers 202404）的 `vendor_sepolicy.cil` 里有
   完整的 `hal_drm_widevine` 段，云机基座（lineage 路线）没有。

2. **补齐类型后仍会 abort：AIDL HAL 注册 servicemanager 缺授权。**
   该 HAL 用 `AServiceManager_addService` 向 **servicemanager**（不是
   vndservicemanager）注册；服务名 `android.hardware.drm.IDrmFactory/widevine`
   没有 `vendor_service_contexts` 映射，落 `default_android_service` 标签。
   只照抄真机 widevine 段里的 vndbinder/vndservicemanager 规则时，缺：
   - `(allow hal_drm_widevine servicemanager_{{vers}} (binder (call transfer)))`
   - `(allow hal_drm_widevine default_android_service_{{vers}} (service_manager (add find)))`

   实测 avc `denied { add } tclass=service_manager` → `addService` 返回 -1 →
   进程 `CHECK failed: status == STATUS_OK` → SIGABRT，init 反复 `restarting`。

## 证据

详见同目录 `log-fail.txt`（修复前与第一版补丁的失败态）与 `log-pass.txt`
（最终通过态）。要点：

- 修复前串口（`qemu-20261028-014035.log:2802`）：
  `SELinux: Context u:object_r:hal_drm_widevine_exec:s0 is not valid (left unmapped)`
  + init `Could not start service 'vendor.drm-widevine-hal'`；
- 修复前 `ls -Z` 显示二进制 `u:object_r:unlabeled:s0`；
- 第一版补丁（只补类型/域转换/vndbinder）开机
  （`qemu-20261028-040303.log:2833,2853`）：
  `avc: denied { add } ... tcontext=default_android_service ... permissive=0`
  → `received signal 6`，guest logcat `Failed to Widevine Factory HAL, status=-`；
- 基座策略对照：`mediacodec`、`hal_audio_default`、`hal_radio_default` 等
  通过 servicemanager 注册的 vendor HAL 都带有上面两条同类授权。

## 改动

1. **新增机型层策略片段** `devices/manifest/xiaomi/gold/sepolicy/vendor_sepolicy.cil`
   （`07_build_vendor` 以 marker `;; vendor_sepolicy.cil` 整块追加/替换到
   `/vendor/etc/selinux/vendor_sepolicy.cil`），内容：
   - 类型/属性：`hal_drm_widevine`、`hal_drm_widevine_exec` + roletype，
     归入 `domain`、`halserverdomain`、`halclientdomain`、`hal_drm`、
     `hal_drm_server`、`hal_allocator_client`、`file_type`、`exec_type`、
     `vendor_file_type`；
   - 启动链：`init_*` 对 exec 的 file 权限、`init_*`→domain 的 process
     transition/siginh/rlimitinh、`typetransition init_* *_exec process *_domain`；
   - APEX 内库（`vendor_file_*`）只读映射、vndbinder/vndservicemanager 双向授权；
   - **servicemanager 注册**：`servicemanager_* binder call/transfer` +
     `default_android_service_* service_manager add/find`；
   - allocator/mediacodec fd use；`/data/vendor/mediadrm`（gold 标签
     `vendor_data_file_*`）；`debugfs_tracing_*` write、`mnt_vendor_file_*`
     与 `block_device_*` 目录搜索。

   相对真机规则的裁剪：去掉云机不存在的 `/dev/drm_wv`、kisd、gz、vcu、
   teeperf、kb/dkb、sysfs_mmcblk、`vendor_mtk_soc_prop`、`debugfs_ion`、
   `persist_data_file`；`mediadrm_vendor_data_file` 改成本盘实际的
   `vendor_data_file_*`。平台类型按 vendor CIL 约定使用 `{{vers}}` 影子名，
   构建阶段替换为 `202404`。

2. **设备 Python 流水线同步**：`devices/xiaomi/gold-15/build/vendor/build/sepolicy.py`
   新增 `WIDEVINE_MARKER` / `widevine_hal_cil(vers)`，并在 `hal_cil_chain` 中
   MEDIA 段之后注入（`just device build` 路线与引擎路线规则等价，均 19 条 allow）。

3. 重建链：`07_build_vendor → 09_build_patch → 11_pack_vda`（`pack-vda` 需带
   `--lineage-zip` 与 `--build-id`，否则 device.json id 会丢 `_cn` 段）。

## 验证

| # | 验证点 | 结果 |
| :--- | :--- | :--- |
| 1 | 产物抽件 | dist `patch.img` 内 `/vendor/etc/selinux/vendor_sepolicy.cil` 与装配树逐字节一致（sha256 `8d165887…`） |
| 2 | init 启动 | 串口 `starting/started service 'vendor.drm-widevine-hal' has pid 554`；崩溃/重启计数 0 |
| 3 | 运行态 | Enforcing 下 `getprop init.svc.vendor.drm-widevine-hal=running`，pid 554 自开机未变 |
| 4 | 服务注册 | `service list` 出现 `38 android.hardware.drm.IDrmFactory/widevine` |
| 5 | 标签 | 二进制 `u:object_r:hal_drm_widevine_exec:s0`；`/data/vendor/mediadrm` 为 `vendor_data_file` |
| 6 | avc | 本次开机 `avc: denied ... hal_drm_widevine` 0 条 |
| 7 | 框架端到端 | 探针 `MediaDrm.isCryptoSchemeSupported(Widevine UUID)=true`（04:05 `app_process`；04:11 重启后用 `dalvikvm -cp … DrmProbe` 复验仍为 true） |
| 8 | 双流水线一致 | Python `widevine_hal_cil("202404")` 生成 19 条 allow，含上述两条注册规则 |
| 9 | 重启后复验 | 04:11 实例重启（另行修复宿主双网关，见 `../issue-daemon-backend-reap/`）后：`su 0 getprop init.svc.vendor.drm-widevine-hal=running`、`service list` 仍有 `IDrmFactory/widevine`、`ps` 进程 946、`hal_drm_widevine` avc 0 条 |

## 遗留

1. **未验证真正的解密播放**：Widevine L3 provisioning 需要 Google 认证的设备
   证书与唯一设备 ID（联网换证书），云机没有认证密钥；`openSession` / 许可证
   流程需要在有认证环境的应用侧再验。本次只验到「HAL 常驻 + 框架能枚举工厂」。
2. **ClearKey**：04:05 探针报 `clearkey_supported=false`（当时 clearkey HAL 未起）；
   04:11 重启后 clearkey HAL（`android.hardware.drm-service.clearkey`，pid 859）已
   running，探针为 `clearkey_supported=true`。本次仍未做 ClearKey 的
   `openSession`/解密验证。
3. **服务落 `default_android_service` 标签**（服务名未在 `vendor_service_contexts`
   映射，与真机一致）。若后续要专属 service 标签，需要额外加
   `vendor_service_contexts` 与标签类型。
4. 规则按真机段裁剪；若后续播放路径触发新的 avc（例如 HAL 读
   `/apex/.../etc` 下 `vendor_configs_file` 或访问 TEE），按 avc 迭代补充。
5. 设备 Python 流水线（`just device build`）本次只做了规则生成校验，未重跑
   发布 `out/device/...` 产物——实例 5 引导的是引擎流水线 dist 产物；该路线
   的 `patch.img` 需下次 `just device build` 时自然带上（策略文本已就位）。
