# pd2454：vivo X200 Ultra（`vivo/PD2454` / 骁龙 SM8750 `sun` / Android 15）

1 条机型条目：#262 记录 `vivo/PD2454`（`15-AP3A.240905.015.A2-20250416`，`lineage-22.2` + `kernel 6.6`）一阶段 `VMountVerifyPartitions` 依赖真实 `vbmeta_a` 块设备与 `vdc` `avb=vbmeta`，以及二阶段 OriginOS 5 `libsensorservice.so` 私有 `EventPayload::Tag::data`、Codec2 `media_config.xml` 与 `tango`/`netboosterd` 屏蔽闭环。

## 条目

| # | 问题标识 | 说明 | 状态 |
| :--- | :--- | :--- | :--- |
| 262 | [issue-pd2454-vbmeta-gpt-and-sensors-data-tag](issue-pd2454-vbmeta-gpt-and-sensors-data-tag/solution.md) | ① 一阶段 `VMountVerifyPartitions` 查无 `/dev/block/by-name/vbmeta_a` 重启至 bootloader，改为 `06_build_vendor_boot` `merge-v14` 在 `vdc` 加 `,avb=vbmeta` + `13_pack_vda` 追加 `vbmeta_a` GPT 分区写入 `DisabledVBMetaImage()`；② 反汇编官方 `libsensorservice.so` 确认 `type 3/5/6/7/8/36` 强制走 `EventPayload::Tag::data`（`0xa`），修复 `02_prepare` 为 `vivo`/`iqoo` 生成 `payload="data"` 并过滤 `head_tracker`、规范化 `media_config.xml` 为 `default1`/`default9`；③ 补齐 `PD2454` 64 位 `abilist` 并通过 `cph_empty.rc` 重定向屏蔽 `tango-binfmt.rc` 与 `netboosterd.rc` | **已闭环**（2026-11-19，实例 1）：`verify_boot.py` `passed=true`（`bootCompleted=true`、`postBootCompleted="1"`、`surfaceFlingerState="running"`、`serviceCount="387"`），1440×3168 清晰渲染简体中文 OriginOS 向导页（`meanBrightness=225.25`） |
