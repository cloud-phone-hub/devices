# 内核树回上游：改动外置为补丁集（`kernels/patches/`）+ 02 构建期施加

## 一、问题

`devices/kernels/` 下四棵树（21-6.1 / 22-6.6 / 22-6.12 / 23-6.12）都直接在自己的分支上提交
CDH 改动，与 LineageOS 上游漂移，导致：

- 同步新上游时无法直接 fast-forward，只能手工 rebase 一堆 CDH 提交；
- 看不出「哪一行是我们加的、为什么加」——改动散在 12/16/4 个文件里、夹在 50 条提交中；
- 目标要求：分支回到与上游一致，改动以**模块或补丁**形式落在 `devices/modules` / `devices/kernels/patches`。

## 二、根因

改动内容决定了外置方式，四类改动里只有一类真能做成外部模块：

| 改动 | 位置 | 能否变 `.ko` | 原因 |
| :-- | :-- | :-- | :-- |
| pioneer permissive | `security/selinux` | 可以 | 只需 vmlinux 的 kprobe API |
| composer fd 0 | `fs/open.c`、`fs/file.c`、`binder.c` | 勉强（kprobe 改参数/返回值，热路径） | 语义依赖 VFS 内部 |
| virtio-gpu（含 vram 16 KiB 对齐） | `drivers/gpu/drm/virtio/` | **不能** | `CONFIG_DRM_VIRTIO_GPU=y` 内建；外移等于 fork 整个驱动，且同 device 只能绑一个 driver |
| 模拟 WiFi | `mac80211_hwsim.c` | **不能**（现有流水线） | 依赖 `ieee80211_*`/`cfg80211_*`，而 02 只产出 `out/vmlinux.symvers`，模块间导出不可见 |

所以方案定为：**内核分支 = 纯上游；改动 = 按树分目录的补丁集；02 在构建卷内施加**。

## 三、证据

见 `log-fail.txt`（改动前状态）：

- 净 diff：22-6.6 12 文件 +1354/−13；22-6.12 16 文件 +1398/−15；23-6.12 4 文件 +1122/−2；
  21-6.1 净 0（路径重定向已回退给 `cdh_redirect.ko`）。
- `out/kernel/lineage-23-6.12/Module.symvers` 共 19471 行，第三列**全部是 `vmlinux`**；
  `ieee80211_*` / `cfg80211_*` / `virtio_gpu_*` 导出数均为 0。
- 02 源码注释与 `cp out/vmlinux.symvers export/Module.symvers` 印证：只编 Image 不产生模块导出表。

## 四、改动

1. `devices/kernels/patches/`（新）
   - `README.md`：目录表、上游基线、为什么不全做成模块、应用/生成方式、有意不收录的三处。
   - `lineage-22-6.6/`：3 个补丁 + `commits.txt`（50 条提交溯源）+ README
     （初版为 4 个，composer fd0 补丁后经用户判定丢弃而删除，见第七节）。
   - `lineage-22-6.12/`：3 个补丁 + `commits.txt` + README（同样删去 fd0 补丁）。
   - `lineage-23-6.12/`：3 个补丁 + `commits.txt` + README。
   - `lineage-21-6.1/`：无补丁，README 说明为什么是空的 + `commits.txt`。
2. 四棵树 `git tag cdh-backup-20261102` 后 `git reset --hard <上游基线>`：
   21-6.1 → `7012e868a414`、22-6.6 → `bf3c4b5c67f89`、22-6.12 → `afb0ad713e3c`、23-6.12 → `4f6bf47200d9`。
3. `02_build_kernel/plan.go`：新增 `--patches`/`CDH_KERNEL_PATCHES`、`resolvePatches`
   （缺省 `<源码同级>/patches/<tag>`）、`patchDigest` 并把 `patches` 纳入配方（四键 → 五键）、
   计划输出增 `内核补丁:` 行。
4. `02_build_kernel/kernel.go`：补丁随配置片段进同一次内存 tar（`.cdh/patches/`）；
   `containerScript` 在配置之前**先全体 `git apply --check` 再统一 `git apply`**，失败即中止。
5. `02_build_kernel/README.md`：步骤 3/4、参数表、关键约定同步。

## 五、验证

见 `log-pass.txt`：

