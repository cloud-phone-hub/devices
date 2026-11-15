# #227：Vendor 深度占位、SELinux 最小授权与 `init.svc.*` 安全对齐（`gold`）

## 问题

1. 截至 `#225` 闭环后，`gold`（`15-OS3.0.11.0.VNQCNXM`）`aligner` 的 `services` 域仍存在 `59` 条 `capability` 差异（`service list` 真机 `471` 对云机 `419`）：其中大量 MediaTek 与小米厂商定制 AIDL HAL 服务在云机中缺失。
2. `build_props` 域中 `init.svc.*` 仍有若干系统服务与触发器（`vndservicemanager`、`connsyslogger`、`aee_aed`、`aee_aed64`、`aee-reinit`、`update_verifier`）与真机运行/停止态不一致；同时另有 10 个涉及 SELinux `neverallow` / `/system` 语义的高风险服务与 4 个 32 位服务状态需明确隔离边界，不能盲目伪造。

## 根因

1. **三类 HAL 缺少配置驱动占位注册**：此前仅 `android.hardware.radio` 实现了按文件批量注册占位 Binder 的机制（`#225`）；`neuralnetworks`、`gnss`、`soundtrigger3` 三个常驻厂商/音频/定位 HAL 进程尚不支持从 `/vendor/etc/` 配置文件按行读取额外服务名注册 `AIBinder` 空桩，导致 `mtk` 与 `xiaomi` 多达 44 项 vendor 专属服务无法就位。
2. **SELinux 服务注册最小权限缺口**：在 `#226` 精确同步 `vendor_service_contexts` 之后，`hal_neuralnetworks_default`（承载 NN + 42 项厂商占位）在 `Enforcing` 模式下向 `servicemanager` 注册这批 vendor `_service` 类型仍缺少 `(service_manager (add find))` 授权。
3. **`init.svc.*` 缺席**：`vndservicemanager` 与 `system_ext`/`system` 下的 `connsyslogger`、`aee_aed`、`aee-reinit`、`aee_aed64`、`update_verifier` 要么未定义安全驻留/一次性桩，要么原厂 `.rc` 未被 `modules.redirect` 接管，导致对应 `init.svc.*` 属性为空。

## 证据

- 修复前快照（见 [`log-fail.txt`](log-fail.txt) 与 `batch4-pre-runtime.txt`）：
  - `gold` 旧实例 `service list` 数量为 `420`（`aligner` 采集 `419`），`services` 域 `capability/fatal = 59`、`raw = 497`。
  - `init.svc.{vndservicemanager,connsyslogger,aee_aed,aee_aed64,aee-reinit,update_verifier}` 在运行态均为空值。
- 修复后实测（见 [`log-pass.txt`](log-pass.txt) 与 `batch4-post-runtime.txt`）：
  - 全新冷启动实例 `9f0cf68f69184fc9-cloud`（`127.0.0.1:5911`）实测 `getenforce=Enforcing`、`sys.boot_completed=1`、`service_list_count=464`（`aligner` 采集 `463`，真机 `471`），`44/44` 占位服务全部 `OK`，`6` 项安全 `init.svc.*` 全部就位，且未触发任何崩溃过滤记录。
  - `services` 域 `capability/fatal` 由 `59` 降至 `14`（`-45`）、`raw` 由 `497` 降至 `311`（`-186`）。

## 改动

1. **三个 HAL 接入配置文件驱动 Binder 占位注册**：
   - `engine/vendor/hardware/android.hardware.neuralnetworks/src/neuralnetworks_service.cpp`：新增 `OnPlaceholderCreate` / `OnPlaceholderDestroy` / `OnPlaceholderTransact`（L132–143）、`RegisterPlaceholdersFromFile`（L155–221），在 `main`（L227–246）中 `AServiceManager_addService` 成功检查（`EX_NONE`）后读取 `/vendor/etc/cdh_nn_placeholders.txt`。
   - `engine/vendor/hardware/android.hardware.gnss/src/gnss_service.cpp`：新增 `RegisterPlaceholdersFromFile`（L1059–1125），并在 `main`（L1127–1138）中读取 `/vendor/etc/cdh_gnss_placeholders.txt`。
   - `engine/vendor/hardware/android.hardware.soundtrigger3/src/main.cpp`：新增 `RegisterPlaceholdersFromFile`（L51–110），并在 `main`（L114–131）中读取 `/vendor/etc/cdh_audio_placeholders.txt`。
