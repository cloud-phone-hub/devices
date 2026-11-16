# devices/manifest 设备数据库规范（devices / configs / versions）

`devices/manifest/` 采用三表 JSON 文件目录结构（按表为第一层目录，下分 `<brand>/<device>`），便于多电脑通过 Git 协作与合并：

- `devices/<brand>/<device>.json`：设备表（`devices`），每台设备 1 个 JSON 文件
- `configs/<brand>/<device>/<name>.json`：配置模版表（`configs`），每套代际配置模版 1 个 JSON 文件（内含 `fileTrees` 与 `sepolicy`，不拆散文件）
- `versions/<brand>/<device>/<buildId>.json`：版本表（`versions`），每个 ROM 版本 1 个精简 JSON 文件（仅存模版关联 `configId`、实测身份与版本差异 `overrides`）

---

## 设备表 (`devices/<brand>/<device>.json`)
* 设备ID `id`
* 官方 `official`
    * 例如，小米，vivo，OPPO，荣耀
* 品牌 `brand`
    * 例如，`xiaomi`，`redmi`，`poco`，`honor`
* 机型 `model`
    * 例如，`Xiaomi 17 Pro Max`，`Honor Magic7 RSR`
* 设备 `device`
    * 例如，`gold`，`popsicle`，`sun`
* 平台 `platform`
    * 例如，`qcom`, `mtk`, `tensor`, `kirin`
* 分辨率 `resolution`
    * 例如，`1920x1080`, `2400x1080`, `1280x2800`
* 屏幕大小 `inch`
    * 例如，`70x155`, `73x159`
* 屏幕密度 `density`
    * 例如，`400`, `560`
* cpu核心 `smp`
    * 默认8核
* 内存 `memory`
    * `[8192]`
* 存储 `storage`
    * `[128000, 256000]`
* 扩展信息 `extra`
    * `board`、`verifiedBootHash`、`hostHvc`、`cmdline`

---

## 配置模版表 (`configs/<brand>/<device>/<name>.json`)
统筹同机型在同代际（`deviceId + androidVersion`）下可跨多个 ROM 版本复用的公共构建配置模版。

* 模版ID `id`
* 设备ID `deviceId`：关联设备表 `devices(id)`
* 模版名称 `name`：如 `popsicle-android16`、`gold-android15`、`sun-android15`
* 安卓版本 `androidVersion`：如 `"16"`
* lineage `lineageVersion`：如 `"lineage-23.2"`
* `unpack` 公共解压分区配方
    * `official` 官方包分区集合，支持 `"cust.img:userdata.img"` 从后者解压出前者
    * `lineage` 底包分区集合，如 `["vendor_a.img", "vendor_dlkm_a.img"]`
* `buildModules` 模块与路径重定向对象，例如：
    ```json
    "redirect": [
        {"type": "dir", "from": "/vendor/etc", "to": "/mnt/vendorpatch/vendor/etc"},
        {"type": "dir", "from": "/vendor/bin", "to": "/mnt/vendorpatch/vendor/bin"},
        {"type": "dir", "from": "/vendor/lib", "to": "/mnt/vendorpatch/vendor/lib"},
        {"type": "dir", "from": "/vendor/lib64", "to": "/mnt/vendorpatch/vendor/lib64"},
        {"type": "file", "from": "/vendor/build.prop", "to": "/mnt/vendorpatch/vendor/build.prop"},
        {"type": "file", "from": "/vendor/ueventd.rc", "to": "/mnt/vendorpatch/vendor/ueventd.rc"},
        {"type": "dir", "from": "/vendor_dlkm/lib", "to": "/mnt/vendorpatch/vendor_dlkm/lib"},
        {"type": "file", "from": "/odm", "to": "/odm"},
        {"type": "dir", "from": "/odm", "to": "/mnt/vendorpatch/odm"},
        {"type": "file", "from": "/system/bin/update_verifier", "to": "/mnt/vendorpatch/system/bin/update_verifier"}
    ],
    "external": ["cdh_redirect", "device_info", "input_identity", "virtio_media"]
    ```
    * `input_identity` 输入设备身份段（`input_identity` 模块**必需、无缺省**）：声明该机型全部内核侧输入设备，08 按 `from-manifest=input-devices` 渲染成 `/lib/modules/cdh_input_identity.devices`，模块加载时读取解析注册。能力码（`keys`/`switches`/`msc`/`abs.axis`）接受名称或整数（名称经构建期 Go 转码）；`bus/vendor/product/version` 用十进制数字（缺省 0）；`devices` 可空数组，`aligns`/`touch_major`/`goodix_fp` 可缺省。与 `modules.input`（QEMU 侧身份）合起来覆盖全部输入设备。例如：
    ```json
    "input_identity": {
      "touch_major": true, "goodix_fp": true,
      "devices": [
        {"name": "uinput-fpc", "bus": 6, "vendor": 1638, "product": 2184, "version": 0,
         "keys": ["KEY_UP","KEY_LEFT","KEY_RIGHT","KEY_DOWN"],
         "abs": [{"axis":"ABS_Z","min":0,"max":255,"fuzz":0,"flat":0}]}
      ],
      "aligns": [{"name": "mtk-kpd", "keys": ["KEY_VOLUMEDOWN"]}]
    }
    ```
