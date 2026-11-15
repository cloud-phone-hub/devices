# m22 的 `android.se.omapi` 声明归位：从机型静态树搬到引擎级公共补件目录（07 新增步骤 2c）

> 触发提问：「`android.se.omapi.xml` 这个文件是不是应该放到 `android.hardware.secure_element`？」
> 结论：**不是**（它声明的是 system 侧 `com.android.se` 注册的另一个服务），但它也不该留在机型静态树——
> 已迁到引擎级公共补件目录 `engine/vendor/vintf/common/`，由 07 新增的**步骤 2c** 补进装配树。

## 1. 问题

`m22` 的 VINTF 片段 `manifest/flyme/m22/vendor/etc/vintf/manifest/android.se.omapi.xml` 是机型静态树里
**唯一一份** VINTF 片段，内容里没有任何机型数值。它声明：

```xml
<manifest version="1.0" type="device">
    <hal format="aidl"><name>android.se.omapi</name><version>1</version>
        <fqname>ISecureElementService/default</fqname></hal>
</manifest>
```

而 `engine/vendor/hardware/android.hardware.secure_element` 模块提供的是
`android.hardware.secure_element.ISecureElement/eSE1`（见模块 `src/secure_element_service.cpp` 与
`vendor/etc/init/android.hardware.secure_element-service.rc` 的 `interface` 行）——**不同包、不同进程**。
于是问题拆成两半：

1. 能不能放进 `android.hardware.secure_element` 模块？——不能。
2. 那它该放哪？——不能留在机型静态树（内容与机型无关，全机型共用），需要一层「引擎级补件」，
   而 07 当时没有这一层。

缺这份声明的后果（实例 27 实测）：Android 14+ 的 servicemanager 对 AIDL 命名服务做 VINTF 强校验，
`com.android.se` 的 `addService` 抛 `VINTF declaration error`，persistent 进程崩溃 → AMS 立刻重启，
每 50~70 ms fork 一次，最终 binder 事务缓冲打爆（`-28 ENOSPC`），连带 `com.android.phone` 反复被杀。

## 2. 根因

### 2.1 为什么不能进 `android.hardware.secure_element` 模块

| | 声明方 | 注册方 | 进程 |
| :--- | :--- | :--- | :--- |
| `android.hardware.secure_element.ISecureElement/eSE1` | 模块自带（`<模块>/vendor/etc/vintf/`，本模块**尚无**该片段） | `/vendor/bin/hw/android.hardware.secure_element-service` | vendor HAL |
| `android.se.omapi.ISecureElementService/default` | 本文件 | system 侧 `com.android.se`（`SecureElementService.java`） | 框架应用 |

`vendorbuild` 的模块 `vendor/**` 只在 `device.json` 声明该模块时才上树（07 步骤 4 取
`Targets + VendorTree`）。把框架应用的声明塞进 HAL 模块，等于「声明跟着 HAL 一起上树/下树」：
`device.json` 里撤掉 HAL 就丢掉框架声明，两个生命周期完全不同的东西被绑在一起。

### 2.2 为什么也不该留在机型静态树

- 内容里没有机型数值（无 board、无通道、无实例名），全机型通用；放机型层等于每台机抄一遍，换机型必漏。
- 它是「基底还原」阶段的缺口，不是「机型定制」。07 的层级职责是：基底（步骤 2/2b）→ 引擎级补件（2c）
  → 模块产物 / 机型静态树 / 平台层（4）→ 厂商覆盖（5）；把基底缺口放进第 4 层，会被后来的静态树改动掩盖。
- 结论落地成 07 的新步骤 2c：**只补缺**（基底已有同名片段则以基底为准），层级早于所有后层。

### 2.3 为什么引擎侧原本没有来源

- 步骤 2 只从 lineage 基底白名单铺底：`out/roms/lineage/lineage-23.2/super/unpack_vendor/etc/vintf/`
  全树无 `android.se.omapi` 声明。
