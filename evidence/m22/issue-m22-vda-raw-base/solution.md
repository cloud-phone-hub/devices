# 11_pack_vda 只认 qcow2 底盘，lineage-23.2 底包改放 raw 后必挂

- 设备：flyme/m22（MEIZU 22，Flyme 12.5.0.0A / Android 16 / QTI `sun`）
- 组件：`engine/build/11_pack_vda`（`plan.go` / `vda.go`）
- 产物：`engine/out/dist/flyme/m22/12.5.0.0A/`（`vda.img` 13.00 GiB + `data.qcow2` 110.27 GiB 虚拟 + `patch.img` + `device.json`）

## 问题

11 的第一道工序是取 lineage UTM 底包里的 vda 当「底盘」（GPT/EFI 不动，只按分区名
原位替换 super/boot/vendor_boot）。换成新底包 `UTM-VM-lineage-23.2-20260922-UNOFFICIAL-virtio_arm64only.zip`
后，步骤 2 直接失败：

```
==> [步骤 2] 准备 vda 底盘
    输入: roms/lineage/UTM-VM-lineage-23.2-20260922-UNOFFICIAL-virtio_arm64only.zip 的 LineageOS_on_arm64.utm/Data/vda.qcow2
[err] 阶段 pack_vda 失败: 底包 … 里没有 LineageOS_on_arm64.utm/Data/vda.qcow2（成员: LineageOS_on_arm64.utm/Data/efi_vars.fd, LineageOS_on_arm64.utm/Data/vda.img, LineageOS_on_arm64.utm/Data/vdb.qcow2, LineageOS_on_arm64.utm/config.plist）
```

底包里的成员是 `vda.img`（不是 `vda.qcow2`）。

## 根因

旧实现把「底盘来源」写死成单一布局：

1. `defaultBaseMember = "LineageOS_on_arm64.utm/Data/vda.qcow2"`，`Plan.BaseMember`
   缺省就填这一个值，没有第二候选；
2. zip 分支解出后**无条件**调 `qemu-img convert`；
3. `--base` 直给路径时才按魔数分流。

lineage UTM 底包在 23.2 这一版改了布局：UTM 的 `Data/vda.img` 直接是 raw
（13.00 GiB，首 8 字节全零 = 没有 qcow2 头，是原始盘镜像），`vdb.qcow2` 才是
qcow2（`51 46 49 fb` 魔数）。于是「成员名找不到」先挂；即使把成员名改成 `vda.img`，
无条件 `qemu-img convert` 也会把 raw 当输入格式猜（qemu-img 能猜对 raw，但 13 GiB
非稀疏写盘在移动硬盘上要白写十几 GB，且对 raw 的 `-S 4k` 语义无效）。

## 证据

`log-fail.txt`：失败原文 + 四种底包的成员实测。

按魔数区分（`python3` 读 zip 成员前 8 字节，无需解压全量）：

```
LineageOS_on_arm64.utm/Data/vda.img   -> 0000000000000000（全零，raw）
LineageOS_on_arm64.utm/Data/vdb.qcow2 -> 514649fb00000003（qcow2）
```

产物侧 GPT 自证底盘是真的 raw 盘（不是转换残片）：

```
$ python3 -c "读 out/dist/flyme/m22/12.5.0.0A/vda.img 的 LBA1"
sig b'EFI PART'  part_lba 2  num 128  entry_size 128
EFI            0.00 ~  0.13 GiB
super          0.13 ~ 12.13 GiB
misc          12.13 ~ 12.13 GiB
persist       12.13 ~ 12.14 GiB
metadata      12.14 ~ 12.17 GiB
firmware      12.17 ~ 12.30 GiB
vendor_boot_a 12.30 ~ 12.40 GiB
vendor_boot_b 12.40 ~ 12.49 GiB
boot_a        12.49 ~ 12.57 GiB
boot_b        12.57 ~ 12.65 GiB
BIOS          12.65 ~ 12.65 GiB
```

## 改动

`plan.go`：

- 新增 `defaultBaseRawMember = "LineageOS_on_arm64.utm/Data/vda.img"`，与 qcow2 成员
  一起组成 `defaultBaseMembers`（qcow2 优先，raw 兜底）；
