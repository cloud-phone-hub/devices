# 云机共享存储未挂载 + 标准目录属主/权限与真机不一致（gold / Redmi Note 13 5G）

- 设备：`xiaomi/gold`（Redmi Note 13 5G / MT6833，镜像基线 `OS3.0.10.0.VNQCNXM`）
- 真机：`07cf5fc1784783d5`（USB）；云机：daemon 实例 2，`c46c631991f2196b-cloud`
- 日期：2026-10-31
- 结果：`files.storage` 由 **17/20 路径 `stat` 失败** 变为 **`ok=true`、20 项 `uid:gid:mode` 与真机逐项一致**
  （`0o42770` / `0o42771` / `0o100660` 全部命中，见 `stat-table.txt`）

## 一、问题

云机里 `/storage/emulated/0` 与真机行为完全不同：

1. `sm list-volumes emulated` 返回 `emulated;0 unmounted`——**共享存储没有挂载**，
   于是 `device-checker` 的 `files.storage` 20 个路径里 17 个 `stat` 失败（真机 `statFailed=0`）。
2. 手工 `sm mount 'emulated;0'` 之后路径有了，但标准目录**不存在**：
   `Alarms/DCIM/Pictures/…` 与 `Android/data`、`Android/obb` 全缺，`Pictures` 之类的私有目录
   还会被 FUSE 反向重建成 `uid=10211`（MediaProvider），与真机的 `10224` 不符。
3. 改成开机自动对齐后又踩两个坑：
   - 服务在 init `main` 阶段（约 18 s）就被拉起，那时 vold 还没挂上 emulated，脚本空转；
     等 `sys.boot_completed` 再触发时 init 回 `requested start, but it is already running (flags: 6)`，
     **只跑一次、什么都没做**。
   - 脚本对每个目录 `chown 10224:1023` + `chmod 2770` 全部返回 0，但回读恒为 `770`：
     setgid 位被内核静默清掉。

## 二、根因

### 1. 挂载：framework 不管、vold 也不主动挂

`emulated;0` 在 gold 云机上处于「system locked」态，`StorageManagerService` 不会替它挂载，
`vold` 也没有触发挂载的路径，所以必须由我们自己调 `sm mount 'emulated;0'`（该命令在
`u:r:pioneer:s0` + root 下可用）。挂载成功后 FUSE 视图 `/storage/emulated/0` 才可用，
但 **FUSE 只映射已有目录**：底层 `/data/media/0` 没有的目录不会凭空出现。

### 2. 目录：真机的标准目录是 vold/MediaProvider 建的，云机这条链没跑

真机首次开机由 MediaProvider 在 `/data/media/0` 下建 12 个标准目录（`10224:1023 2770`）
以及 `Android/data`（`1023:1078 2771`）、`Android/obb`（`1023:1079 2771`）和两个 `.nomedia`。
云机镜像里 `/data/media/0` 直接是空的，这段初始化没发生过，只能自己补。

### 3. 启动时机：`class main` 早于 vold 完成，`boot_completed` 再触发又被 oneshot 吞掉

dmesg 里第一次的时序是「main 阶段 ≈17.9 s 启动 → 挂载失败 → 35.7 s 退出」，
随后 `on property:sys.boot_completed=1` 的 `start` 被 init 以 `already running` 拒绝。
把服务标 `disabled`、只由 `boot_completed` 触发（脚本内再加一层 `sys.boot_completed` 轮询）
才是稳定时序。

### 4. setgid 需要 CAP_FSETID：缺了不会报错

`chmod 2770` 要置 `S_ISGID`，内核要求 `CAP_FSETID`；init 服务的
`capabilities` 里只写了 `CHOWN DAC_OVERRIDE FOWNER` 时，`chmod` **返回 0 但 setgid 被清掉**。
对照实验：同样一段 `mkdir + chmod 2770`，走 root 全权限通道（js 通道）回读是 `2770`，
走 init 服务回读是 `770`——差异只在 capability 集合。

## 三、证据

- `log-fail.txt`：三段失败现场——
  ① `run-00034` 云机 `files.storage statFailed=17`；
  ② `/data/local/tmp/cph-storage-align.log` 17:50:21 段（挂载成功但目录全 `770`）；
  ③ 同 log 17:55:14 段（重试 5 轮仍 `不一致 14 项`，说明不是 MediaProvider 回改，而是权限位被清）。
- `log-pass.txt`：同 log 17:58:11 段（`对齐后不一致 0 项`，`stat … 2770/2771`）、
  当次 boot 的 dmesg 服务时序、`/data/media/0` 与 FUSE 两侧 `ls -ln`、
  以及**当前实例**重新抓的 `date -u`/`sm list-volumes`/`ls -ln`（跨两小时、跨一次重启仍在位）。
