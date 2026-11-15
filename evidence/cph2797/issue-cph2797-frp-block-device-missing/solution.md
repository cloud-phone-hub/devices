# FRP 承载块设备缺失 → PersistentDataBlockService 超时崩掉 system_server（cph2797）

- 机型：oppo / OPPO Find X9（`CPH2797`，`cph2797`，ColorOS 16.0.8.301 / MT6993，Android 16）
- 发现时间：2026-11-04
- 盘集：本轮新引擎流水线首轮（实例 1）
- 编号：#153
- 状态：已闭环

## 一、问题

cph2797 首次起机 `sys.boot_completed` 永不置位，`boot_check.py` 判
`[fail] 超时 600s 未出现 sys.boot_completed=1（当前日志 71314 行）`。
qemu.log / logcat crash buffer 里 system_server 以 `*** FATAL EXCEPTION IN SYSTEM PROCESS`
反复自杀，栈固定是

```
java.lang.RuntimeException: Failed to boot service com.android.server.pdb.PersistentDataBlockService:
    onBootPhase threw an exception during phase 500
Caused by: java.lang.IllegalStateException: Service PersistentDataBlockService init timeout
    at PersistentDataBlockService.waitForInitDoneSignal(PersistentDataBlockService.java:319)
    at PersistentDataBlockService.onBootPhase(PersistentDataBlockService.java:308)
```

600 s 窗口内同一栈出现 18 次（pid 762 → 3467 → …，间隔约 31 s），整机困在
system_server ↔ zygote 重启循环里。「init timeout」只是笼统的时间戳结论，看不出卡在哪一步。

## 二、根因

1. **等待链**（Android 16 平台，与 15r / m22 / ss9380 / popsicle 完全同一条）：
   `onStart()` 把初始化丢进 `SystemServerInitThreadPool`，lambda 末尾才
   `signalInitDone()`；`onBootPhase(500)` 是 `await(10, SECONDS)`，超时即抛
   `IllegalStateException` 崩掉 system_server。lambda 第一步
   `enforceChecksumValidity()` 要打开 `ro.frp.pst` 指向的块设备做校验和读写，
   这一步抛异常 → 信号永不来 → 必崩。

2. **本盘没有该节点**。厂商 `vendor/build.prop` 有两行同键（值一致，MTK 口径、
   无 `bootdevice` 前缀）`ro.frp.pst=/dev/block/by-name/frp`（原件第 374、445 行），
   而盘集 GPT 来自 lineage 底盘、没有 `frp` 分区
   （`grep -ic frp out/dist/oppo/cph2797/16.0.8.301_EX01/vda-info.txt` = 0），
   运行态 `ls -l /dev/block/by-name/frp` → `No such file or directory`。

3. **光补软链还有两道闸**（同链条机型已实测）：软链指向 misc(`vda3`) 后，节点标签由
   ueventd 按 `/dev/block/vda*` 命名规则给成 `misc_block_device`，而 plat 只给
   system_server 开了 `frp_block_device`；`vda3` 出厂是 `0600 root:root`，
   system_server 以 system 身份读写，DAC 也不过。缺任何一道都是同一条 10 s 超时链。

## 三、证据

- 失败态：`log-fail.txt`
  - 崩溃栈原文与 18 次计数、pid 递增节奏；
  - `ro.frp.pst` 两行的行号、GPT 无 frp 分区的核对、run1 运行态节点缺失；
  - 等待链与「软链不够」的机制说明。
- 通过态：`log-pass.txt`
  - 三件的行数与 md5、构建日志的落位与「保留不改写」留痕、盘集 patch.img sha256；
  - **从最终 `vendor.img` 用 debugfs 读回** rc / ueventd / cil 三件实文（cil 第 4665 行，
    `{{vers}}` 已替换成 `202504`）；
  - VM 内 `/dev/block/by-name/frp -> /dev/block/vda3`、`PersistentDataBlockService init
    timeout` 计数 18 → **0**、`FATAL EXCEPTION IN SYSTEM PROCESS` 0、
    `sys.boot_completed=1` 落在 110.99 s、判定 `[pass]`。
- 单变量旁证：实例 2（FRP 已修、GPU 收口未修）里 PDB 计数已归 0，boot 仍失败但卡在
  libEGL 断言 → 证明本条修法独立生效，也才暴露出 #154。

## 四、改动

照抄同链条范式，三件全部落清单（不改阶段代码）：

| 件 | 内容 |
| --- | --- |
| [vendor/etc/init/frp_symlink.rc](file:///Volumes/work/cloud-phone-hub/devices/manifest/oppo/cph2797/vendor/etc/init/frp_symlink.rc) | `on post-fs-data` 建 `by-name` 目录并 `symlink /dev/block/vda3 /dev/block/by-name/frp` |
| [vendor/etc/ueventd.mt6993.rc](file:///Volumes/work/cloud-phone-hub/devices/manifest/oppo/cph2797/vendor/etc/ueventd.mt6993.rc) | `/dev/block/vda3 0660 root system`；**接管件**，必须自己写回 `import /vendor/etc/ueventd.virt.rc`（07 的 `placeVendorUeventdRC` 见同名件即让位，漏 import 会让 `/dev/hvc*` 回落 0600、Remote KeyMint EACCES） |
| [sepolicy/vendor_sepolicy.cil](file:///Volumes/work/cloud-phone-hub/devices/manifest/oppo/cph2797/sepolicy/vendor_sepolicy.cil) | `(allow system_server_{{vers}} misc_block_device_{{vers}} (blk_file (open read write append getattr ioctl map lock)))`，权限集合照抄 m22 / ss9380 |

选「软链到 misc」而不是改 `ro.frp.pst` 属性：属性值来自厂商原件，改掉就多一处与原厂不一致的
手工差异，且 `frp` 语义只是「有块设备能读写」；misc(`vda3`) 在本盘未被使用，是同一链条
四台机型已经验证过的落点。

## 五、验证

1. `device build cph2797 --from build_vendor`（后续 `--from build_odm` 又复跑一次，
   06→11 全链通过，patch.img sha256 `9a82ee2261c7`）；
2. debugfs 读回最终镜像里的三件实文（见 log-pass §2）；
3. 实例 2 / 实例 3 两次起机：PDB 超时计数 0、system_server FATAL 0；
4. 实例 3 判 `[pass] 引导完成（日志 17248 行）+ 截图非黑（平均亮度=4.3 峰值=255）`，
   `sys.boot_completed=1` @ 110.99 s，launcher(4620) / systemui(5017) / surfaceflinger(663) 存活；
5. 停机。

通过帧已随本目录归档：`desktop-pass.png`（1264x2760，平均亮度 4.3 / 峰值 255，画面为
ColorOS 开机向导页）。

## 六、遗留

- `ro.frp.pst` 指向 misc 意味着 PDB 会写 `vda3` 的元数据区；云机不需要真实 FRP，
  只要 `enforceChecksumValidity()` 读写通过即可，不追求与真机 FRP 语义一致。
- 首轮引导焦点在 `com.google.android.setupwizard/.SetupWizardActivity`（判定截图因此偏暗，
  平均亮度 4.3），尚未跑完开机向导；后续要验收桌面内容需另行处理 SetupWizard。
- `/odm/bin/hw/{esepower,secure_element}-service` 与 `/odm/bin/hw/android.hardware.nfc-service.nxp`
  因缺 `libMcClient.so` / `libmemunreachable.so` 持续 linker 崩溃循环（三轮都在，不在引导链上，未处理）。
