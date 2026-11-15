#!/bin/bash
# 校验：把 patches/<tree>/*.patch 打到上游基线上，结果应与 cdh-backup-20261102 tag 逐字节一致。
# 例外：lineage-21-6.1 的补丁是 2026-11-02 从 22-6.6 移植来的（不是本树历史回退的产物），
# 备份 tag 里没有这些改动，所以只验证「能干净应用」，不比对 tag。
ROOT=/Volumes/work/cloud-phone-hub/engine
cd "$ROOT" || exit 1

verify() {
    local t=$1 b=$2 cmp_tag=${3:-1}
    local T=/tmp/cdh-pass-$t
    rm -rf "$T"; mkdir -p "$T"
    local files=""
    if compgen -G "kernels/patches/$t/*.patch" > /dev/null; then
        files=$( (cd "kernels/patches/$t" && grep -h '^+++ b/' *.patch | sed 's|^+++ b/||' | sort -u) )
    fi
    echo "## ${t} baseline ${b}"
    if [ -z "$files" ]; then
        echo "  无补丁（纯上游树）"
        echo
        return
    fi
    git -C "kernels/$t" archive "$b" -- $files | tar -x -C "$T"
    for p in kernels/patches/$t/*.patch; do
        printf "  %-46s " "$(basename "$p")"
        if (cd "$T" && git apply --check "$ROOT/$p"); then echo 可应用; fi
    done
    (cd "$T" && for p in "$ROOT"/kernels/patches/$t/*.patch; do git apply "$p"; done)
    if [ "$cmp_tag" = "0" ]; then
        echo "  结果: 移植补丁，备份 tag 里无对应改动，跳过 cmp（已在容器里编译验证）"
        echo
        return
    fi
    local bad=0
    for f in $files; do
        git -C "kernels/$t" show "cdh-backup-20261102:$f" > /tmp/cdh-ref.txt 2>/dev/null
        if ! cmp -s "$T/$f" /tmp/cdh-ref.txt; then
            echo "  [差异] $f"
            bad=1
        fi
    done
    [ $bad = 0 ] && echo "  结果: 全部文件与 tag 逐字节一致"
    echo
}

echo "# 验证 1：补丁打到上游基线后，与 cdh-backup-20261102 的内容逐字节一致"
echo
verify lineage-21-6.1 7012e868a414 0
verify lineage-22-6.6 bf3c4b5c67f89
verify lineage-22-6.12 afb0ad713e3c
verify lineage-23-6.12 4f6bf47200d9

echo "# 验证 2：四棵树状态（HEAD / 工作区 / 相对备份 tag 的差异 / CDH 代码残留）"
echo
for t in lineage-21-6.1 lineage-22-6.6 lineage-22-6.12 lineage-23-6.12; do
    printf "%-18s HEAD=%s  脏文件=%s  与备份差异行=%s  CDH残留=%s\n" \
        "$t" \
        "$(git -C kernels/$t rev-parse --short HEAD)" \
        "$(git -C kernels/$t status --porcelain | wc -l | tr -d ' ')" \
        "$(git -C kernels/$t diff cdh-backup-20261102 HEAD --stat | wc -l | tr -d ' ')" \
        "$(grep -rl -e cdh_perm_types -e cdh_sigkill -e CONFIG_CDH_PIONEER_PERMISSIVE kernels/$t --include='*.c' --include='*.h' --include='Kconfig' 2>/dev/null | wc -l | tr -d ' ')"
done
echo
