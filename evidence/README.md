# devices 证据索引

本目录（`devices/evidence/`）是 cloud-phone-hub 把 lineage 底座 + 自研 HAL 拼成可运行云机镜像的**唯一**证据落点。
一个条目一个目录（`issue-<slug>/`），内含 `solution.md`（六段式：问题 → 根因 → 证据 →
改动 → 验证 → 遗留）+ `log-fail.txt` / `log-pass.txt`（截图可一并放入）。

**2026-11-02 起按机型分组**：条目放在 `devices/evidence/<分组>/issue-<slug>/`，每个分组一份
`README.md` 索引（下表「索引」列）。分组判据见下节；slug 与编号 `#N` 都是稳定标识，
重构只换位置，不改名、不重排。2026-11-15 起证据根由 `engine/evidence/` 迁到
`devices/evidence/`（agent 的 `evidence.search` / `ev.new` 与 cdha 的
`/api/runs/{id}/evidence` 已同步指向新根）。

## 分组

| 分组 | 机型 / 厂商 | 条目数 | 编号 | 索引 |
| :--- | :--- | :--- | :--- | :--- |
| `gold/` | Xiaomi Redmi Note 13 5G（MT6833），含同一物理机的 CN 变体 `note13-5G-15` | 52 | #2~#41、#72、#96~#100、#123、#146、#195、#222、#227、#246 | [gold/README.md](gold/README.md) |
| `m22/` | MEIZU 22（Flyme 12.5.0.0A / QTI `sun` / Android 16） | 34 | #42~#71、#73~#75、#— | [m22/README.md](m22/README.md) |
| `ss9380/` | Samsung Galaxy S25 Ultra（SM-S9380 / OneUI 16，国行 CHC） | 13 | #83~#87、#89~#95、#338 | [ss9380/README.md](ss9380/README.md) |
| `cph2797/` | OPPO Find X9（ColorOS 16.0.8.301 / MT6993） | 9 | #76~#82、#153、#165 | [cph2797/README.md](cph2797/README.md) |
| `oriole/` | Google Pixel 6（lineage-21 / Android 14） | 18 | #101、#103~#106、#149、#150、#155、#159~#162、#164、#197、#198、#200、#201、#256 | [oriole/README.md](oriole/README.md) |
| `popsicle/` | Xiaomi 17 Pro Max（HyperOS 3 & 4 / Android 16，lineage-23.2 + kernel 6.12.81） | 10 | #125、#126、#135、#136、#139、#140、#166、#169、#247、#248 | [popsicle/README.md](popsicle/README.md) |
| `15r/` | OnePlus 15R（ColorOS 16.0.10.500_EX01 / 骁龙 SM8750 `canoe`，lineage-23.2 + kernel 6.12.81） | 9 | #127、#128、#129、#130、#131、#132、#133、#151、#158 | [15r/README.md](15r/README.md) |
| `sun/` | Honor Magic7 RSR Porsche Design（MagicOS 9 / 骁龙 SM8750 `sun` / Android 15，lineage-22.2 + kernel 6.6） | 2 | #171、#259 | [sun/README.md](sun/README.md) |
| `g1170/` | BLU G75（MT6768 / Helio G81 / Coosea ODM Android 15，lineage-22.2 + kernel 6.6） | 1 | #185 | [g1170/README.md](g1170/README.md) |
| `rmx6688/` | realme GT7（真我 GT7 国行 / MT6991 天玑 9400+ / ColorOS 15.0.2 Realme UI 6.0 / Android 15，lineage-22.2 + kernel 6.6） | 1 | #194 | [rmx6688/README.md](rmx6688/README.md) |
| `asusai2501/` | ASUS Zenfone 12 Ultra / ASUSAI2501（`ZWWAI2501` / 骁龙 SM8750 `sun` / Android 15，lineage-22.2 + kernel 6.6） | 2 | #260、#293 | [asusai2501/README.md](asusai2501/README.md) |
| `pd2454/` | vivo X200 Ultra（`vivo/PD2454` / OriginOS 5 / 骁龙 SM8750 `sun` / Android 15，lineage-22.2 + kernel 6.6） | 1 | #262 | [pd2454/README.md](pd2454/README.md) |
| `nx809j/` | REDMAGIC 11 Pro（`redmagic/NX809J` / 骁龙 SM8850 `canoe` / Android 16，lineage-23.2 + kernel 6.12.81） | 1 | #267 | [nx809j/README.md](nx809j/README.md) |
| `asteroids/` | Nothing Phone (3a)（`nothing/Asteroids` / 骁龙 SM7635 `volcano` / GRF vendor=14，system=15/16，kernel 6.1） | 2 | #268、#270 | [asteroids/README.md](asteroids/README.md) |
| `aito/` | Motorola Razr 2024（`motorola/aito` / MediaTek `mt6878` / GRF vendor=14，system=16，kernel 6.1） | 1 | #271 | [aito/README.md](aito/README.md) |
| `x6876/` | Infinix X6876（`infinix/Infinix-X6876` / MediaTek `mt6878` / GRF vendor=14，system=16，kernel 6.1） | 1 | #273 | [x6876/README.md](x6876/README.md) |
| `xplore-x1/` | Blackview XPLORE X1（`blackview/XPLORE_X1` / Doke OS 5 / MT6877 / Android 15，lineage-22.2 + kernel 6.6） | 1 | #296 | [xplore-x1/README.md](xplore-x1/README.md) |
| `g1130/` | BLU G65L（`blu/G1130` / Unisoc UMS9230T / GRF vendor=13，system=14，lineage-20.0 + kernel 5.15） | 3 | #297、#336、#337 | [g1130/README.md](g1130/README.md) |
| `arw/` | HMD Crest Max（`hmd/ARW` / Unisoc UMS9620 / GRF vendor=13，system=14，lineage-20.0 + kernel 5.15） | 1 | #304 | [arw/README.md](arw/README.md) |
| `common/` | 引擎级 / 跨机型（构建流水线、选核、vendor 装配口径、内核与模块机制、运行侧验收工具、选核口径、QEMU 触控与 VNC 推流、设备侧目录迁移、ROM 根迁移、repo manifest 收口、devices 仓库声明、agent 产物根迁移、节点 ROM 根配置修复、消灭 engine 目录与统一 out/devices 产物根、Host Radio/Wi-Fi SOCKS5 VPN 与 Camera/Bluetooth/NFC 全栈模拟、实例停机全后端同步回收、硬件风控特征对齐与传感器防聚类、VirGL YV12 步幅与动态壁纸硬解、蜂窝双 PDN 网卡与 IMS IPv6/Connectivity 全栈对齐、SwiftShader `VK_KHR_external_fence_fd` 与 HWC3/IWlan 运行态收敛、独立 ODM 分区 NFC 识别回填与 `/dev/nq-nci` 探针兜底、TEE 证书证明链三态隔离与真实 AVB/RKP V3 改造、相机按需休眠 `CDCM_PAUSE` 与 `sun` 指纹/体感死循环卡顿根治、荣耀 `opkey`/APN 双 PDN 打通与全仓 `prepare` 对齐及轮询掉帧根治） | 165 | #102、#107、#108、#116、#117、#119~#122、#124、#134、#137、#138、#139、#141~#145、#147、#148、#152、#154、#156、#157、#163、#167、#168、#170、#172~#184、#186~#193、#196、#199、#202~#221、#223~#226、#228~#245、#250、#251、#252、#253、#254、#255、#257、#258、#261、#263、#264、#265、#266、#269、#272、#274~#292、#294、#295、#298~#301、#305~#335 | [common/README.md](common/README.md) |

