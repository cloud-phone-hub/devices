#!/bin/bash
# 21-6.1 移植验证：在容器内把 patches/lineage-21-6.1/*.patch 打到基线树上，
# 只编被补丁动到的两个子系统（virtio-gpu 内建 + mac80211_hwsim 模块），
# 用来确认补丁代码在 6.1 上能过编译；跑完把工作树还原。
# 日志落在 /out/logs（宿主挂载，便于留证）。
set -uo pipefail
cd /src
OUT=/out
mkdir -p "$OUT/logs"

echo "==> 应用补丁"
for p in /patches/*.patch; do
	git apply --check "$p" || { echo "  [check 失败] $p"; exit 1; }
done
for p in /patches/*.patch; do
	git apply "$p" && echo "  $(basename "$p")"
done

make O="$OUT" ARCH=arm64 LLVM=1 gki_defconfig > "$OUT/logs/cfg1.log" 2>&1
echo "cfg1 rc=$?"
KCONFIG_CONFIG="$OUT/.config" scripts/kconfig/merge_config.sh -m -r \
	"$OUT/.config" \
	arch/arm64/configs/lineageos/virtio.config \
	arch/arm64/configs/lineageos/peripheral/wifi.config \
	/cdh.config > "$OUT/logs/cfg2.log" 2>&1
echo "cfg2 rc=$?"
# DEBUG_INFO_BTF 关掉：6.1 的 libbpf 编不过新版 clang（与补丁无关）；WERROR 关掉只留 warning。
scripts/config --file "$OUT/.config" --set-str LOCALVERSION "-cdh-buildcheck" --disable LOCALVERSION_AUTO \
	--disable DEBUG_INFO_BTF --disable DEBUG_INFO_BTF_MODULES
make O="$OUT" ARCH=arm64 LLVM=1 olddefconfig > "$OUT/logs/cfg3.log" 2>&1
echo "cfg3 rc=$?"

for s in DRM_VIRTIO_GPU MAC80211 MAC80211_HWSIM CFG80211 DEBUG_INFO_BTF; do
	printf '  %-18s %s\n' "$s" "$(sed -n "s/^CONFIG_${s}=//p" "$OUT/.config" | head -1)"
done

echo "==> prepare"
make O="$OUT" ARCH=arm64 LLVM=1 -j"$(nproc)" CONFIG_WERROR=n prepare > "$OUT/logs/prepare.log" 2>&1
echo "prepare rc=$?"

echo "==> drivers/gpu/drm/virtio/"
make O="$OUT" ARCH=arm64 LLVM=1 -j"$(nproc)" CONFIG_WERROR=n drivers/gpu/drm/virtio/ > "$OUT/logs/virtio.log" 2>&1
echo "virtio rc=$?  error=$(grep -c 'error:' "$OUT/logs/virtio.log")  warning=$(grep -c 'warning:' "$OUT/logs/virtio.log")"

echo "==> drivers/net/wireless/mac80211_hwsim.ko"
make O="$OUT" ARCH=arm64 LLVM=1 -j"$(nproc)" CONFIG_WERROR=n drivers/net/wireless/mac80211_hwsim.ko > "$OUT/logs/hwsim.log" 2>&1
echo "hwsim rc=$?  error=$(grep -c 'error:' "$OUT/logs/hwsim.log")  warning=$(grep -c 'warning:' "$OUT/logs/hwsim.log")"

echo "==> 产物"
ls -l "$OUT/drivers/net/wireless/mac80211_hwsim.ko" 2>/dev/null
ls -l "$OUT/drivers/gpu/drm/virtio/built-in.a" 2>/dev/null

echo "==> 还原工作树"
git checkout -- drivers/gpu/drm/virtio drivers/net/wireless/mac80211_hwsim.c
echo "  脏文件=$(git status --porcelain | wc -l | tr -d ' ')  HEAD=$(git rev-parse --short HEAD)"
