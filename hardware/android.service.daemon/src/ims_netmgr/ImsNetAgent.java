package com.cdh.ims;

import android.content.AttributionSource;
import android.content.ContentResolver;
import android.content.ContentUris;
import android.content.ContentValues;
import android.content.Context;
import android.content.ContextWrapper;
import android.database.Cursor;
import android.net.ConnectivityManager;
import android.net.Network;
import android.net.NetworkCapabilities;
import android.net.NetworkRequest;
import android.net.NetworkSpecifier;
import android.net.TelephonyNetworkSpecifier;
import android.net.Uri;
import android.os.Binder;
import android.os.Bundle;
import android.os.Handler;
import android.os.IBinder;
import android.os.Looper;
import android.os.Process;
import android.telephony.SubscriptionInfo;
import android.telephony.SubscriptionManager;
import android.telephony.TelephonyManager;
import android.util.Log;
import android.util.Xml;

import org.xmlpull.v1.XmlPullParser;

import java.io.File;
import java.io.FileInputStream;
import java.lang.reflect.Field;
import java.lang.reflect.Method;
import java.util.ArrayList;
import java.util.HashSet;
import java.util.List;
import java.util.Locale;
import java.util.Set;

/**
 * 常驻蜂窝 APN 预置补齐与 IMS 承载网请求守护（对齐真机 com.mediatek.ims / ImsService 行为）。
 *
 * 运行于 uid=1001 (radio)：
 * 1. 检查当前插卡 PLMN 在 telephony.db 中是否具备 default 与 ims 接入点；若国行/精简底包缺失广电(46015)或
 *    海外运营商(如美国 T-Mobile/AT&T/Verizon、英/日/韩/东南亚等)条目，从 /vendor/etc/cph-apns-preset.xml
 *    无侵入补齐至 content://telephony/carriers（不触碰 super.img / product.img）；
 * 2. 复用系统已安装 IMS 包（如 com.mediatek.ims）上下文向 ConnectivityService 发起
 *    TRANSPORT_CELLULAR + NET_CAPABILITY_IMS 请求，驱动 DataNetworkController 自动拉起 apn="ims" (ccmni0)。
 */
public final class ImsNetAgent {
    private static final String TAG = "CphImsNetAgent";
    private static final String PRESET_APNS_XML = "/vendor/etc/cph-apns-preset.xml";
    private static final Uri CARRIERS_URI = Uri.parse("content://telephony/carriers");
    private static final String[] CANDIDATE_PKGS = new String[] {
        "com.mediatek.ims",
        "org.codeaurora.ims",
        "com.shannon.imsservice",
        "com.spreadtrum.ims",
        "com.android.phone",
        "com.android.shell",
    };

    private static final class PresetApn {
        final String carrier;
        final String mcc;
        final String mnc;
        final String apn;
        final String type;
        final String protocol;
        final String roamingProtocol;
        final String mmsc;
        final String mmsProxy;
        final String mmsPort;

        PresetApn(String carrier, String mcc, String mnc, String apn, String type,
                  String protocol, String roamingProtocol, String mmsc, String mmsProxy, String mmsPort) {
            this.carrier = carrier;
            this.mcc = mcc;
            this.mnc = mnc;
            this.apn = apn;
            this.type = type;
            this.protocol = protocol;
            this.roamingProtocol = roamingProtocol;
            this.mmsc = mmsc;
            this.mmsProxy = mmsProxy;
            this.mmsPort = mmsPort;
        }
    }

    private final Context pkgContext;
    private final ConnectivityManager cm;
    private final SubscriptionManager sm;
    private final TelephonyManager tm;
    private final Handler handler;
    private final Set<String> provisionedNumerics = new HashSet<>();
    private int activeSubId = -1;
    private boolean imsConnected = false;
    private ConnectivityManager.NetworkCallback activeCallback = null;

    private ImsNetAgent(Context pkgContext, ConnectivityManager cm,
                        SubscriptionManager sm, TelephonyManager tm, Handler handler) {
        this.pkgContext = pkgContext;
        this.cm = cm;
        this.sm = sm;
        this.tm = tm;
        this.handler = handler;
    }

