# servicetracker 从 prebuilt 改回源码：逆向 AIDL + 自写实现，不再下发原厂二进制

> 2026-10-29，m22（MEIZU 22 / Flyme 12.6 / Android 16 / qcom `sun`）。
> #65 把 `vendor.qti.hardware.servicetrackeraidl` 收成 `kind=prebuilt`（原样下发
> 原厂 `bin/hw` 二进制 + `impl.so` + `-V1-ndk.so`），用户追问「不用源码，能跑起来？
> 你通过逆向实现源码」——本轮把这条通道换成「逆向 AIDL + 自写 C++ 实现」，
> 产物从 3 个原厂件变成 1 个自编二进制，接口 wire 层与原厂逐项对齐。
> 产物：`vendor.img` `111db9ce3c1878afe45c720a6c3a5697b50d91a39591f76ce8954e84875e41c1`
> （206,147,584 B；末次重跑于 23:33，因 `.aidl` 里改了一句注释导致源码指纹变化；
> 二进制本身不受注释影响，sha 仍为 `91ff07dc…`）。

## 1. 问题

`kind=prebuilt` 能跑，但本质是「把看不见的二进制塞进镜像」：

- 模块里 3 个 ELF 全是原厂产物（二进制 52,080 B / impl.so 85,584 B / `-V1-ndk.so` 102,544 B），
  源码不可得、行为不可改、审计只能靠反汇编；
- 原厂 `strings` 保留了编译期源路径 `vendor/qcom/proprietary/fwk-utils/servicetracker/aidl/1.0/default/service.cpp`，
  说明这是 QTI 私有 HAL，AOSP/LineageOS 无公开源码（本仓 grep 也无任何
  servicetracker 的 `.aidl` / `.cpp` / `.h` / `.bp`）；
- 同仓的 `vendor.qti.hardware.perf2` 已经有「逆向 AIDL + 自写实现」的先例
  （`kind=aidl` + 模块内 `aidl/` + `src/`），servicetracker 应当同构。

要解决的是：**在不拿到原厂源码的前提下，让自写实现与框架侧（Flyme system_server）
的 binder 调用 byte 级兼容**，并把 `prebuilt` 通道从这个模块上撤掉。

## 2. 根因

AIDL 的 wire 格式只由四件事决定，与源码无关，因此全部可以从原厂 `-V1-ndk.so`
反解出来：

1. **descriptor** —— `vendor.qti.hardware.servicetrackeraidl.IServicetracker`；
2. **接口 hash** —— `fd4771abe6e1c694687c871ee5d888b6e9cbf7b0`（在 so 的 `.rodata` 里，
   原厂 `BnServicetracker` 用它做版本校验）；
3. **transaction code 与方向** —— `BpServicetracker::<方法>` 里 `AIBinder_transact`
   的 `w1` 立即数（code）与 `w4`（flags：`0x10000001` oneway / `0x10000000` 同步）；
4. **每个参数的形状与 parcelable 字段序** —— Bp 侧 mangled 签名给出类型形状；
   parcelable 的 `writeToParcel` 给出字段序（structured parcelable 的 wire 是
   `[int32 长度占位（写完回填）][字段…]`，所以首尾两个 int32 要扣掉）；
   数组元素类型由 `.rela.dyn` 的 `GLOB_DAT` 指向哪个模板实例化确定。

反解结论（`engine/out/tmp/rev-st/` 下的 `rev_st.py` / `rev_func.py` 输出）：

| code | 方法 | 方向 | 入参 | 回包 |
|---|---|---|---|---|
| 1 | bindService | oneway | ServiceData, ClientData | 无 |
| 2 | destroyService | oneway | ServiceData | 无 |
| 3 | getClientConnections | sync | String | ClientConnection[] |
| 4 | getPid | sync | String | int32 |
| 5 | getPids | sync | String[] | int32[] |
| 6 | getRunningServicePid | sync | — | int32[] |
| 7 | getServiceBCount | sync | — | int32（计数）+ ServiceRecord[] |
| 8 | getServiceConnections | sync | String | ServiceConnection[] |
| 9 | getclientInfo | sync | String | ClientRecord |
| 10 | getserviceInfo | sync | String | ServiceRecord |
| 11 | isServiceB | sync | String | boolean |
| 12 | killProcess | oneway | int32 | 无 |
| 13 | startService | oneway | ServiceData | 无 |
| 14 | unbindService | oneway | ServiceData, ClientData | 无 |

