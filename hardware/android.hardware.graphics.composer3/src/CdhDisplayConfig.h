#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace aidl::android::hardware::graphics::composer3::cdh {

// CdhDisplayConfig 汇总运行态从 ro.vendor.cdh.display.* 读取的面板对齐参数。
struct CdhDisplayConfig {
  std::vector<int32_t> refresh_rates = {60, 120, 90};
  int32_t default_refresh_rate = 0;
  bool wide_color_gamut = true;
  float xdpi = 0.0f;
  float ydpi = 0.0f;

  std::string edid_pnp;
  std::string edid_name;
  uint16_t edid_product_id = 0;
  uint8_t edid_week = 0;
  int32_t edid_year = 1990;
  std::vector<uint8_t> edid_blob;

  bool HasCustomDpi() const { return xdpi > 0.0f && ydpi > 0.0f; }
  bool HasCustomEdid() const { return edid_blob.size() == 128; }
};

// LoadCdhDisplayConfig 在服务启动时读取 vendor 属性并构建 EDID 数据块。
CdhDisplayConfig LoadCdhDisplayConfig();

// BuildEdid128 按 EDID v1.3 规范生成 128 字节面板身份数据块；参数不合法时返回空向量。
std::vector<uint8_t> BuildEdid128(const std::string& pnp,
                                  const std::string& name,
                                  uint16_t product_id,
                                  uint8_t week,
                                  int32_t year);

}  // namespace aidl::android::hardware::graphics::composer3::cdh
