#define LOG_TAG "drmhwc"

#include "CdhDisplayConfig.h"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <cstring>
#include <cutils/properties.h>
#include <log/log.h>

namespace aidl::android::hardware::graphics::composer3::cdh {
namespace {

std::string ReadPropTrimmed(const char* key) {
  char buf[PROPERTY_VALUE_MAX] = {0};
  if (property_get(key, buf, "") <= 0) {
    return {};
  }
  std::string s(buf);
  const auto first = s.find_first_not_of(" \t\r\n");
  if (first == std::string::npos) {
    return {};
  }
  const auto last = s.find_last_not_of(" \t\r\n");
  return s.substr(first, last - first + 1);
}

bool ParseBoolString(const std::string& raw, bool default_val) {
  if (raw.empty()) {
    return default_val;
  }
  std::string lower = raw;
  std::transform(lower.begin(), lower.end(), lower.begin(),
                 [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
  if (lower == "1" || lower == "true" || lower == "yes" || lower == "on") {
    return true;
  }
  if (lower == "0" || lower == "false" || lower == "no" || lower == "off") {
    return false;
  }
  return default_val;
}

float ParsePositiveDpi(const std::string& raw) {
  if (raw.empty()) {
    return 0.0f;
  }
  char* end = nullptr;
  const float val = std::strtof(raw.c_str(), &end);
  if (end == raw.c_str() || val <= 0.0f || val > 2000.0f) {
    return 0.0f;
  }
  return val;
}

std::vector<int32_t> ParseRefreshRates(const std::string& raw) {
  std::vector<int32_t> out;
  if (raw.empty()) {
    return out;
  }
  size_t pos = 0;
  while (pos < raw.size()) {
    const size_t comma = raw.find(',', pos);
    const std::string token =
        (comma == std::string::npos) ? raw.substr(pos) : raw.substr(pos, comma - pos);
    const auto first = token.find_first_not_of(" \t");
    if (first != std::string::npos) {
      const auto last = token.find_last_not_of(" \t");
      const std::string sub = token.substr(first, last - first + 1);
      char* end = nullptr;
      const long hz = std::strtol(sub.c_str(), &end, 10);
      if (end != sub.c_str() && *end == '\0' && hz >= 30 && hz <= 240) {
        const int32_t rate = static_cast<int32_t>(hz);
        if (std::find(out.begin(), out.end(), rate) == out.end()) {
          out.push_back(rate);
        }
      }
    }
    if (comma == std::string::npos) {
      break;
    }
    pos = comma + 1;
  }
  return out;
}

}  // namespace

std::vector<uint8_t> BuildEdid128(const std::string& pnp,
                                  const std::string& name,
                                  uint16_t product_id,
                                  uint8_t week,
                                  int32_t year) {
  if (pnp.size() != 3 || name.empty()) {
    return {};
  }
  for (char c : pnp) {
    if (c < 'A' || c > 'Z') {
      return {};
    }
  }
  if (year < 1990) {
    year = 1990;
  } else if (year > 1990 + 255) {
    year = 1990 + 255;
  }

  std::vector<uint8_t> edid(128, 0);
  const uint8_t header[8] = {0x00, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x00};
  std::memcpy(edid.data(), header, sizeof(header));

  const uint16_t mfg =
      static_cast<uint16_t>(((static_cast<uint16_t>(pnp[0] - '@') & 0x1fU) << 10) |
                            ((static_cast<uint16_t>(pnp[1] - '@') & 0x1fU) << 5) |
                            (static_cast<uint16_t>(pnp[2] - '@') & 0x1fU));
  // Bytes 8..9: 大端 16-bit EISA 3 字母厂商 PnP ID
  edid[8] = static_cast<uint8_t>((mfg >> 8) & 0xffU);
  edid[9] = static_cast<uint8_t>(mfg & 0xffU);
  // Bytes 10..11: 小端 uint16 product_id
  edid[10] = static_cast<uint8_t>(product_id & 0xffU);
  edid[11] = static_cast<uint8_t>((product_id >> 8) & 0xffU);
  // Bytes 12..15: 小端 uint32(1)
  edid[12] = 0x01;
  edid[13] = 0x00;
  edid[14] = 0x00;
  edid[15] = 0x00;
  // Byte 16..19: 周、年偏移与 EDID v1.3 标识 (01 03)
  edid[16] = week;
  edid[17] = static_cast<uint8_t>(year - 1990);
  edid[18] = 0x01;
  edid[19] = 0x03;

  // Bytes 54..71: Monitor Name 描述槽 (00 00 00 fc 00 + name + '\n' + spaces)
  edid[54] = 0x00;
  edid[55] = 0x00;
  edid[56] = 0x00;
  edid[57] = 0xfc;
  edid[58] = 0x00;
  for (size_t i = 59; i < 72; ++i) {
    edid[i] = 0x20;
  }
  const size_t copy_len = std::min<size_t>(name.size(), 12);
  std::memcpy(&edid[59], name.data(), copy_len);
  edid[59 + copy_len] = 0x0a;

  uint32_t sum = 0;
  for (size_t i = 0; i < 127; ++i) {
    sum += edid[i];
  }
  edid[127] = static_cast<uint8_t>((256U - (sum & 0xffU)) & 0xffU);
  return edid;
}

CdhDisplayConfig LoadCdhDisplayConfig() {
  CdhDisplayConfig cfg;

  const auto parsed_rates =
      ParseRefreshRates(ReadPropTrimmed("ro.vendor.cdh.display.refresh_rates"));
  if (!parsed_rates.empty()) {
    cfg.refresh_rates = parsed_rates;
  }

  const std::string def_rate_raw =
      ReadPropTrimmed("ro.vendor.cdh.display.default_refresh_rate");
  if (!def_rate_raw.empty()) {
    char* end = nullptr;
    const long hz = std::strtol(def_rate_raw.c_str(), &end, 10);
    if (end != def_rate_raw.c_str() && *end == '\0' && hz >= 30 && hz <= 240) {
      cfg.default_refresh_rate = static_cast<int32_t>(hz);
    }
  }

  const std::string wide_raw = ReadPropTrimmed("ro.vendor.cdh.display.wide_color_gamut");
  if (!wide_raw.empty()) {
    cfg.wide_color_gamut = ParseBoolString(wide_raw, true);
  } else {
    const std::string sf_wide =
        ReadPropTrimmed("ro.surface_flinger.has_wide_color_display");
    if (!sf_wide.empty()) {
      cfg.wide_color_gamut = ParseBoolString(sf_wide, true);
    }
  }

  cfg.xdpi = ParsePositiveDpi(ReadPropTrimmed("ro.vendor.cdh.display.xdpi"));
  cfg.ydpi = ParsePositiveDpi(ReadPropTrimmed("ro.vendor.cdh.display.ydpi"));

  std::string pnp = ReadPropTrimmed("ro.vendor.cdh.display.edid_pnp");
  std::transform(pnp.begin(), pnp.end(), pnp.begin(),
                 [](unsigned char c) { return static_cast<char>(std::toupper(c)); });
  cfg.edid_pnp = pnp;
  cfg.edid_name = ReadPropTrimmed("ro.vendor.cdh.display.edid_name");

  const std::string pid_raw = ReadPropTrimmed("ro.vendor.cdh.display.edid_product_id");
  if (!pid_raw.empty()) {
    char* end = nullptr;
    const unsigned long v = std::strtoul(pid_raw.c_str(), &end, 10);
    if (end != pid_raw.c_str() && *end == '\0' && v <= 0xffffUL) {
      cfg.edid_product_id = static_cast<uint16_t>(v);
    }
  }

  const std::string week_raw = ReadPropTrimmed("ro.vendor.cdh.display.edid_week");
  if (!week_raw.empty()) {
    char* end = nullptr;
    const unsigned long v = std::strtoul(week_raw.c_str(), &end, 10);
    if (end != week_raw.c_str() && *end == '\0' && v <= 0xffUL) {
      cfg.edid_week = static_cast<uint8_t>(v);
    }
  }

  const std::string year_raw = ReadPropTrimmed("ro.vendor.cdh.display.edid_year");
  if (!year_raw.empty()) {
    char* end = nullptr;
    const long v = std::strtol(year_raw.c_str(), &end, 10);
    if (end != year_raw.c_str() && *end == '\0' && v >= 1990) {
      cfg.edid_year = static_cast<int32_t>(v);
    }
  }

  cfg.edid_blob = BuildEdid128(cfg.edid_pnp, cfg.edid_name, cfg.edid_product_id,
                               cfg.edid_week, cfg.edid_year);

  ALOGI("cdh-composer3 config: rates=%zu default_rate=%d wide_color=%d dpi=(%.5f, %.5f) edid=%s/%s",
        cfg.refresh_rates.size(), cfg.default_refresh_rate,
        cfg.wide_color_gamut ? 1 : 0, cfg.xdpi, cfg.ydpi,
        cfg.edid_pnp.c_str(), cfg.edid_name.c_str());
  return cfg;
}

}  // namespace aidl::android::hardware::graphics::composer3::cdh
