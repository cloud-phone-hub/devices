# /dev/snd 只有 timer、没有 PCM 节点：QEMU 命令行根本没接声卡

- 设备：xiaomi/gold（Redmi Note 13 5G），engine 构建
- 日期：2026-10-27
- 状态：**已修复并验证**（实例 index 4，`pcmC0D0p` 已出现且 HAL 成功打开）
- 产物：daemon 重编 `out/core/daemon/dist/darwin/daemon`（10969170 B，go1.26.5），
  改动在 `core/host/daemon/run/qemu.go:631-633`
- 失败态：`log-fail.txt`（实例 index 3，`/dev/snd` 只有 `timer`）
- 通过态：`log-pass.txt`（实例 index 4，boot 50 s）
- 设备侧原件（同一份）：`devices/xiaomi/gold-15/build/doc/evidence/issue-qemu-hda-sound-card/`
- 关联：`devices/xiaomi/gold-15/build/doc/evidence/issue-video-fast-playback/`（音频时钟偏快的用户侧表现）

## 1. 问题

engine 镜像里 `/dev/snd/` 只有一个 `timer`，没有 `pcmC0D0p`：

```
$ adb shell "ls -l /dev/snd/"
crw-rw---- 1 system audio 116, 33  timer
```

而自研 audio HAL 的输出路径要打开 `/dev/snd/pcmC0D0p`：

```cpp
// engine/vendor/hardware/android.hardware.audio/src/ModuleImpl.cpp（openOutputStream）
auto alsa = std::make_unique<cdh::audio::AlsaPcm>();
if (!alsa->open("/dev/snd/pcmC0D0p", cdh::audio::kSampleRateHz, cdh::audio::kChannelCount)) {
    alsa.reset();          // 打不开就回退纯内存 VirtualDac，不阻断播放
}
```

打不开时 HAL 回退纯内存 VirtualDac，**播放链路仍然通**（REMOTE_SUBMIX 捕获给
scrcpy 正常），但 AudioFlinger 的 burst 失去真实消费节奏的背压，音频时钟偏快，
在线视频表现为倍速播放（对应 #24）。

对照事实：**实例 1（gold-15 原厂 ROM）的 `/dev/snd/` 也只有 `timer`** —— 所以这不是
本次移植引入的回归，而是 **daemon 的 QEMU 命令行从来没有声卡设备**。

## 2. 根因

`core/host/daemon/run/qemu.go` 拼装的参数里**没有任何声卡相关项**：

```
$ grep -rn "intel-hda\|hda-duplex\|audiodev\|virtio-sound" --include="*.go" core/host/daemon/
（无输出）
```

guest 侧内核其实早就具备了驱动能力：

```
$ adb shell "zcat /proc/config.gz | grep -E 'CONFIG_SND_HDA|CONFIG_SND_VIRTIO'"
CONFIG_SND=y
CONFIG_SND_PCM=y
CONFIG_SND_HDA=y
CONFIG_SND_HDA_INTEL=y          <== 控制器驱动在
CONFIG_SND_HDA_GENERIC=y        <== 通用 codec 解析器在
# CONFIG_SND_HDA_CODEC_REALTEK is not set   （QEMU 的 codec 走 generic，不需要厂商 codec）
CONFIG_SND_VIRTIO=y             <== virtio-snd 也在，但 HAL 硬编码的是 PCM 节点名
```

**缺的只是 QEMU 侧的设备模型**：没有 `intel-hda` 控制器 + `hda-duplex` codec，
内核就不会创建 `controlC0` / `pcmC0D0p`，只剩一个与硬件无关的 `timer`。

## 3. 证据

### 3.1 QEMU 设备清单（修复前，实例 3 的实际命令行）

```
-machine virt -rtc base=localtime -m 8192 -smp 8 -accel hvf
-object memory-backend-shm,id=stage-ram,size=8192M,share=on -numa node,memdev=stage-ram
-device vhost-user-media-pci,...（×2）
-device virtio-gpu-gl-pci,blob=on,hostmem=256M,venus=on,edid=on,...
-device virtio-blk-pci,drive=vda/vdb/vdc
-device virtio-net-pci,netdev=net0/net2
-device virtio-multitouch-pci / virtio-tablet-pci / virtio-keyboard-pci
-device virtio-serial,id=stage-vserial
-device virtconsole,chardev=stage-hvc0..hvc10
-device virtio-rng-pci
-chardev stdio,mux=on,id=charconsole -serial chardev:charconsole
                    ^^^^ 全表无声卡
```

### 3.2 内核侧只有 timer 的原因

`/dev/snd/timer` 由 `CONFIG_SND_TIMER=y` 无条件创建（与硬件无关）；
`controlC0` / `pcmC0D0p` 必须由具体声卡的 probe 路径创建。没有 PCI 声卡设备，
`snd_hda_intel` 不会 probe，所以只剩 timer。

### 3.3 QEMU 11.0.4 具备所需设备模型

```
$ out/qemu/dist/darwin/bin/qemu-system-aarch64 -device help | grep -E "intel-hda|hda-duplex"
name "hda-duplex", bus HDA, desc "HDA Audio Codec, duplex (line-out, line-in)"
name "intel-hda",  bus PCI, desc "Intel HD Audio Controller (ich6)"

$ out/qemu/dist/darwin/bin/qemu-system-aarch64 -audiodev help
Available audio drivers:
none
coreaudio
wav
```

## 4. 改动

文件：`core/host/daemon/run/qemu.go`（追加 10 行）

