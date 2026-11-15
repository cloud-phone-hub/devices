# devices/modules —— 外部内核模块

每个子目录是一个可独立编译的 out-of-tree 模块。**目录名就是模块名**，也是清单里引用的名字。

vendor_dlkm 自 2026-11 起按**全自建**处理：基座 .ko 一个不留、`modules.*` 五份索引
全量重建（见 `08_build_vendor_dlkm/README.md`）。所以「云机要用的模块从哪来」只有
两个答案：**本目录的外部模块**，或**内核树内兄弟模块**（`CloudInTreeModules`）。

```
modules/<目录>/
├── module.json   自述：产物名、注入阶段、支持的内核代际、伴随文件、参数
├── Kbuild        编译入口（02 扫 "有 Kbuild 的目录" 认作模块）
├── patches/      可选：对目录内源码的定制补丁，在 Kbuild 里应用
└── …源码
```

**编译入口归模块自己**：03 只发一条 `make M=<模块目录> modules`，模块要传的构建变量、
源码在哪个子目录、要不要先打补丁，都写在自己的 `Kbuild` 里——范例是 `kernelsu/`：源码在
`source/kernel/`（由 `manifest/kernel.xml` 从上游拉取，不入库），`CONFIG_KSU=m` 等参数与
`patches/*.patch` 的应用全在那里表达，03 没有为它加分支。因此产物也不必落在模块根：
03 在整棵模块树里找 `outputs` 声明的 `.ko`，并要求唯一命中。

## module.json

| 字段 | 必填 | 含义 |
| :--- | :--- | :--- |
| `module` | 是 | 必须等于目录名（防复制粘贴错） |
| `outputs` | 是 | 产物 `.ko`，可多个；**不必等于目录名**（`virtio_media` 产 `cdh_media.ko`） |
| `inject` | 是 | `first-stage` 或 `vendor-dlkm`（见下） |
| `kernel` | 否 | 支持的内核代际，如 `["6.6","6.12"]`；空表示不校验 |
| `assets` | 否 | 伴随文件：`source` 相对**清单目录**，`target` 相对注入根；`from-device` / `from-manifest` 见下 |
| `options` | 否 | `modules.options` 参数，如 `allow_userptr=1` |
| `note` | 否 | 为什么这么注入（给后来者） |

`inject` 只有两种取值，差别是**注入时机**，不是分区偏好：

- `first-stage` —— 由 `05_build_vendor_boot` 放进 vendor_ramdisk 根级
  `lib/modules/<release>/`。init 在 switchroot 之前、SELinux 策略加载之前就要读到，
  所以必须走这里；放 vendor_dlkm 会晚一步，且 `dlkm_loader` 可能重复加载。
- `vendor-dlkm` —— 由 `08_build_vendor_dlkm` 注入 vendor_dlkm 分区，late-init 才被读。

## 当前模块

| 模块 | 产物 | inject | kernel |
| :--- | :--- | :--- | :--- |
| `cdh_redirect` | `cdh_redirect.ko` | first-stage | 5.10 / 5.15 / 6.1 / 6.6 / 6.12 / 6.18 |
| `device_info` | `cdh_device_info.ko` | vendor-dlkm | 5.10 / 5.15 / 6.1 / 6.6 / 6.12 / 6.18 |
| `input_identity` | `cdh_input_identity.ko` | vendor-dlkm | 5.10 / 5.15 / 6.1 / 6.6 / 6.12 / 6.18 |
| `kernelsu` | `kernelsu.ko` | first-stage | 5.10 / 5.15 / 6.1 / 6.6 / 6.12 / 6.18 |
| `virtio_media` | `cdh_media.ko` | vendor-dlkm | 5.10 / 5.15 / 6.1 / 6.6 / 6.12 / 6.18 |

`kernelsu` 的源码不在库里（`source/` 由 `manifest/kernel.xml` 从 `tiann/KernelSU` 拉取，
锁到 tag），产物落在 `source/kernel/kernelsu.ko`，详见该目录 `README.md`。

`input_identity` 自 2026-11 起另含原 `goodix_fingerprint` 模块的 `goodix_fp`
platform driver 形态（`/sys/bus/platform/drivers/goodix_fp`，供 AIDA64/DevInfo
读取；由清单 `goodix_fp` 开关控制，oriole 不注册），原模块目录已删除。

