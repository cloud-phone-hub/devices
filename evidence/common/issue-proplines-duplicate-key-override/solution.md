# proplines 对重复键「只改第一处」→ 运行时被后读盖回，清单收口失效

- 范围：引擎级（`engine/build/internal/proplines`，06 odm 与 07 vendor 共用）
- 首发机型：oneplus/15r 与 oppo/cph2797（本轮 cph2797 起机时定性，15r 同形态未触发）
- 发现时间：2026-11-04
- 编号：#152
- 状态：已闭环

## 一、问题

cph2797 首轮起机（实例 1）`sys.boot_completed` 永不置位，`boot_check.py` 判
`[fail] 超时 600s 未出现 sys.boot_completed=1（当前日志 71314 行）`。
排查 GPU 后端时发现：**基线清单里写了 `ro.hwui.use_vulkan=false`，产物 `vendor/build.prop`
里也确实有这一行，但 VM 里读到的是厂商真值 `true`**。也就是说「按清单装配」这个口径
在这类键上是假的——日志显示改动执行了、产物里也有基线值，运行时却不生效。

难点在于这一层与「清单没写」「属性件没落位」长得一样：`grep` 产物能看到基线值，
只有读 init 的覆盖留痕才能看出问题。

## 二、根因

1. 厂商 vendor `build.prop` 里同名多行是常态，不是异常。cph2797 的厂商原件 917 行，
   `sort | uniq -d` 出 30+ 个重复键，基线 sets 命中的键里有两个撞上了：
   `ro.hwui.use_vulkan`（第 45 行空值、第 243 行 `true`）、
   `ro.vendor.build.security_patch`（第 42、240 行）。

2. `proplines.Apply` 的旧实现按「命中即改第一处」处理（见改动前的
   [internal/proplines/proplines.go:127](file:///Volumes/work/cloud-phone-hub/engine/build/internal/proplines/proplines.go#L127)
   的 `Apply`）：第一处写成基线值，后面的重复行原样留着。

3. Android init 读 build.prop 是**后读覆盖先读**（本项目的既有口径见 #127，运行态留痕
   形如 `init: Overriding previous property 'k':'旧' with new value '新'`）。同一份文件
   内后面的重复行同样按此规则生效，于是第 243 行的厂商 `true` 把第 45 行刚写进去的
   基线 `false` 盖了回去，`getprop` 读到 `true`。

4. 后果不是「值不对」这么温和：`use_vulkan=true` 让 libhwui 选 `SkiaVulkanPipeline`，
   而云机没有厂商 Vulkan 驱动——同链条在 15r 上的形态是 battery/launcher/systemui
   三进程 `gpuCount` assert 自杀、屏幕全黑（#151）。cph2797 本轮引导更早死在 FRP 卡点
   （#153），所以这一条的渲染侧后果在本机型没有直接现场，定性依据到
   「产物两行、运行时读最后一行、清单收口没生效」为止。

## 三、证据

- 修复前现场：`log-fail.txt`
  - 厂商原件里同键两行的行号（45/243、42/240）；
  - 修复前构建日志的 `[prop] 覆盖 ro.hwui.use_vulkan=false`（无「删除重复」字样）；
  - run1 qemu.log 第 645 行 `Overriding previous property 'ro.hwui.use_vulkan':'false' with new value 'true'`；
  - 同一轮 22 条覆盖留痕逐条比对：只有本条方向是「基线值 → 厂商原值」。
- 修复后现场：`log-pass.txt`
  - 单测 `TestApplyCollapsesDuplicateKeys` 等 6 条全 PASS，`go test ./...` 24 包含 ok、0 FAIL；
  - 构建日志出现 `[prop] 覆盖 ro.hwui.use_vulkan=false（并删除 1 行重复）`、
    `[prop] 覆盖 ro.vendor.build.security_patch=2026-06-01（并删除 1 行重复）`；
  - 产物里四键各只剩一行（45/76/373/375）；
  - VM 内 `[ro.hwui.use_vulkan]: [false]`、`[ro.hardware.egl]: [mesa]` 等 8 条基线 sets 全部实值；
  - 覆盖留痕里 `ro.hwui.use_vulkan` 计数 1 → **0**，实例 3 判
    `[pass] 引导完成（日志 17248 行）+ 截图非黑（平均亮度=4.3 峰值=255）`，
    `sys.boot_completed=1` 落在 110.99 s。

## 四、改动

[engine/build/internal/proplines/proplines.go:127](file:///Volumes/work/cloud-phone-hub/engine/build/internal/proplines/proplines.go#L127)
的 `Apply`：先按 unsets 删行（不变），再按 sets 覆盖/追加时对每个键统计同名行数，
命中多行时**第一处写基线值、其余重复行删除**，改动说明记
`覆盖 <k>=<v>（并删除 N 行重复）`。

选择「删重复行」而不是「改所有同名行」：前者让产物里该键只剩一个答案，运行时读不到
第二个值，日志也不会再有 `Overriding` 留痕；后者仍是两行同值，任何后续工具（人工比对、
diff、别的消费方）都可能只看到其中一行。清单是本包的唯一真值，产物必须与之一一对应。

`Abi()` 保持取最后一行的语义不变——那是「读」侧按 Android 属性语义判定，不是「写」侧。

配套测试：[proplines_test.go:34](file:///Volumes/work/cloud-phone-hub/engine/build/internal/proplines/proplines_test.go#L34)
`TestApplyCollapsesDuplicateKeys`（三行同名、首行已等值时仍收口成一行）；
被替换掉的旧用例 `TestApplyOverwritesFirstOccurrence` 断言的正是「第二处不动」的旧语义。

不改数据侧（把基线改成只覆盖第一处、或把厂商件先手工去重）：那等于承认「清单值可能不生效」，
而且每台机、每个键都要人肉排查，重复键的规模（30+ 个键）决定只能由公共包兜住。

## 五、验证

1. `go test ./internal/proplines/... -v` → 6 条 PASS；`go test ./...` → 24 包含 ok、0 FAIL；
2. 重编译 `cdh` 并**重跑到最终产物**（`device build cph2797 --from build_odm`，
   06→11 全链，patch.img sha256 `9a82ee2261c7`）；
3. 装配树核对：`out/devices/oppo/cph2797/16.0.8.301_EX01/vendor/build.prop` 四个 GPU/补丁键
   各 1 行、值为基线值；
4. 起机核对（实例 3）：VM 内 8 条基线 sets 全部实值、`Overriding ... 'ro.hwui.use_vulkan'` 0 条、
   `sys.boot_completed=1`（110.99 s）、截图非黑、`Fatal signal` 0、`critical process` 0；
5. 停机。

## 六、遗留

- 本改动只治「同一份文件内同名多行」。跨层的盖回（vendor 写了、odm 又写一遍厂商值）
  不在此路径内，那是 #154 的问题。
- 厂商件里其余 30+ 个重复键本轮没有被基线 sets 命中，行为不变；将来命中即自动收口，
  但**清单作者看不到收口是否发生**，只能读构建日志的「并删除 N 行重复」——
  如果以后要把它变成硬约束（例如产物里出现重复键就报错），需要另开条目。
- 未复跑 gold / m22 / oriole 的构建，这三台在收口路径上的构建留痕尚未回填（见 log-pass §4）。
