# Issue: popsicle 基线 `vendor.selinux-labels` 69 条全是死条目（已整表清空）

- 设备：`xiaomi/popsicle`（Xiaomi 17 Pro Max / canoe / lineage-23.2 / kernel 6.12）
- 阶段：`07_build_vendor` 打包段（`pack.go` 的 `loadBaselineLabels` + `setxattr.go` 的 `contextFor`）
- 状态：**已闭环**（2026-11-03，重跑 07 实测：除 3 条 cph_*.rc 从 `vendor_file` 落回兜底
  `vendor_configs_file` 外，557 条路径其余标签零变化）
- 关联：[#126 issue-ueventd-frp-device-nodes](../issue-ueventd-frp-device-nodes/solution.md)（同机首次起机链）

## 一、问题

`baselines/popsicle_images_OS3.0.319.0.WPBCNXM_*.json` 的 `vendor.selinux-labels` 有 69 条，
看着像「每个注入件一份标签」，实际逐条对 07 的真实元数据表一查：

| 分类 | 条数 | 具体情况 |
| :--- | :--- | :--- |
| 装配树里**没有这个路径** | **54** | `bin/daemon`、`bin/logcat-dump`、`bin/netmgr_setup`、`etc/fstab.qcom`、`usr/idc/QEMU *.idc`、`bin/hw/*vibrator-cdh-popsicle`、`bin/hw/*ir-service*`、perf / audio 那一整串 `lib64/libqti-*.so` 等（全表见 `log-fail.txt`） |
| 被 `vendor_file_contexts` 规则**整条盖掉**（基线值从未生效） | 3 | `bin/dlkm_loader_mainline` 基线写 `pioneer_exec`，规则给 `dlkm_loader_exec`；`bin/tablet2multitouch` 同样 `pioneer_exec` vs `tablet2multitouch_exec`；`bin/hw/android.hardware.security.keymint-service` 基线写 `hal_keymint_default_exec`，规则给的**恰好同值**——同值也不留，留着只会让读表的人以为标签由基线决定 |
| 删掉后落到**兜底且值相同** | 9 | `etc/init/{dlkm_loader_mainline,tablet2multitouch}.rc`、`etc/init/hw/init.virt.rc`、`etc/primary_audio_policy_configuration.xml`、`etc/vintf/manifest/{android.hardware.security.keymint-service,android.hardware.vibrator-service,mapper.minigbm_upstream}.xml`（7 条都是 `vendor_configs_file`，正好等于 `etc/` 兜底）、`etc/init/android.hardware.security.keymint-service.rc` 同上、`lib64/libtablet2multitouch.so`（`vendor_file`，等于 `lib64/` 兜底） |
| 真正只有 baseline 能给 | **3** | `etc/init/cph_{disable_provision,logcat_dump,start_adbd}.rc`：基线要 `vendor_file`，而 `etc/` 兜底是 `vendor_configs_file`，删了就改值 |

## 二、根因

1. **整段是从 gold / 旧工作区模板复制的**：`cph_` 前缀（OPPO）的 rc 出现在 Xiaomi 机型上、
   `bin/daemon` / `netmgr_setup` 属于 `android.service.daemon` 模块，而 popsicle 的
   `modules.external` 只有 `["cdh_redirect"]`——模块没启用，注入树里自然没有这些文件。
2. **标签真值有三层，基线的优先级排在规则表之后**：`setxattr.go` 的 `contextFor()` 顺序是
   **`vendor_file_contexts` 规则 → 官方镜像 inode → 基线 `selinux-labels` → 内置兜底**。
   注入件（官方镜像没有）若被规则表命中，基线那条就永远不生效。`07_build_vendor/README.md`
   第 761 行把顺序写成「官方 > 基线 > 规则 > 兜底」，与代码相反，是这次没能及早发现的原因。

## 三、证据

- 07 元数据表（`img/vendor-meta-rules.txt`，557 条 dir/file/symlink）：`selinux_source=baseline`
  只有 **12 行**，其余全是 `rule` / `official` / `default`——69 条配置里真正参与决策的不到 1/5。
- 装配树逐路径 `lexists`：54 条不存在（`log-fail.txt`）。
- 两个「失效」条目的实际标签由 `dlkm_loader_exec` / `tablet2multitouch_exec` 规则给出，
  与基线写的 `pioneer_exec` 不同——照着基线读会以为 daemon 类进程跑在 `pioneer_exec` 域里。
- 镜像内 xattr 直读（`debugfs -R "ea_list <path>" vendor.img`）：`etc/init/cph_start_adbd.rc`
  的 `security.selinux = "u:object_r:vendor_file:s0"`——确实是基线给的值，不是 `etc/` 兜底的
  `vendor_configs_file`，证明保留这 3 条是必要的（`log-pass.txt` 第 5 节）。

## 四、改动

只改 1 份基线的一个字段：`vendor.selinux-labels` 由 **69 条整表清空**：

```json
"selinux-labels": {
}
```

判据（可复用）：对每条 `k:v`，先看 07 产物里有没有 `k`；没有就删。有则算「去掉基线后会落到
什么」——`vendor_file_contexts` 命中就用规则值，否则用 `defaultLabelFor(k)`；与该值相同就删，
不同就留。按这个判据，69 条里只有 3 条 `cph_*.rc` 会改值（基线写 `vendor_file`，`etc/`
兜底是 `vendor_configs_file`），其余 66 条删掉后标签一个字节都不变。

**最终这 3 条也删了**（用户决定）：`vendor_file` 与同目录的
`etc/init/android.hardware.security.keymint-service.rc` 等官方 rc 不一致——init 读 rc 用的就是
`vendor_configs_file`，当初的 `vendor_file` 是模板搬用留下的，不是运行需要。删后这 3 个文件
落回兜底、与邻居对齐（实测见下节）。

另一条更早的误判：`contextFor()` 是**规则优先于基线**，所以「基线与兜底不同」不等于
「基线有效」——`bin/hw/android.hardware.security.keymint-service` 一度被我按「兜底是
`vendor_file`、需要 `hal_keymint_default_exec`」留下，实际元数据表里它的 `selinux_source`
一直是 `rule`：规则表已经给了同值，基线那条属于重复声明。

## 五、验证

`cdh device build popsicle --from build_vendor`（07→11）重跑，与本轮开始前的元数据表
（`out/tmp/popsicle-meta-before/`）逐路径对比：

| 项 | 结果 |
| :--- | :--- |
| 条目数 | 557 → **557**（路径一个没增没减，`仅 before` / `仅 after` 均为空） |
| SELinux 值变化 | **3 条**：`etc/init/cph_{disable_provision,logcat_dump,start_adbd}.rc` \t `vendor_file` → `vendor_configs_file`（落回 `etc/` 兜底） |
| 其余 554 条标签 | **零变化** |
| `selinux_source=baseline` 行数 | 12 → **0** |
| 镜像内 xattr 直读 | `vendor.img` 与 `patch.img` 里这 3 个 rc 都是 `vendor_configs_file`，与邻居 `android.hardware.security.keymint-service.rc` 一致 |
| 构建 | `[ok] vendor.img`、`[ok] vendor_dlkm.img`、`[ok] patch.img`、`[ok] 已完成` |
| 属性未受影响 | `vendor 属性: 基线 build-prop-sets 288 项 / unsets 1 项`，与此前一致 |

## 六、遗留

1. **其他机型没清**：这类「模板复制 + 模块未启用」的死条目大概率同样存在于其余 11 份基线，
   本次只处理 popsicle。批量清理要按同一判据逐机跑一次 07 取证。
2. **README 与代码优先级**（已修）：`07_build_vendor/README.md` 第 761 行的顺序已改成
   「规则表 > 官方 inode > 基线 > 兜底」，并注明「规则表已覆盖的路径，基线写了也永远不生效」。
3. **基线 `note` 描述**（已修）：原文「标签缺失即报错退出」与 `pack.go` 实现不符（缺标签只是落到
   规则或兜底，不报错）；已改成「优先级在规则与官方 inode 之后，故本表当前为空，规则与兜底
   已能给出注入件的正确值；表里缺路径不报错，会落到规则或兜底」。
4. **这 3 个 rc 的实际作用未逐条复核**：`cph_{disable_provision,logcat_dump,start_adbd}.rc`
   现在与官方 rc 同标签（`vendor_configs_file`），未上机验证这 3 个 rc 里的服务是否正常启动；
   若某条策略需要 `vendor_file`，会表现为该服务被 `init` 拒读而静默不启动。
