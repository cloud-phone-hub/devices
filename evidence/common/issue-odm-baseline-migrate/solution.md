# ss9380 / cph2797 / popsicle / 15r / oriole 的 odm 属性件仍是手抄副本（未进基线 `odm` 段）

- 日期：2026-11-03 / 补 15r + oriole：2026-11-04
- 影响机型：`samsung/ss9380`、`oppo/cph2797`、`xiaomi/popsicle`、`oneplus/15r`、`google/oriole`（5 台，同一条改动 → 进 `common/`）
- 关联：`#144`（12 台 README 与模板对齐时把这 3 台登记为「待迁」、15r 登记为「不可迁」，本轮全部兑现）、
  `#143`（`odm` 段的语义与实现本身）

## 一、问题

三台机型目录里各有一份手抄的 `odm/etc/build.prop`（30 / 144 / 500 行），基线没有 `odm` 段：

| 机型 | 手抄副本 | 基线 `odm` 段 | 06 的属性件来源 |
| :--- | :--- | :--- | :--- |
| ss9380 | 30 行 / md5 `bca0651007108c025720340ca4f19d76` | 无 | 静态树（`selectOdmProps`） |
| cph2797 | 144 行 / md5 `fb4f67b29d2fcdc34993aaff881805a9` | 无 | 静态树 |
| popsicle | 500 行 / md5 `8ba9d7f1a1ed1293266023d551dc0da9` | 无 | 静态树 |

后果不是「构建失败」，而是**静默漂移**：

- 官方 odm 属性件换版后（设备代号、指纹、`ro.odm.build.*`、`ro.vendor.qti.va_odm.support`），
  静态副本不会跟着变，产物里的 `/odm/etc/build.prop` 仍是旧身份；
- 本地增量（锁 64 位 abilist、关 Dolby）没有「相对原件」的表达，只能靠注释说明，后人对账
  得自己 diff 两份文件；
- `#144` 已把 `vendor/build.prop` 全部收敛为「07 现解厂商原件 + 套基线」，odm 侧成了
  唯一的例外，口径不一致。

## 二、根因

1. **迁移欠一次实测**：基线 `odm` 段必须写 `img` + `prop`（镜像名 + 镜像内绝对路径），
   而这三台在迁移当时都没有 odm 解包树（`out/roms/` 是构建产物，不入库），凭猜写错
   会让 06 取不到原件、退静态副本并只打一行 `[warn]`——所以上一轮选择「先留副本」。
2. **06 的落点固定 `odm/etc/build.prop`**：即便 odm 是独立逻辑分区（`odm_a.img` 根下
   有 `etc/`），产物也只落 `etc/`，所以「镜像内属性件在哪个路径」必须实测，不能按
   AOSP 习惯推成 `/build.prop`——而 cph2797 恰好相反：`/etc/build.prop` 只是 4 行
   import 存根，正文在 `/build.prop`。
3. **厂商包形态不一致**：popsicle 的 `super.img` 里就有独立 `odm_a`，cph2797 的 odm 在
   `payload.bin` 里（OTA zip），ss9380 在三星 `AP_*.tar.md5` 的 `super.img.lz4` 里——
   三种路线都要能只抽 odm，才谈得上「不必整包解」。

## 三、证据

`log-fail.txt` 记改前观测与可复跑命令，`log-pass.txt` 记本轮的实测输出。要点：

- 01 支持「只抽 odm」：`RunVendor{Keeps: ["odm.img"]}` → `splitOnly={"odm"}`，整包只还原
  `super/odm_a.img`（ss9380 1.1 MiB / popsicle 3.30 GiB / cph2797 2997 MiB），不必解
  system/product；popsicle 与 ss9380 走 `super.img` 拆 liblp，cph2797 走 payload 分区表
  （`rom.vendor.images` 筛掉 48 个分区，只还原 odm + 恒保留的 vendor_boot）。
- 属性件路径三台不同：
  - ss9380：`odm_a.img` 根下只有 `etc/`，属性件 `odm_a.img:/etc/build.prop`（31 行 /
    md5 `52c42542e5a462602576c1151ed185c5`）；
  - popsicle：根下有 `app/bin/etc/firmware/lib…`，属性件 `odm_a.img:/etc/build.prop`
    （495 行 / md5 `7c4d59708467b71e17b722839a85798e`）；
  - cph2797：根下有 `app/bin/build.prop/etc/firmware/lib/lib64/priv-app/vendor`，
    `odm_a.img:/etc/build.prop` **只有 4 行 import 存根**（`import /odm/build.prop`、
    `${ro.boot.prjname}/build.{gsi.,}prop`、`/mnt/vendor/my_product/etc/…`），正文在
    `odm_a.img:/build.prop`（138 行 / md5 `30786e407078d3269c4cee7e81cb32e8`）。