2. **`gold` 机型清单装配（`/Volumes/work/cloud-phone-hub/devices/manifest/configs/redmi/gold/gold-android15.json`）**：
   - **44 项占位清单**（`buildVendor.fileTrees`，L645–647）：
     - `etc/cdh_nn_placeholders.txt`（42 项：`IApuwareUtils/default`、24 项 `ILbs/*`、`IDms/default`、`IMtkAudio/default`、`IBGService/default`、`IISPModule/internal/1`、`IMMAgent/default`、`IMtkPowerService/default`、`IMtkpower_applist/default`、`INetdagents/default`、`INvram/default`、`INwkOptNew/default`、`IVideoTelephony/default`、`IMiCharge/default`、`IMiKeyBagService/default`、`IMiSharedAuth/default`、`IMiCertificate/default`、`IMrm/default`、`IMrmRemoteAuth/default`）；
     - `etc/cdh_gnss_placeholders.txt`（1 项：`vendor.mediatek.hardware.gnss.IMtkGnss/default`）；
     - `etc/cdh_audio_placeholders.txt`（1 项：`android.hardware.bluetooth.audio.IBluetoothAudioProviderFactory/default`）。
   - **最小 SELinux 授权**（`vendor_sepolicy.cil`，L925）：为 `hal_neuralnetworks_default` 追加 `15` 条 `(allow hal_neuralnetworks_default <type> (service_manager (add find)))` 规则，涵盖 `hal_aidl_dms_service`、`mtk_hal_{audio,bgs,isphal,lbs,mmagent,power,applist,netdagent,nvramagent,nwk_opt,videotelephony}_service` 与 `hal_{micharge,mikeybag,mrm}_service`，**不向 `appdomain` 开放 `find`**，确保不触发 `neverallow` 且普通 App 的查询边界与真机一致。
   - **6 个安全 `init.svc.*` 对齐**：
     - `etc/init/cph_gold_vendor_svc_align.rc`（L673）末尾追加 `vndservicemanager`（`/system/bin/sleep 2147483647`，`class core`，`user system`，`group system`，`seclabel u:r:pioneer:s0`）→ `running`；
     - `modules.redirect`（L345–364）+ `fileTrees`（L648–651）引入 `etc/cdh_init_override/{consyslogger,aee_aed,aee_aed64,update_verifier}.rc`，分别对齐 `connsyslogger`、`aee_aed`、`aee_aed64`（`running`）与 `aee-reinit`、`update_verifier`（`stopped`）。
3. **显式排除与保留边界（本条刻意不改项）**：
   - **刻意保留云机多出 `2` 项服务**：`android.hardware.media.c2.IComponentStore/default1`（MediaCodec/C2 媒体链路必需）、`vendor.xiaomi.hardware.mlipay.IMlipayService/default`（IFAA TEE 对齐必需）。
   - **刻意不伪造 `10` 项真机服务**：`android.hardware.gatekeeper.IGatekeeper/default`、`gsiservice`、`miui.powerkeeper.PowerMillet`、`mtkIms`、`mwis`、`vendor.mediatek.hardware.camera.atms.IATMs/default`、`vendor.mediatek.hardware.composer_ext.IComposerExt/default`、`vendor.mediatek.hardware.nps.nos.fastswitch.INativeFastSwitch/default`、`vendor.mediatek.hardware.pq_aidl.IPictureQuality_AIDL/default`、`vow_bridge`（涉及 SELinux `neverallow` 或 `/system` 核心进程真实行为）。
   - **刻意不伪造 `4` 项 32 位 `init.svc.*`**：`boringssl_self_test32`、`boringssl_self_test32_vendor`、`boringssl_self_test_apex32`、`zygote_secondary`（配套刻意纯 64 位策略保持空值）。

## 验证

1. **构建与单测**：`engine/build` 全 22 个 Go 测试包 `go test -count=1 ./...` 通过；重新构建 `gold`（`15-OS3.0.11.0`）产出 `patch.img`（sha256 `1b8fdab3ff6437e4db8d7485a9f0fd921cd9f55fedeec63ba5bde5686c3`）。
2. **全新实例运行时验收**（`gold_15-OS3.0.11.0.VNQCNXM/2`，`9f0cf68f69184fc9-cloud`，`127.0.0.1:5911`）：
   - `getenforce=Enforcing`，`sys.boot_completed=1`，`service list` 计 `464` 行服务条目（`44/44` 占位存在、`0` 个遗漏、无禁用项、无保留项丢失）。
   - `init.svc.{vndservicemanager,connsyslogger,aee_aed,aee_aed64}=running`，`init.svc.{aee-reinit,update_verifier}=stopped`，4 项 32 位 `init.svc.*` 均为空。
3. **19 域全量重采与差异收口**（见 [gold-aligner-20261114](../../../../report/xiaomi/gold-aligner-20261114/README.md)）：
   - `services` 域：`capability/fatal` `59 -> 14 (-45)`、`raw` `497 -> 311 (-186)`、`serviceList.count` `419 -> 463`（对比真机 `471`）；配合 [`#226`](../../common/issue-sepolicy-official-vendor-service-contexts-sync/solution.md) 将 `IDevice/mtk-neuron_shim` 描述符差异一并清零。
   - `build_props` 域：上述 6 项安全 `init.svc.*` 差异清零，`raw` `483 -> 479`。
   - 19 域总计：`identity=553`、`capability=414`、`stealth=1183`、`availability=3`、`fatal=967`、`raw=7416`（剔除干净冷启动 `settings` 域蓝牙/TWS 默认状态波动后，其余 18 域 `fatal` `778 -> 727 (-51)`）。

## 遗留

- `services` 域剩余 `14` 条 `capability` 差异全部落在既定策略边界内：`count`/`names` 汇总 `2` 项 + 刻意保留 `2` 项 + 刻意排除 `10` 项；如未来需恢复 32 位或补实现某项 `/system` 服务再按需拆单跟踪。
- 全量差异收尾后注意 `cdh_init_override` 与 `modules.redirect` 的 32 条上限约束（当前 `gold-android15.json` 需避免无限扩充重定向条目）。
