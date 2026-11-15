#!/bin/bash
# 校验：pioneer / crash_dump / surfaceflinger 的 permissive 改由 vendor CIL 表达之后，
# gold 的启动期策略（init 现场 secilc）仍编得出来，且 permissive 集合与当年内核补丁
# 的 `permissive_map` 效果一致。
#
# 做法：拿 gold 真实的 CIL 输入编两遍，用 sediff 比「改前 / 改后」：
#   - plat_sepolicy.cil：lineage-22.2 基底 system 分区 /system/etc/selinux/ 下（ext4）；
#   - plat_pub_versioned.cil：同基底 vendor 分区 /etc/selinux/ 下（vendor 侧自带，
#     与 plat 同源，配套 plat_sepolicy_vers.txt=202504）；
#   - vendor_sepolicy.cil：取 07 的产物（已含模块层 `@android.service.daemon` 块，
#     里面就有 `(typepermissive pioneer)`）；
#   - 改后 = 上面这份 + gold 机型层片段新加的两行（脚本从仓库里 grep，保证与清单同源，
#     而不是在这里手写一遍）。
# 基底没有 system_ext/product 策略分件（22.2 的 system_ext_a.img 是空的），故只编三份。
#
# 为什么关 neverallow（-N）：lineage 的 vendor 规则本身就与 plat 的 neverallow 相撞
# （如 `init` 写 `sysfs_type`），那是基底的历史包袱、与本次改动无关；这里只验证
# 「符号能解析、permissive 位能落下」。
#
# 依赖镜像 cdh-secilc:latest（ubuntu:26.04 + secilc / checkpolicy / setools / e2fsprogs，
# 构建命令见 solution.md 第九节）。
set -uo pipefail
ROOT=/Volumes/work/cloud-phone-hub/engine
IMG=cdh-secilc:latest
SUPER=$ROOT/out/roms/lineage/lineage-22.2/super
VENDOR_CIL=$ROOT/out/devices/xiaomi/gold/OS3.0.10.0.VNQCNXM/vendor/etc/selinux/vendor_sepolicy.cil
GOLD_FRAG=$ROOT/manifest/xiaomi/gold/sepolicy/vendor_sepolicy.cil

for f in "$SUPER/system_a.img" "$SUPER/vendor_a.img" "$VENDOR_CIL" "$GOLD_FRAG"; do
    [ -e "$f" ] || { echo "缺少输入：$f"; exit 1; }
done

docker run --rm \
    -v "$SUPER":/rom:ro -v "$VENDOR_CIL":/in/vendor_sepolicy.cil:ro -v "$GOLD_FRAG":/in/gold.cil:ro \
    "$IMG" bash -c '
set -uo pipefail
W=/tmp/w; rm -rf $W; mkdir -p $W; cd $W

echo "==> 1. 从 lineage-22.2 基底取 plat / plat_pub_versioned"
debugfs -R "cat /system/etc/selinux/plat_sepolicy.cil" /rom/system_a.img > plat_sepolicy.cil 2>/dev/null
debugfs -R "cat /etc/selinux/plat_pub_versioned.cil" /rom/vendor_a.img > plat_pub_versioned.cil 2>/dev/null
for f in plat_sepolicy plat_pub_versioned; do
    [ -s $f.cil ] || { echo "  [失败] 取不到 $f.cil"; exit 1; }
    printf "  %-22s %8s B  sha256=%s\n" "$f.cil" "$(stat -c%s $f.cil)" "$(sha256sum $f.cil | cut -c1-16)"
done

echo "==> 2. 取 07 产出的 vendor_sepolicy.cil（含模块层 pioneer 块）"
cp /in/vendor_sepolicy.cil vendor_before.cil
grep -q "^(typepermissive pioneer)" vendor_before.cil || { echo "  [失败] 产物里没有 (typepermissive pioneer)"; exit 1; }
echo "  vendor_before.cil      $(stat -c%s vendor_before.cil) B"

echo "==> 3. 拼「改后」：gold 机型层片段里新增的两行"
grep -hE "^\(typepermissive (crash_dump|surfaceflinger)\)$" /in/gold.cil > add.cil
echo "  从片段取到 $(wc -l < add.cil) 行："; sed "s/^/    /" add.cil
cp vendor_before.cil vendor_after.cil && cat add.cil >> vendor_after.cil

echo "==> 4. secilc 编译两遍（-N 关 neverallow，理由见脚本头）"
for tag in before after; do
    secilc -m -M true -G -N -o $tag.bin \
        plat_sepolicy.cil plat_pub_versioned.cil vendor_$tag.cil > $tag.log 2>&1
    rc=$?
    echo "  $tag: rc=$rc  $(stat -c%s $tag.bin 2>/dev/null) B"
    [ $rc = 0 ] || { tail -20 $tag.log; exit 1; }
done

echo "==> 5. permissive 集合（seinfo 计数 + setools 列名）"
for tag in before after; do
    printf "  %-6s Permissives=%s  %s\n" "$tag" \
        "$(seinfo $tag.bin --permissive= 2>/dev/null | grep -o "Permissives: *[0-9]*" | grep -o "[0-9]*")" \
        "$(python3 -c "import setools; p=setools.SELinuxPolicy(\"$tag.bin\"); print(sorted(t.name for t in p.types() if t.ispermissive))")"
done

echo "==> 6. sediff（只应多出 crash_dump / surfaceflinger）"
sediff before.bin after.bin > diff.txt 2>&1
sed "s/^/    /" diff.txt
if grep -q "crash_dump (Added permissive)" diff.txt && \
   grep -q "surfaceflinger (Added permissive)" diff.txt && \
   [ "$(grep -c "Added permissive" diff.txt)" = 2 ]; then
    echo "  结果: 只多了 crash_dump / surfaceflinger 两条 permissive，与内核补丁当年的三型集合一致"
else
    echo "  [失败] permissive 差集与预期不符"
    exit 1
fi
'
