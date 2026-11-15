# issue: gold 的 `modules.in-tree` 仍声明 `cdh_virt_wifi.ko`，03 硬失败

机型：`xiaomi/gold`（Redmi Note 13 5G，lineage-22.2 / 内核 6.6.127-4k-cdh）
阶段：`03_build_modules`
时间：2026-11-04
改动类型：**只改清单**（`devices/manifest/xiaomi/gold/baselines/gold_images_OS3.0.10.0.VNQCNXM_20260715.0000.00_15.0_cn_a9729c6694.json`），零代码

## 1. 问题

按新引擎重跑 gold 全流水线（`cdh device build gold`），02 内核编完后进 03，容器内 `make modules`
正常收尾却没有产物：

```
内核模块数量异常:cdh_virt_wifi.ko count=0
[err] 阶段 build_modules 失败: 模块编译失败（exit status 9）
```

03 的名单是「引擎公共集 8 项 + 清单追加项」，日志里打出的正是 9 项：

```
in-tree: virtio_console.ko, rfkill.ko, cfg80211.ko, mac80211.ko, mac80211_hwsim.ko,
         libarc4.ko, zram.ko, zsmalloc.ko, cdh_virt_wifi.ko
```

`cdh_virt_wifi.ko` 来自清单 `modules.in-tree`。

## 2. 根因

**这条声明是过期产物名，6.6 树里已经没有这个模块了。**

- `cdh_virt_wifi.ko` 不是上游模块名，是原 `gold-15` 私有分支把
  `drivers/net/wireless/virtual/virt_wifi.o` 的发布名改掉（连带改 `Makefile` 与
  `android/abi_gki_aarch64_virtual_device`）得到的产物；
- 2026-11-02 起 `kernels/lineage-22-6.6` 直接取 LineageOS `lineage-22.1` 上游分支，
  **有意不收录**那 11 条 pixel6-15 提交（`devices/kernels/patches/lineage-22-6.6/README.md`
  「有意不收录」一节原文：「这些都是 oriole（Pixel 6）线的改动；gold 用 `0002` 的 hwsim
  假 WiFi，不需要 `virt_wifi`」）；
- 现在树里的 Makefile 是上游原样（`obj-$(CONFIG_VIRT_WIFI) += virt_wifi.o`），**只丢掉了改名、
  没丢掉模块本身**：`out/kernel/lineage-22-6.6/config:2710` 仍是 `CONFIG_VIRT_WIFI=m`，
  所以这条线上能编出来的是上游名 `virt_wifi.ko`，声明里的 `cdh_virt_wifi.ko` 这个名字
  在树上和配置里都不存在 → `make modules` 正常收尾、`find` 找不到该文件、容器 `exit 9`；
- gold 的模拟 WiFi 走 `0002-hwsim-fake-wifi.patch`（改 `mac80211_hwsim.c`，控制面 `hvc9`、
  数据面直桥 `hvc10`），产物 `mac80211_hwsim.ko` 已在引擎公共集里，机型侧不需要任何追加项——
  所以修法是**摘掉这条声明**（而不是改成 `virt_wifi.ko` 把它继续注入镜像）。

三条内核线的取值实测一致（都是 `=m`），差别只在**产物名**与**清单声明**是否对得上：

| 内核线 | `.config` 的 `CONFIG_VIRT_WIFI` | 树里的发布名 | 清单 `modules.in-tree` | 结果 |
| :--- | :--- | :--- | :--- | :--- |
| `lineage-21-6.1`（oriole AP2A） | `=m`（2632 行） | 上游 `virt_wifi.ko` | `["virt_wifi.ko"]` | 名字对得上，产物已有 `virt_wifi.ko` |
| `lineage-22-6.6`（gold） | `=m`（2710 行） | 上游 `virt_wifi.ko`（改名提交已回退） | `["cdh_virt_wifi.ko"]` | **名字对不上 → `count=0` 硬失败** |
| `lineage-23-6.12`（m22/15r/cph2797/ss9380/popsicle） | `=m`（2844 行） | 上游 `virt_wifi.ko` | 未声明（走公共集） | 不找它，正常 |

也就是说 6.12 那批机型早在 m22 #47 已把这条差异摘掉，gold 是**同一条坑上唯一没跟着改的清单**；
cph2797 #76 当年撞到的是同一形态的报错（当时是它的内核线不该带 `cdh_virt_wifi.ko`）。

## 3. 证据

