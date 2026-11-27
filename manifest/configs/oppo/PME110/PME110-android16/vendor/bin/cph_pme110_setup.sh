#!/system/bin/sh
# CDH setup for OPPO Find X9s Pro (PME110):
# Seed decrypted OplusCamera RUS configs into app sandbox so uid>10000 camera app
# loads valid unencrypted JSON/protobuf configs without needing /odm/lib64/libOplusSecurity.so.

SRC=/mnt/vendorpatch/odm/etc/camera/config
CAM_DIR=/data/user/0/com.oplus.camera
if [ ! -d "$CAM_DIR" ] && [ -d /data/data/com.oplus.camera ]; then
    CAM_DIR=/data/data/com.oplus.camera
fi

if [ -d "$SRC" ]; then
    i=0
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

    if [ -d "$CAM_DIR" ]; then
        UG=$(stat -c %u:%g "$CAM_DIR" 2>/dev/null)
        CTX=$(stat -c %C "$CAM_DIR" 2>/dev/null)
        DST="$CAM_DIR/files/odm/etc/camera/config"
        STAMP="$CAM_DIR/files/.cdh_rus_v1"
        mkdir -p "$DST"
        cp -f "$SRC"/* "$DST"/
        if [ -n "$UG" ]; then
            chown -R "$UG" "$CAM_DIR/files"
        fi
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
