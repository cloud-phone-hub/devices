# Issue: `modules.redirect` 数组里混着 770 条注释串（12 机型统一挪到机型 README）

- 设备：跨机型（12 台：`flyme/m22`、`google/oriole`、`nothing/3a`、`oneplus/15r`、
  `oneplus/ace3`、`oppo/cph2797`、`realme/rmx3823`、`realme/rmx5112`、`realme/rmx6688`、
  `samsung/ss9380`、`xiaomi/gold`、`xiaomi/popsicle`）
- 阶段：`05_build_vendor_boot` 的伴随文件来源（`modules.redirect` → `cdh_redirect.rules`）
- 状态：**已闭环**（2026-11-03；popsicle 实跑 05→11 验证，其余机型 dry-run + 待下次构建自动刷新）
- 关联：[#107 issue-cdh-redirect-kernel-module](issue-cdh-redirect-kernel-module/solution.md)（规则表迁进清单的那次改动）

## 一、问题

13 份 `device.json`（`oppo/pme110` 没有 `modules` 段）里有 **12 份**的
`modules.redirect` 数组混着字符串元素：合计 **770 条**（其中 **82 条**是空串），
与 **135 条**规则对象交错排列：

| 机型 | 字符串元素 | 其中空串 | 规则 | 现状 |
| :--- | ---: | ---: | ---: | :--- |
| `flyme/m22` | 51 | 7 | 12 | README 已有，追加一节 |
| `google/oriole` | 34 | 5 | 10 | 新建 README |
| `nothing/3a` | 47 | 5 | 10 | 新建 README |
| `oneplus/15r` | **274** | 17 | 13 | 新建 README |
| `oneplus/ace3` | 39 | 5 | 10 | 新建 README |
| `oppo/cph2797` | 51 | 7 | 12 | 新建 README |
| `realme/rmx3823` | 39 | 5 | 10 | 新建 README |
| `realme/rmx5112` | 39 | 5 | 10 | 新建 README |
| `realme/rmx6688` | 39 | 5 | 10 | 新建 README |
| `samsung/ss9380` | 51 | 7 | 12 | 新建 README |
| `xiaomi/gold` | 39 | 5 | 10 | README 已有，追加一节 |
| `xiaomi/popsicle` | 67 | 9 | 16 | 新建 README |

问题有两面：

1. **JSON 是数据，却被当注释用**：`json.load` 读出来是 770 个字符串元素，任何按数组
   遍历的脚本（横向对比、规则校验、迁移工具）都得先 `isinstance(x, str)` 过滤；
   编辑器里也没法折叠——`popsicle` 的 `device.json` 221 行里只有 16 行是规则，
   `oneplus/15r` 的 redirect 段渲染出来 287 行里只有 13 行是规则。
2. **它们不是无效数据**：`05_build_vendor_boot` 会把它们**原样渲染**进 ramdisk 的
   `lib/modules/cdh_redirect.rules`（`RedirectEntry.Comment` 非 nil 即输出一行）。
   所以「直接删掉」会改变产物字节——这一点决定了这次改动不能当成纯格式化。

## 二、根因

#107 把规则表从机型目录的独立 `kernel/cdh_redirect.rules` 文件搬进 `device.json`
（为了「伴随文件由清单渲染」，不再每机型多一份文件）。搬的时候为了不丢原 `.rules`
里的注释，`internal/device` 把数组的一项设计成 `Comment *string` / `Rule *RedirectRule`
**二选一**：

```go
// RedirectEntry 是 modules.redirect 数组的一项：要么是一行注释/空行，要么是一条规则。
type RedirectEntry struct {
    Comment *string
    Rule    *RedirectRule
}
```

于是「给读规则表的人看的说明」和「给构建代码读的规则」挤在同一个 JSON 数组里。
职责错位：数组的读者是 05，说明的读者是维护机型的人。

## 三、证据

- 渲染链路：`ModuleSection.RenderRedirect()` 逐项输出，规则为
  `<type>  <from>  <to>`（两个空格），注释原样；单测
  `TestReadAssetRedirectFromDevice` 断言 `"# ---- /vendor ----"` 那行会进产物。
- 计数：12 机型 / 770 条字符串（82 空串）/ 135 条规则，逐机型见上表。
- 最长一份是 `oneplus/15r`：274 条字符串、15454 字符（含 `/odm` 整树屏蔽、NFC/KeyMint
  类死声明屏蔽的实测理由），比它的 13 条规则长两个数量级。
- 删除前后的产物差异（popsicle 实跑）：`cdh_redirect.rules` 由带注释版变为
  **1425 B**，与「16 条规则渲染文本」的字节数**逐字节相等**（见 `log-pass.txt` 第 3 节）。

## 四、改动

1. **12 份 `device.json` 的 `modules.redirect` 只留规则对象**（770 → 0 条字符串），
   规则序列、顺序、内容一个字节不改。文本级删除注释行，不经 `json.dump` 重排，
   其余字段逐字节不动。
2. **说明落到机型 README**（10 份新建、2 份追加一节 `## modules.redirect（cdh_redirect 规则表）`），
   每节三块：
   - 一句约定（数组只放规则对象，说明在本节）；
   - 规则表（`# / type / from / to`，顺序即匹配顺序）；
   - **原说明全文**（删注释前 05 渲染出的 `.rules` 原文，含分段标题与逐条理由）。
3. **公共文档改口径**：`devices/modules/cdh_redirect/README.md` 的「规则文件」与
   「迁一台新机型要做什么」两处，说明字符串元素仍被渲染器支持、但清单里已统一不用。

`RedirectEntry.Comment` 与对应单测**保留**（渲染器兼容旧清单；「说明与规则同等重要」
的诉求改由 README 承接，不是放弃）。

## 五、验证

| 项 | 结果 |
| :--- | :--- |
| JSON 合法性 + 规则一致性 | 13 份全部可解析；规则对象序列与改动前备份**逐条相等**；字符串元素 0 条 |
| 其余字段 | 除 `modules.redirect` 外整份 JSON 与备份等价（脚本内做 deep-equal 校验） |
| 13 机型 dry-run | `--dry-run --only 05_build_vendor_boot` 全部 exit=0（`oriole` / `3a` 需 `--manifest`，因它们有多份清单） |
| 单测 | `go test -count=1 ./05_build_vendor_boot` ok（含 `TestReadAssetRedirectFromDevice`，渲染兼容路径未破） |
| popsicle 实跑 05 | 配方 `vboot-5b620c132f4a` → **`vboot-677118237f0b`**（规则内容在配方哈希里，改注释会自动失效重打）；`cdh_redirect.rules` **1425 B** = 16 条规则渲染字节数 |
| popsicle 实跑 11 | `vda.img` 13.50 GiB，**5 个分区已刷新** |

## 六、遗留

1. **其余 11 个机型未重跑 05/11**：它们当前产物里的 ramdisk 规则表仍是旧版（带注释）。
   注释被 `cdh_redirect.ko` 跳过，不影响运行；配方号已把规则内容计入，下次任何构建都会
   自动重打，不需要手工干预。
2. **设备上不再自带说明**：`cat /lib/modules/cdh_redirect.rules` 现在只有 `dir/file` 行。
   要看「为什么这么改」得回到 `manifest/<vendor>/<device>/README.md`——这是本次的
   有意取舍（README 换 JSON 的可读性）。
3. **两处维护点**：README 的「规则表」与「原说明全文」都含规则行，改规则要同步。
   约定是**规则表为准**，原说明块只作历史理由保留。
4. `oppo/pme110` 没有 `modules` 段，未动；它接入 redirect 时按新口径写（数组只放对象）。
