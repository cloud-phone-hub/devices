# vold 的 netlink socket 被拒 → `Exiting(13)` 死循环

- 机型：samsung / Galaxy S25 Ultra（`SM-S9380`，`ss9380`，OneUI Android 16）
- 发现时间：2026-10-30
- 盘集：build7 → build8
- 状态：已解决

## 一、问题

实例起来后 `logcat` 完全不可读：`vold` 以毫秒级频率输出下面两行，把 main buffer
整块冲掉。

```
I vold    : Socket created with fd:-1
E vold    : Exiting(13)
```

后果有两层：

1. `vold` 起不来 → storage 栈（`/data`、`/metadata` 相关）初始化不完整；
2. 日志被冲掉，**后续所有卡点都看不到**（此前一直抓不到
   `PersistentDataBlockService` 与 WiFi 的真实报错，就是被它淹掉的）。

## 二、根因

`dmesg` 去重统计后只有一条 avc，重复 253 次以上：

```
avc: denied { create } for pid=290 comm="vold"
     scontext=u:r:vold:s0 tcontext=u:r:vold:s0 tclass=netlink_socket
```

即 `vold` 建自己的 netlink socket（uevent 监听）就被拒，`main` 直接返回 13。

权限归属错位：lineage 把这条写在 **plat** 的 `vold.te`
（`allow vold self:netlink_socket create_socket_perms;`），而本机 plat 是官方 OneUI 的
`plat_sepolicy.cil`（`vers=202504`），三星把这条挪到了 **vendor** 侧——
官方 `vendor_sepolicy.cil` 第 18458 行：

```cil
(allow vold_202404 self (netlink_socket (read write create bind)))
```

本机 vendor 侧策略是 lineage 基底，于是两边都没有 → 拒绝。

## 三、证据

- 失败态：`log-fail.txt`（刷屏原文 + avc 统计 + plat/vendor 双侧缺失的对照）
- 通过态：`log-pass.txt`（`init.svc.vold=running`、`comm="vold"` 计数 0、
  `Socket created with fd:-1` 只剩极早期 2 条）

## 四、改动

`devices/manifest/samsung/ss9380/sepolicy/vendor_sepolicy.cil` 追加第二段（原句取自官方）：

```cil
(allow vold_{{vers}} vold_{{vers}} (netlink_socket (read write create bind)))
```

`{{vers}}` 由 07 步骤按 `plat_sepolicy_vers.txt`（本机型 `202504`）展开。
权限集合与官方完全一致，不额外扩张。

## 五、验证

1. `07_build_vendor`→`11_pack_vda` 重跑通过（build8，`patch.img` sha256 `e8a9bf71183e`）；
2. `debugfs -R "cat /vendor/etc/selinux/vendor_sepolicy.cil"` 确认片段落到第 4840 行；
3. 新实例（index 2）起来后：
   - `getprop init.svc.vold` → `running`
   - `dmesg | grep -c 'comm="vold"'` → `0`
   - `logcat | grep -c 'Socket created with fd:-1'` → `2`（仅极早期）
4. 关键收益：main buffer 不再被冲，后续卡点（PDB 超时、WiFi 属性）才第一次可见。

## 六、遗留

- 本机型**没有** `precompiled_sepolicy`，init 现场用 `/system/bin/secilc` 编译，
  所以改片段只需重打 vendor + 重启（不必重编预编译策略）。
- 同类「plat 权限被三星挪到 vendor」的条目还有多处，逐条见本目录 `README.md`。
