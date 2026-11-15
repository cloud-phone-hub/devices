# gold 首启直达 launcher（OOBE 绕过）—— 里程碑证据

机型：xiaomi/gold（Redmi Note 13 5G）
清单：`gold_images_OS3.0.10.0.VNQCNXM_20260715.0000.00_15.0_cn_a9729c6694`
产物：`engine/out/dist/xiaomi/gold/OS3.0.10.0.VNQCNXM`
失败态：`log-fail.txt`（06:07，实例 17）
通过态：`log-pass.txt` + `launcher.png`（06:55 截图，实例 19，全新 data；
实例 18 为同一 vendor/system 内容的先行验证）

## 1. 问题

空白 data 首次启动，系统分区可引导（`sys.boot_completed=1`），但界面停在
`com.miui.huanji/.provision.ui.ProvisionCTAActivity`（迁移数据页）或 OOBE
向导页，`device_provisioned=0`，永远进不了 launcher。

## 2. 根因

两条根因叠加，第二条在补上第一条后才暴露：

1. **清单丢失金实例的 provisioning 补丁。**
   最终 patch.img 中没有 `/vendor/bin/setup_display.sh` 与
   `/vendor/etc/init/cph_disable_provision.rc`，没有任何服务去置
   `device_provisioned` / `user_setup_complete` 或禁用
   `com.android.provision`，首启必然进向导。

2. **SELinux 拒绝 cmd 的 ShellCallback 回程，`cmd` 永不退出。**
   `/system/bin/cmd` 运行在调用者进程（此处 `auto_provision` 的
   `seclabel u:r:pioneer:s0`）内，`cmdMain` 通过 `ShellCallback` 在该进程注册
   binder 节点；命令执行完，system_server 回程 `call` 该节点触发
   `onCommandFinished`。该调用被 SELinux 拒绝（system_server 侧 enforcing）：

   ```
   avc: denied { call } for comm="binder:638_16"
     scontext=u:r:system_server:s0 tcontext=u:r:pioneer:s0
     tclass=binder permissive=0
   ```

   pioneer 自身是 permissive 域，但判定主体是 **system_server**，permissive
   位救不了反向调用。于是 `cmd settings put` 执行完仍停在
   `cmdMain → pthread_cond_wait`，auto_provision 卡在第一行，
   `device_provisioned` 恒为 0。同类 `cmd window`、`cmd deviceidle` 全部
   rc=137（被外部强杀），而 `content update`、`svc`、`settings get` 正常。

## 3. 证据

- `log-fail.txt`：
  - 补丁文件缺失（`ls` 报 No such file or directory）与空白 data 首启进入向导；
  - 补上补丁后 `init.svc.auto_provision=running` 长时间不变、
    `device_provisioned=0`；
  - `debuggerd -b` 取到的 `cmd` 原生栈停在
    `cmdMain → pthread_cond_wait`；
  - 对照实验：`content update` rc=0 且值生效；`cmd` 各子命令 rc=137；
  - qemu.log 中的 binder avc 拒绝原文；
  - finddevice 锁链与 framework 重启（失败态的连锁噪声）。
- 产物取证：`log-pass.txt` 第 0 节列出 patch.img/vda.img 的 sha256、patch.img
  内两文件与 CIL 的抽取校验（字节一致、标签正确）。
- 运行态取证：`log-pass.txt` 第 1–3 节，`cmd` rc=0、provisioned=1、
  `mCurrentFocus=com.miui.home/.launcher.Launcher`、binder avc 计数 0。
- 截图：`launcher.png`（1080x2400 桌面）。

## 4. 改动

1. `manifest/xiaomi/gold/vendor/bin/setup_display.sh`（新，0755）
   sha256 `972528860b90d5a0d9899d7aa4f27773e5c53253c17674ee0eacc877112878c8`
   —— 与金实例构建树中同名文件逐字节一致；重试置
   `screen_off_timeout / stay_on_while_plugged_in / lockscreen.disabled /
   user_setup_complete / device_provisioned`，禁用 provision 与 bugreport，
   `svc power stayon true`、`cmd deviceidle disable` 等。