    public static void main(String[] args) {
        try {
            Looper.prepareMainLooper();
            Context sysCtx = obtainSystemContext();
            Context callerCtx = resolveRadioPackageContext(sysCtx);
            ConnectivityManager cm =
                (ConnectivityManager) callerCtx.getSystemService("connectivity");
            SubscriptionManager sm =
                (SubscriptionManager) callerCtx.getSystemService("telephony_subscription_service");
            TelephonyManager tm =
                (TelephonyManager) callerCtx.getSystemService("phone");
            if (cm == null) {
                Log.e(TAG, "ConnectivityManager unavailable");
                return;
            }
            Handler h = new Handler(Looper.myLooper());
            ImsNetAgent agent = new ImsNetAgent(callerCtx, cm, sm, tm, h);
            h.post(agent::reconcile);
            Looper.loop();
        } catch (Throwable t) {
            Log.e(TAG, "fatal error in ImsNetAgent", t);
        }
    }

    private static Context obtainSystemContext() throws Exception {
        Class<?> atClass = Class.forName("android.app.ActivityThread");
        Method sysMain = atClass.getMethod("systemMain");
        Object at = sysMain.invoke(null);
        Method getSysCtx = atClass.getMethod("getSystemContext");
        return (Context) getSysCtx.invoke(at);
    }

    private static Context resolveRadioPackageContext(Context sysCtx) {
        final int myUid = Process.myUid();
        for (final String pkg : CANDIDATE_PKGS) {
            try {
                final Context raw = sysCtx.createPackageContext(pkg, 0);
                if (raw != null && raw.getApplicationInfo() != null &&
                    raw.getApplicationInfo().uid == myUid) {
                    final AttributionSource attr = new AttributionSource.Builder(myUid)
                        .setPackageName(pkg)
                        .build();
                    patchContextImplAttribution(raw, pkg, attr);
                    Context wrapped = new ContextWrapper(raw) {
                        @Override
                        public String getPackageName() {
                            return pkg;
                        }

                        @Override
                        public String getOpPackageName() {
                            return pkg;
                        }

                        @Override
                        public AttributionSource getAttributionSource() {
                            return attr;
                        }
                    };
                    try {
                        Method setOuter =
                            raw.getClass().getDeclaredMethod("setOuterContext", Context.class);
                        setOuter.setAccessible(true);
                        setOuter.invoke(raw, wrapped);
                    } catch (Throwable ignored) {
                    }
                    Log.i(TAG, "bound caller package context: " + pkg + " (uid=" + myUid + ")");
                    return wrapped;
                }
            } catch (Throwable ignored) {
            }
        }
        return sysCtx;
    }

    private static void patchContextImplAttribution(Context raw, String pkg, AttributionSource attr) {
        Class<?> cls = raw.getClass();
        for (String fieldName : new String[] {"mOpPackageName", "mBasePackageName"}) {
            try {
                Field f = cls.getDeclaredField(fieldName);
                f.setAccessible(true);
                f.set(raw, pkg);
            } catch (Throwable ignored) {
            }
        }
        try {
            Field fAttr = cls.getDeclaredField("mAttributionSource");
            fAttr.setAccessible(true);
            fAttr.set(raw, attr);
        } catch (Throwable ignored) {
        }
    }

    private static Object obtainISub() {
        try {
            Class<?> smCls = Class.forName("android.os.ServiceManager");
            Object binder = smCls.getMethod("getService", String.class).invoke(null, "isub");
            if (binder == null) {
                return null;
            }
            Class<?> stubCls = Class.forName("com.android.internal.telephony.ISub$Stub");
            return stubCls.getMethod("asInterface", Class.forName("android.os.IBinder")).invoke(null, binder);
        } catch (Throwable ignored) {
            return null;
        }
    }

