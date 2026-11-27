# realme GT5 240W（`realme/RMX3823`）证据索引

- **平台与系统**：骁龙 8 Gen 2（`SM8550-AB` / `kalama`），GRF `vendor=Android 13, system=Android 15`（realme UI 6.0 / `15-TP1A.220905.001-20250912`），`lineage-20.0`，kernel `5.15.167`
- **产物路径**：`out/devices/dist/realme/RMX3823/15-TP1A.220905.001-20250912/`（rEFInd EFI 底盘，启动载荷位于 `vda.img` EFI 分区 `/kernel` + `/combined-ramdisk.img` + `/EFI/BOOT/refind.conf`）
- **机型配置路径**：`devices/manifest/devices/realme/RMX3823.json`、`devices/manifest/configs/realme/RMX3823/RMX3823-android13/`
- **跨机型通用约定**：见 [../README.md](../README.md)

## 条目

| # | 问题标识 | 说明 | 状态 |
|---|---|---|---|
| #357 | [issue-rmx3823-kalama-qcom-cellular-nfc-bt-camera-aps-audit](./issue-rmx3823-kalama-qcom-cellular-nfc-bt-camera-aps-audit/) | realme GT5 240W 真机规格/4600mAh 电池/6GiB ZRAM/去 kernelsu/高通 GRF 纯蜂窝/NXP NFC/蓝牙与 `OplusCamera` 拍录全链路打通 | 已验证闭环 |
