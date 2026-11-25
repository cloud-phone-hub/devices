# 独立 ODM 分区机型 NFC 漏识别、配置回填与 `/dev/nq-nci` 探针闭环（首发 `xiaomi/popsicle`）

- **编号**：`#331`
- **日期**：2026-11-25
- **设备 / 机型**：跨机型引擎与 HAL 收敛（首发验证：`xiaomi/popsicle` Xiaomi 17 Pro Max #1，`16-OS3.0.319.0.WPBCNXM`，Android 16 + `lineage-23.2` + `kernel 6.12.81`）

## 1. 问题

`Xiaomi 17 Pro Max #1`（`popsicle_16-OS3.0.319.0.WPBCNXM`）宿主 QEMU 已挂载虚拟 NFC 串口通道（`-chardev socket,id=stage-nfc,... -device virtconsole,chardev=stage-nfc,id=stage-hvc11-nfc`，客机 `/dev/hvc11` 在线），官方 `/system/app/XMNfcNci/XMNfcNci.apk`（`com.android.nfc`，NXP SN220/SN300 标准 AIDL NCI 栈）也完整存在，但云机内完全不支持 NFC：

1. `service check android.hardware.nfc.INfc/default` 与 `service check nfc` 均返回 `not found`；
2. `pm list features | grep -i nfc` 为空；
3. 原有实例因首启缺少 `android.hardware.nfc.any` 被 `NfcBootCompletedReceiver` 设为 `enabled=2`（`COMPONENT_ENABLED_STATE_DISABLED`）；即便手工启用并补齐 HAL 后，`NfcApplication.onCreate()` 仍提前返回不创建 `NfcService`。

## 2. 根因

由构建期与运行期四处串联缺口共同导致：

1. **`02_prepare` 漏探测未解包的独立 `odm_a.img` 权限**：小米将 `popsicle` 的 6 份 NFC 权限声明（`android.hardware.nfc{,.hce,.hcef,.uicc,.ese}.xml`、`com.android.nfc_extras.xml`）全部放在独立分区 `odm_a.img:/etc/permissions/`，而 `vendor_a.img:/etc/permissions/` 未放任何 NFC XML。`02_prepare` 执行时 `unpack_odm` 尚未解包，`enrichPermissionsFromSystemPartitions()` 仅探测了 `system` / `product` / `system_ext`，漏掉了 `odm_a.img`（及 `mi_ext_a.img`），导致 `device-profile.json` 误记 `hasNfc: false`，在 `apply_profile.go` 中覆盖了 AI 规格的 `enableNfc: true`，未将 `android.hardware.nfc` 写入 `hardware/config.json`。
2. **`09_build_vendor` `backfillOfficialNfcConfigs` 因 `vendor/etc` 含芯片模板而跳过独立 `odm/etc` 主配置**：`popsicle` 在 `vendor_a.img:/etc/` 下放有 11 份芯片模板（`libnfc-SN220_*.conf` / `libnfc-SN300_*.conf`），但真正的主配置 `libnfc-nci.conf`、`libnfc-nxp.conf`、`libnfc-nxp_RF.conf` 仅存于独立 `odm_a.img:/etc/`。云机运行态 `/odm` 重定向至精简载体后，若只扫 `vendor` 或在 `copiedVendor > 0` 时提前终止，`vendor/etc/libnfc-nci.conf` 会缺失。
3. **`09_build_vendor` `ensureEnabledModulePermissions` 缺少独立 ODM NFC 权限回填与兜底合成**：`apply-vendor-files` 用官方 `vendor/etc/permissions` 替换后，对权限声明在独立 `odm` 的机型未从官方 `unpack_odm` / `odm_a.img` 回填 `*nfc*.xml` 或合成标准 AOSP `nfc{,.hce,.hcef}.xml`。
4. **小米澎湃 OS `XMNfcNci` 硬检 `/dev/nq-nci` 节点**：反汇编 `XMNfcNci.apk` 的 `NfcApplication.onCreate()`（`00a4..00af`）确认其硬编码执行 `new File("/dev/nq-nci").exists()`，节点缺失（或 SELinux 无权 `stat`）时直接 `return-void` 跳过 `new NfcService(...)`；此前 `cdh_device_info` 仅注册了荣耀所需的 `/dev/pn544`（见 [../issue-honor-chora-camera-vendor-tags-and-nfc-pn544](../issue-honor-chora-camera-vendor-tags-and-nfc-pn544/solution.md)），未覆盖高通/小米系的 `/dev/nq-nci`。