    private int resolveActiveSubId() {
        // 直接经 ISub Binder 查询（绕过 app_process 无权限读 cache_key.* 导致 PropertyInvalidatedCache 恒返回 -1）
        Object isub = obtainISub();
        List<Integer> activeIds = new ArrayList<>();
        int defData = -1;
        int defVoice = -1;
        int defSms = -1;

        if (isub != null) {
            Class<?> ic = isub.getClass();
            for (int slot = 0; slot <= 1; slot++) {
                try {
                    int[] slotSubs = (int[]) ic.getMethod("getSubId", int.class).invoke(isub, slot);
                    if (slotSubs != null) {
                        for (int id : slotSubs) {
                            if (id > 0 && !activeIds.contains(id)) {
                                activeIds.add(id);
                            }
                        }
                    }
                } catch (Throwable ignored) {
                }
            }
            if (activeIds.isEmpty()) {
                try {
                    int[] allActive = (int[]) ic.getMethod("getActiveSubIdList", boolean.class).invoke(isub, false);
                    if (allActive != null) {
                        for (int id : allActive) {
                            if (id > 0 && !activeIds.contains(id)) {
                                activeIds.add(id);
                            }
                        }
                    }
                } catch (Throwable ignored) {
                }
            }
            defData = invokeISubGetter(isub, "getDefaultDataSubId");
            defVoice = invokeISubGetter(isub, "getDefaultVoiceSubId");
            defSms = invokeISubGetter(isub, "getDefaultSmsSubId");
        }

        if (!activeIds.isEmpty()) {
            int chosen = activeIds.contains(defData) ? defData : activeIds.get(0);
            if (defData != chosen && isub != null) {
                // 换卡后旧 defaultDataSubId 已不在槽位：自动将当前在槽活跃卡切换为默认数据/语音/短信卡
                invokeISubSetter(isub, "setDefaultDataSubId", chosen);
                if (!activeIds.contains(defVoice)) {
                    invokeISubSetter(isub, "setDefaultVoiceSubId", chosen);
                }
                if (!activeIds.contains(defSms)) {
                    invokeISubSetter(isub, "setDefaultSmsSubId", chosen);
                }
                Log.i(TAG, "auto-promoted active subId=" + chosen + " as default data sub (was " + defData + ")");
            }
            return chosen;
        }

        if (defData > 0) {
            return defData;
        }
        return -1;
    }

    private static int invokeISubGetter(Object isub, String methodName) {
        try {
            Object v = isub.getClass().getMethod(methodName).invoke(isub);
            if (v instanceof Integer) {
                return (Integer) v;
            }
        } catch (Throwable ignored) {
        }
        return -1;
    }

    private static void invokeISubSetter(Object isub, String methodName, int subId) {
        try {
            isub.getClass().getMethod(methodName, int.class).invoke(isub, subId);
        } catch (Throwable t) {
            Log.w(TAG, "ISub." + methodName + "(" + subId + ") failed: " + t.getMessage());
        }
    }

    private static String getSysProp(String key, String defVal) {
        try {
            Class<?> sp = Class.forName("android.os.SystemProperties");
            String v = (String) sp.getMethod("get", String.class, String.class).invoke(null, key, defVal);
            return v != null ? v : defVal;
        } catch (Throwable ignored) {
            return defVal;
        }
    }

    private String resolveSimNumeric(int subId) {
        if (tm != null && subId > 0) {
            try {
                TelephonyManager subTm = tm.createForSubscriptionId(subId);
                if (subTm != null) {
                    String op = subTm.getSimOperator();
                    if (isValidNumeric(op)) {
                        return op.trim();
                    }
                }
            } catch (Throwable ignored) {
            }
            try {
                String op = tm.getSimOperator();
                if (isValidNumeric(op)) {
                    return op.trim();
                }
            } catch (Throwable ignored) {
            }
        }
        String raw = getSysProp("gsm.sim.operator.numeric", "");
        for (String part : raw.split(",")) {
            if (isValidNumeric(part)) {
                return part.trim();
            }
        }
        return "";
    }

    private static boolean isValidNumeric(String s) {
        if (s == null) {
            return false;
        }
        String t = s.trim();
        if (t.length() < 5 || t.length() > 6) {
            return false;
        }
        for (int i = 0; i < t.length(); i++) {
            char c = t.charAt(i);
            if (c < '0' || c > '9') {
                return false;
            }
        }
        return true;
    }

    private static boolean hasApnType(String typeList, String target) {
        if (typeList == null || typeList.trim().isEmpty()) {
            return "default".equalsIgnoreCase(target);
        }
        for (String part : typeList.split(",")) {
            String p = part.trim();
            if ("*".equals(p) || target.equalsIgnoreCase(p)) {
                return true;
            }
        }
        return false;
    }