6 个 parcelable 的字段序（全部由 `writeToParcel` 反解，`+0x18` 步长 = `std::string` 24 B）：

```
ClientData        { String, int }                                      32 B
ClientConnection  { String, int, int }                                 32 B
ServiceConnection { String, int, int }                                 32 B
ServiceData       { String, String, int, double, bool }                72 B
ClientRecord      { String, int, ClientConnection[] }
ServiceRecord     { String, String, int, bool, double, ServiceConnection[] }
```

两个数组元素类型由 `.rela.dyn` 证实：`0x14748 → AParcel_writeStdVectorParcelableElement<ClientConnection>`、
`0x14758 → …<ServiceConnection>`；`lsr x2, x8, #5`（32 B/元素）与之吻合。

**关键坑（与 #65 的逆向记录不同）**：`getPid` / `isServiceB` / `getServiceBCount`
的 int 与 boolean **不是出参**。AIDL 禁止基本类型作 `out`（aidl 34/35/36/37 一致报
`'pid' can't be an out parameter because int can only be an in parameter`），
所以原厂只能写成返回值；`getServiceBCount` 的 Bp 侧反汇编证实了回包序：
先 `AParcel_readInt32`（写 `*count`）再 `AParcel_readParcelableArray`（写服务数组），
即 `[count][services]` —— 正是「返回值先写、出参后写」的生成规则。
按 `out int` 写源码会编译不过，按「两个 out」写则回包序会反。

## 3. 证据

### 3.1 原厂二进制就是这 7 个 descriptor + 这一个 hash

```
$ strings -a vendor.qti.hardware.servicetrackeraidl-V1-ndk.so | grep 'vendor\.qti\.hardware\.servicetrackeraidl\.' | sort -u
vendor.qti.hardware.servicetrackeraidl.ClientConnection
vendor.qti.hardware.servicetrackeraidl.ClientData
vendor.qti.hardware.servicetrackeraidl.ClientRecord
vendor.qti.hardware.servicetrackeraidl.IServicetracker
vendor.qti.hardware.servicetrackeraidl.ServiceConnection
vendor.qti.hardware.servicetrackeraidl.ServiceData
vendor.qti.hardware.servicetrackeraidl.ServiceRecord
$ strings -a …-V1-ndk.so | grep -o 'fd4771abe6e1c694687c871ee5d888b6e9cbf7b0'
fd4771abe6e1c694687c871ee5d888b6e9cbf7b0
```

### 3.2 事务码与方向（Bp 侧立即数）

`bindService code=1 flags=0x10000001` … `unbindService code=14 flags=0x10000001`，
oneway 的是 1/2/12/13/14，其余 `0x10000000`。完整表见 `log-pass.txt` 第 2 节。

### 3.3 回包序（决定 `getServiceBCount` 只能写成「返回值 + out」）

```
$ llvm-objdump -d … | BpServicetracker::getServiceBCount
  e194: ldr x0, [sp, #0x8]        ; reply parcel
  e198: mov x1, x21               ; x21 = 第 2 个参数 = int*
  e19c: bl  AParcel_readInt32     ; ← 先读计数
  e1b8: mov x1, x20               ; x20 = 第 1 个参数 = vector<ServiceRecord>*
  e1bc: bl  AParcel_readParcelableArray   ; ← 后读数组
```

### 3.4 本仓自算的生成代码与之逐 case 对齐

```
$ grep 'case (FIRST_CALL_TRANSACTION' vendor/out/android15/tmp/srvctracker-ndk-work/gen/src/…/IServicetracker.cpp
+0 bindService / +1 destroyService / +2 getClientConnections / +3 getPid /
+4 getPids / +5 getRunningServicePid / +6 getServiceBCount / +7 getServiceConnections /
+8 getclientInfo / +9 getserviceInfo / +10 isServiceB / +11 killProcess /
+12 startService / +13 unbindService
```