## 3. 证据

- 失败日志与字节码取证：[log-fail.txt](log-fail.txt)
  - 修复前 `Service nfc: not found`、`Service android.hardware.nfc.INfc/default: not found`、`pm list features | grep -i nfc` 为空；
  - `dexdump` 定位 `NfcBootCompletedReceiver.onReceive` 在缺少 `android.hardware.nfc.any` 时调用 `setApplicationEnabledSetting("com.android.nfc", 2, 0)`，以及 `NfcApplication.onCreate` 在 `00a6: const-string v2, "/dev/nq-nci"` → `00af: if-eqz v1, 00d1` 提前退出。
- 通过日志：[log-pass.txt](log-pass.txt)
  - `02_prepare` 与 `09_build_vendor` 新增单测及包级回归测试全绿；
  - `Xiaomi 17 Pro Max #1` 42 秒零人工干预干净冷启后：`/dev/nq-nci -> /dev/null`（`u:object_r:nfc_device:s0`）就位，`Service android.hardware.nfc.INfc/default: found`、`Service nfc: found`、6 项 `android.hardware.nfc*` feature 全齐、`mState=on`、`listenTech=0xf`、`pollTech=0x2f`；
  - 调用 `/api/daemon/nfc/tap` 与 `/api/daemon/nfc/remove` 全通（`connected:true, initialized:true, discovering:true`，`rxCommands: 29 -> 90`、`txResponses: 45 -> 139`，含 `mt=0` NDEF 数据帧交互），`hal_nfc_default` 与 `nfc` 域零 SELinux 拒绝。

## 4. 改动

1. **`02_prepare` 独立 ODM 权限探测与配置扫描**（`agent/modules/pipeline/stages/02_prepare/vendor_etc_configs.go` + `standalone_odm_nfc_test.go`）：
   - `enrichPermissionsFromSystemPartitions()` 新增 `activeSKU` 参数并在 `parts` 首位加入 `odm_a.img`/`odm.img`（探测 `etc/permissions/`、`odm/etc/permissions/` 及匹配当前激活区域的 `sku_<activeSKU>/`，严格隔离国行无 NFC 与海外有 NFC 的多 SKU 机型如 `redmi/gold`）及 `mi_ext_a.img`/`mi_ext.img`；
   - `scanVendorConnectivityAndSKUs()` 同步扫描 `odmEtcDirs` 下的 NFC 配置文件并识别 `sn220`/`sn300` 为 `NXP NFC`。
2. **`09_build_vendor` 独立 ODM NFC 配置与权限回填**（`agent/modules/pipeline/stages/09_build_vendor/{files_compat.go,assemble.go,standalone_odm_nfc_test.go}`）：
   - 新增 `officialOdmRoots(plan)`；`backfillOfficialNfcConfigs()` 在扫描 `officialVendorRoots` 后始终继续扫描 `officialOdmRoots` 与同目录独立 `odm_a.img`/`odm.img` 回填核心配置（`libnfc-nci.conf`、`libnfc-nxp.conf`、`libnfc-nxp_RF.conf` 等），不再因 `vendor/etc` 存在芯片模板而漏掉 ODM 主配置；
   - 新增 `ensureEnabledNfcPermissions()`：当启用 `android.hardware.nfc` 且非 ST NFC 时，若 `vendor/etc/permissions/android.hardware.nfc.xml` 缺失，优先从官方 `unpack_odm` 或独立 `odm_a.img`/`odm.img` 回填原厂 NFC 权限 XML（自动跳过含 `<unavailable-feature` 的文件），仍缺失时合成标准 `android.hardware.nfc{,.hce,.hcef}.xml`。
