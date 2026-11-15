# 02/03 缺机型级内核配置入口：gold 口径片段被 m22 硬套，两项静默失效

- 设备：flyme/m22（MEIZU 22，Flyme 12.5.0.0A / Android 16 / QTI `sun`）
- 组件：`engine/build/02_build_kernel`、`engine/build/03_build_modules`、`internal/kbuild`
- 产物：`engine/out/kernel/lineage-23-6.12/Image`（43,969,024 B，配方 `lineage-23-6.12-cdh-caaa3aa6868c`）

## 1. 问题

02/03 的配置片段原本只有一条路：`--config` / `CDH_KERNEL_CONFIG`，缺省落到引擎级
`build/02_build_kernel/lineage-kernel.config`。那段是 gold 口径，m22 直接套用后有两项
**不报错地失效**：

1. `CONFIG_CDH_PATH_REDIRECT=y` 在产物 `config` 里整条消失——lineage-23.2 源码里
   根本没有这个符号；
2. `CONFIG_VIRTIO_CONSOLE=y`（片段注释写"内建，不再作为模块"）实测是 `=m`——
   阶段脚本在 `merge_config` 之后又执行了 `scripts/config --module VIRTIO_CONSOLE`。

m22 也不该带 gold 的路径重定向：m22 走 lineage 分区重打包，`/vendor`、`/vendor_dlkm`
挂载点无需改名（v1 设备侧结论，见 #46 的补丁取舍表）。

## 2. 根因

`resolveConfig` 只有三级回退（`--config` → `CDH_KERNEL_CONFIG` → 引擎默认），
没有"清单声明机型片段"这一级。机型差异无处承载，只能改引擎公共文件——而那个文件
同时服务 gold，改它就是拆 gold 的台。

同时，引擎片段的 `CONFIG_VIRTIO_CONSOLE=y` 与注释"内建"是拆分前的旧结论，
与脚本实际行为（强制模块）矛盾，谁都没校验过。

## 3. 证据

`log-fail.txt`：

1. `kernels/lineage-23-6.12` 里 `grep -rn CDH_PATH_REDIRECT --include=Kconfig --include=Makefile`
   无任何输出（源码无此符号）；
2. 旧构建卷 `cdh-kernel-lineage-23-6.12-cdh-eef9316b0b57` 里，片段原文与产物 `.config`
   对照：

```
--- 片段原文（/src/.cdh/kernel.config）---
CONFIG_CDH_PATH_REDIRECT=y
CONFIG_VIRTIO_CONSOLE=y
--- 产物（/src/out/.config）---
CONFIG_VIRTIO_CONSOLE=m
CONFIG_CDH_PIONEER_PERMISSIVE=y      # CDH_PATH_REDIRECT 整条不存在
```

3. v1 设备侧 m22 的片段（`devices/flyme/m22/kernel/build/lineage-kernel.config`）
   只有四项事实：`CDH_PIONEER_PERMISSIVE=y`、`MAC80211=m`、`MAC80211_HWSIM=m`、
   `CRYPTO_LIB_ARC4=y`——没有 PATH_REDIRECT，也没有 VIRTIO_CONSOLE。

## 4. 改动

- `internal/kbuild/kbuild.go`：新增 `ManifestKernelConfig(manifest)`，读
  `rom.lineage.kernel.config`，缺省回退 `rom.vendor.kernel.config`；未声明返回空串。
- `02_build_kernel/plan.go`、`03_build_modules/plan.go`：`resolveConfig` 增加清单级
  回退，优先级 `--config` → `CDH_KERNEL_CONFIG` → 清单 → 引擎默认；相对路径按
  `engine.Root()` 解析，日志来源写「清单 rom.lineage.kernel.config」。
- 新增 `manifest/flyme/m22/kernel.config`：按 v1 m22 的四项事实重写，并显式声明
  `CONFIG_VIRTIO_CONSOLE=m`（03 的 in-tree 首项依赖它）；注释里写明
  `CONFIG_CRYPTO_LIB_ARC4=y` 会被 `MAC80211=m` 的 select 压回 `m`、
  `CONFIG_CDH_PATH_REDIRECT` 故意不声明。
- 基线清单 `12.5.0.0A.json`：`rom.lineage.kernel.config` 指向该片段，并加
  `config-note` 记录取舍。
- `internal/kbuild/manifest_test.go`：新增 `TestManifestKernelConfig`（优先底包段、
  回退官方段、未声明空前串）。

## 5. 验证

`log-pass.txt`：

- 02 日志首行变为 `配置片段: manifest/flyme/m22/kernel.config（清单
  rom.lineage.kernel.config）`，配方由 `lineage-23-6.12-cdh-eef9316b0b57` 变为
  `lineage-23-6.12-cdh-caaa3aa6868c`（片段哈希参与配方）；