- 每个补丁对上游基线 `git apply --check` 全部通过；把 patch 应用到「从基线解出的同名文件」后
  与原工作树 `cmp` **逐字节一致**（证明补丁无损）。
- `go build ./...` 与 `go test ./02_build_kernel/` 通过。
- `build-kernel --dry-run`（gold / lineage-22-6.6）输出 `==> 内核补丁: 4 个`，
  配方由 `lineage-22-6.6-cdh-72b2c01b0d4a` 变为 `…-3ca8e19d2f59`，缓存状态判定为「需重建」。
  （删去 fd0 补丁后为 3 个、配方 `…-fe3aded55694`，见第七节。）
- 四棵树 `git status` 干净、`git diff <基线> HEAD` 为空、树内 `cdh_perm_types` /
  `cdh_sigkill` / `CONFIG_CDH_PIONEER_PERMISSIVE` 残留为 0。

## 六、遗留

1. **整链重跑未做**：补丁后需按 02→03→05…11 重编并起机，确认产物与补丁前一致
   （内核 Image 会因树 HEAD 变化重编，配方已自动失效）。
2. **22-6.12 无机型引用**，是否保留整棵树待定；保留补丁是为了将来接机型不重做。
3. **有意丢弃**：`kernel/signal.c` 的 `cdh_sigkill` 临时探针（22-6.6 / 22-6.12 各 10 行）、
   22-6.12 的 `CDH.md` 与 `cdh/*.json`。需要时可从 `cdh-backup-20261102` 取回。
4. **pioneer 模块化仍可选**：做成 `modules/cdh_pioneer` 后，删掉各树的
   `0001-selinux-pioneer-permissive.patch` 即可（已调研过 kprobe 路线）。
   > 后续（2026-11-02）：该补丁已整体删除，permissive 改由 vendor CIL 表达，见第九节。
5. `03_build_modules` 复用同一构建卷，补丁已在卷内生效；若引入「只跑 03」的独立入口，
   需确认它不会拿到未打补丁的卷。

## 七、后续：删掉 composer fd0 补丁（2026-11-02）

用户判定 `0002-composer-fd0-ebadf.patch`（`fs/open.c` / `fs/file.c` / `drivers/android/binder.c`，
83 行）是已经丢弃的试验性改法，不进补丁集。该补丁靠 `comm` 串匹配给 MTK composer 线程
保底 fd ≥ 3、在 fd 0 上兜 `/dev/null`；在 VFS 热路径上按线程名分支本身也不可接受。

- 删除两棵 22.x 树的 `0002-composer-fd0-ebadf.patch`（22-6.6 83 行、22-6.12 同源但上下文随
  6.12 基线漂移），其余补丁顺延编号：`0002-virtio-gpu-import-damage.patch`、
  `0003-hwsim-fake-wifi.patch`。两棵树的 README 均把它移入「有意不收录」并写明取回方式；
  总 README 表 22-6.6 / 22-6.12 补丁数均 4 → 3。
- 复查：`verify.sh` 剩余 3 个补丁对基线 `apply --check` 全通过，且与
  `cdh-backup-20261102` 逐字节一致；四棵树工作区干净、CDH 残留 0；
  `go test ./02_build_kernel/`、`gofmt`、`go vet` 干净。
- dry-run（gold / 22-6.6）：`内核补丁: 3 个`，配方
  `lineage-22-6.6-cdh-72b2c01b0d4a` → `…-fe3aded55694`，缓存状态「需重建」。

用户确认后，`lineage-22-6.12` 里的同名补丁也一并删除（该树仍无机型引用，
不影响任何在用产物）。两树 fd0 的溯源提交均为 `8731c2bc90aec`、`7bb68fb72ea62`、
`d209e5abe274f`（提交在 `cdh-backup-20261102` 上，随时可取回）。
## 八、后续：21-6.1 移植 virtio-gpu 与 hwsim 两个补丁（2026-11-02）

`lineage-21-6.1` 原本是四棵树里唯一「零补丁」的（历史净改动为 0），因此云机侧
Pixel 6 那条线拿不到 virtio-gpu 的 imported dma-buf 修补与假 WiFi。按用户要求把
22-6.6 的 `0002`/`0003` 移植过去。

