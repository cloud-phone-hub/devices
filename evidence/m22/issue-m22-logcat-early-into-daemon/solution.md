# #71 m22 early 版 logcat 取证并入 android.service.daemon

日期：2026-10-30　设备：flyme/m22（Android 16，基底 lineage-23.2）

## 1. 问题

`manifest/flyme/m22/vendor/etc/init/cph_logcat_dump_early.rc` 与模块
`android.service.daemon` 自带的 `vendor/etc/init/cph_logcat_dump.rc` 是**同一件事的两份**：

| 项 | 机型层 early 版 | 模块版 |
| --- | --- | --- |
| service | `logcat_dump_early` | `logcat_dump` |
| 命令 | `/system/bin/sh /vendor/bin/cph-logcat-dump` | 同 |
| seclabel | `u:r:pioneer:s0` | 同 |
| 输出 | `/dev/ttyAMA0`（脚本内固定） | 同 |
| 触发 | `on early-boot` | `sys.boot_progress.systemserver=1`、`sys.boot_completed=1` |

两轮改动之后，m22 静态树 `etc/init` 只剩 `frp_symlink.rc` 这一个真正的机型事实
（`cph_logcat_dump_early.rc` 是历史取证脚手架，`init.qcom.rc` 已在 #70 下线），
这份副本成了唯一的例外。

## 2. 根因

它当初写在机型层而不是模块层，是因为要解决一个**只在 m22 出现**的场景：
zygote / SurfaceFlinger / SystemServer 反复 SIGABRT 时，`sys.boot_progress.systemserver`
与 `sys.boot_completed` 永远不会被置位，模块版那两个触发点全程不生效，取证断链。
于是就近在机型静态树加了一份 `on early-boot` 的独立服务。

但「触发时机」并不构成机型事实：脚本 `/vendor/bin/cph-logcat-dump` 在模块里、
seclabel `u:r:pioneer:s0` 是 CDH 内核补丁的 forced permissive 全局域（模块的
`cph_daemon_service.rc` 也在用）、输出节点由 pl011 驱动无条件创建。
按 `vendor/build/README.md` 的切分口径，rc 里只放设备无关的服务定义与启动门控，
机型层不放同名/同能力副本——两份同时进装配树时后层覆盖且难以追溯。

## 3. 证据

- `/dev/ttyAMA0` 在两个机型上都存在，不是 m22 专有：
  - m22 的 `vendor_boot` cmdline 只有 `console=tty0`，但 **GRUB 引导的实际 cmdline 含
    `console=ttyAMA0`**（`../issue-m22-instance31-binder-stall/dmesg-31.txt:52`）；
    pl011 在 `dmesg-31.txt:176-177` probe 成 `ttyAMA0` 并 "legacy console enabled"；
  - gold 的 `vendor_boot-info.txt:6` 自带 `console=tty0 console=ttyAMA0`。
- early 版在 m22 上确实跑过（不是死文件），init 日志三段齐全：
  `dmesg-31.txt:731` `init: Parsing file /vendor/etc/init/cph_logcat_dump_early.rc...` →
  `:2277` `processing action (early-boot) from (/vendor/etc/init/cph_logcat_dump_early.rc:19)` →
  `:2278-2280` `starting service 'logcat_dump_early'... pid 787`；随后 `:2288-2298`、`2562-2578`
  有 `scontext=u:r:pioneer:s0` 对 `path="/dev/ttyAMA0"` 的 `open/write/ioctl/getattr`
  avc（`permissive=1`，能写但会刷 denied）。
- 两份 rc 逐行对照后**只有触发点不同**（见第 1 节表格），命令、seclabel 完全一致。
- 模块已自带脚本：`vendor/hardware/android.service.daemon/vendor/bin/cph-logcat-dump`
  （274 B，`logcat -v threadtime -b main -b system -b events -b crash -b kernel > /dev/ttyAMA0`）。