`+6` 的写侧顺序为 `AParcel_writeData(_aidl_out, _aidl_return)` →
`AParcel_writeData(_aidl_out, out_services)`，与原厂读侧 `[int32][array]` 一致。

### 3.5 新二进制仍带原厂 descriptor 与 hash

```
$ strings -a out/vendor/android16/bin/hw/vendor.qti.hardware.servicetrackeraidl-service | grep -x 'vendor\.qti\.hardware\.servicetrackeraidl\.[A-Za-z]*'
（7 条，与原厂同集合）
$ … | grep -o fd4771abe6e1c694687c871ee5d888b6e9cbf7b0
fd4771abe6e1c694687c871ee5d888b6e9cbf7b0
$ llvm-readelf -d … | grep NEEDED
libc++.so / libbinder_ndk.so / liblog.so / libdl.so / libm.so / libc.so
```

自编二进制**不再依赖** `-impl.so` / `-V1-ndk.so`（接口代码编进去了），因此这两个
原厂 so 可以从模块与镜像里消失。

### 3.6 兼容面确认：除这个 HAL 自己，没别人引用那两个 so

```
$ grep -rl 'vendor.qti.hardware.servicetrackeraidl' out/tmp/unpack-vendor_a
…/bin/hw/…-service、…/etc/init/…rc、…/etc/selinux/vendor_service_contexts、
…/etc/vintf/manifest/…xml、…/lib64/hw/…-impl.so、…/lib64/…-V1-ndk.so
```

客户端是 system 侧 Java（只认 descriptor + hash），不链 native 库。

## 4. 改动

| 位置 | 改动 |
|---|---|
| `vendor/hardware/vendor.qti.hardware.servicetrackeraidl/aidl/vendor.qti.hardware.servicetrackeraidl/1/` | 新增 `IServicetracker.aidl`（14 方法，顺序=transaction code）+ 6 个 parcelable + `.hash`（`fd4771ab…`） |
| 同模块 `src/servicetracker_service.cpp` | 新增 151 行自写实现：`BnServicetracker` 子类，14 方法全部「拒绝成为放大器」语义（登记类 no-op、查询类空集合、`getPid=-1`、`isServiceB=false`、`killProcess` 明确不执行），`main()` 用 `AServiceManager_addService` 注册到 `…IServicetracker/default` |
| 同模块 `vendor/` | **删除**原厂 3 个 ELF（`bin/hw/…-service`、`lib64/hw/…-impl.so`、`lib64/…-V1-ndk.so`），只留 `etc/init/*.rc` 与 `etc/vintf/manifest/*.xml` 两个静态件 |
| `vendor/build/modules/vendor.qti.hardware.servicetrackeraidl.json` | `kind: prebuilt → aidl`，改为 `srcDir/src` + `aidlPackages` + `aidlPackageVersions` + `services` + `aidlRootModule` + `workSubdir`，与 perf2 同构；`prebuiltFiles` 整段删除 |
| `vendor/hardware/…/aidl/…/IServicetracker.aidl` | 同包跨文件显式 `import`（aidl 不在同包里隐式解析）；`getPid`/`isServiceB`/`getServiceBCount` 的 int/boolean 用返回值而非 `out` |

未改 sepolicy（域、exec 标签、服务名标签沿用 #65 的模块层片段），未改 rc/VINTF。

## 5. 验证

### 5.1 构建层

```
$ out/bin/cdh-vendor hal list | grep servicetracker
  vendor.qti.hardware.servicetrackeraidl aidl -> vendor.qti.hardware.servicetrackeraidl-service

$ out/bin/cdh-vendor hal build vendor.qti.hardware.servicetrackeraidl
==> AIDL 代码生成
    vendor.qti.hardware.servicetrackeraidl (v1, hash fd4771abe6e1)
==> 编译 7+1 个源文件到 .o
==> 链接服务二进制
    写 bin/hw/vendor.qti.hardware.servicetrackeraidl-service
[ok] 构建完成：codegen + 编译 + 链接
```

