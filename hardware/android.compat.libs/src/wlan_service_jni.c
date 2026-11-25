// 纯 C 源码实现高通 /vendor/app/IWlanService（vendor.qti.iwlan / .qtidataservices）
// 所需的 libWlanServiceJni.so 兼容桩（零高通 QMI 私有库依赖）。
//
// 背景：
//   在直通官方 super/vendor_a.img（仅重定向 bin/etc/lib/lib64）的骁龙机型上，
//   PackageManager 会扫描官方 /vendor/app/IWlanService/IWlanService.apk 并拉起
//   QualifiedNetworksService / IWlanNetworkService（进程名 .qtidataservices）。
//   IWlanProxy 无条件调用 vendor.qti.iwlan.JniHelper（System.loadLibrary("WlanServiceJni")），
//   原厂 libWlanServiceJni.so 强依赖十余个 QMI 闭源库且云机无物理基带 socket，缺失时会导致
//   开机连续 6 次 UnsatisfiedLinkError 崩溃。
//
// 行为：
//   1. initialize() 将 IWlanProxy.iwlanModemSupport 置为 false；
//   2. getAllQualifiedNetworks / getDataRegistrationState / getDataCallList /
//      setupDataCall / deactivateDataCall 同步调用 IWlanProxy.processRequestAck(serial)
//      与 IWlanProxy.processResponse(...) 归还 IWLAN_PROXY_WL 唤醒锁并清理挂起请求，
//      使 Telephony 框架干净走蜂窝 IRadioData 通道。

#include <jni.h>
#include <stddef.h>
#include <stdint.h>

static void cdh_clear_exc(JNIEnv *env) {
    if ((*env)->ExceptionCheck(env)) {
        (*env)->ExceptionClear(env);
    }
}

static void cdh_disable_iwlan_modem_support(JNIEnv *env) {
    jclass proxy_cls = (*env)->FindClass(env, "vendor/qti/iwlan/IWlanProxy");
    if (!proxy_cls) {
        cdh_clear_exc(env);
        return;
    }
    jfieldID fid =
        (*env)->GetStaticFieldID(env, proxy_cls, "iwlanModemSupport", "Z");
    if (fid) {
        (*env)->SetStaticBooleanField(env, proxy_cls, fid, JNI_FALSE);
    }
    cdh_clear_exc(env);
    (*env)->DeleteLocalRef(env, proxy_cls);
}

static void cdh_complete_iwlan_request(JNIEnv *env, jint slot_id, jint serial) {
    if (slot_id < 0) {
        return;
    }
    jclass proxy_cls = (*env)->FindClass(env, "vendor/qti/iwlan/IWlanProxy");
    if (!proxy_cls) {
        cdh_clear_exc(env);
        return;
    }
    jfieldID inst_fid = (*env)->GetStaticFieldID(
        env, proxy_cls, "instances", "[Lvendor/qti/iwlan/IWlanProxy;");
    if (!inst_fid) {
        cdh_clear_exc(env);
        (*env)->DeleteLocalRef(env, proxy_cls);
        return;
    }
    jobjectArray arr =
        (jobjectArray)(*env)->GetStaticObjectField(env, proxy_cls, inst_fid);
    if (!arr) {
        cdh_clear_exc(env);
        (*env)->DeleteLocalRef(env, proxy_cls);
        return;
    }
    jsize len = (*env)->GetArrayLength(env, arr);
    if (slot_id >= len) {
        (*env)->DeleteLocalRef(env, arr);
        (*env)->DeleteLocalRef(env, proxy_cls);
        return;
    }
    jobject proxy = (*env)->GetObjectArrayElement(env, arr, slot_id);
    (*env)->DeleteLocalRef(env, arr);
    if (!proxy) {
        cdh_clear_exc(env);
        (*env)->DeleteLocalRef(env, proxy_cls);
        return;
    }

    // 1. 释放 obtainRequest 持有的 IWLAN_PROXY_WL 唤醒锁。
    jmethodID ack_mid =
        (*env)->GetMethodID(env, proxy_cls, "processRequestAck", "(I)V");
    if (ack_mid) {
        (*env)->CallVoidMethod(env, proxy, ack_mid, serial);
        cdh_clear_exc(env);
    }

    // 2. 从 mRequestList 移除该请求，防止挂起表累积。
    jclass resp_cls = (*env)->FindClass(
        env, "vendor/qti/hardware/data/iwlandata/IWlanResponseInfo");
    if (resp_cls) {
        jmethodID init_mid = (*env)->GetMethodID(env, resp_cls, "<init>", "()V");
        jfieldID ser_fid = (*env)->GetFieldID(env, resp_cls, "serial", "I");
        jfieldID err_fid = (*env)->GetFieldID(env, resp_cls, "error", "I");
        jmethodID proc_mid = (*env)->GetMethodID(
            env, proxy_cls, "processResponse",
            "(Lvendor/qti/hardware/data/iwlandata/IWlanResponseInfo;ZLjava/lang/String;)Lvendor/qti/iwlan/IWlanRequest;");
        if (init_mid && ser_fid && err_fid && proc_mid) {
            jobject resp = (*env)->NewObject(env, resp_cls, init_mid);
            if (resp) {
                (*env)->SetIntField(env, resp, ser_fid, serial);
                (*env)->SetIntField(env, resp, err_fid, 1);
                jstring tag = (*env)->NewStringUTF(env, "cdh-iwlan-stub");
                jobject req = (*env)->CallObjectMethod(
                    env, proxy, proc_mid, resp, JNI_FALSE, tag);
                cdh_clear_exc(env);
                if (req) {
                    (*env)->DeleteLocalRef(env, req);
                }
                if (tag) {
                    (*env)->DeleteLocalRef(env, tag);
                }
                (*env)->DeleteLocalRef(env, resp);
            }
        }
        cdh_clear_exc(env);
        (*env)->DeleteLocalRef(env, resp_cls);
    }

    (*env)->DeleteLocalRef(env, proxy);
    (*env)->DeleteLocalRef(env, proxy_cls);
}

