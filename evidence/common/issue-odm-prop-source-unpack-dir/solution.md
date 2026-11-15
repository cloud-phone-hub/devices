# 06 的 odm 属性件留档目录名写死 `unpack_vendor` → 读到厂商 vendor 件，odm 层把 vendor 收口全盖回

- 范围：引擎级（`engine/build/internal/vendorimg` + `06_build_odm`，影响所有把 odm 属性件
  声明在独立分区镜像里的机型）
- 首发机型：oppo/cph2797（实例 2）
- 发现时间：2026-11-04
- 编号：#154
- 状态：已闭环

## 一、问题

cph2797 第二轮起机（实例 2）：FRP 卡点已由 #153 关掉（`PersistentDataBlockService init
timeout` 计数 18 → 0），system_server 越过 phase 500，但引导仍不动 ——
surfaceflinger 与 zygote preload 阶段反复 SIGABRT，断言原文

```
Abort message: 'couldn't find an OpenGL ES implementation, make sure one of
persist.graphics.egl, ro.hardware.egl and ro.board.platform is set'
```

600 s 窗口内该断言 137 条（surfaceflinger 侧 70、`zygote_preload_graphics` 侧 67），
判定 `[fail] 超时 600s 未出现 sys.boot_completed=1（当前日志 49976 行）`。

诡异之处在于**基线明明锁了 GPU 后端名、07 的产物也确实写着 `ro.hardware.egl=mesa`**
（#152 修完后 vendor 层四个键各只剩一行、值全对），但 VM 里读到的是厂商真值。
qemu.log 给出的方向是「属性被后读的那层盖回去了」：

```
Overriding previous property 'ro.hardware.egl':'mesa' with new value 'mali'
Overriding previous property 'ro.hardware.hwcomposer':'drm' with new value 'mtk_common'
Overriding previous property 'ro.opengles.version':'196608' with new value '196610'
```

## 二、根因

1. Android 属性装载顺序是 system → system_ext → product → vendor → vendor_dlkm → **odm**，
   后读覆盖先读。上面三条盖回只能来自 vendor 之后的层，即 odm 层重新声明了这些键。

2. 基线里 cph2797 的 odm 段是 `img: odm_a.img` + `prop: /build.prop`，语义是
   「厂商 odm 原件 = odm_a.img 里的 /build.prop」，实测该件 **138 行、且不含任何 GPU 键**
   （留档见 `engine/out/tmp/cph2797-official-odm-build.prop`）。06 的产物却应是
   「138 行原件 + 5 条 sets」，实际却产出 **922 行**（sha256 `a3f0ad343852`）——
   正好是厂商 vendor 件 917 行 + 5 条追加。

3. 根因在 06 的取件链第一步：解包留档目录名由 `vendorimg.UnpackDir()` 给出，而它
   曾经无条件返回 `<镜像目录>/unpack_vendor`（`UnpackDirName` 常量），因为历史上
   只有 07 用这个目录、而 07 只解厂商 vendor。06 复用它时没有换名，于是
   `img: odm_a.img` + `prop: /build.prop` 拼出的查找路径
   `<super>/unpack_vendor/build.prop` **恰好是 07 步骤 1b 解出来的厂商 vendor build.prop**
   ——同目录、同相对路径、不同分区。

4. 时序决定了它有多隐蔽：run1（首轮构建，`unpack_vendor` 还是空的）06 自己把 odm 件解进
   那个目录，读到的恰好是对的内容，产物正确 —— **纯属侥幸**；只要跑过一次全量构建，
   07 的 vendor 整树就躺在同名目录里，此后每次 06 都静默把 odm 属性件做成 vendor 的副本，
   再经「odm 层最后读」把基线 vendor 段的收口值全部按厂商原值盖回。

## 三、证据

`log-fail.txt`：
1. 同一台机两次构建的取件留痕对比 —— run1 `138 行 / sha256 0606e97aa3aa` vs
   run2 `917 行 / sha256 6af1959e68ad`，路径字符串完全相同；
2. run2 产物 922 行（= 917 + 5）的行数算术；
3. run2 qemu.log 里三条「基线收口值 → 厂商原值」盖回（egl / hwcomposer / opengles），
   以及 `ro.hwui.use_vulkan` 出现两次（vendor 副本把那两行重复键也整份搬进 odm 层）；
4. 断言原文与 137 次计数、崩溃主体分布；树内 `lib64/egl/` 只有 mesa 三件；
5. **单测复现**：把 `UnpackDir` 换回旧实现、其余代码不动，
   `TestBuildODMReadsOdmUnpackDirNotVendor` 直接 FAIL，产物里出现诱饵标记
   `ro.marker.partition=vendor` 与 `ro.hardware.egl=mali` —— 与实例 2 运行时逐键对应。

`log-pass.txt`：命名规则 5 例、06 取件用例、`go test ./...` 全绿；run3 构建日志的
`unpack_odm` 取件链与 141 行产物；VM 内 8 条基线 sets 实值、盖回计数 0、
`boot_check` `[pass]`（`sys.boot_completed=1` @ 110.99 s）。

## 四、改动

[engine/build/internal/vendorimg/vendorimg.go:101](file:///Volumes/work/cloud-phone-hub/engine/build/internal/vendorimg/vendorimg.go#L101)
的 `UnpackDir()` 改为**按镜像名推导目录名**，新增
[:110](file:///Volumes/work/cloud-phone-hub/engine/build/internal/vendorimg/vendorimg.go#L110)
`UnpackDirNameFor()`：去扩展名 → 去一层 A/B 槽位后缀（`_a` / `_b`）→ 加 `unpack_` 前缀。

| 镜像 | 目录 |
| --- | --- |
| `vendor_a.img` / `vendor.img` | `unpack_vendor`（历史名不变） |
| `odm_a.img` | `unpack_odm` |
| `vendor_dlkm_a.img` | `unpack_vendor_dlkm` |
| 空串 | 空串（调用方据此判「没有解包树可用」） |

06 侧无需改逻辑：[06_build_odm/plan.go:122](file:///Volumes/work/cloud-phone-hub/engine/build/06_build_odm/plan.go#L122)
本来就是 `plan.UnpackDir = vendorimg.UnpackDir(plan.OfficialImg)`，
[:425](file:///Volumes/work/cloud-phone-hub/engine/build/06_build_odm/plan.go#L425)
`officialOdmProp()` 的「留档优先 → 按镜像逐件解 → 静态树兜底」三级顺序保持原样，
只是第一级不再可能命中别的分区的树；三处注释同步指向本条证据。

为什么不选别的修法：
- 「06 不复用留档，永远逐件解」：能治本机型，但丢掉整树解包复用（cph2797 是 4 GB 级镜像，
  逐件解一次也代价不大，可 gold/oriole 这类 `img: vendor_a.img` 的机型正是靠复用同一棵树，
  改掉等于让它们重解一遍）；
- 「给基线加 `odm.unpack_dir` 字段」：把目录名这个实现细节提升成清单字段，等于允许机型作者
  再写错一次，而目录名本来就能从镜像名唯一推出；
- 「只改 cph2797 的属性件」：那是绕过——问题在引擎侧的目录命名，不在清单数据。

## 五、验证

1. 单测：`go test ./internal/vendorimg/... ./06_build_odm/...`（含新增用例）+
   `go test ./...` → 24 包含 ok、0 FAIL；旧实现下新用例 FAIL、修复后 PASS（负向验证已实跑）；
2. 重编译 `cdh` 并**从 06 起重跑到最终产物**：`device build cph2797 --from build_odm`
   （06→11，exit 0，patch.img sha256 `9a82ee2261c7`）；
3. 产物核对：`odm/etc/build.prop` 141 行（138 + 3 abilist 追加）、四个 GPU 键计数 **0**；
   留档件 `super/unpack_odm/build.prop` 在位；
4. 起机核对（实例 3）：`[pass] 引导完成（日志 17248 行）+ 截图非黑（平均亮度=4.3 峰值=255）`、
   EGL 断言 137 → **0**、surfaceflinger 崩溃 0、`critical process` 0、
   `sys.boot_completed=1` @ 110.99 s，launcher / systemui / surfaceflinger 存活；
5. 停机（`/api/stop`，daemon running 清空）。

失败轮（实例 2）没有截图可归档：SF 与 zygote preload 全程断言自杀，屏幕停在 EFI 阶段。
通过帧随本目录归档：`desktop-pass.png`（1264x2760，平均亮度 4.3 / 峰值 255）与
`desktop-late.png`（长稳后一帧，平均亮度 0.6 / 峰值 255，开机向导深色页）。

## 六、遗留

- 影响面：基线 `odm.img` 声明为 `odm_a.img` 的机型全部受益，本轮实测 m22 / 15r /
  cph2797 / ss9380 / popsicle 属于这一类（`gold`、`oriole` 声明 `vendor_a.img`，
  目录名不变、行为与历史一致）；ss9380 / popsicle 的复验要在各自起机轮次里完成。
- **15r 本轮「通过」是同一种侥幸的另一半**：它的全量运行里 06 先于 07 步骤 1b，
  读到自己刚解出的 123 行 odm 件（`out/tmp/15r-build.log:490`，sha `07c5a726332e`，
  产物 126 行）；而现在同一目录里躺着的是 07 后解出的 **319 行 vendor 件**
  （`grep -n '^ro.hardware.egl=' → 128:ro.hardware.egl=adreno`）。也就是说 15r 只要
  在 07 之后单跑一次 06，就会复现与 cph2797 完全相同的盖回（#151 已锁的
  `egl=mesa` 被 `adreno` 顶掉 → SF 崩溃循环）。修复后 15r 的留档目录是 `unpack_odm`，
  与 07 的树彻底分离，阶段顺序不再影响正确性。
- 留档目录的生命周期仍靠「谁解谁清」维持：`super/unpack_vendor/build.prop` 现在是 07
  整树解包留下的 917 行厂商 vendor 件（run1 那份侥幸正确的 138 行 odm 件已被覆掉），
  修复后 06 不再读那个路径，因此覆掉与否都不影响正确性；但 06 与 07 之间没有
  「必须先解包」的顺序约束，全新工作副本上首跑只跑 06 时靠的是「按镜像逐件解」兜底。
- 「产物层数之间的盖回」目前只有运行态 qemu.log 能看出来。若要把
  「odm 层不得重新声明 vendor 层已收口的键」变成构建期硬校验，需要另开条目。