**分组判据**：条目 `solution.md` 首部的「设备/机型」行指向哪台机就进哪个目录；同一条改动
同时落在 3 台以上机型、或结论对所有机型成立的，进 `common/`。`gold` 与 `note13-5G-15` 是
同一台物理机的两个 ROM 变体，合并为一组（正文里两种写法都指这台机）。

**编号现状**：#139 在 `common/`（build.prop 基底来源）与 `popsicle/`（定制应用从 userdata 取）
各有一条，是既有撞号；新条目一律取当前最大值 +1（#141 避开了 popsicle 的 #140，#142、#143、#144、
#145 接在其后，#146 给 gold 的 in-tree 过期声明、#147 给运行侧验收的 websocket 抓帧工具、
#148 给三台基线过期的顶层 `kernel`、#149 给 oriole AP2A 基线缺「纯 64 位锁」、
#150 给 oriole 锁补齐后的 system_server 崩溃循环（已闭环：删 `ro.frp.pst` + 关 `ro.hwui.use_vulkan`）、
#151 给 15r 基线未锁 GPU 后端名（`egl=adreno` 崩 SF、`hwui.use_vulkan` 崩三个 UI 进程 → 全黑屏），
#152 给属性收口的引擎级缺陷（`proplines.Apply` 对 sets 命中的同名多行只改第一处，被 init
的「后读覆盖先读」盖回）、#153 给 cph2797 的 FRP 承载块设备缺失（PDB 超时崩 system_server）、
#154 给 06 的 odm 属性件留档目录名写死（独立 odm 分区机型静默读到厂商 vendor 件，整层盖回
vendor 收口 → libEGL 断言 137 次）、
#155 给 oriole 5.10 内核 `0002-hwsim-fake-wifi.patch` 在 kthread 中调 `netif_receive_skb` 引发的单核 TCP BH 锁自死锁、
#156 给 `device_info` 从清单 `uname-version` 渲染伴随文件读取并打通 5.10/6.1 与 `/mnt/vendorpatch` 内核态直读、
#157 给 `devices/modules/` 全部 6 个外部模块对齐兼容 `devices/kernels/` 全版本 5.10/6.1/6.6/6.12/6.18、
#158 给 15r 漏写 `modules.input` 段致 `virtio-tablet` 被屏蔽且 `virtio-multitouch` 未开 `abs-to-mt` 而无法操作 VNC 界面），
不回改历史编号（`AGENTS.md` 第 2 条）。

