// Copyright (c) 2025-2026 fei_cong(https://github.com/feicong/feicong-course)

#ifndef WIFI_CONFIG_H_
#define WIFI_CONFIG_H_

// wifi 设备值外置配置（对齐 audio/gnss/media 的 XML 外置配方）。
//
// 设备树里这些参数由 config/wifi.json 生成编译期 wifi_target.inc（constexpr 常量）
// 烘焙进二进制；拆分后改为**运行时**从 /vendor/etc/wifi_config.xml 读取，gold 供文件、换机无需重编。
// 仅宿主通道相关的 6 个运行参数外置（原 wifi_target.inc 内容）；服务名/接口名/能力
// 上限等仍是源码常量（与 audio 一致：只外置设备树注入值，不扩张范围）。
//
// 文件格式（根元素的属性，缺失则回落编译期内置默认）：
//   <wifi-config channel="/dev/hvc9"
//                scan-profile="/sys/module/mac80211_hwsim/parameters/scan_profile"
//                frame-max="1048576" reconnect-delay-ms="1000"
//                read-timeout-ms="30000" protocol-version="1"/>
//
// 解析纪律：纯 std + libxml2（按 staticLibs 静态编入，无异常）、整数走 strtol（非 std::stoi）。

#include <libxml/parser.h>
#include <libxml/tree.h>

#include <cstdint>
#include <cstdlib>
#include <string>

namespace cdh_wifi {

// ---- libxml2 读取便利层（头文件形式，故 inline）----
// 读整个文件成 DOM；失败返回 nullptr（调用方回退内置默认）。
inline xmlDocPtr LoadXmlDoc(const std::string &path) {
  return xmlReadFile(path.c_str(), nullptr,
                     XML_PARSE_NONET | XML_PARSE_NOBLANKS);
}

// 取属性值；缺失返回 false（值拷进 std::string，属性内存即时释放）。
inline bool ReadProp(const xmlNode *node, const char *name, std::string *out) {
  if (node == nullptr) return false;
  xmlChar *v = xmlGetProp(const_cast<xmlNode *>(node), BAD_CAST name);
  if (v == nullptr) return false;
  out->assign(reinterpret_cast<const char *>(v));
  xmlFree(v);
  return true;
}

struct WifiConfig {
  std::string hostChannel = "/dev/hvc9";
  std::string scanProfilePath =
      "/sys/module/mac80211_hwsim/parameters/scan_profile";
  int32_t frameMax = 1 << 20;
  int32_t reconnectDelayMs = 1000;
  int32_t readTimeoutMs = 30000;
  int32_t protocolVersion = 1;
};

// 配置路径：env WIFI_CONFIG_XML 优先，否则设备默认落点。
inline std::string WifiConfigPath() {
  const char *env = std::getenv("WIFI_CONFIG_XML");
  if (env != nullptr && env[0] != '\0') return env;
  return "/vendor/etc/wifi_config.xml";
}

inline int32_t ParseInt(const std::string &s) {
  const char *begin = s.c_str();
  char *end = nullptr;
  const long v = std::strtol(begin, &end, 10);
  if (end == begin) return 0;
  return static_cast<int32_t>(v);
}

// 读配置属性并覆盖内置默认。文件不存在/属性缺失时保持默认值。
inline WifiConfig LoadWifiConfig() {
  WifiConfig cfg;
  xmlDocPtr doc = LoadXmlDoc(WifiConfigPath());
  if (doc == nullptr) return cfg;
  xmlNode *root = xmlDocGetRootElement(doc);
  if (root == nullptr) {
    xmlFreeDoc(doc);
    return cfg;
  }

  std::string v;
  if (ReadProp(root, "channel", &v) && !v.empty()) {
    cfg.hostChannel = v;
  }
  if (ReadProp(root, "scan-profile", &v) && !v.empty()) {
    cfg.scanProfilePath = v;
  }
  if (ReadProp(root, "frame-max", &v)) {
    cfg.frameMax = ParseInt(v);
  }
  if (ReadProp(root, "reconnect-delay-ms", &v)) {
    cfg.reconnectDelayMs = ParseInt(v);
  }
  if (ReadProp(root, "read-timeout-ms", &v)) {
    cfg.readTimeoutMs = ParseInt(v);
  }
  if (ReadProp(root, "protocol-version", &v)) {
    cfg.protocolVersion = ParseInt(v);
  }
  xmlFreeDoc(doc);
  return cfg;
}

}  // namespace cdh_wifi

#endif  // WIFI_CONFIG_H_