- 厂商 vendor 里的 VINTF 片段不随装配走：m22 官方 ROM 把这份声明放在
  `/etc/vintf/manifest_<sku>.xml` 的合成清单里（`dump.erofs` 可见），引擎侧不读合成清单。
- 所以它只能靠「补件」层补，靠任何既有层都拿不到。

## 3. 证据

### 3.1 归属证据（不是 secure_element HAL）

```
$ grep -rn "addService\|ISecureElement" vendor/hardware/android.hardware.secure_element/src/secure_element_service.cpp
42:    "android.hardware.secure_element.ISecureElement/eSE1";
105:  if (AServiceManager_addService(element->asBinder().get(),
$ cat vendor/hardware/android.hardware.secure_element/vendor/etc/init/android.hardware.secure_element-service.rc
    interface aidl android.hardware.secure_element.ISecureElement/eSE1
```

### 3.2 缺口证据（引擎侧无来源）

```
$ find vendor/hardware -path "*/vendor/etc/vintf/*" -type f | wc -l
29                      # 模块自带 VINTF 片段共 29 份，secure_element 不在其中
$ grep -rln "android.hardware.secure_element" out/devices/flyme/m22/12.5.0.0A/vendor/etc/vintf/
（装配树无）
$ grep -rln "android.hardware.secure_element" out/roms/lineage/lineage-23.2/super/unpack_vendor/
（基底无）
$ dump.erofs --cat --path=/etc/vintf/manifest_<sku>.xml out/roms/flyme/m22/12.5.0.0A/super/vendor_a.img
（官方 ROM 的合成清单里有 se.omapi，但引擎侧不读 → 等同于无来源）
```

### 3.3 迁移前现状（机型静态树唯一一份）

```
$ find manifest -path "*vendor/etc/vintf*" -type f
manifest/flyme/m22/vendor/etc/vintf/manifest/android.se.omapi.xml     # 全仓仅此一份
$ find manifest/flyme/m22/vendor -type f | wc -l
26                       # 其余 25 件是 rc / cdh_*.xml / build.prop 覆盖等机型定制件
```

### 3.4 不变量（迁移后行为不能变）

服务定义、fqname、根元素 `version="1.0"` 逐字保留；07 步骤 6 的 schema 收口只降不升，1.0 原样保留。
详见同目录 `log-fail.txt`（错误认知与操作事故）与 `log-pass.txt`（实跑与回读）。

## 4. 改动

| # | 文件 | 内容 |
| :--- | :--- | :--- |
| 1 | `engine/vendor/vintf/common/android.se.omapi.xml`（新建） | 片段本体照搬；注释重写为「为什么在引擎级公共目录、为什么不是模块、改动背景与实例 27 证据、根元素为何写 1.0」 |
| 2 | `engine/build/07_build_vendor/vintfcommon.go`（新建） | **步骤 2c**：把 `vendor/vintf/common/*.xml` 补进 `<装配树>/etc/vintf/manifest/`，只补缺；目录缺失/为空只打一行跳过 |
| 3 | `engine/build/07_build_vendor/plan.go` | `Plan.VintfCommonDir`（`engine.Root()/vendor/vintf/common`）；步骤 0 摘要新增 `VINTF 补件` 行；`--dry-run` 新增 `[步骤 2c]` 计划命令 |
| 4 | `engine/build/07_build_vendor/execute.go` | 步骤 2b 之后串入 `materializeVintfCommon` |
| 5 | `engine/build/07_build_vendor/vintfcommon_test.go`（新建） | 三例：只补缺 + 旁注过滤、目录缺失只跳过、摘要三态 |
| 6 | `devices/manifest/flyme/m22/vendor/etc/vintf/`（删除） | 机型静态树那份 `android.se.omapi.xml`（连同空目录 `etc/vintf/manifest`、`etc/vintf`） |
| 7 | `engine/build/07_build_vendor/README.md` | 一句话 + 步骤总览新增 2c 行 + `### 步骤 2c` 整节（归属/层级/打印示例/与 #53 3.6 的边界）+ 代码地图 |
| 8 | `engine/vendor/build/README.md` | 切分口径表新增一行：**system 侧**服务的 VINTF 声明 → `vendor/vintf/common/`（07 步骤 2c） |