2. `manifest/xiaomi/gold/vendor/etc/init/cph_disable_provision.rc`（新，0644）
   sha256 `9e7413fccd0467b0b7a5a5d3490659e3b9088adc19701f5e0d9863de237f5c83`
   —— `auto_provision`（oneshot，root，`seclabel u:r:pioneer:s0`）与
   `keep_awake` 两个服务，`on property:sys.boot_completed=1` 启动。
3. `manifest/xiaomi/gold/sepolicy/vendor_sepolicy.cil`（追加）
   ```cil
   (allow system_server_{{vers}} pioneer (binder (call transfer)))
   ```
   构建时按 `plat_sepolicy_vers.txt` 替换为 `system_server_202404`；解决根因 2。
   同一文件此前的 `fd use`（cmd/dumpsys 的 stdio fd）与 `fifo_file write`
   （dumpsys 输出 pipe）继续保留。
4. 删除诊断件 `vendor/bin/cdh_logcat.sh`、`vendor/etc/init/cdh_logcat.rc`，
   从最终产物确认已不存在。

## 5. 验证

1. 全链重跑 07（build-vendor）→ 09（build-patch）→ 11（pack-vda，带
   `CDH_LINEAGE_ZIP`、`CDH_BUILD_ID`），最终 dist：
   - `device.json` id = `gold_cn_OS3.0.10.0.VNQCNXM`；
   - patch.img sha256 `b0f5231be108…`，vda.img sha256 `a71d5c144e9d…`；
   - debugfs 从 patch.img 抽件：两文件字节一致且 mode 正确
     （0755 system_file / 0644 vendor_file），CIL 中规则存在。
2. 回退内核诊断提交 `0a7728888fdb`（保留 `f377db7883de`/`3d90bc8daa1d`）后，
   重跑 02→04→07→09→11，最终 boot.img sha256 `4b55ff65422a…`、内核
   `6.6.127-4k-cdh`（源码 HEAD `8589080b584f`）；`cdh_sig` 探针输出计数 0。
3. `POST /api/create {"index":0}` 新建实例 19（**全新 data**，非复用旧实例），
   等待约 100 秒：
   - `sys.boot_completed=1`，`auto_provision` 两次执行均 exit 0；
   - `settings get global device_provisioned` = 1，
     `settings get secure user_setup_complete` = 1；
   - `timeout -s KILL 6 cmd settings get global device_provisioned` → `1`，rc=0；
     `cmd deviceidle get deep` → `ACTIVE`，rc=0；
   - qemu.log 中 `system_server → pioneer tclass=binder` 的 avc 计数 0；
   - logcat 中 `Failed transaction (2147483646)` 计数 0；
   - `mCurrentFocus=com.miui.home/com.miui.home.launcher.Launcher`；
   - 截图 `launcher.png` 为桌面。
4. 结论：空白 data 首启直接进入 launcher，OOBE 被绕过；两条根因均已闭环，
   最终产物（含回退诊断后的内核）已在全新实例上复验。

## 6. 遗留

- ~~`setup_sound_card` 在 VM（无声卡）下 `std::stoi` 抛异常 abort~~ **已消失**：
  `../issue-qemu-hda-sound-card/` 给 QEMU 接上 intel-hda 后，该 oneshot 的输出变为
  `I setup_sound_card: No recognized sound card found, Exiting.`，不再 abort
  （2026-10-27 在实例 4 上复核）。
- pioneer 读 `/vendor/bin/setup_display.sh` 仍有 permissive=1 的 avc
  （标签 system_file），与金实例行为一致；按“非侵入、与金实例对齐”原则
  未额外放宽策略。
- 启动早期 finddevice provider 等待与线程级 SIGSEGV 仍偶发，但
  system_server pid 未变、launcher 正常；如后续长稳测试暴露问题另行固证。
- CIL 版本化影子名依赖 `plat_sepolicy_vers.txt`（当前 `202404`），底包升级
  后由构建阶段自动替换，无需改本文件。
- 诊断内核已回退；user 态 `uname -r` / `/proc/version` 仍显示
  `6.6.89-android15-8-…`，这是 `cdh_device_info` 模块的 uts 伪装
  （`devices/modules/device_info/uts/uts.c`），真实内核以串口日志与
  `/dev/kmsg` 为准（`6.6.127-4k-cdh`）。
- 实例 19 保留运行供人工查看；其余调试实例（3–18）与旧实例 1、
  cdh-forensics 容器已清理。