* `buildVendorBoot`
    * `fstab` 要将 lineage `vendor_boot` 里的 fstab 的后缀改名到指定的名称（如 `"canoe"`）
* `buildOdm`
    * `img` 镜像名称（如 `"odm_a.img"`）
    * `buildProp` `build.prop` 的路径（如 `"/etc/build.prop"`）
    * `build-prop-sets` 要修改的属性集合 `{}`
    * `build-prop-unsets` 要删除的属性集合 `[]`
    * `fileTrees` odm 下的静态文件 `{}`
* `buildVendor`
    * `hardware` 要集成的硬件 HAL 列表
    * `files` 增加/替换/清理规则
    * `build-prop-sets` 要修改的公共属性集合 `{}`（不含随包变化的 `ro.vendor.boot_security_patch`）
    * `build-prop-unsets` 要删除的属性集合 `[]`
    * `selinux-labels` 配置 key 路径的 value 标签 `{}`
    * `fileTrees` vendor 下的静态文件 `{}`
    * `sepolicy` selinux 规则文件 `{}`
* `buildKernel`：模版基线内核版本配置（若某 ROM 版本有内核升级，在 `versions.overrides.buildKernel` 中覆盖）
    * `uname` 如 `"Linux localhost 6.12.23-android16-5-g75e9b1c7ae7c-abogki463945075-4k #1 SMP PREEMPT Thu Nov 27 08:45:40 UTC 2025 aarch64 Toybox"`
    * `version` 如 `"6.12"`
    * `lineageKernel` 可选的借用说明（`borrowed-from` / `borrowed-reason`）
* `buildVendorDlkm`：模版基线 `vendor_dlkm` `/etc/build.prop` 校验信息（若某 ROM 版本有差异，在 `versions.overrides.buildVendorDlkm` 中覆盖）
* `buildPatch`
    * `vendor-cil-rewrite` CIL 规则改写列表
* `buildSuper`
    * `images` 集合：如果不是 `super.img` 需要将里面的集合打包成 `super.img`
* `buildVda`
    * `partitions` 数组：将里面的对象追加到 vda 镜像

### 按 pipeline 步骤拆分的目录布局（可选）

配置模版可以继续写成一个 `<name>.json`，也可以拆成「身份文件 + 每步骤一个子目录」。
拆分后身份字段（`id`/`deviceId`/`name`/`androidVersion`/`lineageVersion`/`extra`/`updatedAt`）
仍留在 `<name>.json`，其余各写入同目录下对应步骤的子目录：

| 子目录 | 配置键 | file-map 字段（铺成真实文件） |
| --- | --- | --- |
| `unpack/` | `unpack` | — |
| `kernel/` | `buildKernel` | — |
| `module/` | `buildModules` | — |
| `vendor_boot/` | `buildVendorBoot` | — |
| `odm/` | `buildOdm` | `fileTrees` → `odm/<目标路径>` |
| `vendor/` | `buildVendor` | `fileTrees` → `vendor/<目标路径>`；`sepolicy` → `vendor/sepolicy/<文件名>` |
| `vendor_dlkm/` | `buildVendorDlkm` | — |
| `patch/` | `buildPatch` | — |
| `super/` | `buildSuper` | — |
| `vda/` | `buildVda` | — |
| `hardware/` | `buildVendor.hardware`（迁出存放） | — |