**编号规则**：`#N` 是历史编号，一经分配不再复用，也不因分组而重排——同一机型的条目编号
未必连续（gold 的 #42~#71 在 m22 目录里就是这种交错）。被移除的条目编号空着，去向见下文
「已移除条目」。

## 通用约定

- **单一落点**：新增 issue 直接在本目录下建 `<分组>/issue-<slug>/`，**不再**在
  `devices/<vendor>/<device>/build/doc/` 下另建一份再复制（2026-10-31 起，见 `AGENTS.md`
  第 2 条）。两份同名条目会天然漂移：ss9380 迁移时就出现过设备侧 `solution.md` 已补
  「七、复测记录」而 engine 侧还停在旧版的例子。历史机型目录下仍留有同构副本，内容
  一致时以本目录为准。
- **格式欠账**：早期条目只留了 `solution.md`（日志未单独落盘，不补造）；另有 12 个早期条目
  是五段式（`落地方案` 承担「改动」、`验证结果` 同时承担「证据」，末段可能是「关键文件 /
  落位 / 取证要点」而非「遗留」），保留原件结构未改写。
- **底座同名文件必须同名覆盖**，不要另起文件名追加（VINTF 片段、init rc 都是）；否则会出现
  #20/#21/#35 那类 `Conflicting FqInstance` / duplicate service。
- **新阶段不要自带小工具副本**：`pick` / `pickWithSource` / `absolute` / `readText` /
  `isFile` / `isDir` / `tailLines` / `shortHash` / `alignUp` / `orMissing` / `resolveDevice` /
  `resolveVersionDir` 一律用 `build/internal/stagekit`，版本路径段用 `env.VersionSegment()`。
  跨包同名副本复制一次就会静默漂移（`isDir` 展不展开 `~`、`shortHash` 算不算 sha256），
  编译不报错、测试不覆盖。
- **加内核模块不改阶段代码**：产物名 / 注入阶段 / 伴随文件写在 `modules/<目录>/module.json`，
  阶段按 `inject` 派生（05 first-stage、08 vendor_dlkm）；清单里只写「这台机型启用哪些」，
  `modules.external` 必填（写 `[]` 表示不编），见 `devices/modules/README.md`。
- **盘集要装「底盘 GPT 里没有的分区」，只改清单**（#123，
  [gold/issue-rom-preinstall-apps](gold/issue-rom-preinstall-apps/solution.md)）：
  `rom.vendor.images` 是 01 的保留镜像白名单，顶层 `vda.partitions` 声明哪些镜像打进 vda
  （写镜像文件名，分区名 = 去 `.img`）。两道闸门在解析期：`vda.partitions` 里的项必须在
  `images` 里（否则 01 不抽）、不得是 `super`/`boot`/`vendor_boot`（它们有步骤 3/4 原位替换，
  写进去会把同一块盘写两遍）。`images: []` 等同未声明（走默认集 `super + vendor_boot`），
  不能理解成「除 super 外都不要」。
- **清单加了保留镜像，要确认 01 真的重抽了**（#123）：完成标记 `.unpacked.json` 现在记
  `keeps`，清单新增项会触发「补抽」（按 `only` 收窄，不重写 `super.img`）；看到
  `[skip] 已解包` 而解包根没有新镜像，就是这条链断了。扩盘/追加 GPT 后用
  `gold/issue-rom-preinstall-apps/verify-vda-gpt.py` 复核（主/备头 CRC、`alternate`、表字节、
  分区数据 sha256）。