3. **虚拟 NFC 模块与内核探针双层 `/dev/nq-nci` 兜底**：
   - `devices/modules/device_info/nfc/nfc_sysfs.c`：新增注册只读杂项字符设备 `/dev/nq-nci`（`0444`），与 `/dev/pn544` 并列；
   - `devices/hardware/android.hardware.nfc/vendor/etc/init/android.hardware.nfc-service.rc`：在 `on post-fs-data` 增加 `symlink /dev/null /dev/nq-nci` 与 `restorecon /dev/nq-nci`（内核已注册字符设备时 `EEXIST` 无感跳过，未重编 `vendor_dlkm` 的存量镜像亦可直接生效），同步刷新 `out/devices/vendor/android{13..17}` 的 `init` 脚本；
   - `devices/hardware/android.hardware.nfc/sepolicy/{vendor_file_contexts,vendor_sepolicy.cil}`：显式绑定 `/dev/nq-nci u:object_r:nfc_device:s0` 与 `android.hardware.nfc-service u:object_r:hal_nfc_default_exec:s0`，放行 `vendor_init` 创建符号链接及 `nfc` 域读取 `nfc_device` `lnk_file`（经核验与 `plat_sepolicy.cil` 零 `neverallow` 冲突）。
4. **`popsicle` 清单更新与运行实例热更**：
   - 更新 `devices/manifest/configs/xiaomi/popsicle/popsicle-android16/hardware/config.json`（新增 `android.hardware.nfc`）与 `devices/manifest/devices/xiaomi/popsicle/device-profile.{json,md}`（`hasNfc: true` + 6 项 NFC 特性 + 3 份 `odm/etc/libnfc-*.conf`）；
   - 将 NFC HAL、原厂 ODM `libnfc-*.conf`、6 份 NFC 权限 XML 与 SELinux 规则注入 `out/devices/dist/xiaomi/popsicle/16-OS3.0.319.0.WPBCNXM/patch.img`（前缀 `37bf65da0334`），并恢复存量实例 `#1` `userdata` 中 `com.android.nfc` 的启用状态。

## 5. 验证

1. **单元测试**：`cd agent/modules/pipeline && go test ./stages/02_prepare ./stages/09_build_vendor` 全部 PASS（含独立 `odm_a.img` ext4 提取、区域 SKU 隔离、ODM 配置+权限回填、兜底合成与 SELinux 规则校验）。
2. **`Xiaomi 17 Pro Max #1` 零干预冷启与刷卡实测**：
   - 冷启 42 秒到位后无需任何手工命令，`android.hardware.nfc-service`（`pid 642`）与 `com.android.nfc`（`pid 7902`）自动常驻运行；
   - `service check android.hardware.nfc.INfc/default` → `found`，`service check nfc` → `found`；
   - `pm list features | grep -i nfc` 完整输出 `nfc`、`nfc.any`、`nfc.ese`、`nfc.hce`、`nfc.hcef`、`nfc.uicc` 共 6 项；
   - `dumpsys nfc`：`mState=on`、`mScreenState=ON_UNLOCKED`、`listenTech=0xf`、`pollTech=0x2f`，SIM 卡钱包与小米钱包 HCE 服务正常注册；
   - `POST /api/daemon/nfc/tap` 与 `POST /api/daemon/nfc/remove` 返回 `{"ok":true,"status":{"connected":true,"initialized":true,"discovering":true,...}}`，`rxCommands` 由 29 增至 90、`txResponses` 由 45 增至 139。

## 6. 遗留

- 无功能遗留。`6.12` 内核外部模块下次随常规版本重编时将一并带入 `nfc_sysfs.c` 的 `/dev/nq-nci` 字符设备节点；在此之前 `android.hardware.nfc-service.rc` 的 `post-fs-data` 符号链接兜底已确保全量存量镜像冷启即生效。