    private void ensureCarrierApns(int subId) {
        String numeric = resolveSimNumeric(subId);
        if (!isValidNumeric(numeric)) {
            return;
        }
        String cacheKey = subId + ":" + numeric;
        if (provisionedNumerics.contains(cacheKey)) {
            return;
        }

        // app_process 未在 AMS 注册 ProcessRecord：优先用 getContentProvider(null, pkg, "telephony", 0, false)
        // （无需 ACCESS_CONTENT_PROVIDERS_EXTERNALLY 权限，直接兼容 uid=1001 radio），回退 getContentProviderExternal。
        Object am = null;
        Object provider = null;
        IBinder token = new Binder();
        boolean usedExternal = false;
        AttributionSource attr = pkgContext.getAttributionSource();
        try {
            Class<?> amCls = Class.forName("android.app.ActivityManager");
            am = amCls.getMethod("getService").invoke(null);
            Object holder = null;
            try {
                Method getCp = am.getClass().getMethod(
                    "getContentProvider",
                    Class.forName("android.app.IApplicationThread"),
                    String.class, String.class, int.class, boolean.class);
                holder = getCp.invoke(am, null, pkgContext.getPackageName(), "telephony", 0, false);
            } catch (Throwable ignored) {
            }
            if (holder == null) {
                holder = am.getClass()
                    .getMethod("getContentProviderExternal", String.class, int.class, IBinder.class, String.class)
                    .invoke(am, "telephony", 0, token, "*cmd*");
                usedExternal = true;
            }
            if (holder != null) {
                Field fProv = holder.getClass().getField("provider");
                provider = fProv.get(holder);
            }
        } catch (Throwable t) {
            Throwable cause = t.getCause() != null ? t.getCause() : t;
            Log.w(TAG, "acquire telephony provider failed: " + cause.getMessage());
            provisionedNumerics.add(cacheKey);
            return;
        }
        if (provider == null) {
            provisionedNumerics.add(cacheKey);
            return;
        }

        try {
            boolean hasDefault = false;
            boolean hasIms = false;
            Cursor cur = null;
            try {
                Bundle qArgs = new Bundle();
                qArgs.putString("android:query-arg-sql-selection", "numeric=?");
                qArgs.putStringArray("android:query-arg-sql-selection-args", new String[] {numeric});
                Method qMethod = provider.getClass().getMethod(
                    "query", AttributionSource.class, Uri.class, String[].class, Bundle.class,
                    Class.forName("android.os.ICancellationSignal"));
                cur = (Cursor) qMethod.invoke(provider, attr, CARRIERS_URI,
                    new String[] {"_id", "apn", "type"}, qArgs, null);
                if (cur == null) {
                    return;
                }
                while (cur.moveToNext()) {
                    String apn = cur.getString(1);
                    String type = cur.getString(2);
                    if (hasApnType(type, "default")) {
                        hasDefault = true;
                    }
                    if (hasApnType(type, "ims") || "ims".equalsIgnoreCase(apn)) {
                        hasIms = true;
                    }
                }
            } catch (Throwable t) {
                Throwable cause = t.getCause() != null ? t.getCause() : t;
                Log.w(TAG, "query telephony/carriers failed for " + numeric + ": " + cause.getMessage());
                return;
            } finally {
                if (cur != null) {
                    try {
                        cur.close();
                    } catch (Throwable ignored) {
                    }
                }
            }

            if (hasDefault && hasIms) {
                provisionedNumerics.add(cacheKey);
                Log.i(TAG, "APN DB already complete for numeric=" + numeric + " (default+ims)");
                return;
            }

            String mcc = numeric.substring(0, 3);
            String mnc = numeric.substring(3);
            List<PresetApn> presets = loadPresetsFor(mcc, mnc);
            if (presets.isEmpty()) {
                presets.add(new PresetApn("Internet", mcc, mnc, "internet", "default,supl", "IPV4V6", "IPV4V6", null, null, null));
                presets.add(new PresetApn("IMS", mcc, mnc, "ims", "ims", "IPV4V6", "IPV4V6", null, null, null));
            }

            Method insMethod = provider.getClass().getMethod(
                "insert", AttributionSource.class, Uri.class, ContentValues.class, Bundle.class);
            long insertedDefaultId = -1L;
            int insertedCount = 0;
            for (PresetApn p : presets) {
                boolean isImsRow = hasApnType(p.type, "ims") && !hasApnType(p.type, "default");
                boolean isDefaultRow = hasApnType(p.type, "default");
                if (isImsRow && hasIms) {
                    continue;
                }
                if (!isImsRow && hasDefault) {
                    continue;
                }
                ContentValues cv = new ContentValues();
                cv.put("name", p.carrier);
                cv.put("numeric", numeric);
                cv.put("mcc", mcc);
                cv.put("mnc", mnc);
                cv.put("apn", p.apn);
                cv.put("type", p.type);
                cv.put("protocol", p.protocol);
                cv.put("roaming_protocol", p.roamingProtocol);
                cv.put("carrier_enabled", 1);
                cv.put("current", 1);
                cv.put("edited", 1);
                if (p.mmsc != null && !p.mmsc.isEmpty()) {
                    cv.put("mmsc", p.mmsc);
                }
                if (p.mmsProxy != null && !p.mmsProxy.isEmpty()) {
                    cv.put("mmsproxy", p.mmsProxy);
                }
                if (p.mmsPort != null && !p.mmsPort.isEmpty()) {
                    cv.put("mmsport", p.mmsPort);
                }
                try {
                    Uri res = (Uri) insMethod.invoke(provider, attr, CARRIERS_URI, cv, new Bundle());
                    if (res != null) {
                        insertedCount++;
                        if (isDefaultRow && insertedDefaultId < 0) {
                            insertedDefaultId = ContentUris.parseId(res);
                        }
                    }
                } catch (Throwable t) {
                    Throwable cause = t.getCause() != null ? t.getCause() : t;
                    Log.w(TAG, "insert preset APN " + p.apn + " (" + numeric + ") skipped: " + cause.getMessage());
                }
            }

            if (!hasDefault && insertedDefaultId > 0 && subId > 0) {
                try {
                    Method updMethod = provider.getClass().getMethod(
                        "update", AttributionSource.class, Uri.class, ContentValues.class, Bundle.class);
                    Uri prefUri = Uri.parse("content://telephony/carriers/preferapn/subId/" + subId);
                    ContentValues pref = new ContentValues();
                    pref.put("apn_id", insertedDefaultId);
                    updMethod.invoke(provider, attr, prefUri, pref, new Bundle());
                } catch (Throwable ignored) {
                }
            }

            provisionedNumerics.add(cacheKey);
            Log.i(TAG, "provisioned " + insertedCount + " preset APN(s) for numeric=" + numeric +
                " (hadDefault=" + hasDefault + ", hadIms=" + hasIms + ")");
        } catch (Throwable t) {
            Log.w(TAG, "ensureCarrierApns failed: " + t.getMessage());
        } finally {
            if (usedExternal) {
                try {
                    am.getClass()
                        .getMethod("removeContentProviderExternalAsUser", String.class, IBinder.class, int.class)
                        .invoke(am, "telephony", token, 0);
                } catch (Throwable ignored) {
                }
            }
        }
    }

