# issue: in-tree 模块名单按机型代际声明（6.12 无线没有 cdh_virt_wifi.ko）

## 1. 问题

03_build_modules 在 6.12 源码上直接失败：

```
    in-tree:    virtio_console.ko, rfkill.ko, cfg80211.ko, mac80211.ko, mac80211_hwsim.ko, libarc4.ko, cdh_virt_wifi.ko
内核模块数量异常:cdh_virt_wifi.ko count=0
[err] 阶段 build_modules 失败: 模块编译失败（exit status 9）
```

`make modules` 本身全部成功（791 行 编译日志无错误），卡在「按名单取 `.ko`」这一步：
名单里的 `cdh_virt_wifi.ko` 在这棵 6.12 树里根本不会产出。

## 2. 根因

`kbuild.InTreeModules` 是引擎里**写死的 6.6/gold 口径**默认表：

- 6.6 树 `drivers/net/wireless/virtual/Makefile` 有 `obj-$(CONFIG_VIRT_WIFI) += cdh_virt_wifi.o` +
  `cdh_virt_wifi-y := virt_wifi.o`——把上游 `virt_wifi.o` 改名产出 `cdh_virt_wifi.ko`；
- 6.12 换成 mac80211_hwsim 模型（hwsim 假扫描/假连接 + `hvc10` 数据面直桥），
  6.12 树里没有 `cdh_virt_wifi` 这个目标，`virt_wifi.c` 也不再被改名。

于是「哪些 `.ko` 要随版本导出」这条机型事实被写进了引擎常量，换代际就必然撞墙；
而且这个校验是**硬失败**（count≠1 → exit 9），不像其余步骤能告警降级。

## 3. 证据

```
$ grep -rn "cdh_virt_wifi.ko" kernels/lineage-23-6.12 --include=Makefile --include=Kconfig
（无输出）

$ ls kernels/lineage-23-6.12/drivers/net/wireless/virtual/
Kconfig  Makefile  mac80211_hwsim.c  mac80211_hwsim.h  virt_wifi.c

$ grep -n "cdh_virt_wifi" kernels/lineage-22-6.6/drivers/net/wireless/virtual/Makefile
3:obj-$(CONFIG_VIRT_WIFI) += cdh_virt_wifi.o
4:cdh_virt_wifi-y := virt_wifi.o
```

（6.6 有、6.12 没有；同一份默认表不可能同时服务两代。）

## 4. 改动

- `build/internal/kbuild/kbuild.go`：新增 `ManifestInTreeModules(manifest)`——读
  `rom.lineage.kernel.in-tree-modules`，缺省回退 `rom.vendor.kernel.in-tree-modules`，
  都未声明返回 nil（沿用内置默认）。口径与 `ManifestFacts` 一致：先底包、再官方。
- `build/03_build_modules/plan.go` `resolveInTree`：取值顺序改为
  `--in-tree-module` → `CDH_KERNEL_IN_TREE_MODULES` → 清单 → `kbuild.InTreeModules` 默认表；
  非法名仍按 `ModuleNamePattern` 报错。
- `manifest/flyme/m22/baselines/12.5.0.0A.json` `rom.lineage.kernel.in-tree-modules`：声明 6 项
  （`virtio_console/rfkill/cfg80211/mac80211/mac80211_hwsim/libarc4`），并附
  `in-tree-modules-note` 说明 6.6 的 `cdh_virt_wifi.ko` 为什么不在表内。
- `build/internal/kbuild/manifest_test.go`：`TestManifestInTreeModules`（声明整表取代、未声明返回 nil）。

## 5. 验证

- 计划：`in-tree: virtio_console.ko, rfkill.ko, cfg80211.ko, mac80211.ko, mac80211_hwsim.ko, libarc4.ko`
  （6 项，无 `cdh_virt_wifi.ko`），模块配方随之从 `201adb120001` 变为 `9669bd33c849`。
- 03 通过：`[ok] 模块构建完成: out/kernel/lineage-23-6.12/modules（10 个 .ko）`＝ 6 in-tree + 4 外部
  （`device_info`、`goodix_fingerprint`、`input_identity`、`virtio_media`）。
- 08 消费这批 `.ko` 做真注入：`写入 15 个文件，回读 15/15 内容一致`、回收 24 个基座模块腾空间。
- `go test ./internal/kbuild/` 全通过。

## 6. 遗留

- 默认表 `kbuild.InTreeModules` 保持 6.6 原样（gold 线仍在用 `cdh_virt_wifi.ko`），
  新代际一律由清单声明；后续可考虑把 `CDH_VIRT_WIFI`/`MAC80211_HWSIM` 两种模型的差异写进
  `02_build_kernel/README.md` 的「换代际检查单」。
- 名单校验仍是硬失败：`.config` 里把某项从 `m` 改成 `y`（或 `n`）会让 03 直接失败。这是有意的
  （导出清单与配置必须一致），但错误信息目前只有 `count=0`，未提示「检查清单/配置是否同代」。
