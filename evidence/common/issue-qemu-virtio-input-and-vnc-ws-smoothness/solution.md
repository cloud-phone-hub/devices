# Issue: QEMU virtio-input abs-to-mt 悬停坐标被内核去重吞噬致点击/滑动首帧错位与 VNC WebSocket 推流全链路 60FPS 优化

- 设备：全机型通用（`common/`，首发对比复现与验证于 `xiaomi/popsicle` `16-OS4.0.5.0.XPBCN` 与 `asus/AI2205` `13-TKQ1.220928.001-20240229`）
- 阶段：`qemu/source/hw/input/virtio-input-hid.c`、`qemu/source/io/channel-websock.c`、`qemu/source/util/buffer.c`、`qemu/source/ui/vnc.c`、`qemu/source/ui/vnc-enc-tight.c`
- 状态：**已闭环**（2026-11-07）
- 关联：[#169](../../popsicle/issue-hyperos4-impeller-opengles-hwaccel/solution.md)

## 一、问题

在运行中的两个实例之间对比操作体验时（`ASUS_AI2205_13-TKQ1.220928.001` 实例 `3` vs `popsicle_16-OS4.0.5.0.XPBCN` 实例 `1`），即便 `popsicle` 已在 Guest 侧启用 Impeller OpenGL ES（Mesa VirGL）硬件加速，用户在 Web 控制台（noVNC）操作 `popsicle` 时仍明显感觉不如 `ASUS` 流畅跟手，具体表现为两类症状：

1. **点击不准、滑动首帧跳变/反向、首次点击被吞**：在浏览器 noVNC 中把鼠标移到某图标或区域后按下左键点击或拖动，第一下经常点偏到**上一次松开鼠标时的旧位置**，或者滑动起始点从旧位置瞬间跳到新位置（导致左右滑桌面变成斜滑甚至反向回弹）。
2. **全彩渐变壁纸机型（`popsicle` `1200×2608`）在浏览器 noVNC 下滑动严重掉帧（仅 `10 FPS`）**：`ASUS`（`1088×2400`，纯黑壁纸）在浏览器下滑动帧率为 `29.3 ~ 30.0 FPS`，而 `popsicle`（`1200×2608`，HyperOS 4 全彩噪点渐变壁纸）滑动时帧率暴跌至 **`10.0 FPS`（每帧间隔 `95 ~ 114 ms`）**，画面呈跳跃感。

## 二、根因

通过对运行中两个实例的 Android `getevent -lt /dev/input/event1`、`dumpsys input`、以及直接连接各自 VNC TCP/WebSocket 端口采集 RFB 协议帧耗时与矩形分片分布，定位到 **QEMU 触控转换层** 与 **QEMU VNC + WebSocket 编码传输层** 的两大底层根因：

### 根因 A：`virtio-input-hid.c` 在 `!vhid->touching`（鼠标悬停移动）时提前发送 `ABS_MT_POSITION_X/Y`，触发 Linux 内核 `input_handle_abs_event` 去重吞噬按下坐标

1. 浏览器 noVNC 在用户移动鼠标但未按下按键时，会持续向 QEMU VNC 发送 `PointerEvent(button_mask=0, x, y)`。
2. 旧版 `virtio_input_handle_event()` 在 `INPUT_EVENT_KIND_ABS` 分支中，哪怕 `!vhid->touching`（未按下），也会立即将 `ABS_MT_POSITION_X (0x35)` 和 `ABS_MT_POSITION_Y (0x36)` 发送给 Guest Linux 内核：
   - **Android `InputReader` 侧**：因为当前没有任何活动触控点（`ABS_MT_TRACKING_ID == -1`），`MultiTouchInputMapper` 直接打印警告并丢弃该坐标：
     `MultiTouchInputMapper: Touch event Mellon: slot 0 is not currently in use, ignoring.`
   - **Linux 内核 `drivers/input/input.c` 侧**：`input_handle_abs_event()` **已经把内核 `absinfo[ABS_MT_POSITION_X/Y].value` 更新为了鼠标悬停处的 `(X_new, Y_new)`**！
3. 当用户在 `(X_new, Y_new)` 原地按下鼠标左键（`PointerEvent(button_mask=1, X_new, Y_new)`）时：
   - QEMU 在 `virtio_input_handle_sync()` 中依次发送 `ABS_MT_SLOT 0`、`ABS_MT_TRACKING_ID 1`、`ABS_MT_POSITION_X X_new`、`ABS_MT_POSITION_Y Y_new`、`BTN_TOUCH 1`、`SYN_REPORT`；
   - **Linux 内核 `input_handle_abs_event()` 发现 `X_new`、`Y_new` 与刚才悬停时缓存的 `absinfo.value` 完全相等，直接按重复值过滤吞掉（不向 `/dev/input/event1` 下发 `ABS_MT_POSITION_X/Y`）**！
   - 结果 Android `InputReader` 只收到 `ABS_MT_TRACKING_ID = 1` + `BTN_TOUCH = 1` + `SYN_REPORT`，被迫沿用**上一次触控结束前的旧坐标 `(X_old, Y_old)`** 作为本次 `ACTION_DOWN` 的起点！
4. 此外，旧代码在 `INPUT_EVENT_KIND_BTN` 中虽将 `BTN_LEFT` 映射为 `vhid->touching`，却未 `return`，继续把 `EV_KEY BTN_MOUSE (0x110)` 发给了触控屏设备，干扰 Android 输入设备分类与按键状态机。

### 根因 B：QEMU VNC Tight 编码配置、脏行高度切碎、`buffer_shrink` 强制重分配与 WebSocket 8KB 分片叠加导致全彩壁纸机型掉至 10 FPS

对比测量 `ASUS`（`5912/5913`）与 `popsicle`（`5920/5921`）在相同 noVNC Tight 编码请求下的服务端响应：
- `ASUS` 桌面背景是 `#000000` 纯黑，88% 的屏幕区域在 Tight 编码器中命中 3 字节 `VNC_TIGHT_FILL` 纯色块，单帧仅 `132` 个矩形、`50 ~ 71 KB`，耗时 `29.8 ms`；
- `popsicle` 桌面是 `1200×2608` 全彩渐变噪点壁纸，几乎没有纯色块或 `<=256` 色调色板块，暴露了 QEMU VNC + WebSocket 链路的 **5 处性能瓶颈**：
  1. **`tight_conf` 切片过碎且对全彩渐变块强跑 `zlib deflate`（`ui/vnc-enc-tight.c`）**：noVNC 默认请求压缩级别 `2`，对应 `tight_conf[2]` 的 `max_rect_size = 6144`、`max_rect_width = 1024`、`raw_zlib_level = 2`。一帧 `1200×2608` 全屏更新被切碎成 **`568` 个小矩形**，且每个真彩色渐变块都要跑一遍 `deflate(..., level=2)`，单帧仅 CPU 压缩就耗时 **`91 ~ 113.9 ms`**！而在增量滑动时，由于相近渐变行偶发 16 像素块 `memcmp == 0`，逐行 `find_and_clear_dirty_height` 更将单帧切碎成 **`831` 个高度仅 1~2 像素的碎片矩形**，导致浏览器 noVNC 每帧调用 **831 次 `ctx.putImageData()`** 卡死主线程。
  2. **`buffer_shrink()` 末尾遗留无条件 `buffer_adj_size(buffer, 0)` 引发每帧上千次 `g_realloc`（`util/buffer.c`）**：QEMU `util/buffer.c` 的 `buffer_shrink()` 原本设计用指数滑动平均 `avg_size` 控制缩容，但函数末尾第 88 行多写了一句无条件的 `buffer_adj_size(buffer, 0);`！这导致每次 `buffer_reset(&vs->tight->tight)` 或 `buffer_advance()` 清空缓冲时，缓冲区都被强制 `g_realloc` 缩回 4 KB，下一个矩形写入时又翻倍 `g_realloc` 扩容——单帧 568 个矩形触发 **`1,120+` 次 `realloc` 系统调用**。
  3. **`QIO_CHANNEL_WEBSOCK_MAX_BUFFER = 8192`（8 KB）导致大帧在主线程触发数百次百兆级 `memmove`（`io/channel-websock.c`）**：WebSocket 通道每次最多只从 `vs->output` 取 `8192` 字节封装成 1 个 WebSocket 帧，随后立即调用 `buffer_advance(&vs->output, 8192)` 将剩余数 MB 数据向前 `memmove`！一帧 4 MB 的画面被切成 500 个 8 KB WebSocket 帧，在 QEMU 主事件循环里累计搬运 `(500 × 2 MB) ≈ 1 GB` 内存。
  4. **`VNC_REFRESH_INTERVAL_BASE = 30 ms` 硬锁帧率上限为 33.3 FPS，且 `FramebufferUpdateRequest` 不立即触发下发（`ui/vnc.c`）**：定时器基础周期为 `30 ms`（33.3 FPS）；且当客户端发回 `FramebufferUpdateRequest (msg=3)` 时，`protocol_client_msg` 只对 `data[0] > 3` 唤醒刷新率，`framebuffer_update_request` 也不立即调用 `vnc_update_client`，白白空等下一个定时器周期。
  5. **macOS 默认 TCP `SO_SNDBUF` 仅 128 KB**：导致大于 128 KB 的帧频繁触发 `EAGAIN` 与 GSource 挂载/卸载开销。

## 三、证据

- `log-fail.txt`：
  - 修复前 `adb logcat` 中 `MultiTouchInputMapper: slot 0 is not currently in use, ignoring` 告警与 `getevent -lt /dev/input/event1` 实测复现悬停移动 `(500, 1200)` 后原地按下左键时内核吞掉 `ABS_MT_POSITION_X/Y`（导致点击落在旧坐标 `(100, 200)`）的原始事件序列；
  - 修复前 `popsicle` vs `ASUS` 的 VNC Tight 单帧耗时（`113.9 ms / 568 rects` vs `29.8 ms / 132 rects`）与 WebSocket noVNC 连续滑动帧率采样（`popsicle` 仅 `10.0 FPS`，单帧切碎达 `831` 个矩形）。
- `log-pass.txt`：
  - 修复后 `getevent -lt /dev/input/event1` 验证：悬停移动 `mask=0` 零事件下发，原地按下左键 `mask=1` 完整下发 `ABS_MT_POSITION_X = 00001eb0 (7856)`、`ABS_MT_POSITION_Y = 00003706 (14086)`，滑动终点精确落位；
  - 修复后 `popsicle` 单帧 Tight 编码耗时从 `113.9 ms`（`568` 个矩形）降至 **`9.42 ms`（`19` 个矩形，提速 12 倍）**；
  - 修复后通过 WebSocket (`ws://127.0.0.1:5921`) 实测连续滑动帧率：桌面左右滑动从 **`10.0 FPS` 跃升至 `43.0 ~ 45.3 FPS`（稳态帧间隔 `16~17 ms`，平均每帧仅 `14.2` 个矩形）**，`Settings` 列表上下滑动达 **`47.0 FPS`（稳态帧间隔 `16~17 ms`）**，显著超越旧进程下的 `ASUS`（`29.3 FPS`）。

## 四、改动

1. **修复 `virtio-input-hid` 的 `abs-to-mt` 悬停坐标下发与 `BTN_MOUSE` 误发（`qemu/source/include/hw/virtio/virtio-input.h`、`qemu/source/hw/input/virtio-input-hid.c`）**：
   - 在 `VirtIOInputHID` 结构体中新增 `bool prev_touching;`；
   - 在 `virtio_input_handle_event()` 中：
     - `INPUT_EVENT_KIND_BTN`：当 `vhid->abs_to_mt && btn->button == INPUT_BUTTON_LEFT` 时，仅更新 `vhid->touching = btn->down` 并立即 `return`，不再向触控屏发送 `EV_KEY BTN_MOUSE`；
     - `INPUT_EVENT_KIND_ABS`：当 `vhid->abs_to_mt` 开启时，仅更新 `vhid->last_abs_x` / `vhid->last_abs_y` 并立即 `return`，**绝不在 `!vhid->touching` 悬停期间向内核发送 `ABS_MT_POSITION_X/Y`**；
   - 在 `virtio_input_handle_sync()` 中统一组装完整的 MT-B 触控帧：仅当 `vhid->touching` 为真时依次发送 `ABS_MT_SLOT(0)`、（首次按下时）`ABS_MT_TRACKING_ID`、`ABS_MT_POSITION_X(last_abs_x)`、`ABS_MT_POSITION_Y(last_abs_y)`、`ABS_MT_TOUCH_MAJOR`、`ABS_MT_PRESSURE`、（首次按下时）`BTN_TOUCH=1` 与 `SYN_REPORT`；松开（`!vhid->touching && vhid->prev_touching`）时发送 `ABS_MT_SLOT(0)`、`ABS_MT_TRACKING_ID(-1)`、`BTN_TOUCH=0` 与 `SYN_REPORT`；纯悬停移动（`!vhid->touching && !vhid->prev_touching`）不向 Guest 发送任何 input 事件。
2. **消除 `buffer_shrink()` 每次清空时的强制 4KB 缩容与 `realloc` 抖动（`qemu/source/util/buffer.c`）**：
   - 删除 `buffer_shrink()` 末尾无条件执行的遗留语句 `buffer_adj_size(buffer, 0);`，使函数内原有的指数滑动平均（`buffer->avg_size`）缓冲复用逻辑正常生效。
3. **将 WebSocket 单帧发送上限从 `8 KB` 提升至 `16 MB`（`qemu/source/io/channel-websock.c`）**：
   - 将 `QIO_CHANNEL_WEBSOCK_MAX_BUFFER` 从 `8192` 提升为 `(16 * 1024 * 1024)`，使整帧 VNC 画面一次封装为单个 WebSocket binary frame，彻底消除主线程 `vs->output` 上数百次 `buffer_advance` `memmove`。
4. **提升 VNC 刷新频率至 60 FPS、立即响应 `FramebufferUpdateRequest`、纵向合并脏行包围盒并增大 Socket 发送缓冲（`qemu/source/ui/vnc.c`）**：
   - 将 `VNC_REFRESH_INTERVAL_BASE` 从 `GUI_REFRESH_INTERVAL_DEFAULT`（`30` ms）改为 `16` ms（`62.5 FPS`）；
   - 在 `protocol_client_msg()` 中将 `if (data[0] > 3)` 改为 `if (data[0] >= 3)`，并在 `framebuffer_update_request()` 中当 `vs->has_dirty || !incremental` 时立即调用 `vnc_update_client(vs, 0)`；
   - 在 `vnc_update_client()` 中将原本按 16 像素列逐行切分的逻辑改为**按脏行纵向合并包围盒**（允许跨 `<= 8` 行干净间隙合并为一个完整水平条带），将单帧矩形数从 `568 ~ 831` 个压降至 `14 ~ 19` 个；
   - 在 `vnc_listen_io()` 中对接入的客户端 socket 设置 `SO_SNDBUF`（依次尝试 `4 MB` / `2 MB` / `1 MB`）。
5. **优化 Tight 编码器对大分辨率真彩色壁纸的吞吐（`qemu/source/ui/vnc-enc-tight.c`）**：
   - 将 `tight_conf[0..2]` 的 `max_rect_size` 提升至 `262144`、`max_rect_width` 提升至 `2048`、`raw_zlib_level` 设为 `0`（对纯色块仍走 3 字节 `VNC_TIGHT_FILL`、对调色板块仍走 `idx_zlib_level=1`，对无法进调色板的真彩色渐变块走 zlib `STORED` 零 CPU 压缩块，既兼容标准 noVNC zlib 解码流又省去 100ms 级别的无效 deflate）；
   - 在 `tight_pack24()` 中增加 `rshift == 16 && gshift == 8 && bshift == 0` 的常量移位快路径。

## 五、验证

重新编译并签名 `/Volumes/work/cloud-phone-hub/out/qemu/dist/darwin/bin/qemu-system-aarch64`，重启 `popsicle_16-OS4.0.5.0.XPBCN` 实例 `1` 后实测：

1. **触控精度验证**：
   - 模拟 noVNC 先悬停移动到 `(288, 650)`（`mask=0`），再在原地按下左键并滑向 `(1080, 650)`：
   - `getevent -lt /dev/input/event1` 显示悬停阶段零事件下发；按下瞬间准确下发 `ABS_MT_POSITION_X = 0x1eb0 (7856)`、`ABS_MT_POSITION_Y = 0x3706 (14086)`，滑动全程无任何跳变，`logcat` 中 `slot 0 is not currently in use` 计数为 **`0`**。
2. **VNC 单帧延迟与 WebSocket 连续滑动帧率对比**：
   - **单帧全屏 Tight 编码 + 传输耗时**：从修复前的 **`113.9 ms`（`568` 个矩形）** 降至 **`9.42 ms`（`19` 个矩形）**，提速 **12 倍**（比 `ASUS` 旧进程的 `29.82 ms / 132 rects` 快 **3 倍**）；
   - **WebSocket (`ws://127.0.0.1:5921`) 桌面连续左右滑动**：从修复前的 **`10.0 FPS`（`95 ms/帧`）** 提升至 **`43.0 ~ 45.3 FPS`（稳态帧间隔 `16 ~ 17 ms`，平均每帧仅 `14.2` 个矩形）**；
   - **WebSocket (`ws://127.0.0.1:5921`) `Settings` 列表连续上下滑动**：达到 **`47.0 FPS`（稳态帧间隔 `16 ~ 17 ms`）**。

## 六、遗留

- 当前运行中的 `ASUS_AI2205_13-TKQ1.220928.001-20240229` 实例 `3`（`pid=60150`）仍跑在重启前的旧 `qemu-system-aarch64` 进程上（帧率受限于旧 `30 ms` 定时器为 `29.3 FPS`）；后续重启该实例即可自动加载新 QEMU 二进制，享受相同的 `60 FPS` VNC 刷新与触控去重修复。