## 4. 改动

只改两处，引擎零改动：

1. `engine/vendor/hardware/android.service.daemon/vendor/etc/init/cph_logcat_dump.rc`：
   在原有 `service logcat_dump` 上增加 `on early-boot → start logcat_dump`，
   三个触发点共用同一个 service；并把「为什么 early-boot」「三触发为何幂等」
   「ttyAMA0 在两个机型上的来源」「本档原在机型层的来历」写进注释。
   幂等依据：服务是 `oneshot` 但 `logcat` 是长跑进程，启动后不退出，后两次 `start`
   命中「已在运行」被 init 忽略，不需要额外状态位。
2. 删除 `devices/manifest/flyme/m22/vendor/etc/init/cph_logcat_dump_early.rc`。

`vendor/build/README.md` 切分口径表同步补两条：公共 VINTF 补件的去向从
`vendor/vintf/common/`（已删）改为 daemon 模块；新增「同一能力散成两份也算重复」，
以本例为判据模板。

## 5. 验证

见 `log-pass.txt`（改动后）与 `log-fail.txt`（改动前）。

| 项 | 结果 |
| --- | --- |
| `hal build android.service.daemon` | `[ok]`，`更新 etc/init/cph_logcat_dump.rc（0644）`；`新增 0，更新 1，已存在同内容 8` |
| 装配树 `cph_logcat_dump.rc` | 369 B → **1981 B**，sha `3659d617…`，与模块源逐字节一致 |
| 三个触发点 | 第 32 行 `on early-boot`、第 36 行 `sys.boot_progress.systemserver=1`、第 39 行 `sys.boot_completed=1` |
| 装机树 logcat 文件 | 只剩 `cph_logcat_dump.rc`（模块）+ `init.logcat-to-serial.rc`（基底，`seriallogging` 写 `hvc1` 的 crash buffer，与本次不同 buffer/落点，不冲突） |
| `logcat_dump_early` 残留 | 仅注释（新的 rc 注释 + `vendor_sepolicy.cil:4535` 的历史说明），**0 处服务定义** |
| 机型静态树 `etc/init` | 只剩 `frp_symlink.rc` |
| 07 全跑 | `EXIT=0`；步骤 4 只写 `cph_logcat_dump.rc` 一件（改动前会连 `cph_logcat_dump_early.rc` 一起写） |
| 装配树条目 | 670 → **669**（删掉一个 rc） |
| `vendor.img` | 206,704,640 → **206,700,544 B**（−4,096，少一个块） |
| 镜像回读 | `ls /etc/init` 只有 1981 B 的 `cph_logcat_dump.rc` 与 694 B 的 `init.logcat-to-serial.rc`；`dump` sha `3659d617…`、Mode 0644、0:0、xattr `u:object_r:vendor_configs_file:s0` |

**sha 不作一致性判据**（mke2fs 非确定性），判据 = 装配树条目 + 逐文件 sha + 镜像 xattr。

## 6. 遗留

- **运行期未复验**：开机后看 `init.svc.logcat_dump=running`、串口（宿主 qemu.log /
  serial.log）里从 early-boot 就有 `logcat` 输出；`pioneer` 写 `ttyAMA0` 仍会有
  avc denied（permissive，历史既有，非本次引入）。
- **代价**：early-boot 起抓会让**所有声明 `android.service.daemon` 的机型**（当前 m22 与
  gold）每次开机都把全 buffer logcat 灌进串口，串口日志体积明显变大。若嫌噪音大，
  可改为属性门控（`on early-boot && property:ro.boot.cdh_logcat_early=1`），
  但那需要在 05 的 cmdline 加 `androidboot.cdh_logcat_early=1`，属独立改动。
- gold 侧同批受益（不再需要各机型各写一份 early rc），但 gold 装配树未重跑，属未验项。
- `patch.img` / `out/dist` 仍是旧版（09/11 未跑）。
