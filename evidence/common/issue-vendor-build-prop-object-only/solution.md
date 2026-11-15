# Issue: 基线 `vendor.build-prop-sets` / `build-prop-unsets` 并存数组与对象两种形态

- 设备：全仓 12 台机型（17 份基线），典型样本 `xiaomi/popsicle`
- 阶段：`07_build_vendor` 步骤 6（`props.go`）
- 状态：**已闭环**（2026-11-03，popsicle `--only 07_build_vendor` 实跑）
- 关联：[#116 issue-vendor-files-unify](../issue-vendor-files-unify/solution.md)（同一阶段另一张表的口径收口）

## 一、问题

`07_build_vendor/props.go` 读 `vendor.build-prop-sets` / `build-prop-unsets` 时「对象优先、旧数组兜底」：

```go
var object map[string]string
if err := json.Unmarshal(raw, &object); err == nil { return object, nil }
var pairs [][]string                       // ← 旧数组形态
...
for _, pair := range pairs {
    if len(pair) < 2 { continue }          // ← 写残的对静默丢弃
    sets[pair[0]] = pair[1]                // ← 重复键静默取最后一次
}
```

17 份基线里只有 5 份是对象，7 份仍是 `[[k, v], …]` 数组，5 份整个 `vendor` 段都没有。
同一字段两种写法、两种语义，没人能从清单上看出「重复键会怎样」。

## 二、根因

数组形态是旧工作区 `props.py` / `register_baselines()` 的历史格式：当年只有 gold-15 一份
基线转成了对象（并在两个 Python 消费端加了 dict/list 分支），其余机型由 `cdh device sync`
原样迁移进来，字段形态跟着旧基线一起搬了过来。`props.go` 照抄了那份「兼容读法」，
于是 `[[k, v], …]` 一直能跑，没人发现它是一个会静默吞错的入口。

## 三、证据

1. **格式分布**（`devices/manifest/*/*/baselines/*.json` 逐份实测，见 `log-fail.txt`）：
   对象 5 份、数组 7 份（popsicle 328 对 / pme110 770 对 / 3a 361+342 对 /
   oriole 186+61 对 / 15r 2 对）、无 `vendor` 段 5 份。
2. **兼容读法确实在吞数据**：popsicle 的 328 对里有 **31 个重复键**
   （`debug.c2.use_dmabufheaps` 5 次、`media.c2.hal.selection` 4 次、
   `debug.stagefright.ccodec` 4 次……），折叠后只剩 288 键。本次核对这 31 个键
   **取值全部相同**，才敢折叠；只要有一个键值不同，旧读法就会静默取最后一次而无人知晓。
3. **形状写错不报错**：`len(pair) < 2` 直接 `continue`，一条属性都不会落盘，`07` 仍然全绿。

## 四、改动

1. `07_build_vendor/props.go`：删掉数组分支，改成「必须是对象」直接报错；
   错误文案带一句迁移指引（`旧数组形态 [[key, value], …] 已停用，先改成对象`）。
   `build-prop-unsets` 同步收紧为对象 `{key: ""}`（值不参与，只为能就地写注释）。
2. `07_build_vendor/props_test.go`：`TestReadVendorBuildPropsLegacyArray`（断言数组仍可读）
   改成 `TestReadVendorBuildPropsRejectsLegacyArray`（断言 sets / unsets 两种数组都报错）。
3. **7 份基线就地迁移**（文本级替换，只重写这两个字段的值区间，其余字节不动）：
   m22 已对象未动；`oriole-bp1a` 61+38、`oriole-cp1a` 186、`3a-B4.1` 361、
   `3a-V3.2` 342、`15r` 2、`pme110` 770、`popsicle` 328→288 + unsets 1。
   同键出现不同取值时脚本直接失败，不做静默折叠。
4. 文档：`manifest/README.md` 写清字段形态与缺省语义；`07_build_vendor/README.md`
   步骤 6 与文件表补「只认对象」。
5. 一次性脚本与备份：`engine/out/tmp/migrate_prop_sets.py`、
   `engine/out/tmp/baselines-backup-20261103-133454/`（17 份迁移前原件）。

## 五、验证

- **语义等价**：对 17 份逐一比对新旧解析结果（按旧读法折叠数组 vs 新对象），
  sets / unsets 全等、非 `vendor` 段全等 → `全部语义等价: True`（`log-pass.txt` 第 1 段）。
- **代码**：`go build ./...`、`go vet ./07_build_vendor`、
  `go test ./07_build_vendor`（38.2 s 全绿）；`TestReadVendorBuildPropsRejectsLegacyArray` PASS。
- **实跑**：`cdh device build popsicle --only 07_build_vendor` →
  `vendor 属性: 基线 build-prop-sets 288 项 / unsets 1 项 -> vendor/build.prop`，`[ok] 已完成`。
- **产物**：`engine/out/devices/xiaomi/popsicle/OS3.0.319.0.WPBCNXM/vendor/build.prop`
  共 369 行 / 318 键：288 项 sets **逐键落对 0 个不符**、产物内**无重复键**、unsets 的
  `ro.vendor.audio.soundfx.type` 已删除。

## 六、遗留

1. **未重跑 09 → 11**：本次只重装配 `vendor.img`，`patch.img` / 盘集仍是上一轮产物。
   属性集合与迁移前逐键相同（有等价校验兜底），下次常规重跑即同步。
2. **5 份基线保持「无 `vendor` 段」**：`oriole-cp3a` / `ace3` / `rmx3823` / `rmx5112` /
   `rmx6688` 是 `cdh device sync` 按固件元数据生成的骨架，整个 `vendor` 段都不存在，
   缺省语义就是「不改任何 vendor 属性」。没有补空的 `"vendor": {}`——补了只会增加
   与 sync 生成结果的 diff，不改变行为。
3. `vendor-recipe.txt` 仍只哈希基底镜像 + HAL 产物根，**不含属性**：改 `build-prop-sets`
   后配方不变、`vendor.img` 字节会变（与 oriole #103 遗留 2 同一条裂缝，07 每次全量重装配兜底）。
4. `popsicle` 那 31 个重复键是「厂商 build.prop 里同键多次覆盖」的痕迹，本次按旧语义
   取最后一次；若将来要还原「为什么覆盖」，得回到官方 `vendor.img:/build.prop` 现场取证。
