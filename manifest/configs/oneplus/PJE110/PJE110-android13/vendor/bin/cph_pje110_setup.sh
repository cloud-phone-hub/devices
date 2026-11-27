#!/system/bin/sh
# CDH setup for OnePlus Ace 3 (PJE110):
# 1. Disable workloadclassifier / vendor.qti.iwlan if still visible;
# 2. Seed plaintext ODM camera configs into com.oplus.camera sandbox.
pm disable-user --user 0 com.qualcomm.qti.workloadclassifier >/dev/null 2>&1
pm disable-user --user 0 vendor.qti.iwlan >/dev/null 2>&1

SRC=/mnt/vendorpatch/odm/etc/camera/config
if [ -d "$SRC" ]; then
    i=0
    CAM_DIR=""
    while [ "$i" -lt 25 ]; do
        if [ -d /data/user/0/com.oplus.camera ]; then
            CAM_DIR=/data/user/0/com.oplus.camera
            break
        fi
        if [ -d /data/data/com.oplus.camera ]; then
            CAM_DIR=/data/data/com.oplus.camera
            break
        fi
        sleep 1
        i=$((i + 1))
    done

    if [ -n "$CAM_DIR" ] && [ -d "$CAM_DIR" ]; then
        UG=$(stat -c %u:%g "$CAM_DIR" 2>/dev/null)
        CTX=$(stat -c %C "$CAM_DIR" 2>/dev/null)
        DST="$CAM_DIR/files/odm/etc/camera/config"
        STAMP="$CAM_DIR/files/.cdh_rus_v1"
        mkdir -p "$DST"
        cp -f "$SRC"/* "$DST"/
        [ -n "$UG" ] && chown -R "$UG" "$CAM_DIR/files"
        chmod -R 0755 "$CAM_DIR/files"
        chmod 0644 "$DST"/* 2>/dev/null
        if [ -n "$CTX" ]; then
            chcon -R "$CTX" "$CAM_DIR/files" >/dev/null 2>&1
        else
            restorecon -RF "$CAM_DIR/files" >/dev/null 2>&1
        fi
        if [ ! -f "$STAMP" ]; then
            touch "$STAMP"
            [ -n "$UG" ] && chown "$UG" "$STAMP"
            [ -n "$CTX" ] && chcon "$CTX" "$STAMP" >/dev/null 2>&1
            am force-stop com.oplus.camera >/dev/null 2>&1
        fi
    fi
fi
exit 0