__attribute__((visibility("default")))
JNIEXPORT jint JNICALL JNI_OnLoad(JavaVM *vm, void *reserved) {
    (void)vm;
    (void)reserved;
    return JNI_VERSION_1_6;
}

__attribute__((visibility("default")))
JNIEXPORT void JNICALL Java_vendor_qti_iwlan_JniHelper_initialize(
    JNIEnv *env, jclass clazz, jint slot_id) {
    (void)clazz;
    (void)slot_id;
    cdh_disable_iwlan_modem_support(env);
}

__attribute__((visibility("default")))
JNIEXPORT void JNICALL Java_vendor_qti_iwlan_JniHelper_getAllQualifiedNetworks(
    JNIEnv *env, jclass clazz, jint slot_id, jint serial) {
    (void)clazz;
    cdh_complete_iwlan_request(env, slot_id, serial);
}

__attribute__((visibility("default")))
JNIEXPORT void JNICALL Java_vendor_qti_iwlan_JniHelper_getDataCallList(
    JNIEnv *env, jclass clazz, jint slot_id, jint serial) {
    (void)clazz;
    cdh_complete_iwlan_request(env, slot_id, serial);
}

__attribute__((visibility("default")))
JNIEXPORT void JNICALL Java_vendor_qti_iwlan_JniHelper_getDataRegistrationState(
    JNIEnv *env, jclass clazz, jint slot_id, jint serial) {
    (void)clazz;
    cdh_complete_iwlan_request(env, slot_id, serial);
}

__attribute__((visibility("default")))
JNIEXPORT void JNICALL Java_vendor_qti_iwlan_JniHelper_deactivateDataCall(
    JNIEnv *env, jclass clazz, jint slot_id, jint serial, jint cid,
    jint reason) {
    (void)clazz;
    (void)cid;
    (void)reason;
    cdh_complete_iwlan_request(env, slot_id, serial);
}

__attribute__((visibility("default")))
JNIEXPORT void JNICALL Java_vendor_qti_iwlan_JniHelper_responseAcknowledgement(
    JNIEnv *env, jclass clazz, jint slot_id) {
    (void)env;
    (void)clazz;
    (void)slot_id;
}

__attribute__((visibility("default")))
JNIEXPORT void JNICALL Java_vendor_qti_iwlan_JniHelper_setupDataCall(
    JNIEnv *env, jclass clazz, jint slot_id, jint serial, jint access_network,
    jobject data_profile_parcel, jboolean roaming_allowed, jint reason,
    jobjectArray addresses, jobjectArray dnses, jint pdu_session_id,
    jobject slice_info_parcel, jboolean match_all_rule_allowed) {
    (void)clazz;
    (void)access_network;
    (void)data_profile_parcel;
    (void)roaming_allowed;
    (void)reason;
    (void)addresses;
    (void)dnses;
    (void)pdu_session_id;
    (void)slice_info_parcel;
    (void)match_all_rule_allowed;
    cdh_complete_iwlan_request(env, slot_id, serial);
}
