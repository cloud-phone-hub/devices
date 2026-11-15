# debugfs 批处理按空白分词，含空格的文件名必然丢元数据（07/09 双阶段命中）

- 设备：flyme/m22（MEIZU 22，Flyme 12.5.0.0A / Android 16 / QTI `sun`）
- 组件：`engine/build/internal/e2fs`（新增）+ `engine/build/07_build_vendor/pack.go` + `engine/build/09_build_patch/meta.go`、`execute.go`
- 产物：`vendor.img`（180,748,288 B，587 条目）、`patch.img`（402,653,184 B，823 条目）

## 问题

07 的步骤 7 打包阶段，`mke2fs -d` 建好镜像后要用 `debugfs -f <脚本>` 回填 mode/uid/gid/
`security.selinux`。厂商 vendor 树里有两个带空格的文件（`find -name "* *"` 全树只有这两条）：

```
vendor/usr/idc/QEMU Virtio MultiTouch.idc
vendor/usr/idc/QEMU Virtio Tablet.idc
```

debugfs 脚本按空白分词，这两个文件的命令全部被拆成多个参数（每个文件 7 条命令：
`sif×3` + `ea_set` + `stat` + `ea_get` 等，14 条失败正对应两个文件）：

```
[warn] debugfs 有 14 条指令执行失败（前 5 条）: sif: Usage: sif <file> field value / …
[err] vendor.img 元数据未落地：2/587 个条目与规则表不符（/usr/idc/QEMU Virtio MultiTouch.idc 无 stat 输出；/usr/idc/QEMU Virtio Tablet.idc 无 stat 输出）
```

同样的缺陷在 09 复现（那次更隐蔽：`mkfs.ext4 -d` 已把内容写进去，只有属主/标签回读不一致）：

```
[err] 阶段 build_patch 失败: 回读校验失败：2/823 条不符（/vendor/usr/idc/QEMU Virtio MultiTouch.idc 回读缺失；/vendor/usr/idc/QEMU Virtio Tablet.idc 回读缺失）
```

## 根因

`debugfs -f` 的每行脚本用空白（空格/Tab）切分参数，**没有引号/转义语法**，文件名本身
带空格就无法表达。受影响的四个命令都要求精确 filespec：

| 命令 | 分词后的实际报错（实测原文） |
| :--- | :--- |
| `stat /usr/idc/QEMU Virtio Tablet.idc` | `stat: Usage: stat <file>`，无输出 → 回读缺失 |
| `sif /usr/idc/QEMU Virtio Tablet.idc mode 0100644` | `sif: Usage: set_inode <inode> <field> <value>` |
| `ea_set -f <值文件> /usr/idc/QEMU Virtio Tablet.idc security.selinux` | `Usage:  ea_set [-r] <file> <attr> <value>`（以 `-f` 形式印 usage，实为多余参数被拒） |
| `ea_get /usr/idc/QEMU Virtio Tablet.idc security.selinux` | 只取到前半截路径 → 取不到标签，按兜底标签写入 |

「少了元数据」不会让 07 报错退出——如果没有回读校验，产物会静默变成 root:root +
无标签的镜像，直到引导期 `setxattr`/访问被拒才暴露。

## 证据

`log-fail.txt` 两段实跑原文。逐项实测的 debugfs 行为（本机 e2fsprogs 1.47.4，
`/opt/homebrew/opt/e2fsprogs/sbin/debugfs`）：

```
$ debugfs -f s.txt /tmp/v.img
  (s.txt) sif /usr/idc/QEMU Virtio Tablet.idc mode 0100644
  sif: Usage: set_inode <inode> <field> <value>
  (s.txt) stat /usr/idc/QEMU Virtio Tablet.idc
  stat: Usage: stat <file>
  (s.txt) ea_set -f val.txt /usr/idc/QEMU Virtio Tablet.idc security.selinux
  Usage:  ea_set [-r] <file> <attr> <value>
```

而 `<inode>` 形式的 filespec 全部生效：

```
$ debugfs -w -f i.txt /tmp/v.img          # 改 mode 到 0600 再 stat 验证确实写入
  sif <597> mode 0100600
  stat <597>
  Inode: 597   Type: regular    Mode:  0600   Flags: 0x80000

$ debugfs -f e.txt vendor.img             # 只读查看标签
  ea_list <597>
  security.selinux (25) = "u:object_r:vendor_file:s0"
```