- 原件与手抄副本的差：ss9380 **只差末尾一个空行**（31 vs 30 行）；popsicle 差 4 行 CDH
  注释 + 4 条 Dolby 属性 `true→false`；cph2797 差 3 行 CDH 注释 + 三条 abilist（第 5–7 行），
  其余 138 行逐字节相同（原件有 2 行行尾是 CRLF，手抄件已归一成 LF）。
- **上一轮「cph2797 固件包残缺」的判断是错的**，错在拿错了文件：
  `core/development/downloader/downloads/CPH2797_…zip` 只有 5,726,808,020 B（下载中断、
  无 zip EOCD）；`roms/devices/oppo/cph2797/` 下那份是 9,250,896,259 B，zip 中央
  目录完整，`payload.bin` 声明 9,250,889,865 B 与实际相符，本轮就是用它解出 odm 的。

## 四、改动

三台基线各加一段 `odm`（位置统一在 `vendor` 后、`vendor-dlkm` 前），并删除手抄副本：

```json
// ss9380：原件已满足 ABI，sets/unsets 皆空
"odm": { "img": "odm_a.img", "prop": "/etc/build.prop",
         "build-prop-sets": {}, "build-prop-unsets": {} }

// cph2797：prop 取 /build.prop（正文），sets = 3 条 abilist + 2 条行尾归一
"odm": { "img": "odm_a.img", "prop": "/build.prop",
         "build-prop-sets": {
           "persist.oplus.bt.em.adb.tool": "true",
           "persist.vendor.oplus.icc.module.enable": "1",
           "ro.odm.product.cpu.abilist": "arm64-v8a",
           "ro.odm.product.cpu.abilist32": "",
           "ro.odm.product.cpu.abilist64": "arm64-v8a" },
         "build-prop-unsets": {} }

// popsicle：原件自带 abilist，增量只有 4 条 Dolby
"odm": { "img": "odm_a.img", "prop": "/etc/build.prop",
         "build-prop-sets": {
           "ro.vendor.display.dolbyvision.support": "false",
           "debug.config.media.video.dolby_vision_suports": "false",
           "debug.config.media.video.dolby_capture_supports": "false",
           "debug.media.video.dolby_vision_dvs_support": "false" },
         "build-prop-unsets": {} }
```

- cph2797 的 `prop` 取 `/build.prop` 的理由：06 落点固定 `odm/etc/build.prop`，运行态这个
  路径被 `modules.redirect` 的文件规则指到 `/mnt/vendorpatch/odm/etc/build.prop`（只覆盖
  这一个文件，不含 `/odm/build.prop`），照抄 4 行存根会让产物只剩 import；摊平正文与迁移
  前的副本口径一致。两条「同值覆盖」是为了不让原件的 CRLF 带进产物（06 不改写行尾）。
- 三份 `odm/etc/build.prop` 与随之空掉的 `odm/` 目录删除（`odm/` 只剩这一个文件）；
- 三台 README 的 `odm/etc/build.prop` 小节改为「基线 `odm` 段，本目录不留副本」，
  写清实测命令、原件行数/md5、增量来源与迁移代价；表格行、维护约定、已知缺口同步；
- `manifest/README.md` 的 13 台对齐表：三台 `odm` 列由「待迁」改「有」，静态树件数
  改为 `18·0·2` / `3·0·1` / `2·0·1`；「待迁」说明段写清 cph2797 取 `/build.prop` 的差异。

## 五、验证

1. **06 端到端实跑**（真清单 + 只放 odm 的假解包根，`CDH_MANIFEST=… cdh build-odm`）：
   - popsicle：`基线 odm 属性: odm_a.img:/etc/build.prop（sets 4 项 / unsets 0 项）`，原件
     495 行 / sha256 `2545d4789a3c`，4 条 `[prop] 覆盖 …=false`，产物 495 行 /
     sha256 `518213f8d4ef`，配方 `odm-75e6bec07719`；
   - ss9380：`sets 0 项`，`厂商原件与基线增量已一致`（31 行 / sha256 `c87db65f1c73`），
     配方 `odm-2954e68cd96d`；
   - cph2797：`sets 5 项`，原件 138 行 / sha256 `0606e97aa3aa`，2 条 `[prop] 覆盖` + 3 条
     `[prop] 追加`，产物 141 行 / sha256 `cfc266f70d54`，配方 `odm-980cd98b019c`；
   - 三台产物均 `odm.img` 8388608 B、`回读校验通过`（mode/uid/gid/标签一致）。