- 新产物 `out/kernel/lineage-23-6.12/config`：`CONFIG_CDH_PATH_REDIRECT` 出现 **0** 次、
  `CONFIG_VIRTIO_CONSOLE=m`、`CONFIG_CDH_PIONEER_PERMISSIVE=y`、
  `CONFIG_MAC80211(_HWSIM)=m`、`CONFIG_CRYPTO_LIB_ARC4=m`；
- `Image` 43,969,024 B、release `6.12.81-4k-cdh`；
- 03 编出 10 个 `.ko`，符号版本逐项一致（`mac80211.ko 323`、`mac80211_hwsim.ko 204`、
  `virtio_console.ko 100` 等）；
- 04→11 续跑：`boot.img` 83,886,080 B（配方 `boot-aaf1fbee2753`）、`vendor.img`
  180,752,384 B、`vendor_dlkm.img` 15,831,040 B（注入 15 文件回读 15/15）、
  `patch.img` 402,653,184 B（792 条目）、`super.img` 复用、dist 五槽刷新一致；
- `go vet ./...` 干净、`go test ./...` 全 ok。

## 6. 遗留

- v1 设备侧 m22 还编了一个 `cpu_probe` 外部模块（`cdh_cpu_probe.ko`）。核对源码后确认
  它是**只读原型**，`/proc/cdh_cpu_profile` 里硬编码 `device=note13-5g-15`、`android=15`
  与 gold 的 CPU 簇/频率，对 m22 是错误身份；v2 的 `cdh_device_info.ko`（midr / cpufreq /
  cache / topology 子模块）已覆盖同一信息面，因此**不移植**，避免注入一份写死别家事实的节点。
- 引擎默认片段仍保留 gold 的 `CONFIG_CDH_PATH_REDIRECT=y` 与旧的
  `CONFIG_VIRTIO_CONSOLE=y` 注释；gold 未声明机型片段时继续走它，等 gold 也切到机型
  片段时再统一清理。
- `CONFIG_CRYPTO_LIB_ARC4` 在 m22 上恒为 `m`（内核 select 规则），片段里写 `=y` 只是
  意图声明；若将来 MAC80211 改成 `=y`，这里才会真的内建。

---

## 7. 回退（2026-10-29）：撤销机型片段，回到统一片段

### 问题

“机型一份、引擎一份”的模型把差异放大成了漏项：两份片段**实质只差一项**
`CONFIG_DMIID`，而这一项恰好只在引擎片段里（`# CONFIG_DMIID is not set`），机型片段没写。
机型片段是**整表取代**（不是叠加），所以 m22 的 `Kconfig default y` 生效，产物变 `=y`。

实测（实例 30，cloud-phone-hub 内核 6.12.81-4k-cdh）：

```
$ adb shell cat /sys/class/dmi/id/product_name   -> QEMU Virtual Machine
$ adb shell cat /sys/class/dmi/id/sys_vendor     -> QEMU
$ adb shell cat /sys/class/dmi/id/bios_vendor    -> EDK II
$ grep DMIID out/kernel/lineage-23-6.12/config   -> CONFIG_DMIID=y
$ grep DMIID out/kernel/lineage-22-6.6/config    -> # CONFIG_DMIID is not set   (gold)
```

正好与引擎片段那句 “Keep SMBIOS available to the kernel without exporting desktop
DMI IDs to Android apps” 相反：宿主身份（QEMU / EDK II / SMBIOS 3.0）直接暴露给
用户态，而 ARM 真机根本没有 `/sys/class/dmi/id` 这棵目录。

### 改动

| 文件 | 改动 |
| :--- | :--- |
| `build/02_build_kernel/lineage-kernel.config` | 吸收机型片段的内容，成为**全机型统一**片段：`VIRTIO_CONSOLE` 由 `=y` 改 `=m`（与脚本 `scripts/config --module` 同向）、补 `ARC4` 会被 select 压成 `=m` 的说明、`DMIID` 关闭注释写实（两代内核共用） |
| `manifest/flyme/m22/baselines/12.5.0.0A.json` | 删除 `rom.lineage.kernel.config`，`config-note` 改为回退说明 |
| `manifest/flyme/m22/kernel.config` | **删除**（内容已并入引擎片段） |

### 验证