`.hash` 被读成 `--version=1 --hash=fd4771abe6e1…`（dry-run 命令行见 `log-pass.txt` 第 3 节）。

### 5.2 产物

| 项 | 值 |
|---|---|
| 自编二进制 | 250,176 B，sha256 `91ff07dc2c89fca274c9400aa5f5f041d8bf4097a45c6136380adcd84248f9c6` |
| 原厂二进制（备份 `/tmp/st-prebuilt-bak/`） | 52,080 B，sha256 `eec0ceb76dca42692f890bb81412784bdf7a4fe89de23aaea3ad04de77cb2f02` |
| 模块产物 | 3 项（编译产物 1 + vendor 静态树 2）；#65 时为 5 项 |

### 5.3 07 实跑（exit=0）

- moduleTargets 期望产物 3 项，`hal list` 显示 `aidl`；二次运行「源码指纹一致 → 复用」。
- 装配树 **658 条目**（文件 427 / 链接 199 / 目录 32）＝ #65 的 660 −2，正好是删掉的两个 so。
- 步骤 6b **27 条**不变，映射行仍为 `vendor.qti.hardware.servicetrackeraidl-service -> vendor_hal_srvctracker_default_exec`。
- `vendor.img` **206,147,584 B**（#65 为 206,073,856 B，+73,728 B ≈ 二进制 250,176−52,080 的块对齐）。

### 5.4 镜像内回读

```
$ debugfs -R "dump /bin/hw/vendor.qti.hardware.servicetrackeraidl-service …"
91ff07dc…  == out/vendor/android16/bin/hw/…-service      # 逐字节一致
$ debugfs -R "dump /etc/init/…rc"; sha256
ea747dbd…  == 模块 vendor/etc/init/…rc
$ debugfs -R "dump /etc/vintf/manifest/…xml"; sha256
d897c1f4…  == 模块 vendor/etc/vintf/manifest/…xml
$ debugfs -R "ls -l /lib64" | grep servicetracker     # 无输出（旧 so 已清除）
$ debugfs -R "ea_get -V /bin/hw/…-service security.selinux"
u:object_r:vendor_hal_srvctracker_default_exec:s0
```

**sha 不作一致性判据**：本轮多次构建得到 `59bb12c3…` / `111db9ce…`（尺寸相同），是
`mke2fs -d` 非确定性；判据固定为「装配树条目数 + 逐文件 sha + 镜像 xattr」，本轮三项全对
（658 / 3 文件 sha 全等 / 2 条 xattr 正确）。

## 6. 遗留

1. **运行期未复验**：`vendor.qti.hardware.servicetrackeraidl-service` 是否起来、
   `Failed to resolve` 是否为 0、Flyme 调 `getServiceBCount/getPid` 是否拿到空结果、
   avc denied 是否为 0，都需要实机/模拟器；宿主机无法执行 aarch64 Android ELF，
   也无 binder 环境。
2. **语义是「空实现」**：原厂维护「服务 ↔ 客户端连接」表供性能/功耗侧查询，
   自写实现全部返回空。若后续有功能依赖这张表（例如 Flyme 的省电策略按
   `getRunningServicePid` 裁剪），需要补真实登记逻辑；`killProcess` 保持 no-op
   （云机真 kill 可能命中宿主/框架）。
3. **`kind=prebuilt` 通道仍在**（`halbuild`/`prebuiltFiles`/`moduleTargets` 的代码路径
   未删）：目前只剩这一个模块用过、已迁走，可评估是否回收；回收前需确认没有别的
   机型 spec 在用。
4. **parcelable 字段名是推测**：wire 只认类型与顺序，字段名不影响兼容性；但如果
   后续要从 Java 侧生成同名类（aidl Java 后端），字段名需与 Flyme 侧一致才能混用。
5. `patch.img` / `out/dist` 仍旧版（09/11 未跑）。
