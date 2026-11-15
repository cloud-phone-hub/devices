# #193 `05_build_vendor_boot` 过滤官方 `super` 不存在的裸逻辑分区与悬空 `bind` 行（`merge-v10`）

- **设备 / 范围**：`common`（引擎 `05_build_vendor_boot`，首发复现于 `realme/RMX6688` `15-AP3A.240617.008-20250825`，通用于所有走 payload/OTA 分区拼装 `super.img` 的 ColorOS / Realme UI / OxygenOS 等多区域共板机型）
- **日期**：2026-11-11
- **状态**：**已闭环**

## 一、问题

在制作 `realme GT7`（`realme/RMX6688`，ColorOS 15.0.2 / Realme UI 6.0，MT6991）云机镜像时，全流程 `01 → 11` 构建零报错通过，但实例冷启动进入 `first_stage_init` 后在内核 `1.15s` 处立即触发 `InitFatalReboot: signal 6` 无限重启循环：

```text
[    1.150598][    T1] init: DM_DEV_STATUS failed for my_company_a: No such device or address
[    1.151479][    T1] init: Failed to mount /mnt/vendor/my_company: No such device or address
[    1.160358][    T1] init: Failed to mount required partitions early ...
[    1.179472][    T1] init: InitFatalReboot: signal 6
```

## 二、根因

1. **厂商通用板级 `fstab.<board>` 与区域 OTA `payload.bin` 逻辑分区集不一致**：
   OPPO / 一加 / realme 在 `vendor_boot.img` 的 `first_stage_ramdisk/fstab.mt6991` 中维护了一份跨区域共用的全量 `my_*` 逻辑分区表（共 10 个 `my_*` 分区，包含运营商/政企定制专用的 `my_company` 与海外预装专用的 `my_preload`），且这些挂载行均带有 `wait,slotselect,logical,first_stage_mount`、**未标记 `nofail`**。而在国行零售版 OTA `payload.bin`（解包落盘为 `super/.super-layout.json` 及 `super/*.img`）中，实际只携带了 16 个逻辑分区（8 个标准 AOSP 分区 + 8 个 `my_*` 分区），根本不存在 `my_company_a` 与 `my_preload_a`。
2. **真机与云机 `super` 元数据差异**：
   物理真机出厂刷写过空尺寸或占位 `super_empty` 元数据槽位，或由 bootloader 动态修订；而云机 `10_pack_super` 按 `super/.super-layout.json`（及 `buildSuper.images`）精确调用 `lpmake` 合成 `super.img`，LP 元数据表中自然没有 `my_company_a` 与 `my_preload_a`。
3. **旧版 `merge-v9` 只过滤物理 `by-name` 分区，未过滤缺失逻辑分区与悬空 `bind` 行**：
   `05_build_vendor_boot/fstab.go` 的 `MergeCloudFstab`（`merge-v9`）只按 lineage 底包 GPT 分区集过滤 `!record.logical` 的 `/dev/block/.../by-name/<part>` 行，对 `record.logical == true` 的厂商行一律无条件保留。结果 `my_company` 与 `my_preload` 留在了 `first_stage_ramdisk/system/etc/fstab.mt6991` 中，`first_stage_init` 在 `DoFirstStageMount` 查询 `DM_DEV_STATUS` 拿到 `ENXIO` 后因缺 `nofail` 直接判定关键分区挂载失败而 `InitFatalReboot`。此外，厂商 `fstab` 末尾还有对应的二阶段 `bind` 挂载行（`/mnt/vendor/my_company /my_company none ro,bind wait,nofail`），若源挂载点已被剔除而 `bind` 行仍带 `wait` 保留，二阶段 `mount_all` 还会无意义阻塞等待源路径超时。

## 三、证据