```
$ cd engine && CDH_MANIFEST=manifest/flyme/m22/baselines/12.5.0.0A.json \
    CDH_LINEAGE_VERSION=lineage-23.2 CDH_KERNEL_SOURCE=kernels/lineage-23-6.12 \
    cdh build-kernel --dry-run
==> 配置片段: build/02_build_kernel/lineage-kernel.config（本阶段默认）
    内核:       lineage-23-6.12（release 6.12.81-4k-cdh）
    构建镜像:   ghcr.io/feicong/droid-ddk:android16-6.12
    缓存配方:   lineage-23-6.12-cdh-b4fe1c987b3a        # 因片段内容变而变
$ cdh build-modules --dry-run
    配置片段:   build/02_build_kernel/lineage-kernel.config
    in-tree:    virtio_console.ko, rfkill.ko, cfg80211.ko, mac80211.ko, mac80211_hwsim.ko, libarc4.ko
```

### 重编验证（2026-10-29 10:47–10:58，02 → 03 → 04 → 11 全链）

先用 `go build -o out/bin/cdh ./build` 刷新二进制（10-28 那份不认 `CDH_MANIFEST`，
会回落成 6.6 清单），再逐阶段重跑：

| 阶段 | 配方（旧 → 新） | 关键结果 |
| :--- | :--- | :--- |
| 02 build-kernel | `b4fe1c987b3a`（原 `caaa3aa6868c`） | `grep DMIID out/kernel/lineage-23-6.12/config` → `# CONFIG_DMIID is not set`（原来是 `=y`）；`Image` 43,969,024 B，sha256 `83d15e7d…93c96c` |
| 03 build-modules | `461b4ca5934a` | 10 个 `.ko` 全部符号版本一致（`virtio_console` 100 个、`mac80211` 323 个……） |
| 04 build-boot | `boot-5a215e13e2f5`（原 `6c0678e2f433`） | `boot.img` 83,886,080 B，sha256 `414801a6…7e92844` |
| 11 pack-vda | `vda-5fe78d51e413`（原 `d3da89ad6175`） | super / boot_a / boot_b / vendor_boot_a / vendor_boot_b 五槽回读一致；`device.json` 554 B（input 段透传） |

起机验证（实例 31，console 5918 / adb 5919 / vnc 5920，`boot_completed=1`）：

```
$ adb shell dmesg | head -2
[    0.000000] Linux version 6.12.81-4k-cdh (root@38588ffa6ec1) …
             #1 SMP PREEMPT Thu Oct 29 02:52:55 UTC 2026      <- 本次新编（旧 10-28 12:04:39）
$ adb shell ls /sys/class/dmi
ls: /sys/class/dmi: No such file or directory                <- 旧行为: product_name=QEMU Virtual Machine
$ adb shell cat /sys/class/dmi/id/product_name
cat: /sys/class/dmi/id/product_name: No such file or directory
$ adb shell uname -r            -> 6.6.89-android15-8-gbe8d201b0d27-ab13762941-4k（伪装）
$ adb shell getprop ro.kernel.version -> 6.12
```

同一实例的输入身份没被这次重编带回去：

```
    7: fts
      Classes: KEYBOARD | TOUCH | TOUCH_MT
      Location: virtio8/input0
      Identifier: bus=0x001c, vendor=0x0001, product=0x0002, version=0x0100
      KeyLayoutFile: /system/usr/keylayout/fts.kl
  Device 2: fts
```

完整命令与输出见同目录 `log-pass.txt` 的「回退后重编验证」一节；起机后桌面帧见
`boot-dmiid-off.png`（vnc 5920 抓帧，无黑屏、无设置向导回退）。

### 遗留

- **vendor_dlkm 里的 `.ko` 仍是 10-28 那批**：本次只跑了 02→03→04→11，03 新编的 10 个
  `.ko` 落在 `out/kernel/lineage-23-6.12/modules/`，尚未经 08→09→10→11 注入 `vendor_dlkm`。
  实测旧 `.ko` 在新内核上全部加载成功（`lsmod` 10 个模块齐全，无 `vermagic` / `disagrees`
  / `Unknown symbol` 报错）：DMIID 改动不影响任何导出符号的 CRC，所以两者**功能等价**。
  若要产物字节也一致，后续跑一次 08→09→10→11 即可。
- `out/bin/cdh` 由 `go build` 直出（7,182,162 B），未走 `just` 的发版包装；若要
  对外发版仍需按构建说明重新打包。
- 机型片段机制（`rom.lineage.kernel.config`）保留在 `kbuild.ManifestKernelConfig` 里，
  将来真需要机型专属项（比如某个 SoC 特有驱动）时仍可用，但默认不再分叉。
- gold（`lineage-22-6.6`）产物本来就是 `# CONFIG_DMIID is not set`，本次统一片段
  对它是**等价**改动（只多了 `PATH_REDIRECT` / `PIONEER` 在 6.6 已有的同值声明），
  尚未为 gold 重编验证；gold 盘集当前不重打也不影响行为。