- 新增 `baseMembers()`（`--member` / `CDH_PACK_VDA_MEMBER` 显式给出时只试那一个）
  与 `baseMemberText()`（日志/指纹/报错共用的可读形式）；
- 删除 `buildPlan` 里「`BaseMember` 缺省填 qcow2」的逻辑，改为缺省留空 = 走候选探测；
  `--base` 帮助文案同步改为「缺省从 lineage 底包取 vda.qcow2 / vda.img」。

`vda.go`：

- 新增 `locateZipMember(archive string, members []string) (string, error)`：按候选顺序
  找第一个存在的成员（精确相等或 `/`+候选 后缀，兼容底包多包一层目录），返回**实际
  成员名**；找不到时把候选与包内前 6 个成员一起打进报错。成员名进缓存指纹
  （`identity = 底包大小+mtime | 成员名`），换成员会自然触发重建；
- zip 分支重排为：**先探测成员 → 再判缓存 → 解到 `vda.stage` → 按魔数分流**。
  `isQcow2(staged)` 为真才 `qemu-img convert`，否则 `os.Rename` 直接当 raw 用并打印
  `[ok] 成员 … 已是 raw，无需转换`；
- `extractZipMember` 增加 `sparse bool`：`io.ReadFull` 循环逐块比较，全零块用
  `Seek` 跳过、末尾 `Truncate(written)` —— 13 GiB raw 底包在 APFS 上只占实际数据量。

## 验证

`log-pass.txt`：

```
==> [步骤 0] 解析计划
    底盘:       … 的 LineageOS_on_arm64.utm/Data/vda.qcow2 / …/vda.img（zip）
==> [步骤 2] 准备 vda 底盘
    提取: LineageOS_on_arm64.utm/Data/vda.img（13.00 GiB）
    [ok] 成员 LineageOS_on_arm64.utm/Data/vda.img 已是 raw，无需转换
    [ok] 底盘就位 13.00 GiB（写出 out/dist/flyme/m22/12.5.0.0A/vda.img）
==> [步骤 3] 替换 super 分区（安卓系统盘）
    [ok] super 9.39 GiB @ 129.0 MiB（容量 12.00 GiB，余量 2.61 GiB），回读一致
```

后续步骤 4~6 全通：boot/vendor_boot 四槽告警沿用底盘、grub 固化 `_a` 槽、
`data.qcow2` 110.27 GiB 虚拟、`device.json` 425 B。单测
`engine/build/11_pack_vda/vda_base_test.go` 两个用例：
`TestLocateBaseMemberPrefersQcow2ThenRaw`（qcow2 优先、缺 qcow2 时回退 `vda.img`、
显式指定的成员不存在时报错带 `vda.qcow2`）与 `TestExtractZipMemberSparseKeepsContent`
（稀疏解出后内容与原成员逐字节一致）。

## 遗留

- 候选探测只覆盖 lineage UTM 的两种布局；若将来底包把 vda 拆成多卷
  （`vda.img.000` 之类）或改成分块 qcow2，仍要扩候选集合。
- raw 底包靠 `Seek` 跳全零块，但**本项目所在的 `/Volumes/work` 是 HFS+，不支持稀疏
  文件**：实测 `du -h vda.raw` / `du -h vda.img` 都是 13G，而同样“seek 1 GiB 后写
  1 字节”的探针在 APFS（`/tmp`）上只占 4.0K。即该优化只在 APFS/ext4 上省空间，
  在 HFS+ 上只减少用户态 write 次数，磁盘占用仍是 13 GiB（见 `log-pass.txt`）。
  当前未做“目标文件系统是否支持稀疏”的预检；若空间吃紧，应考虑直接以 qcow2 形式
  发布底盘或用 `mkfile`+`dd` 外的其它策略。
- `isQcow2` 只认 qcow2 魔数，其余一律当 raw；对 VMDK/VDI 之类的底包会误判成 raw
  直接改名。真要支持需引入 `qemu-img info` 探测。
- 缺 boot/vendor_boot（02~05 内核链路未通）时，步骤 4 只是告警却仍算成功——底盘沿用
  lineage 原版内核，Flyme 侧 HAL 与内核 API 是否匹配尚未验证，属已知欠账。