    private static List<PresetApn> loadPresetsFor(String mcc, String mnc) {
        List<PresetApn> out = new ArrayList<>();
        File f = new File(PRESET_APNS_XML);
        if (!f.isFile() || !f.canRead()) {
            return out;
        }
        FileInputStream fis = null;
        try {
            fis = new FileInputStream(f);
            XmlPullParser xp = Xml.newPullParser();
            xp.setInput(fis, "UTF-8");
            int ev = xp.getEventType();
            while (ev != XmlPullParser.END_DOCUMENT) {
                if (ev == XmlPullParser.START_TAG && "apn".equals(xp.getName())) {
                    String rowMcc = attr(xp, "mcc", "");
                    String rowMnc = attr(xp, "mnc", "");
                    if (mcc.equals(rowMcc) && mnc.equals(rowMnc)) {
                        String proto = attr(xp, "protocol", "IPV4V6").toUpperCase(Locale.ROOT);
                        String rProto = attr(xp, "roaming_protocol", proto).toUpperCase(Locale.ROOT);
                        out.add(new PresetApn(
                            attr(xp, "carrier", "Carrier"),
                            mcc,
                            mnc,
                            attr(xp, "apn", "internet"),
                            attr(xp, "type", "default,supl"),
                            proto,
                            rProto,
                            attr(xp, "mmsc", ""),
                            attr(xp, "mmsproxy", ""),
                            attr(xp, "mmsport", "")
                        ));
                    }
                }
                ev = xp.next();
            }
        } catch (Throwable t) {
            Log.w(TAG, "parse " + PRESET_APNS_XML + " failed: " + t.getMessage());
        } finally {
            if (fis != null) {
                try {
                    fis.close();
                } catch (Throwable ignored) {
                }
            }
        }
        return out;
    }