`input_identity` 自 2026-11 起改为**配置驱动**：设备数据全部来自清单
`modules.input_identity`（静态注册设备集 + QEMU 侧键盘能力对齐 + 触屏
TOUCH_MAJOR 补齐 + goodix_fp 开关），08 按 `assets.from-manifest=input-devices`
渲染成 `/lib/modules/cdh_input_identity.devices`，模块加载时读取解析注册。
已删除 C 档案（`profiles/`、`profile.c`、`profile.h`）与 `profile` 参数；
执行侧零机型分支、零机型缺省。清单缺该段 → 08 构建失败；文件缺失/解析失败 →
加载 `-EINVAL`。新增机型只改清单，详见该目录 `README.md`。

## 什么能放进这里（硬判据）

一个模块能不能搬进 `modules/`，只看它**编译时还伸手要什么**：

1. 只依赖内核**已导出**的符号（`Module.symvers` 里有）与 **UAPI 头**
   （`include/uapi/`、`include/linux/` 里对外可见的部分）→ 能放，走 `M=` 编。
2. 依赖内核**内部私有头**（如 `include/net/mac80211/*`）或未导出符号 → 不能放，
   必须在核树里编（`make modules` 出），加进 `CloudInTreeModules`。
3. 编译器/构建脚本把它当核树一部分（`obj-$(CONFIG_X) += x.o`，如 `drivers/block/zram/`、
   `mm/zsmalloc.c`）→ 不必也不该抄成外部模块，`make modules` 直接出。

按这条判据落地的结论：

| 想要的模块 | 归属 | 理由 |
| :--- | :--- | :--- |
| `mac80211.ko` / `cfg80211.ko` / `libarc4.ko` | in-tree | mac80211 有 53 个 .c、吃树内私有头 `include/net/mac80211/*` |
| `virtio_console.ko` / `rfkill.ko` / `mac80211_hwsim.ko` | in-tree | 同一条无线/串口链路的依赖，一起出才 vermagic 一致 |
| `cdh_virt_wifi.ko`（6.1/6.6） | in-tree（清单 `modules.in-tree`） | 机型差异项，源码在核树 patches 里 |
| `zram.ko` / `zsmalloc.ko` | in-tree（公共集） | `CONFIG_ZRAM=m`/`CONFIG_ZSMALLOC=m`，`drivers/block/zram/`、`mm/zsmalloc.c` |
| `cdh_redirect` / `device_info` / `virtio_media` 等 | **本目录** | 只吃导出符号，可 `M=` 独立编 |

## 加一个模块

1. 建 `modules/<名>/`，写 `Kbuild` + 源码 + `module.json`（`module` 与目录名一致）。
2. **不需要改任何阶段代码**：03 按清单编、05/08 按 `inject` 派生注入集。
3. 在要用它的机型的基线 JSON 顶层 `modules.external` 里加目录名：

   ```json
   "modules": { "external": ["cdh_redirect", "<新模块>"] }
   ```

   顶层 `modules` 与 `vendor` / `vendor-dlkm` / `patch` 平级，**不在 `rom.lineage.kernel` 里面**。
4. 有伴随文件（如机型规则、UTS 数据文件）时写 `assets`，`source` 放在该机型清单目录下，一台机型一份。
   若内容本来就属于机型参数或基线事实，可用 `"from-device": "redirect"`（从 `device.json` 的
   `modules.redirect` 渲染，如 `cdh_redirect`）或 `"from-manifest"`（从基线清单渲染：
   `uname-version`←`rom.official.kernel.uname-version`、`cpufreq`←`modules.cpufreq`、
   `thermal`←`modules.thermal`，如 `device_info`；`input-devices`←`modules.input_identity`，
   如 `input_identity`，**必需无回退**）；此时 `source` 只作兼容兜底，不再需要单独维护文件。

## 约定

- `modules.external` 是**必填**（缺段报错，写 `[]` 表示一个都不编）；不再有
  「`modules/` 下有什么就编什么」的缺省。
- in-tree 模块不在这个目录：公共集见 `build/internal/kbuild` 的 `CloudInTreeModules`，
  机型追加项写在清单 `modules.in-tree`（如 gold 的 `cdh_virt_wifi.ko`）。
- `assets` 的 `target` 是注入根下的相对路径，05 只放 `lib/modules/…` 这类位置，
  不要往 `/vendor/etc` 写（那是 07/08 的静态树职责）。
- 改 `.ko` 源码后必须重跑 `03 → 05/08 → 09 → 11` 才会进盘；只重跑 11 拿不到新模块。