- **移植方式**：把 22-6.6 的两个补丁 `patch -p1 -F3` 打到基线 `7012e868a414` 上
  （hunk 全部命中，仅 6.1 的 `virtgpu_display.c` 有少量上下文差异）→ 容器内编译 →
  `git diff` 导出本目录两个补丁 → `git checkout` 把树还原成上游基线。
- **路径差异**：6.1 的 hwsim 还在 `drivers/net/wireless/` 根下（6.5 起才移到
  `virtual/`），补丁里只有路径与 hunk 行号不同，代码本身与 22-6.6 同一套
  （`diff` 两者只差 42 行，全是文件头与 hunk 头）。
- **API 差异**：virtio-gpu 需要的 `virtio_gpu_object_shmem_init`、`virtio_gpu_resource_id_put`、
  `virtio_gpu_cmd_create_resource`、`virtio_gpu_object_attach`、`virtio_gpu_translate_format`
  在 6.1 上全部存在且签名一致，只需把前两个由 `static` 改为导出。
- **编号**：仍用 `0002`/`0003`（与 22-6.6 对齐），**没有 `0001`**——6.1 的 pioneer
  permissive 至今靠机型侧 `vendor_sepolicy.cil` 的 `(typepermissive pioneer)`，
  `CONFIG_CDH_PIONEER_PERMISSIVE=y` 在 6.1 上被 `olddefconfig` 静默丢弃。`0001` 是留给
  将来移植 pioneer 补丁的位置。
  > 后续（2026-11-02）：四棵树的 `0001` 统一给了 virtio-gpu、`0002` 给 hwsim，
  > `CONFIG_CDH_PIONEER_PERMISSIVE` 已从配置片段删除，见第九节。
- **验证**（见 `log-pass.txt` 的「验证 7」，脚本 `verify-port-21-6.1-build.sh`）：
  - 两个补丁对基线 `git apply --check` 通过；应用后与移植时的文件逐字节一致；随后还原，
    树 `git status` 干净、HEAD 仍为 `7012e868a414`。
  - 容器（`ghcr.io/feicong/droid-ddk:android15-6.6`，arm64）内以
    `gki_defconfig` + `lineageos/virtio.config` + `lineageos/peripheral/wifi.config`
    + 本项目 `lineage-kernel.config` 配置后编译 `drivers/gpu/drm/virtio/` 与
    `drivers/net/wireless/mac80211_hwsim.ko`：**0 error / 0 warning**。
  - 环境坑：6.1 的 `tools/bpf/resolve_btfids` 里的 libbpf 过不了新版 clang
    （`strchr` 隐式声明），与补丁无关；验证时关掉 `DEBUG_INFO_BTF` 即可。
- **遗留**：`google/oriole` 尚未接入流水线（工厂包未接 01_unpack、android-version 与
  内核版本待取证），所以没有编完整 Image，也没有起机；运行时语义（display 斜纹是否
  消失、假 WiFi 是否连上）要等 oriole 接进来再复验。

## 九、后续：pioneer permissive 从内核搬到 sepolicy（2026-11-02）

用户判定 patches 里的 pioneer 补丁不进内核（permissive 本来就是策略语义），改由 SELinux
策略承接。这同时解决了第二节里那条「`0001` 留给 pioneer」的悬空编号。

### 9.1 从补丁集移除

| 树 | 删除 | 顺延后 |
| :-- | :-- | :-- |
| 22-6.6 | `0001-selinux-pioneer-permissive.patch`（96 行） | `0001-virtio-gpu-import-damage` / `0002-hwsim-fake-wifi` |
| 22-6.12 | 同上（96 行） | 同上 |
| 23-6.12 | 同上（81 行，只标 `pioneer`） | `0001-virtio-gpu-vram-16k-align` / `0002-hwsim-fake-wifi` |
| 21-6.1 | —（本来就没有） | 原 `0002`/`0003` → `0001`/`0002`，与其余三棵对齐 |

- `build/02_build_kernel/lineage-kernel.config` 删掉 `CONFIG_CDH_PIONEER_PERMISSIVE=y`
  （54 → 51 行），`plan.go` 注释与 02 的 README 表同步；
- 各树 README 把它移进「有意不收录」并写明取回方式
  （`git -C devices/kernels/<tree> show cdh-backup-20261102:security/selinux/ss/services.c`）。

### 9.2 策略侧承接