    private static String attr(XmlPullParser xp, String name, String defVal) {
        String v = xp.getAttributeValue(null, name);
        return (v != null && !v.isEmpty()) ? v : defVal;
    }

    private void reconcile() {
        long nextDelayMs = 5000L;
        try {
            int subId = resolveActiveSubId();
            if (subId <= 0) {
                if (activeCallback != null) {
                    safeUnregister(activeCallback);
                    activeCallback = null;
                    activeSubId = -1;
                    imsConnected = false;
                }
                handler.postDelayed(this::reconcile, 3000L);
                return;
            }

            ensureCarrierApns(subId);

            if (activeCallback == null && hasExternalImsNetwork()) {
                handler.postDelayed(this::reconcile, 30000L);
                return;
            }

            if (activeCallback == null || activeSubId != subId) {
                if (activeCallback != null) {
                    safeUnregister(activeCallback);
                }
                final int targetSub = subId;
                ConnectivityManager.NetworkCallback cb =
                    new ConnectivityManager.NetworkCallback() {
                        @Override
                        public void onAvailable(Network network) {
                            imsConnected = true;
                            Log.i(TAG, "IMS network connected on subId=" + targetSub + ": " + network);
                        }

                        @Override
                        public void onLost(Network network) {
                            imsConnected = false;
                            Log.i(TAG, "IMS network lost on subId=" + targetSub + ": " + network);
                        }
                    };
                NetworkSpecifier spec = new TelephonyNetworkSpecifier.Builder()
                    .setSubscriptionId(targetSub)
                    .build();
                NetworkRequest req = new NetworkRequest.Builder()
                    .addTransportType(NetworkCapabilities.TRANSPORT_CELLULAR)
                    .addCapability(NetworkCapabilities.NET_CAPABILITY_IMS)
                    .removeCapability(NetworkCapabilities.NET_CAPABILITY_NOT_RESTRICTED)
                    .setNetworkSpecifier(spec)
                    .build();
                cm.requestNetwork(req, cb);
                activeCallback = cb;
                activeSubId = targetSub;
                Log.i(TAG, "requested IMS cellular PDN for subId=" + targetSub +
                    " via pkg=" + pkgContext.getPackageName());
            }
            nextDelayMs = imsConnected ? 30000L : 5000L;
        } catch (Throwable t) {
            Log.w(TAG, "reconcile failed: " + t.getMessage());
        }
        handler.postDelayed(this::reconcile, nextDelayMs);
    }

    private boolean hasExternalImsNetwork() {
        try {
            Network[] nets = cm.getAllNetworks();
            if (nets == null) {
                return false;
            }
            for (Network n : nets) {
                NetworkCapabilities nc = cm.getNetworkCapabilities(n);
                if (nc != null &&
                    nc.hasTransport(NetworkCapabilities.TRANSPORT_CELLULAR) &&
                    nc.hasCapability(NetworkCapabilities.NET_CAPABILITY_IMS)) {
                    return true;
                }
            }
        } catch (Throwable ignored) {
        }
        return false;
    }

    private void safeUnregister(ConnectivityManager.NetworkCallback cb) {
        try {
            cm.unregisterNetworkCallback(cb);
        } catch (Throwable ignored) {
        }
    }
}
