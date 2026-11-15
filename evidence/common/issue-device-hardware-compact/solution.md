# Issue: 各机型 `device.json` 的 `hardware` 段写法不一致（6 台统一成紧凑单行）

- 设备：跨机型（6 台：`flyme/m22`、`google/oriole`、`oppo/cph2797`、`samsung/ss9380`、
  `xiaomi/gold`、`xiaomi/popsicle`；基准是 `oneplus/15r`，它本来就是这种写法）
- 阶段：`07_build_vendor` 的 HAL 装配清单（`device.LoadOne` → `moduleBuilds`）
- 状态：**已闭环**（2026-11-03；popsicle 实跑 07 验证，其余机型 dry-run）
- 关联：[#107 issue-cdh-redirect-kernel-module](issue-cdh-redirect-kernel-module/solution.md)（同属「清单是数据、要能被工具读」这条线）

## 一、问题

`device.json` 里 `"hardware"` 是「这台机要编译/装配哪些 HAL 模块」的声明，13 份清单里
7 份有它（`nothing/3a`、`oneplus/ace3`、`oppo/pme110`、`realme/rmx3823/rmx5112/rmx6688`
六台没有该段），但**写法两种**：

```jsonc
// oneplus/15r：一条模块一行，模块对象与外层内联，{ "variant" 按段内最长键对齐
"hardware": [
    { "android.hardware.audio":          { "variant": "default", "version": "1.0" } },
    ...
]

// 其余 6 台：一条模块 5 行，键、variant、version 各占一行
"hardware": [
    {
        "android.hardware.audio": {
            "variant": "default",
            "version": "1.0"
        }
    },
    ...
]
```

后果是同一个数组，条目多的机型光这一节就 200 多行：`m22` 36 条占 219 行、`cph2797`
29 条占 177 行、`ss9380` 31 条占 189 行。横向比对各机 HAL 声明时得算行号，改一条要
翻半屏。另有两处缩进笔误：`m22` 的 `"hardware-platform"` 缩进 3 空格、`cph2797` 的
5 空格（同文件其余顶层键都是 4）。

## 二、根因

没有「同一份清单只许一种写法」的约定，也没有工具把关：`device.json` 由人工手写 +
`cdh device sync` 迁移，早期为了可读性展开成 5 行块，后来 15r 起改成紧凑单行，两批
文件就各自保留了自己的形态。JSON 解析对空白不敏感，`07` 读出来的 `HardwareSpec`
完全一样，所以这个问题一直不报错、只在人读的时候代价。

## 三、证据

- 条目数与段内行数（改前）：m22 36 条/219 行、oriole 17 条/104 行、cph2797 29 条/177 行、
  ss9380 31 条/189 行、gold 17 条/105 行、popsicle 18 条/111 行、15r 20 条/23 行
  （行数只算 `"hardware": [` 到 `]` 这一节，非整文件）。
- 结构一致性：7 份清单里每个数组元素都恰好只有一个键，内层只有 `variant`/`version`
  两个字段（脚本逐一断言），所以「压成一行」不会丢字段。
- 改前改后语义等价：13 份 `device.json` 与备份 `json.load` 结果 **deep-equal 全 True**
  （见 `log-pass.txt` 第 2 节）。
- 基准可复刻：脚本对 15r 的产出与原文**逐字节相同**，这等于用基准文件反过来校验
  渲染规则（`log-fail.txt` 第 3 节）。

## 四、改动

1. **6 份 `device.json` 的 `hardware` 段压成紧凑单行**：一条模块一行
   `{ "<模块>":<对齐空格>{ "variant": "…", "version": "…" } }`，`{ "variant"` 按**本段
   最长键**对齐（与 15r 的相对规则一致，不跨文件强求同一列——m22/ss9380 有 40 字符的
   `vendor.qti.hardware.servicetrackeraidl`）。
2. **两处缩进笔误归位**：`m22`、`cph2797` 的 `"hardware-platform"` 拉回 4 空格。
3. 15r 不动（脚本内断言「基准必须逐字节不变」）；其余 6 台无 `hardware` 段，跳过。

改动是**纯格式**：`modules.redirect`、`vendor.files`、属性表等其它字段一个字节未动
（替换区间只覆盖 `hardware` 的 `[` … `]`，段前只允许 `"hardware-platform"` 行缩进变化）。

## 五、验证

| 项 | 结果 |
| :--- | :--- |
| 13 份 JSON 语义 | 与备份 **deep-equal 全 True**（含数组顺序） |
| 15r 基准 | 脚本产出与原文**逐字节相同**（渲染规则即基准规则） |
| 整文件行数 | m22 271→91（-180）、cph2797 221→76（-145）、ss9380 243→88（-155）、gold 165→80（-85）、popsicle 155→65（-90）、oriole 143→58（-85） |
| 07 实跑（popsicle） | `[ok] 已完成`：18 个模块全部按新清单装配、装配树 556 条目（文件 328 / 链接 199 / 目录 29） |
| 13 机型 dry-run | `--only 07_build_vendor` 全 exit=0 |

## 六、遗留

1. **`hardware` 列宽不跨文件统一**：列宽按本段最长键算，所以 m22/ss9380 是第 55 列、
   gold/popsicle 是第 44 列、15r 是第 45 列。要全仓同一列就得改列宽或截断长键，收益
   只是「视觉一致」，暂不做。
2. **无 `hardware` 段的 6 台**（`3a`、`ace3`、`pme110`、`rmx3823`、`rmx5112`、`rmx6688`）
   还没接入 HAL 装配；接入时按本形态写（`log-fail.txt` 第 4 节给了模板）。
3. `jq` 之类的 JSON 格式化工具会把这套对齐重排掉，所以**不要**对 `device.json` 跑
   `jq .` 回写；改动一律手写或走脚本。