* 每个子目录里的 `config.json` 是该步骤除 file-map 字段外剩下的字段，**不是**一条配置记录；
  读取时按表合并回记录，写回时各回各的子目录。
* file-map 字段（`fileTrees`/`sepolicy`）在磁盘上就是真实文件，按声明的目标路径摆放，
  可以直接编辑与 diff；`config.json` 里不再保留这两个字段。
* `hardware/config.json` 装的是 `{"hardware": [...]}`，即原本住在 `buildVendor` 里的 HAL 清单；
  读时并回 `buildVendor.hardware`，`vendor/config.json` 里不再保留这一段。
* 拆分只在「设备目录下只有一个配置 JSON」时生效；同机型多代际各有一份 `<name>.json` 时
  （如 `oriole` 的 21/22）无法归属步骤目录，仍按单文件读写。
* `prepare/`、`boot/` 是预留目录，当前没有对应配置键。

---

## 版本表 (`versions/<brand>/<device>/<buildId>.json`)
仅存储具体 ROM 刷机包的实测身份元数据、所绑定的配置模版外键 `configId`，以及针对 `configs` 任意步骤的版本差异覆盖 `overrides`。

* `id`：版本ID
* `buildId`：例如 `16-OS3.0.319.0.WPBCNXM`、`16-OS4.0.5.0.XPBCN`
* `deviceId`：关联设备表 `devices(id)`
* `configId`：关联配置模版表 `configs(id)`
* `osVersion`：厂商 OS 版本号，如 `OS4.0.5.0.XPBCN`
* `buildDate`：ROM 的编译日期 `ro.vendor.build.date.utc` (`yyyyMMdd`)
* `securityPatch`：ROM 的安全补丁日期 `ro.vendor.build.security_patch`（如 `2026-10-01`，合成基线时自动注入 `vendor.build-prop-sets["ro.vendor.boot_security_patch"]`）
* `factoryTgz`：官方刷机包文件名
* `factoryDir`：官方刷机包解包目录名（可选）
* `overrides`：该版本相对 `configs` 模版各构建步骤（`unpack` / `buildKernel` / `buildModules` / `buildVendorBoot` / `buildOdm` / `buildVendor` / `buildVendorDlkm` / `buildPatch` / `buildSuper` / `buildVda`）的差异覆盖（默认 `{}`，支持按阶段对 Map 键值覆盖/`null` 剔除、对数组 `{"add":[...],"remove":[...]}` 增删或直接替换）
* `extra`：版本级补充信息（如 `comment`、`verifiedBootHash`）

---

## 新版本 ROM 就近匹配与合并规则
1. **就近选择配置模版（`ResolveNearestConfig`）**：
   * 优先匹配同 `deviceId` 下 `androidVersion` 相同的 `configs` 模版；
   * 若同代存在多个候选模版，依次按：① 关联版本的 `osVersion` 语义数字向量距离最近；② `buildDate` 时间差最小进行排序；
   * 若该设备首次跨 Android 大版本升级（尚无对应代际模版），则以最近一代模版为蓝本克隆派生新模版（自动更新 `androidVersion` 与 `lineageVersion`）。
2. **合成有效版本配置（`ResolveEffectiveVersion`）**：
   * 以 `configs` 模版的全部 10 个步骤为基底，注入 `versions` 的 `factoryTgz`、`factoryDir`、`securityPatch`（写入 `vendor.build-prop-sets["ro.vendor.boot_security_patch"]`），最后按步骤叠加 `versions.overrides`，并在构建期物化至 `devices/manifest/<brand>/<device>/`（`device.json` + `baselines/<buildID>.json` + `vendor`/`sepolicy`/`odm` 静态树）供 `01_unpack` ~ `11_pack_vda` 透明消费。