## 通用取证手法

- **audio 类问题的判据用 system_server 视角**（`dumpsys audio` 的 `Devices:`），
  `service list` 里 `IModule/primary` 存在**不等于**音频可用；`FAILED_TRANSACTION` 在原厂
  实例上也一样，不能当判据。共享内存 / fd 类授权（`*_device` 的 `chr_file open map`）
  优先于 HAL 代码。
- **`dmesg` 无 `avc: denied` 不能反证「不是 SELinux」**：`binder_user_error()` 触发一次后会把
  `binder_stop_on_user_error` 置 2 并自我抑制。此时改为 pull 两侧策略文件做 `diff`。
- **`/dev/snd` 只剩 `timer` 时先查 QEMU 命令行**，不要翻 ALSA/HAL 代码（`timer` 由
  `CONFIG_SND_TIMER=y` 无条件创建，`pcmC0D0p` 必须由声卡 probe）。
- **SELinux 拒绝对单台机型重复出现时，先排「最长前缀 / 域属性」两类静态错**：属性域按最长
  前缀匹配（`sys.*` 归 `system_prop`，兜底行只吃没命中前缀的属性），进程域必须
  `(typeattributeset domain (X))` 才拿得到 `u:r:X:s0` 角色关联；两者都是「secilc 编得过、
  进镜像、运行时才被拒」。详见 `ss9380/README.md` 与 m22 的 #74。
- **`build.prop` 收口是否生效只能读运行时**：装配树里 `grep` 到基线值不等于 `getprop` 读到它。
  两个必查方向——① 同份文件内同名多行（厂商件里 30+ 个重复键是常态，init 后读覆盖先读，
  见 #152）；② 跨层盖回（装载顺序 system → system_ext → product → vendor → vendor_dlkm → odm，
  odm 最后读，见 #154）。判据是 qemu.log 里的 `init: Overriding previous property 'k':'旧' with new value '新'`
  留痕：出现「前值 = 基线收口值、后值 = 厂商原值」就是收口被破坏。
- **引导链卡死但 `logcat` 已被刷屏时，按「崩溃主体 + 计数」取证**：`grep -c` 具体断言串
  （如 `couldn't find an OpenGL ES implementation`）比读整份 buffer 可靠；同一断言在
  surfaceflinger 侧与 `zygote_preload_graphics` 栈侧的分布要分开数（#154 是 70 / 67）。

## 已移除条目

以下 10 条在 2026-11-02 重构时移除：结论已失效、已被后续条目取代，或已固化为代码/规约
（`AGENTS.md`、`modules/README.md`、`build/internal/stagekit`）。编号保留不复用，按
`AGENTS.md` 第 2 条约定需回溯时查 `git log -- devices/evidence`。

| # | slug | 移除理由 | 结论落点 |
| :--- | :--- | :--- | :--- |
| 1 | issue-gold15-align-note13-lineage-route | 旧 v1「整树对齐 note13」路线，已被 #72 取代 | `gold/issue-gold-lineage-version-align`（版本钉死口径） |
| 88 | issue-firstboot-wallpaper-battery-solidify | 方案撤销，改动已回滚 | 结论留在 `ss9380/README.md` 头部说明 |
| 109 | issue-module-declaration-single-source | 已固化为清单字段约定 | `devices/modules/README.md`、`AGENTS.md` 第 6 条 |
| 110 | issue-build-stage-skeleton | 已固化为公共包 | `build/internal/stagekit`、本文件「通用约定」 |
| 111 | issue-test-dir-per-module | 已固化为测试规范 | `AGENTS.md` 第 4 条 |
| 112 | issue-lineage-version-pin | 已固化为版本口径 | `AGENTS.md` 第 5 条 |
| 113 | issue-kernel-version-borrow | 已固化为借用登记口径 | `AGENTS.md` 第 5 条 |
| 114 | issue-kernel-line-field | 已固化为清单顶层 `kernel` 字段 | `AGENTS.md` 第 5 条 |
| 115 | issue-vendor-dlkm-self-built | 已固化为 vendor_dlkm 全自建口径 | `AGENTS.md` 第 6 条 |
| 118 | issue-composer-compat-removal | 代码整链已删，无持续价值 | `common/issue-composer-compat-vendor-compile` 第 7 节 |
