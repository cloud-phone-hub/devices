# CPH2797：ColorOS 分辨率档位默认「低档」，整机按 640x480 渲染

- 设备：OPPO Find X9 / CPH2797 / ColorOS 16.0.8.301_EX01（`board=mt6993`，联发科）
- 实例：失败 15 → 通过 15（手动写入）/ 16（全新 userdata，rc 路径）
- 日期：2026-10-30
- 上游条目：#81（contexthub 声明旷缺）
- 参考先例：`devices/oppo/findx9-16/build/doc/evidence/issue-oplus-resolution-tier-640x480/`
- 日志：`log-fail.txt`（实例 15）/ `log-pass.txt`（实例 16）

## 1. 问题

#81 之后实例 15 `sys.boot_completed=1`、桌面能起来，但整机在跑 **640x480**：

```
$ wm size
Physical size: 640x480
$ python3 .tmp/rfb_shot.py 127.0.0.1 5912 out.ppm 40
ok 640x480 covered=307200 px
```

ColorOS 桌面版式挤压、图标重叠，设置页旋转错乱。而 device.json 登记的是
`1256x2760`，QEMU 也按该值下发 EDID —— 期望与实际差一个数量级。

## 2. 根因

ColorOS/OPlus 的「屏幕分辨率档位」secure 设置
**`oplus_customize_screen_resolution_adjust`**：`0`=智能、`1`=高分辨率、
`2`=低分辨率，**本机出厂默认 `2`**（由 `oplus-services.jar` 在开机后异步切档）。

真机「低档」映射到面板的第二档物理分辨率；而云机的模式表来自 QEMU EDID，
低档只能落到**最小的 640x480**。OPlus 的切档在 `DisplayModeDirector` 里表现为一条
`PRIORITY_APP_REQUEST_SIZE` 的 SizeVote，直接锁死 base mode：

```
mVotesByDisplay:
    PRIORITY_APP_REQUEST_SIZE -> SizeVote{ mWidth=640, mHeight=480,
                                           mMinWidth=640, mMinHeight=480 }
```

于是 SF/HWC/WM 全链路按 640x480 走，并同步把 DRM 的 crtc 也设成 640x480。

即：显示链路（EDID → 内核 DRM → drm_hwcomposer → QEMU）没有问题，问题在 guest 里
一个厂商私有设置选了不适用于 EDID 模式表的档位 —— 与 findx9-16 先例同因。

## 3. 证据

**（1）EDID / 内核枚举正常，且 preferred 就是 1256x2760**

```
$ cat /sys/class/drm/card0-Virtual-1/modes
1256x2760
1024x768
800x600
640x480
640x480
```

**（2）框架明明知道谁是 default，却选了 mode 4**

```
DisplayDeviceInfo{"Layar Built-In": ..., 640 x 480, modeId 4, ...,
    defaultModeId 1, userPreferredModeId -1, supportedModes [
      {id=1, width=1256, height=2760, fps=60.0},
      {id=2, width=1024, height=768},
      {id=3, width=800,  height=600},
      {id=4, width=640,  height=480},
      {id=5, width=640,  height=480, fps=59.94}]}
    mActiveModeId=4
```

**（3）`wm size` 不是修复路径**

```
$ wm size 1256x2760
Physical size: 640x480
Override size: 1256x1920        # 被 AOSP config_maxDisplayHeight=1920 钳掉
$ dumpsys display | grep mActiveModeId
    mActiveModeId=4             # override 不改变 HWC 模式
```

`cmd display set-user-preferred-display-mode 1256 2760 60` 同样无效（SizeVote
优先级更高）。也曾尝试「重启 HWC + SF」：`start surfaceflinger` 后 crtc 一度变成
1256x2760，但 SF 停止会带崩 system_server，不可作为修复手段。

**（4）同 QEMU / 同 HWC 的 m22 是正常的，排除链路问题**

| 机型 | `wm size` | `mActiveModeId` | `crtc-pos` |
| --- | --- | --- | --- |
| m22（实例 46） | 1080x2400 | 1 | 1080x2400 |
| cph2797（实例 15） | 640x480 | 4 | 640x480 |

两者共用同一套 `hwc3-service.drm_upstream` 与 `init.virt.rc`，差异只在机型私有设置。

**（5）档位键确认出厂默认 2；写入 1 后现场立刻回到原生**

```
$ settings get secure oplus_customize_screen_resolution_adjust
2
$ settings put secure oplus_customize_screen_resolution_adjust 1
$ sleep 18
$ wm size
Physical size: 1256x2760
$ dumpsys display | grep mActiveModeId
    mActiveModeId=1
$ grep crtc-pos /sys/kernel/debug/dri/0/state
        crtc-pos=1256x2760+0+0
```

切换由 OPlus 显示服务异步完成，约 15~18 s 生效，无需重启。

## 4. 改动

`engine/vendor/hardware/android.service.daemon/vendor/bin/cph-setup-display`
（首开机置备脚本，由 `cph_disable_provision.rc` 的 `auto_provision` 在
`on property:sys.boot_completed=1` 时以 oneshot 执行）里，在
`settings put global device_provisioned 1` 之后追加：

```sh
if [ -n "$(getprop ro.build.version.oplusrom)" ]; then
    settings put secure oplus_customize_screen_resolution_adjust 1 2>/dev/null
fi
```

为什么这样写：

- 该脚本是 **engine 共享模块**（m22 / gold / cph2797 同一份），用
  `ro.build.version.oplusrom` 判定只在 ColorOS 生效，其它机型零副作用；
- 与既有 `settings put secure lockscreen.disabled` 同类同域，**不需要任何
  SELinux CIL / file_contexts 改动**；
- 置 `1`（高分辨率）而不是 `0`（智能）：先例实测「智能」只跟随当前态，
  已在 640x480 时切 0 不会自动回到原生；只有 `1` 确定给原生档。

## 5. 验证

**全新实例 16（全新 userdata，零干预）**：

```
$ getprop init.svc.auto_provision
stopped                          # 置备脚本已跑完
$ settings get secure oplus_customize_screen_resolution_adjust
1                                # rc 路径写进去了
$ getprop sys.boot_completed
1
$ wm size
Physical size: 1256x2760         # 无 override
$ dumpsys display | grep mActiveModeId
    mActiveModeId=1
$ python3 .tmp/rfb_shot.py 127.0.0.1 5912 out.ppm 40
ok 1264x2760 covered=3488640 px  # VNC 宽按 16 对齐，1256→1264
```

截图：`input keyevent 3` 回桌面后 `.tmp/cph16-home3.png` 为原生竖屏，ColorOS 桌面图标/Dock 排布正常（不再挤压）；首启时的设置页 `.tmp/cph16-desktop.png` 版式同样正常。

## 6. 遗留

- 首启会停在设置页而不是桌面（`pm disable com.android.provision` 后焦点落在
  Settings），属置备策略问题，另行处理。
- 密度仍有两个来源：`ro.sf.lcd_density=560`（lineage 基座）与 ColorOS
  `display_density_forced=480`（后者生效）。与先例一致暂不改，若 1256x2760 下
  版式/字号异常再按官方 460 ppi 口径统一。
- `com.oplus.battery` 的「设置充电上限」系统弹窗仍会在开机后盖住桌面
  （`am force-stop` 可去除），未根治。
- 实例 13 日志里 `IVibrator/default` 也出现 `Bad state?`（未阻塞开机），同源风险待查。
- `/odm/etc/vintf/manifest/` 还有 40+ 条官方声明未巡检，建议 07 加「声明了但无实现」
  的巡检步骤（与 #81 遗留同项）。
