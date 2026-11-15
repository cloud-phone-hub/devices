# ueventd 板级入口缺失 + GPT 无 frp 分区 → KeyMint EACCES / PDB phase 500 崩

- 机型：xiaomi / Xiaomi 17 Pro Max（`popsicle`，HyperOS 3 `OS3.0.319.0.WPBCNXM`，Android 16，lineage-23.2 + kernel 6.12.81）
- 发现时间：2026-11-02
- 盘集：popsicle 首次整线，实例 5（KeyMint）与实例 7（PDB）
- 状态：已解决

## 一、问题

补完 HAL 清单后，两个与「设备节点」相关的卡点先后出现，症状完全不同但都在 boot 早期：

1. **实例 5**：`keymint` 服务启动即崩，keystore2 起不来，vold 卡在
   `IKeystoreService`，整机停在开机画面（`FATAL EXCEPTION IN SYSTEM PROCESS` = 0，
   所以特别难定位）：

   ```
   [1.291] android.hardware.security.keymint-service.remote: Could not connect to keymaster: Permission denied
   [1.456] servicemanager: Caller(pid=252,uid=0,sid=u:r:vold:s0) Since 'android.system.keystore2.IKeystoreService/default' could not be found …
   ```

2. **实例 7**：system_server 越过 service 构造阶段，在 phase 500 抛 `Failed to boot service`：

   ```
   E PersistentDataBlockService: partition not available?
   E PersistentDataBlockService: java.io.FileNotFoundException:
       /dev/block/bootdevice/by-name/frp: open failed: ENOENT
   E AndroidRuntime: RuntimeException: Failed to boot service
       com.android.server.pdb.PersistentDataBlockService: onBootPhase threw an exception during phase 500
   ```

## 二、根因

两个卡点是同一条链——**init / ueventd 的板级入口只认 `ro.hardware`，而 lineage 基座把云机
需要的规则放在 `*.virt.*` 里**——加上一个 GPT 差异：本盘没有 frp 分区。

### 1) ueventd 板级入口（KeyMint EACCES）

- 05 按清单 `board` 改写 cmdline：`androidboot.hardware=canoe` → `ro.hardware=canoe`；
- 基底 `/vendor/etc/ueventd.rc` 只有两行 import：
  `ueventd.${ro.board.platform}.rc` 与 `ueventd.${ro.hardware}.rc`；
- `/dev/hvc* 0666 system system` 写在 lineage 基底的 `etc/ueventd.virt.rc` 里，名字不匹配
  → **整份不解析**；
- `/dev/hvc3` 于是回落内核默认 `0600 root:root`，Remote KeyMint 的 service 段是
  `user nobody`，`SharedFD::Open(path, O_RDWR)` 直接 EACCES → `LOG(FATAL)` → SIGABRT；
- **日志里没有任何针对 `/dev/hvc3` 的 chr_file avc denied**，这是判别「DAC 而非 SELinux」的
  关键（标签其实早在 `vendor/etc/selinux/vendor_file_contexts` 里）。

### 2) FRP 设备节点（PDB phase 500）

- 厂商属性 `ro.frp.pst=/dev/block/bootdevice/by-name/frp`（HyperOS 3 把它放在 system/odm 侧，
  `vendor/build.prop` 里没有；07 的 `applyVendorBuildProps` 会在装配树 vendor/build.prop 里补上同一行）；
- 本盘 GPT 来自 lineage 底座，只有 `/dev/block/by-name`（BIOS/EFI/boot/firmware/grub_boot/
  metadata/misc/persist/super/vdc），**没有 frp 分区，`bootdevice/by-name` 目录整个不存在**；
- Android 16 的 `PersistentDataBlockService.onStart` 在初始化线程里依次跑
  `enforceChecksumValidity → computeDigestLocked`，第一步 `open(ro.frp.pst)` 抛
  `FileNotFoundException` 冒泡到 lambda，`signalInitDone()` 永不执行，`onBootPhase(500)`
  等 10 s 后抛 `Failed to boot service` → system_server 自杀重启；
- 与 Android 15 的 ss9380/m22 不同：那边只打警告/等不到信号，栈是
  `IllegalStateException: Service PersistentDataBlockService init timeout`；Android 16 直接
  把底层异常抛上来，**栈里就能看到 ENOENT**，反而好定位。

## 三、证据

- 失败态：`log-fail.txt`（实例 5 的 KeyMint EACCES + keystore2 等待栈；实例 7 的 PDB 栈与
  `ls /dev/block/bootdevice/by-name` 现场）