| 类型 | 落点 | 说明 |
| :-- | :-- | :-- |
| `pioneer` | 模块层 `engine/vendor/hardware/android.service.daemon/sepolicy/vendor_sepolicy.cil` | `(typepermissive pioneer)` 自 2026-10-28 就在（`issue-m22-vendor-cil-resolve` 的 m22 修法，与 gold 一致），四机型共用；本次只把注释里「内核侧 CDH_PIONEER_PERMISSIVE 靠它定位」改成「内核补丁已退场，这里是唯一来源」 |
| `crash_dump`、`surfaceflinger` | 机型层 `devices/manifest/xiaomi/gold/sepolicy/vendor_sepolicy.cil` | 新增两行。只在 gold 放宽（当年 22-6.6 的补丁也只对 gold 生效）；23.2 线的 m22/cph2797/ss9380 历史上只标 `pioneer`，不跟着加 |

两个平台域在 plat 里就是裸名声明（`(type crash_dump)` @1524、`(type surfaceflinger)` @4153），
运行期 init 合并时直接可见，不需要 `{{vers}}` 影子名。

### 9.3 依据：原 Kconfig help 里的说法不成立

补丁的 Kconfig help 写「本盘 init 内嵌 secilc 不支持 CIL permissive 语句」，但实测相反：

- gold 的 vendor 基底（lineage 22.2）自己就带着 `(typepermissive shell)`，起机正常；
- 本模块的 `(typepermissive pioneer)` 从 2026-10-28 起随镜像下发，m22 实例 8 起
  「Compiling SELinux policy → Loading SELinux policy」一次过（`issue-m22-vendor-cil-resolve`）；
- 本次用 secilc 3.9 直接编四份真 CIL，带 `typepermissive` 的版本 rc=0，permissive 集合正确。

所以内核侧那套 `permissive_map` 强标是重复机制，删掉不损失能力，还省掉一个 Kconfig 开关
和一处 `security/selinux` 改动。

### 9.4 验证（`log-pass.txt` 的「验证 8」）

| # | 验证点 | 结果 |
| :-- | :-- | :-- |
| 1 | `verify.sh` 四棵树 | 补丁对基线 `apply --check` 全通过；22-6.6 / 22-6.12 / 23-6.12 剩余 2 个补丁与 `cdh-backup-20261102` **逐字节一致**；21-6.1 移植补丁可应用（跳过 cmp）；四棵树脏文件 0、`cdh_perm_types`/`cdh_sigkill`/`CONFIG_CDH_PIONEER_PERMISSIVE` 残留 0 |
| 2 | secilc 实测（脚本 `verify-permissive-cil.sh`） | 从 lineage-22.2 基底取 plat（2412881 B，sha256 `81a8a08f76fceef0`）与 plat_pub_versioned（371390 B，`f07d21e7bc4dace6`），配 07 产出的 `vendor_sepolicy.cil`（即经 09 改写后的那份，验证口径同 #58）编两遍：改前/改后 **rc=0**；permissive 3 → 5：`['pioneer','shell','su']` → `['crash_dump','pioneer','shell','su','surfaceflinger']`；`sediff` 只报这两型 `Added permissive` |
| 3 | dry-run | gold（22-6.6）`内核补丁: 2 个`、配方 `72b2c01b0d4a → b462c4518242`；m22（23-6.12）`2 个`、`0c0573720acb → edfc30e4c8fa`；21-6.1 仍 `2 个`（oriole 推不出镜像标签是已知缺口） |
| 4 | `go test ./02_build_kernel/ ./07_build_vendor/` | ok |

### 9.5 遗留

1. **没重编、没起机**：permissive 的运行时效果按策略语义成立（`pioneer` 早就在起机实例上
   以 CIL 生效），`crash_dump` / `surfaceflinger` 两条要等整链重编后看 avc 是否
   `permissive=1`。
2. 验证镜像 `cdh-secilc:latest` 是本机自建的（`ubuntu:26.04` + `secilc 3.9` + `checkpolicy`
   + `setools` + `e2fsprogs`），不入仓库；重建命令写在脚本头。
3. `22-6.12` 仍无机型引用，将来接机型时 `crash_dump` / `surfaceflinger` 要不要放宽按当时
   需要定，模板在 gold 的机型层片段里。