- `log-fail.txt`：失败那一跑，从 `==> [3/11] build_modules` 到 `内核模块数量异常:cdh_virt_wifi.ko count=0`
  （含 9 项 in-tree 名单、`-j10` 与容器 `exit 9`）。
- `log-pass.txt`：改清单后 `--from build_modules` 续跑，名单变 8 项，
  `[ok] 模块构建完成: out/kernel/lineage-22-6.6/modules（14 个 .ko）`。
- 现场只读核对（未改内核树、未改 `.config`）：
  ```bash
  cd /Volumes/work/cloud-phone-hub/engine
  grep -n VIRT_WIFI out/kernel/lineage-22-6.6/config out/kernel/lineage-21-6.1/config \
                      out/kernel/lineage-23-6.12/config
  #   三条线都是 CONFIG_VIRT_WIFI=m —— 配置没问题，错的只是声明的名字
  sed -n '/obj-/p' kernels/lineage-22-6.6/drivers/net/wireless/virtual/Makefile
  #   obj-$(CONFIG_MAC80211_HWSIM) += mac80211_hwsim.o
  #   obj-$(CONFIG_VIRT_WIFI)      += virt_wifi.o      ← 上游原名，无 cdh 改名
  ls out/kernel/lineage-22-6.6/modules/*.ko | xargs -n1 basename   # 改后 14 个，无 virt_wifi
  sed -n '1,40p' kernels/patches/lineage-22-6.6/README.md   # 「不需要 virt_wifi」原文
  python3 -c "import json;print(json.load(open('manifest/xiaomi/gold/baselines/gold_images_OS3.0.10.0.VNQCNXM_20260715.0000.00_15.0_cn_a9729c6694.json'))['modules'])"
  ```

## 4. 改动

`manifest/xiaomi/gold/baselines/gold_images_OS3.0.10.0.VNQCNXM_….json`：

```diff
   "modules": {
     "external": ["cdh_redirect","device_info","goodix_fingerprint","input_identity","kernelsu","virtio_media"],
-    "in-tree": ["cdh_virt_wifi.ko"]
+    "in-tree": []
   },
```

写空数组而不是删键：`Manifest.InTreeExtras()` 用 `*[]string` 区分「未声明」与「声明为空」，
显式空数组等于向后来者表明**这台机型有意不追加任何 in-tree 项**（与
`devices/manifest/README.md` 对 `vendor.files` 建议的写法一致）；删键则会被读成「未声明」，
下次公共集变动时分不清是漏写还是刻意。

不改代码的理由：03 的行为是对的——名单里的模块找不到就该硬失败（
`03_build_modules/README.md` 排错表第 384 行把这条列成「同名撞车/机型代际不符」）。
真正的错配在清单侧，去代码里加「6.6 跳过 cdh_virt_wifi」的特例属于把机型事实写回引擎，
正是 m22 #47 已经拆掉的那套死表口径。

## 5. 验证

- 03 实跑通过：`out/kernel/lineage-22-6.6/modules/` 14 个 `.ko`（6 个外部模块产物 + 公共集 8 项），
  数量与 in-tree 名单校验、`Module.symvers` 符号版本校验全过（`log-pass.txt`）；
- 名单首行核对：续跑日志打印 `… zsmalloc.ko`（8 项，无 `cdh_virt_wifi.ko`）；
- 清单仍是合法 JSON 且 `modules.in-tree == []`（磁盘回读，不是编辑工具的回显）；
- 后续阶段 04→07 在续跑中接着走（本条只覆盖 03，整链结论见下方遗留与后续条目）。

## 6. 遗留

- gold 的 04~11 尚未跑完，本条只固证到 03；08 的 `modules.load` 由「本次注入集」全量重建，
  不再引用 `cdh_virt_wifi`，若 guest 侧 WiFi 在起机后出现「无 wlan 接口」，回到
  `0002-hwsim-fake-wifi` 这条线排查，**不要**把 `virt_wifi` 加回清单；
- oriole AP2A 的 `["virt_wifi.ko"]` 保持原样：它的 6.1 `.config` 里 `CONFIG_VIRT_WIFI=m`
  且 `out/kernel/lineage-21-6.1/modules/virt_wifi.ko` 实际存在，声明与产物一致，不属于本条问题；
- 6.12 那批机型的 `.config` 带着 `CONFIG_VIRT_WIFI=m`（lineageos 侧带进来的），但清单没声明、
  03 也不去找——多编一个不注入的模块只是白占镜像外空间。要不要把 6.12 的 `VIRT_WIFI` 显式关掉，
  属于 `02_build_kernel/lineage-kernel.config` 的口径问题，本条未动。