```go
args = append(args,
    "-device", "virtio-rng-pci",
    // 声卡：guest 侧自研 audio HAL 用 /dev/snd/pcmC0D0p 把 AudioFlinger 的
    // burst 背压锚定到真实消费节奏（缺节点时回退纯内存 VirtualDac，音频时钟
    // 偏快，在线播放会快进）。guest 内核已带 CONFIG_SND_HDA_INTEL=y 与
    // CONFIG_SND_HDA_GENERIC=y，QEMU 侧只需 intel-hda + hda-duplex。
    // audiodev 选 none：云机不向宿主出声（音频经 REMOTE_SUBMIX 捕获给 scrcpy），
    // 也不依赖宿主音频设备；QEMU 的音频定时器仍按 10ms 周期消费，
    // 所以 snd_pcm_delay 反映真实节奏，背压成立。
    "-audiodev", "none,id=cdhsnd",
    "-device", "intel-hda",
    "-device", "hda-duplex,audiodev=cdhsnd",
    "-chardev", "stdio,mux=on,id=charconsole",
    "-serial", "chardev:charconsole",
)
```

**为什么 audiodev 选 `none` 而不是 `coreaudio`**：

- 云机的音频出口是 REMOTE_SUBMIX → scrcpy，**不需要**从宿主扬声器出声；
- `coreaudio` 在 headless/无音频设备的主机上会让 QEMU 启动失败，风险高；
- 背压来自 QEMU 音频框架的周期定时器（`audio_timer`，10 ms），
  与 backend 类型无关，`none` 同样成立。

**为什么不用 `virtio-sound-pci`**：内核虽然有 `CONFIG_SND_VIRTIO=y`，但 HAL 里
硬编码的是 `/dev/snd/pcmC0D0p`；intel-hda 的节点名与之一致，且 `SND_HDA_GENERIC`
已能解析 QEMU 的 codec，改动面最小。

构建与部署：

```
cd core/host/daemon && go build -o ../../../out/core/daemon/dist/darwin/daemon .
# daemon 是长驻进程，必须重启；重启后 /api/create 拉起的 QEMU 才会带新参数
```

## 5. 验证

### 5.1 参数确实进了 QEMU 命令行

```
$ pgrep -fl qemu-system-aarch64 | tr ' ' '\n' | grep -A2 -B1 "cdhsnd"
-audiodev
none,id=cdhsnd
-device
intel-hda
-device
hda-duplex,audiodev=cdhsnd
```

### 5.2 guest 侧出现完整 PCM 节点

```
$ adb shell "ls -l /dev/snd/"
crw-rw---- 1 system audio 116,  4  controlC0
crw-rw---- 1 system audio 116,  3  pcmC0D0c
crw-rw---- 1 system audio 116,  2  pcmC0D0p     <== HAL 要的就是这个
crw-rw---- 1 system audio 116, 33  timer
```

### 5.3 HAL 真的打开了它（决定性日志）

```
01-01 08:00:16.656   573   573 D DeviceHalAidl: DeviceHalAidl::openOutputStream
01-01 08:00:16.658   544   544 I cdh_vdac_alsa: opened /dev/snd/pcmC0D0p: 48000 Hz 2 ch buffer=7680 period=1920 frames
01-01 08:00:16.658   544   544 I cdh_audio: openOutputStream: primary OK cfgId=2 frames=1 buffer=8192
01-01 08:00:16.677   544   544 I cdh_audio: openInputStream: primary OK cfgId=4 frames=1 buffer=4096
```

`buffer=7680 / period=1920`（40 ms）说明 ALSA 参数协商成功，写路径可用；
不再是"打开失败 → 回退内存 DAC"。

### 5.4 无 SELinux 拒绝

上一轮已预置的 `(allow hal_audio_default audio_device_{{vers}} (chr_file (...)))`
这次正好被用上，`dmesg` 里没有 `hal_audio_default` 相关的 avc。

### 5.5 回归

| 项 | 结果 |
|---|---|
| `dumpsys audio` 的 `Devices:` | `speaker(2)` |
| `listAudioPorts error` | 0 |
| `cmd / input / settings / am` | 全部 rc=0 |
| `persist.system.powerhal.applist_enable` | 0 |
| `debug.sf.vsync_reactor_ignore_present_fences` | true |
| boot | 50 s（复用 data）/ 80 s（首次） |

## 6. 遗留

1. **背压速率的端到端验证未做**：本次只证明 ALSA 节点打开成功、参数协商成功。
   "在线视频是否还倍速"需要用真实播放场景长时间观察，建议后续用 `dumpsys media.audio_flinger`
   之外的方式（如播放固定时长音频并比对 wall clock）验证。
2. **`-audiodev none` 不向宿主出声**：这是刻意选择（云机音频走 REMOTE_SUBMIX）。
   若将来需要在宿主机直接听声，把 `none` 换成 `coreaudio` 并在有音频设备的机器上验证即可。
3. **`pcmC0D0c`（采集）未接入**：HAL 的 input 仍是虚拟麦克风（日志显示
   `sampleRate 8000, channelMask 0x10` 单声道），未走 ALSA 采集。若后续要做
   "真实麦克风"，可复用同一套 `AlsaPcm`，节点已具备。
4. **实例 1（gold-15）同样缺声卡节点**：本次只改了 engine 的 daemon 参数，
   gold-15 的镜像本身没有这个问题（其音频链路不依赖 ALSA 背压），未做改动。