- `stat-table.txt`：`device-checker files.storage` 20 条路径的真机/云机 `uid:gid:mode` 逐项对照，
  不一致 0 项（atime/ctime/ino/dev/size 之类天然噪声已单列说明）。
- 关键实验记录（本轮实测，供复现）：
  - js 通道 `mkdir /data/media/0/SetgidProbe && chmod 2770` → 5 s/15 s/40 s 回读均 `2770`；
  - init 服务（无 FSETID）同样命令 → 回读 `770`，`chmod` 退出码 0；
  - 补 `FSETID` 后同路径 → `2770`。

## 四、改动

1. 新增开机脚本 `engine/vendor/hardware/android.service.daemon/vendor/bin/cph-storage-align`
   - 诊断日志固定落 `/data/local/tmp/cph-storage-align.log`（`exec >> … 2>&1`），
     早期 logcat 会被 buffer 冲掉，落文件才留得住；
   - 先等 `sys.boot_completed=1`（90×2 s），再 40×3 s 轮询「`is_mounted` → `sm mount 'emulated;0'`」；
   - `ensure_dir()` 统一 `mkdir -p` + `chown` + `chmod`，`align_dirs()` 一次对齐
     12 个标准目录（`10224:1023 2770`）、`Android/data`（`1023:1078 2771`）、
     `Android/obb`（`1023:1079 2771`）、两个 `.nomedia`（`10224:<gid> 0660`）；
   - `count_mismatch()` 对齐后回读校验，日志里直接给出「对齐后不一致 N 项」，
     避免「chmod 返回 0 就当成功」的误判。
2. 新增 rc `engine/vendor/hardware/android.service.daemon/vendor/etc/init/cph_storage_align.rc`
   ```rc
   service cph_storage_align /system/bin/sh /vendor/bin/cph-storage-align
       class main
       user root
       group root
       seclabel u:r:pioneer:s0
       capabilities CHOWN DAC_OVERRIDE FOWNER FSETID
       oneshot
       disabled
   on property:sys.boot_completed=1
       start cph_storage_align
   ```
   `disabled` 解决「main 阶段抢跑 + boot_completed 触发被吞」；`FSETID` 解决 setgid 被清。
3. 产物：`07 build-vendor`（`vendor.img` 内 `/vendor/bin/cph-storage-align` 4252 B、
   `/vendor/etc/init/cph_storage_align.rc` 1237 B）→ `09 build-patch` →
   `11 pack-vda` 发布到 `engine/out/dist/xiaomi/gold/OS3.0.10.0.VNQCNXM/`。
   本轮 `patch.img` sha256 前缀 `304f3a9c9bd2`，`vda-recipe.txt` = `vda-d44b6b713de9`。

## 五、验证

- 重启实例 2 后（本次 boot，dmesg）：
  `processing action (sys.boot_completed=1)` → `starting service 'cph_storage_align'`
  → `exited with status 0 … took 0.582000 seconds`，即**只跑一次、0.58 s 完成**。
- 诊断日志：`启动（boot_completed=1）` → `emulated;0 已挂载（第 1 次尝试）` →
  `对齐后不一致 0 项` → `stat Alarms: 10224:1023 2770` … → `完成（失败 0 项）`。
- 宿主侧 `dumpsys`/`ls -ln` 与 FUSE 侧 `/storage/emulated/0` 两者一致（`drwxrws--- 2 10224 1023`）。
- `device-checker`（`run-00039`，18:10）云机 `files.storage ok=true, statFailed=0`；
  与真机逐项对照见 `stat-table.txt`：20/20 路径 `uid:gid:mode` 完全相同。

## 六、遗留

- 云机 `/data/media/0` 比真机多一个 `Pic2`（`1023:1023`）、`MIUI` 属主是 `10211` 而非 `10224`；
  这些不在 `files.storage` 的 20 条检查路径里，属 MIUI 侧遗留目录，未动（避免动到已装应用的数据目录）。
- 服务只在 `boot_completed` 跑一次：若后续有应用在运行期把目录删掉（不会自动重建），
  需要重启实例或手工执行 `/vendor/bin/cph-storage-align`。
- 本次验证（`run-00039`）当时靠一套临时的宿主 TCP 代理把 `dev.checker` 顶上线
  （云机 emulated RIL `SETUP_DATA_CALL` 返回 `INSUFFICIENT_RESOURCES`、slirp `restrict=on`）。
  该手段**已全部撤除**：真正的通路是引导时把 Wi-Fi 开起来接 `CDH-A`，见
  `../issue-gold-wifi-disabled/solution.md`（#100）；`run-00041` 就是走 Wi-Fi 直连复跑的，
  `files.storage` 结论不变（`st_mode/st_uid/st_gid` 差异 0 项）。