- 修复前 `merge-v9` 构建输出、`.super-layout.json` 16 分区清单、残留 `my_company`/`my_preload` 的 `fstab.mt6991` 与 `1.15s` `InitFatalReboot` 串口日志见 [log-fail.txt](log-fail.txt)。
- 修复后 `merge-v10` 单测、构建输出、净化后的 `fstab.mt6991` 及干净盘冷启动 `dm-0..dm-15` 一次挂齐直入二阶段的串口日志见 [log-pass.txt](log-pass.txt)。

## 四、改动

1. **`engine/build/stages/05_build_vendor_boot/fstab.go`**：
   - 将缓存合并修订号从 `merge-v9` 升级为 `merge-v10`（自动使旧缓存失效重算）；
   - 新增 `logicalBaseName` 与 `logicalPartitionsFromVendorDir(vendorDir)`：当厂商解包目录为整包 `super.img` 直通模式或无 `super/` 子目录（如空单测夹具）时返回 `nil` 保持原行为；当存在 `super/.super-layout.json` 或 `super/*.img` 时，提取实际存在的逻辑分区基名集合（自动剥离 `.img` 与 `_a`/`_b` 槽位后缀）；
   - 新增 `MergeCloudFstabWithLogical`（原 `MergeCloudFstab` 作为 `logical=nil` 兼容包装）：对 `record.logical && !strings.Contains(record.source, "/")` 的裸逻辑分区行，若不在 `logical` 集合中则计入 `Dropped` / `DroppedPartitions` 并记录其挂载点至 `droppedMounts`；随后对二阶段 `bind` 挂载行（`hasFstabOption(record.fields, "bind")`），若 `record.source` 命中 `droppedMounts` 且未被其他有效挂载行覆盖（`!activeMounts[record.source]`），同步予以剔除；
   - 在 `buildCloudFstab` 日志与 `.fstab.<board>.src` 来源记录中打印 `super 逻辑分区集: N 个（...）`，使分区裁剪决策完全透明可审计。
2. **`engine/build/stages/05_build_vendor_boot/vendor_boot_test.go`**：
   - 新增 `TestMergeCloudFstabMissingLogicalPartitions`，覆盖缺失逻辑分区 `my_company`/`my_preload` 剔除、对应悬空 `/mnt/vendor/my_*` `bind` 行连带剔除、已存在逻辑分区 `my_product`/`my_stock` 及其 `bind` 行保留，以及带路径的逻辑挂载（如 `/mnt/vendor/mi_ext`）不受误伤。

## 五、验证

1. **单元测试**：
   - `cd engine/build && go test -count=1 ./...` 全 24 个包（含新增 `TestMergeCloudFstabMissingLogicalPartitions`）全部 `PASS`。
2. **构建与真机镜像冷启动验证**：
   - `RMX6688` 执行 `05_build_vendor_boot` 输出 `算法 fstab-rev: merge-v10`，准确识别 `super 逻辑分区集: 16 个`，将 `my_company, my_preload` 列入剔除集并同步移除 2 条悬空 `bind` 行（保留行数 `46 -> 40`，丢弃行数 `54 -> 60`，产物配方 `vboot-829750f081f3`）；
   - 全新干净盘实例 `1` 冷启动在 `0.90s ~ 1.18s` 依次创建 `dm-0..dm-15` 共 16 个逻辑块设备，`1.23s ~ 1.77s` 全部 `erofs` 分区与 `/mnt/vendorpatch` 零报错挂载成功，`8.52s` 二阶段 `my_*` `bind` 挂载瞬时通过无 `wait` 卡顿，彻底根治 `InitFatalReboot`（后续完整系统验收见 [../../rmx6688/issue-rmx6688-android15-bringup/solution.md](../../rmx6688/issue-rmx6688-android15-bringup/solution.md)）。

## 六、遗留

- 无。整包 `super.img` 原样直通机型（如三星/部分小米包）因未解出 `super/.super-layout.json` 而天然跳过裁剪（返回 `nil`）；未来若某整包 `super.img` 机型同样存在 `fstab` 多写不存在逻辑分区的情况，可在 `01_unpack` 读取 `lpmetadata` 时一并导出 `.super-layout.json` 复用同一套过滤逻辑。