2. **产物与改前手抄件逐行对照**：
   - popsicle `diff` **只少 5 行 CDH 注释**，属性值逐条一致；
   - ss9380 **只多末尾一个空行**（语义等价）；
   - cph2797 少 3 行 CDH 注释、abilist 从第 5–7 行挪到文件末尾，**属性值逐条一致**（含
     两条 CRLF 已归一）。
3. **cph2797 的 odm 路径反证**：把 `prop` 改回 `/etc/build.prop` 再跑 06，原件只有 4 行 /
   sha256 `c968cfee899f`，5 条 sets 全部 `[prop] 追加`，产物只剩 **9 行**（与迁移前 144 行正文
   差 135 行）——这正是「按 AOSP 习惯猜路径」的代价，所以实测值写进基线 `odm.note`。
4. **失败面显式化**：在缺 `odm_a.img` 的解包根上跑 06 → `[err] 取不到 odm 属性件：镜像
   …/super/odm_a.img 里没有 /etc/build.prop，机型静态树也没有
   manifest/oppo/cph2797/odm/etc/build.prop（先跑 01_unpack，或核对清单 odm.img / odm.prop）`。
   改前这种情况会用静态副本静默继续。
5. **静态校验**：17 份基线 JSON 全解析（7 份有 `odm` 段：m22 / cph2797 / ss9380 / gold /
   popsicle / 15r / oriole），机型目录残留静态 odm 属性件 **0**。
6. `go build ./...`、`go test -count=1 ./06_build_odm ./internal/proplines` 全绿。

## 六、遗留

- 五台都只跑了 `06_build_odm`，未重跑 `07 → 09 → 11`，也未起机复验 `/odm/etc/build.prop`
  的装载结果（属性值逐条相同，预期无差异）。
- **cph2797 的另一处混淆待清**：`core/development/downloader/downloads/` 下那份中断的
  `CPH2797_…zip`（5.33 GiB）仍留在盘上，名字与完整包完全相同。
- `oriole` 的 `odm/etc/build.prop` 已迁入基线（本节补记）。残留静态件清零。
- 06 的 `warnOdmStaticDrift` 现在对 5 台永远无事可做（静态副本已删）——留着。

## 七、补记：15r 于 2026-11-04 迁入

15r 最初在 #144 里被登记为「不可迁」，理由是 06 落点固定 `etc/`、迁移等于换路径。
本轮实测推翻了那个结论：

- AOSP init 硬编码 `import /odm/etc/build.prop`（不依赖 `/odm/build.prop` 存在）；
- 15r 的 `dir /odm` 整棵重定向让 patch.img 里任何子路径都能被运行态读到，
  06 产 `odm/etc/build.prop` → patch → `/odm/etc/build.prop` 可达；
- 存根里的 import 目标（`${ro.boot.prjname}/build.gsi.prop`、`my_manifest`）在云机全部 ENOENT。

与 cph2797 完全同形态：`odm_a.img:/build.prop` 正文 123 行 + `/etc/build.prop` 4 行存根。
sets 12 项：1 条 wsa 改动 + 8 条同值归 CRLF + 3 条 abilist 追加。
产物 126 行 sha256 `6eae2cff47ce`，配方 `odm-ea17804e2cf2`。
静态件 `manifest/oneplus/15r/odm/build.prop` 已删（`git rm`）。

## 八、补记：oriole 于 2026-11-04 迁入

oriole 是第五台，也是形态最特殊的一台：**Pixel 6 没有独立 odm 分区**。

- 官方工厂包是 zip 套 image-*.zip（平铺分区，没有 super.img、没有 payload.bin）。
  01 走 flatTarget 路线落盘 `super/<name>_a.img`。
- `vendor.img` 是 ext4（非 EROFS），`/odm/etc/build.prop` 是 vendor 镜像内的子目录。
  用 debugfs 取得（`debugfs -R "cat /odm/etc/build.prop" super/vendor_a.img`）。
- 官方原件 30 行 / md5 `df5fec0b557a53f503f54b3486146617`；`abilist=arm64-v8a,armeabi-v7a,armeabi`、
  `abilist32=armeabi-v7a,armeabi`（官方带 32 位）。
- 基线 `odm` 段：`img: vendor_a.img`、`prop: /odm/etc/build.prop`、sets 3 条（同 gold/m22 口径，纯 64 位）。
- 06 实跑：产物 30 行 / sha256 `365dd76a2699`，配方 `odm-1952c5fcaaba`；`abilist64` 官方已是 `arm64-v8a`，所以只有 2 条 `[prop] 覆盖` 日志。
- 静态件已删（31 行 / md5 `f08ab45d…`，多两行装饰性注释 `#  start`/`#  end`）。
- 4 份基线均写入 odm 段（同一套 sets，所有版本线统一锁口径）。
- 至此 **全仓残留静态 odm 属性件为 0**，13 台机型均无 `odm/etc/build.prop` 手抄副本。