- 通过态：`log-pass.txt`（`/dev/hvc*` 权限已就位、`IKeyMintDevice/default` 注册成功、
  `/dev/block/bootdevice/by-name/frp -> /dev/block/vda3`、`sys.boot_completed=1`、
  `FATAL EXCEPTION IN SYSTEM PROCESS` = 0）
- 先例对照：`devices/evidence/ss9380/issue-pdb-frp-block-device/solution.md`（同一 PDB 卡点的
  Android 15 版本）、`manifest/flyme/m22/vendor/etc/ueventd.qcom.rc`（hvc 与 vda3 的注释取证）

## 四、改动

1. **代码（通用收口）** `engine/build/07_build_vendor/fstab.go` 新增
   `placeVendorUeventdRC`：按 `plan.Board` 生成 `vendor/etc/ueventd.<board>.rc`，内容
   `import /vendor/etc/ueventd.virt.rc`；上层已提供同名文件时保留不覆盖，`board` 为空告警。
   与既有的 `placeVendorInitRC`（`init.virtio.rc → init.<board>.rc`）同属「板级入口收口」，
   在 `assemble.go` 步骤 6 紧挨着调用。
2. **单测** `engine/build/07_build_vendor/fstab_test.go` 新增 3 例：
   `TestPlaceVendorUeventdRCWritesBoardEntry`（生成并 import）、
   `…KeepsExistingEntry`（静态树提供时不覆盖）、`…SkipsWithoutVirtBase`（无 virt 基底不生成）。
3. **机型静态树**（popsicle 新建 `manifest/xiaomi/popsicle/vendor/`）：
   - `etc/ueventd.canoe.rc`：`import /vendor/etc/ueventd.virt.rc` +
     `/dev/block/vda3 0660 root system`（07 看到同名文件会保留，不再生成）；
   - `etc/init/frp_symlink.rc`：`on post-fs-data` 建 `bootdevice/by-name` 并把 `frp`
     软链到 `vda3`（misc 分区，GRUB 引导不消费 BCB）。
4. **机型策略**（popsicle 新建 `manifest/xiaomi/popsicle/sepolicy/vendor_sepolicy.cil`）：
   ```cil
   (allow system_server_{{vers}} misc_block_device_{{vers}} (blk_file (open read write append getattr ioctl map lock)))
   ```
   节点标签是 `misc_block_device`（按 `/dev/block/vda*` 命名规则打），而 plat 只给
   system_server 开了 `frp_block_device`；权限集合照抄 m22 / ss9380。

## 五、验证

1. `go test ./07_build_vendor/ -run "Ueventd|InitRC|Fstab" -v` 10 个用例全 PASS，`gofmt -l` 无输出；
2. 重建日志（`.tmp/popsicle-frp.log`）：`写 …/vendor/etc/init/frp_symlink.rc`、
   `写 …/vendor/etc/ueventd.canoe.rc`、`保留 ueventd.canoe.rc（已由上层提供），不改写`、
   `追加 …/vendor/etc/selinux/vendor_sepolicy.cil <- manifest/xiaomi/popsicle/sepolicy`；
3. 设备侧：`ls -l /dev/block/bootdevice/by-name/` 出现 `frp -> /dev/block/vda3`，
   `/dev/block/vda3` 为 `brw-rw---- root system u:object_r:misc_block_device:s0`；
4. `service list` 有 `android.hardware.security.keymint.IKeyMintDevice/default`、
   `android.hardware.keystore` 链路正常，`grep -c 'Could not connect to keymaster'` = 0；
5. `sys.boot_completed=1`，`grep -c 'Failed to boot service com.android.server.pdb'` = 0。

## 六、遗留

- `frp_symlink.rc` 用 misc(vda3) 承载 FRP 数据块，语义上等价于真机独占小分区；云机不需要
  真实 FRP，只要 `enforceChecksumValidity()` 读写通过即可；
- ueventd 的通用收口只覆盖「lineage 基底有 `ueventd.virt.rc`」这一种情况；若将来基底改名为
  `ueventd.<something>.rc`，`legacyUeventdEntry` 常量需同步，否则会静默不 import；
- 板级入口这块还有第三处同类风险未收口：`/vendor/etc/init/*.rc` 里若出现
  `import /vendor/etc/init/hw/init.virt.rc` 以外的 `*.virt.rc` 引用，需逐条确认（当前
  popsicle 只在 `init.canoe.rc` 里 import `init.virt.rc`，已随 `placeVendorInitRC` 覆盖）。