inode 从哪来：`debugfs ls -l <父目录>` 的输出**首列就是 inode、末列是完整文件名**，
中间是 mode(类型)/uid/gid/size/date/time，所以只能「按行尾等于 ` ` + 相对名」匹配，
不能拿空白分词后的字段当名字：

```
$ debugfs -f l.txt vendor.img
  (l.txt) ls -l /usr/idc
      596  100644 (1)      0      0     305  1-Jan-2009 08:00 QEMU Virtio MultiTouch.idc
      597  100644 (1)      0      0     305  1-Jan-2009 08:00 QEMU Virtio Tablet.idc
      598  100644 (1)      0      0     476  1-Jan-2009 08:00 tablet2multitouch.idc
```

## 改动

新增共享件 `engine/build/internal/e2fs/filespec.go`：

```go
func Filespecs(debugfs, image string, paths []string) (map[string]string, error)
```

- 不含空白的路径**原样返回**，命令文件保持可读、可 diff；
- 含空白的路径按父目录归组，一次 `debugfs -f` 跑完全部 `ls -l`（不是每个文件起一次进程）；
- 解析规则：`ls -l` 行按 `strings.HasSuffix(trimmed, " "+base)` 匹配，`base` 用
  `path[len(current)+2:]` 取当前目录下的相对名，inode 取 `strings.Fields(trimmed)[0]`；
- 父目录自身含空白 → 直接报错「路径 %q 的父目录含空白」，不允许静默漏写；
- 解析不全 → 报错「debugfs 未能定位 N/M 个含空白路径的 inode」，附一条样例路径。

调用侧：

- `07_build_vendor/pack.go`：新增薄包装 `imageFilespecs`（按 `entries` 组装 `"/"+entry.Path`，
  返回等长切片供写入/校验按下标取用）；`verifyImageMeta` 增加 `filespecs` 参数，
  内部建 `display` 映射使报错显示真实路径而不是 `<15>`。
- `09_build_patch/meta.go`：`readImageMeta` 先取 `specs`，再构造 `targets` 切片与
  `pathOf` 反查表（`spec -> 原路径`），`stat`/`ea_get` 脚本用 `targets`，解析结果按
  `specs[path]` 存，标签解析回来时经 `pathOf` 还原路径。
- `09_build_patch/execute.go`：`buildMetaCommands` 用 `specs[entry.Path]` 生成
  `sif` 与 `ea_set -f`。

## 验证

`log-pass.txt`：

- 07：打包阶段无 `[warn] debugfs 有 … 条指令执行失败`，`vendor-meta-rules.txt` 587 条
  与装配树条目数一致，`resize2fs -M` 收紧到 180,748,288 B；
- 09：`[ok] 回填 823 条目，回读 823/823 一致`，`patch.img` 402,653,184 B。
- 单测 `engine/build/internal/e2fs/filespec_test.go` 三个用例：普通路径不启 debugfs
  进程、含空格路径解出 `<inode>` 且与 `dump.erofs`/`debugfs stat` 的 inode 一致、
  父目录含空白时返回错误。

补测过程中踩到一个**测试自身的坑**：最初用 `debugfs -w -R "write src /usr/idc/QEMU
Virtio Tablet.idc"` 造样本，`-R` 同样按空白分词，镜像里根本没生成该文件，测试报
「未能定位 1/1」，误以为是实现有 bug。改用 `mke2fs -q -F -t ext4 -d tree` 建树后通过
（教训：凡 debugfs/mke2fs 的路径参数，造数据也不能省引号）。

## 遗留

- 只在 macOS + e2fsprogs 1.47.4 上实测；其他版本若 `ls -l` 列序变化，`HasSuffix`
  匹配仍成立（只依赖首列 inode 与末列名），但需要回归测试。
- 依赖「父目录不含空白」这一前提。若将来厂商树出现含空白的**目录**，`ls` 无法定位，
  目前是显式报错；彻底解法是给 debugfs 打补丁支持引号，或改用 e2fsprogs 的
  `debugfs -f` 之外接口（如 `libext2fs` 绑定）。
- 07/09 两处都走同一实现，`09` 的 `pathOf` 反查只为报错可读；若新增第三处调用，
  应把「spec ↔ 路径」这层也下沉到 `internal/e2fs`，避免重复构造 map。
