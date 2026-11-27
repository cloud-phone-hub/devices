# pje110：OnePlus Ace 3（`oneplus/PJE110` / ColorOS 15.0 / 骁龙 8 Gen 2 `SM8550` / GRF `vendor=A13, system=A15`）

本分组记录 OnePlus Ace 3（`oneplus/PJE110`，`lineage-20.0` + kernel `5.15.167`）在真机规格对齐、ColorOS 15 高通平台蜂窝/NFC/蓝牙补齐以及跨代 GRF 原生 `OplusCamera` 全链路打通中的问题与实证。

## 条目

| # | 问题标识 | 说明 | 状态 |
| :--- | :--- | :--- | :--- |
| 354 | [issue-pje110-kalama-qcom-cellular-nfc-bt-camera-aps-audit](issue-pje110-kalama-qcom-cellular-nfc-bt-camera-aps-audit/solution.md) | OnePlus Ace 3（`oneplus/PJE110` / SM8550 / GRF `vendor=A13, system=A15`）全维度审计与修复：对齐真机 `1264x2780 @ 560dpi`、`12GB+256GB/16GB+512GB/16GB+1TB`、`5500mAh` 电池、`6GiB ZRAM`；修正 `board/fstab=qcom` + 补 `android.hardware.soundtrigger3` + 屏蔽 `/vendor/app`（`IWlanService.apk`）打通 ColorOS 15 纯蜂窝外网；补齐 `"host-hvc-nfc": "hvc11"` + NXP SN220 AIDL 占位与 SELinux 放行打通 NFC；修复 `09_build_vendor` `ensureCompatLibs` 漏拷 `libAlgoProcess.so`；根治 `cdh_oplus_vendor_tags.inc` 10 项最长前缀劫持与 `camera_provider_service.cpp` `StreamType::INPUT` 非零 `producerUsage` 违背规范（`-38 ENOSYS`）并全量重编 A13~A17 `camera` HAL | **已闭环**（2026-11-27，冷启终验 `patch.img sha256: 0a44022ecaad`）：`Enforcing` / `restarting=0` / 静置 96.0% 空闲；蓝牙 `ON` 0 崩溃；关 Wi-Fi 纯蜂窝 `ccmni1` `ping 223.5.5.5` 0% 丢包（0.253 ms）；NFC `mState=on` + `/api/nfc/tap` 成功（`rx=48/tx=60`）；`OplusCamera` 后摄/前摄真彩色预览、拍照 `.jpg` 原图及相册查看清晰、VIDEO 录像 `.mp4`（1.3 MB）0 崩溃、相册 1080p30 满帧 0 丢帧回放、退桌面 `CDCM_PAUSE` 冻结（729→729） |