**刻意没做的事**：没有给 `android.hardware.secure_element` 模块补 VINTF 片段。`issue-m22-hardware-list-align`
3.6 节已记录「声明一个不在 FCM 里的 vendor 包会把 `getService` 拖成 `waitForDeclaredService` 阻塞」，
而实例 36 已验证 `ISecureElement/eSE1` 不声明也能注册成功（`service list` 可见）。本目录只放
「system 侧确实会注册」的声明，这条边界写进了 07 README。

## 5. 验证

**质量门**（`engine/build`）：

```
$ gofmt -l -w 07_build_vendor/        # 无输出
$ go vet ./07_build_vendor/           # 通过
$ go test ./07_build_vendor/          # ok  14.457s
```

**实跑 07**（`cdh device build m22 --only 07_build_vendor`，装配树先删掉片段以走真实写入路径）：

| 判据 | 结果 |
| :--- | :--- |
| 步骤 2c 日志 | `[步骤 2c] 补引擎级公共 VINTF 片段` / 输入 `vendor/vintf/common（1 个片段：android.se.omapi.xml）` / `新增 1 / 已存在跳过 0（共 1 个片段）` |
| 步骤 2b 未受影响 | 摊平结果与上一版一致（装配树 661 条目：文件 428 / 链接 199 / 目录 34） |
| 机型静态树 | `26 个文件` → 写入 23 + `usr` 3（不再含 VINTF 片段） |
| 装配树片段 | `vendor/etc/vintf/manifest/android.se.omapi.xml` 2,804 B，sha256 `39ab9aa3…`，与 `vendor/vintf/common/` 同源同内容 |
| VINTF schema 收口 | `schema -> 9.0（36 个片段，0 个待改写）`：1.0 原样保留，未被抬到 9.0 |
| 标签 | `vendor-meta-rules.txt`：`etc/vintf/manifest/android.se.omapi.xml file 0644 0 0 … u:object_r:vendor_configs_file:s0`（fallback 规则；官方镜像没有这个文件，取不到 EROFS 真值） |
| 唯一性 | 装配树 `manifest/` 下 35 个 xml，`android.se.omapi` 只出现 1 次（无重复 fqname） |
| 镜像回读 | `debugfs -R "cat /etc/vintf/manifest/android.se.omapi.xml"` 与源文件逐行一致 |
| `vendor.img` | `10198921364dde42…`，205,897,728 B（上一版 `1cc87515c5…`，同尺寸；差异仅来自片段注释由 1,986 B 变 2,804 B） |

## 6. 遗留

1. **运行期未复验**：下次起 m22 实例确认 `com.android.se` 不再崩、无 fork 风暴
   （`logcat | grep -c "VINTF declaration error"` 应为 0），`service check android.se.omapi.ISecureElementService/default` = found；
   结果回填本目录 `log-pass.txt`。
2. **全机型继承**：`vendor/vintf/common/` 是引擎级目录，所有跑 07 的机型都会拿到这份声明。若某机型 system
   里没有 `com.android.se`，会变成「已声明但无人注册」，`waitForService` 的调用方会被拖住（同 #53 3.6）。
   届时按需加 `device.json` 开关（例如 `vintf-common` 白名单），而不是回退到机型静态树。
3. **缓存配方未纳入补件摘要**：`ComputeVendorRecipe` 仍只含基底哈希 / HAL 产物根 / APEX 模式。07 没有
   「配方命中就跳过装配」的逻辑（每次都装配），配方只作标签；将来若加跳过逻辑，必须把
   `vendor/vintf/common` 的树摘要并进去。
4. **`out/dist` 仍是旧版**：`out/dist/flyme/m22/12.5.0.0A` 不含本次 `vendor.img`，要发布需重跑 08/09/11 或整条流水线。
