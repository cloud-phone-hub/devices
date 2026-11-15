# PersistentDataBlockService 在 phase 500 卡 10 s → system_server 崩

- 机型：samsung / Galaxy S25 Ultra（`SM-S9380`，`ss9380`，OneUI Android 16）
- 发现时间：2026-10-30
- 盘集：build7 → build8
- 状态：已解决

## 一、问题

system_server 启动到 `PHASE_ACTIVITY_MANAGER_READY`（500）时崩，crash buffer：

```
E AndroidRuntime: *** FATAL EXCEPTION IN SYSTEM PROCESS
E AndroidRuntime: java.lang.IllegalStateException: Service PersistentDataBlockService init timeout
```

随后 zygote 重启循环，`sys.boot_completed` 永不置位。

难点在于「init timeout」只是个笼统的时间戳结论，看不出卡在哪一步；而当时 `vold`
正以毫秒级刷屏（见 `issue-vold-netlink-socket/`），把 logcat main buffer 冲掉了，
只能靠反编译 + 静态取证定位。

## 二、根因

反编译 `system/framework/services.jar` 的
`Lcom/android/server/pdb/PersistentDataBlockService;`，等待链是：

- `onStart()`：`SystemServerInitThreadPool.submit(lambda, "PersistentDataBlockService.onStart")`
- `onBootPhase(500)`：`mInitDoneSignal.await(10, SECONDS)`，超时抛
  `IllegalStateException("Service PersistentDataBlockService init timeout")`
- `signalInitDone()` 只在 `PersistentDataBlockService$$ExternalSyntheticLambda0.run()`
  末尾调用，lambda 依次执行：
  `enforceChecksumValidity()` → `automaticallyDeactivateFrpIfPossible()` →
  `setOldSettingForBackwardCompatibility()` → `publishBinderService("persistent_data_block")` →
  `signalInitDone()`

→ lambda 里任何一步抛异常，信号永远不来 → 10 s 后超时。

本机型卡在**第一步**：官方 vendor `build.prop` 第 144 行
`ro.frp.pst=/dev/block/bootdevice/by-name/frp`，而本盘 GPT 来自 lineage 底盘、没有
`frp` 分区。机型静态树已用 `vendor/etc/init/frp_symlink.rc` 把它软链到 misc（`vda3`）：

```
/dev/block/bootdevice/by-name/frp -> /dev/block/vda3
/dev/block/vda3  brw-rw---- root system u:object_r:misc_block_device:s0
```

plat 只给 system_server 开了 `frp_block_device`，本节点的实际标签是
**`misc_block_device`**（ueventd 按 `/dev/block/vda` 命名规则打标签），于是
`open()` 被拒 → 抛异常 → 信号不来。

## 三、证据

- 失败态：`log-fail.txt`（崩溃栈原文、反编译出的等待链、`ro.frp.pst` 与块设备标签、
  以及对「fkeymaster 依赖」这一错误方向的排除：`services.jar` 的 dex 里
  `strings | grep -i fkey` 为 0 条）
- 通过态：`log-pass.txt`（`PersistentDataBlockService init timeout` 计数 0，
  崩溃栈换成下一个卡点 WiFi 属性）

## 四、改动

`devices/manifest/samsung/ss9380/sepolicy/vendor_sepolicy.cil` 追加第三段：

```cil
(allow system_server_{{vers}} misc_block_device_{{vers}} (blk_file (open read write append getattr ioctl map lock)))
```

与 `flyme/m22` 同一修法（m22 缺这条时卡在完全相同的崩溃栈），权限集合照抄 m22。

## 五、验证

1. `07_build_vendor`→`11_pack_vda` 重跑通过（build8，`patch.img` sha256 `e8a9bf71183e`）；
2. `debugfs` 读回 `/vendor/etc/selinux/vendor_sepolicy.cil`，第三段落在第 4864 行；
3. 新实例（index 2）起来后 `logcat -b crash -d | grep -c 'PersistentDataBlockService init timeout'` → `0`；
4. 连带效果：system_server 越过 phase 500，暴露出下一个卡点
   `vendor.wifi.dualconcurrent.interface`（日志刷屏停了之后才第一次可见）。

## 六、遗留

- `ro.frp.pst` 指向 misc 分区意味着 PDB 会写 `vda3` 的元数据区；云机场景无需真实 FRP，
  只要 `enforceChecksumValidity()` 能读写通过即可，不追求与真机 FRP 语义一致。
- `/system/bin/fabric_crypto` 每 5 s SIGABRT（`Not found Fkeymaster service`）与本卡点无关，
  尚未处理，暂不影响 boot_completed（另见 README 索引的后续条目）。
