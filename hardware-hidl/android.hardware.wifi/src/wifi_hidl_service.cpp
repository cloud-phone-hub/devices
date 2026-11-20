// engine/vendor/hardware —— Android 13 HIDL android.hardware.wifi@1.6 自研服务。
//
// 接入 wifi_host_channel（/dev/hvc9 -> mac80211_hwsim scan_profile），注册 IWifi@1.6/default，
// 提供完整虚拟 STA 接口（wlan0）与单调递增时间戳的 LinkLayerStats（防 WifiScoreReport 除零崩溃）。

#define LOG_TAG "CdhWifiHidl"

#include <android/hardware/wifi/1.6/IWifi.h>
#include <android/hardware/wifi/1.6/IWifiChip.h>
#include <android/hardware/wifi/1.6/IWifiStaIface.h>
#include <hidl/HidlTransportSupport.h>
#include <log/log.h>

#include "wifi_host_channel.h"

#include <chrono>
#include <cstdint>
#include <cstdio>
#include <mutex>
#include <string>
#include <vector>

namespace {

using ::android::sp;
using ::android::hardware::hidl_array;
using ::android::hardware::hidl_bitfield;
using ::android::hardware::hidl_string;
using ::android::hardware::hidl_vec;
using ::android::hardware::Return;
using ::android::hardware::Void;

namespace V1_0 = ::android::hardware::wifi::V1_0;
namespace V1_1 = ::android::hardware::wifi::V1_1;
namespace V1_2 = ::android::hardware::wifi::V1_2;
namespace V1_3 = ::android::hardware::wifi::V1_3;
namespace V1_4 = ::android::hardware::wifi::V1_4;
namespace V1_5 = ::android::hardware::wifi::V1_5;
namespace V1_6 = ::android::hardware::wifi::V1_6;

using V1_0::ChipId;
using V1_0::ChipModeId;
using V1_0::CommandId;
using V1_0::IfaceType;
using V1_0::MacAddress;
using V1_0::Rssi;
using V1_0::StaApfPacketFilterCapabilities;
using V1_0::StaBackgroundScanCapabilities;
using V1_0::StaBackgroundScanParameters;
using V1_0::StaRoamingCapabilities;
using V1_0::StaRoamingConfig;
using V1_0::StaRoamingState;
using V1_0::WifiChannelInMhz;
using V1_0::WifiDebugHostWakeReasonStats;
using V1_0::WifiDebugRingBufferStatus;
using V1_0::WifiDebugRingBufferVerboseLevel;
using V1_0::WifiDebugRxPacketFateReport;
using V1_0::WifiDebugTxPacketFateReport;
using V1_0::WifiStatus;
using V1_0::WifiStatusCode;

WifiStatus okStatus() {
  return WifiStatus{WifiStatusCode::SUCCESS, ""};
}

WifiStatus unsupportedStatus() {
  return WifiStatus{WifiStatusCode::ERROR_NOT_SUPPORTED, "Not supported on virtual cloud radio"};
}

uint64_t bootTimeMs() {
  return static_cast<uint64_t>(
      std::chrono::duration_cast<std::chrono::milliseconds>(
          std::chrono::steady_clock::now().time_since_epoch())
          .count());
}

MacAddress readIfaceMac(const std::string &ifname) {
  MacAddress mac{{0x02, 0x00, 0x00, 0x00, 0x00, 0x00}};
  const std::string path = "/sys/class/net/" + ifname + "/address";
  std::FILE *fp = std::fopen(path.c_str(), "r");
  if (fp != nullptr) {
    char buf[64] = {0};
    if (std::fgets(buf, sizeof(buf), fp) != nullptr) {
      unsigned int b[6] = {0};
      if (std::sscanf(buf, "%02x:%02x:%02x:%02x:%02x:%02x",
                      &b[0], &b[1], &b[2], &b[3], &b[4], &b[5]) == 6) {
        for (size_t i = 0; i < 6; ++i) mac[i] = static_cast<uint8_t>(b[i]);
      }
    }
    std::fclose(fp);
  }
  return mac;
}

class WifiStaIfaceImpl : public V1_6::IWifiStaIface {
 public:
  explicit WifiStaIfaceImpl(std::string ifname) : ifname_(std::move(ifname)) {}

  Return<void> getType(getType_cb _hidl_cb) override {
    _hidl_cb(okStatus(), IfaceType::STA);
    return Void();
  }

  Return<void> getName(getName_cb _hidl_cb) override {
    _hidl_cb(okStatus(), ifname_);
    return Void();
  }

  Return<void> registerEventCallback(const sp<V1_0::IWifiStaIfaceEventCallback> &,
                                     registerEventCallback_cb _hidl_cb) override {
    _hidl_cb(okStatus());
    return Void();
  }

  Return<void> getCapabilities(getCapabilities_cb _hidl_cb) override {
    const uint32_t caps = V1_0::IWifiStaIface::StaIfaceCapabilityMask::LINK_LAYER_STATS |
                          V1_0::IWifiStaIface::StaIfaceCapabilityMask::STA_5G;
    _hidl_cb(okStatus(), caps);
    return Void();
  }

  Return<void> getApfPacketFilterCapabilities(getApfPacketFilterCapabilities_cb _hidl_cb) override {
    _hidl_cb(okStatus(), StaApfPacketFilterCapabilities{0, 0});
    return Void();
  }

  Return<void> installApfPacketFilter(CommandId, const hidl_vec<uint8_t> &,
                                      installApfPacketFilter_cb _hidl_cb) override {
    _hidl_cb(unsupportedStatus());
    return Void();
  }

  Return<void> readApfPacketFilterData(readApfPacketFilterData_cb _hidl_cb) override {
    _hidl_cb(unsupportedStatus(), hidl_vec<uint8_t>{});
    return Void();
  }

  Return<void> getBackgroundScanCapabilities(getBackgroundScanCapabilities_cb _hidl_cb) override {
    _hidl_cb(unsupportedStatus(), StaBackgroundScanCapabilities{});
    return Void();
  }

  Return<void> getValidFrequenciesForBand(V1_0::WifiBand band,
                                          getValidFrequenciesForBand_cb _hidl_cb) override {
    std::vector<WifiChannelInMhz> freqs;
    const uint32_t b = static_cast<uint32_t>(band);
    if ((b & static_cast<uint32_t>(V1_0::WifiBand::BAND_24GHZ)) != 0 || b == 0) {
      for (int ch = 0; ch < 11; ++ch) freqs.push_back(2412 + ch * 5);
    }
    if ((b & static_cast<uint32_t>(V1_0::WifiBand::BAND_5GHZ)) != 0) {
      for (uint32_t f : {5180u, 5200u, 5220u, 5240u, 5745u, 5765u, 5785u, 5805u, 5825u}) {
        freqs.push_back(f);
      }
    }
    _hidl_cb(okStatus(), freqs);
    return Void();
  }

  Return<void> startBackgroundScan(CommandId, const StaBackgroundScanParameters &,
                                   startBackgroundScan_cb _hidl_cb) override {
    _hidl_cb(unsupportedStatus());
    return Void();
  }

  Return<void> stopBackgroundScan(CommandId, stopBackgroundScan_cb _hidl_cb) override {
    _hidl_cb(unsupportedStatus());
    return Void();
  }

  Return<void> enableLinkLayerStatsCollection(bool,
                                              enableLinkLayerStatsCollection_cb _hidl_cb) override {
    _hidl_cb(okStatus());
    return Void();
  }

  Return<void> disableLinkLayerStatsCollection(
      disableLinkLayerStatsCollection_cb _hidl_cb) override {
    _hidl_cb(okStatus());
    return Void();
  }

  Return<void> getLinkLayerStats(getLinkLayerStats_cb _hidl_cb) override {
    V1_0::StaLinkLayerStats stats{};
    stats.iface.beaconRx = 1;
    stats.iface.avgRssiMgmt = -50;
    stats.timeStampInMs = bootTimeMs();
    _hidl_cb(okStatus(), stats);
    return Void();
  }

  Return<void> getLinkLayerStats_1_3(getLinkLayerStats_1_3_cb _hidl_cb) override {
    V1_3::StaLinkLayerStats stats{};
    stats.iface.beaconRx = 1;
    stats.iface.avgRssiMgmt = -50;
    stats.timeStampInMs = bootTimeMs();
    _hidl_cb(okStatus(), stats);
    return Void();
  }

  Return<void> getLinkLayerStats_1_5(getLinkLayerStats_1_5_cb _hidl_cb) override {
    V1_5::StaLinkLayerStats stats{};
    stats.iface.V1_0.beaconRx = 1;
    stats.iface.V1_0.avgRssiMgmt = -50;
    stats.timeStampInMs = bootTimeMs();
    _hidl_cb(okStatus(), stats);
    return Void();
  }

  Return<void> getLinkLayerStats_1_6(getLinkLayerStats_1_6_cb _hidl_cb) override {
    V1_6::StaLinkLayerStats stats{};
    stats.iface.V1_0.wmeBePktStats.rxMpdu = 10;
    stats.iface.V1_0.wmeBePktStats.txMpdu = 10;
    stats.iface.V1_0.beaconRx = 1;
    stats.iface.V1_0.avgRssiMgmt = -50;
    stats.timeStampInMs = bootTimeMs();
    _hidl_cb(okStatus(), stats);
    return Void();
  }

  Return<void> startRssiMonitoring(CommandId, Rssi, Rssi,
                                   startRssiMonitoring_cb _hidl_cb) override {
    _hidl_cb(unsupportedStatus());
    return Void();
  }

  Return<void> stopRssiMonitoring(CommandId, stopRssiMonitoring_cb _hidl_cb) override {
    _hidl_cb(unsupportedStatus());
    return Void();
  }

  Return<void> getRoamingCapabilities(getRoamingCapabilities_cb _hidl_cb) override {
    _hidl_cb(unsupportedStatus(), StaRoamingCapabilities{});
    return Void();
  }

  Return<void> configureRoaming(const StaRoamingConfig &, configureRoaming_cb _hidl_cb) override {
    _hidl_cb(unsupportedStatus());
    return Void();
  }

  Return<void> setRoamingState(StaRoamingState, setRoamingState_cb _hidl_cb) override {
    _hidl_cb(okStatus());
    return Void();
  }

  Return<void> enableNdOffload(bool, enableNdOffload_cb _hidl_cb) override {
    _hidl_cb(okStatus());
    return Void();
  }

  Return<void> startSendingKeepAlivePackets(CommandId, const hidl_vec<uint8_t> &, uint16_t,
                                            const MacAddress &, const MacAddress &, uint32_t,
                                            startSendingKeepAlivePackets_cb _hidl_cb) override {
    _hidl_cb(okStatus());
    return Void();
  }

  Return<void> stopSendingKeepAlivePackets(CommandId,
                                           stopSendingKeepAlivePackets_cb _hidl_cb) override {
    _hidl_cb(okStatus());
    return Void();
  }

  Return<void> setScanningMacOui(const hidl_array<uint8_t, 3> &,
                                 setScanningMacOui_cb _hidl_cb) override {
    _hidl_cb(unsupportedStatus());
    return Void();
  }

  Return<void> startDebugPacketFateMonitoring(
      startDebugPacketFateMonitoring_cb _hidl_cb) override {
    _hidl_cb(unsupportedStatus());
    return Void();
  }

  Return<void> getDebugTxPacketFates(getDebugTxPacketFates_cb _hidl_cb) override {
    _hidl_cb(unsupportedStatus(), hidl_vec<WifiDebugTxPacketFateReport>{});
    return Void();
  }

  Return<void> getDebugRxPacketFates(getDebugRxPacketFates_cb _hidl_cb) override {
    _hidl_cb(unsupportedStatus(), hidl_vec<WifiDebugRxPacketFateReport>{});
    return Void();
  }

  Return<void> setMacAddress(const MacAddress &, setMacAddress_cb _hidl_cb) override {
    _hidl_cb(okStatus());
    return Void();
  }

  Return<void> getFactoryMacAddress(getFactoryMacAddress_cb _hidl_cb) override {
    _hidl_cb(okStatus(), readIfaceMac(ifname_));
    return Void();
  }

  Return<void> setScanMode(bool, setScanMode_cb _hidl_cb) override {
    _hidl_cb(okStatus());
    return Void();
  }

 private:
  std::string ifname_;
};

class WifiChipImpl : public V1_6::IWifiChip {
 public:
  Return<void> getId(getId_cb _hidl_cb) override {
    _hidl_cb(okStatus(), 0);
    return Void();
  }

  Return<void> registerEventCallback(const sp<V1_0::IWifiChipEventCallback> &,
                                     registerEventCallback_cb _hidl_cb) override {
    _hidl_cb(okStatus());
    return Void();
  }

  Return<void> registerEventCallback_1_2(const sp<V1_2::IWifiChipEventCallback> &,
                                         registerEventCallback_1_2_cb _hidl_cb) override {
    _hidl_cb(okStatus());
    return Void();
  }

  Return<void> registerEventCallback_1_4(const sp<V1_4::IWifiChipEventCallback> &,
                                         registerEventCallback_1_4_cb _hidl_cb) override {
    _hidl_cb(okStatus());
    return Void();
  }

  Return<void> getCapabilities(getCapabilities_cb _hidl_cb) override {
    _hidl_cb(okStatus(), 0);
    return Void();
  }

  Return<void> getCapabilities_1_3(getCapabilities_1_3_cb _hidl_cb) override {
    _hidl_cb(okStatus(), 0);
    return Void();
  }

  Return<void> getCapabilities_1_5(getCapabilities_1_5_cb _hidl_cb) override {
    _hidl_cb(okStatus(), 0);
    return Void();
  }

  Return<void> getAvailableModes(getAvailableModes_cb _hidl_cb) override {
    V1_0::IWifiChip::ChipIfaceCombinationLimit lim{{IfaceType::STA}, 1};
    V1_0::IWifiChip::ChipIfaceCombination comb{{lim}};
    V1_0::IWifiChip::ChipMode mode{0, {comb}};
    _hidl_cb(okStatus(), {mode});
    return Void();
  }

  Return<void> getAvailableModes_1_6(getAvailableModes_1_6_cb _hidl_cb) override {
    V1_6::IWifiChip::ChipConcurrencyCombinationLimit lim{
        {V1_6::IfaceConcurrencyType::STA}, 1};
    V1_6::IWifiChip::ChipConcurrencyCombination comb{{lim}};
    V1_6::IWifiChip::ChipMode mode{0, {comb}};
    _hidl_cb(okStatus(), {mode});
    return Void();
  }

  Return<void> configureChip(ChipModeId modeId, configureChip_cb _hidl_cb) override {
    std::lock_guard<std::mutex> lk(mu_);
    configured_ = true;
    modeId_ = modeId;
    _hidl_cb(okStatus());
    return Void();
  }

  Return<void> getMode(getMode_cb _hidl_cb) override {
    std::lock_guard<std::mutex> lk(mu_);
    if (!configured_) {
      _hidl_cb(WifiStatus{WifiStatusCode::ERROR_NOT_AVAILABLE, "Not configured"}, 0);
      return Void();
    }
    _hidl_cb(okStatus(), modeId_);
    return Void();
  }

  Return<void> requestChipDebugInfo(requestChipDebugInfo_cb _hidl_cb) override {
    _hidl_cb(okStatus(), V1_0::IWifiChip::ChipDebugInfo{"mac80211_hwsim", "cdh-virtual-1.0"});
    return Void();
  }

  Return<void> requestDriverDebugDump(requestDriverDebugDump_cb _hidl_cb) override {
    _hidl_cb(okStatus(), hidl_vec<uint8_t>{});
    return Void();
  }

  Return<void> requestFirmwareDebugDump(requestFirmwareDebugDump_cb _hidl_cb) override {
    _hidl_cb(okStatus(), hidl_vec<uint8_t>{});
    return Void();
  }

  Return<void> createApIface(createApIface_cb _hidl_cb) override {
    _hidl_cb(unsupportedStatus(), nullptr);
    return Void();
  }

  Return<void> getApIfaceNames(getApIfaceNames_cb _hidl_cb) override {
    _hidl_cb(okStatus(), hidl_vec<hidl_string>{});
    return Void();
  }

  Return<void> getApIface(const hidl_string &, getApIface_cb _hidl_cb) override {
    _hidl_cb(WifiStatus{WifiStatusCode::ERROR_WIFI_IFACE_INVALID, ""}, nullptr);
    return Void();
  }

  Return<void> removeApIface(const hidl_string &, removeApIface_cb _hidl_cb) override {
    _hidl_cb(WifiStatus{WifiStatusCode::ERROR_WIFI_IFACE_INVALID, ""});
    return Void();
  }

  Return<void> createBridgedApIface(createBridgedApIface_cb _hidl_cb) override {
    _hidl_cb(unsupportedStatus(), nullptr);
    return Void();
  }

  Return<void> removeIfaceInstanceFromBridgedApIface(
      const hidl_string &, const hidl_string &,
      removeIfaceInstanceFromBridgedApIface_cb _hidl_cb) override {
    _hidl_cb(unsupportedStatus());
    return Void();
  }

  Return<void> createNanIface(createNanIface_cb _hidl_cb) override {
    _hidl_cb(unsupportedStatus(), nullptr);
    return Void();
  }

  Return<void> getNanIfaceNames(getNanIfaceNames_cb _hidl_cb) override {
    _hidl_cb(okStatus(), hidl_vec<hidl_string>{});
    return Void();
  }

  Return<void> getNanIface(const hidl_string &, getNanIface_cb _hidl_cb) override {
    _hidl_cb(WifiStatus{WifiStatusCode::ERROR_WIFI_IFACE_INVALID, ""}, nullptr);
    return Void();
  }

  Return<void> removeNanIface(const hidl_string &, removeNanIface_cb _hidl_cb) override {
    _hidl_cb(WifiStatus{WifiStatusCode::ERROR_WIFI_IFACE_INVALID, ""});
    return Void();
  }

  Return<void> createP2pIface(createP2pIface_cb _hidl_cb) override {
    _hidl_cb(unsupportedStatus(), nullptr);
    return Void();
  }

  Return<void> getP2pIfaceNames(getP2pIfaceNames_cb _hidl_cb) override {
    _hidl_cb(okStatus(), hidl_vec<hidl_string>{});
    return Void();
  }

  Return<void> getP2pIface(const hidl_string &, getP2pIface_cb _hidl_cb) override {
    _hidl_cb(WifiStatus{WifiStatusCode::ERROR_WIFI_IFACE_INVALID, ""}, nullptr);
    return Void();
  }

  Return<void> removeP2pIface(const hidl_string &, removeP2pIface_cb _hidl_cb) override {
    _hidl_cb(WifiStatus{WifiStatusCode::ERROR_WIFI_IFACE_INVALID, ""});
    return Void();
  }

  Return<void> createStaIface(createStaIface_cb _hidl_cb) override {
    std::lock_guard<std::mutex> lk(mu_);
    if (staIface_ == nullptr) {
      staIface_ = new WifiStaIfaceImpl("wlan0");
    }
    _hidl_cb(okStatus(), staIface_);
    return Void();
  }

  Return<void> getStaIfaceNames(getStaIfaceNames_cb _hidl_cb) override {
    std::lock_guard<std::mutex> lk(mu_);
    if (staIface_ != nullptr) {
      _hidl_cb(okStatus(), hidl_vec<hidl_string>{"wlan0"});
    } else {
      _hidl_cb(okStatus(), hidl_vec<hidl_string>{});
    }
    return Void();
  }

  Return<void> getStaIface(const hidl_string &ifname, getStaIface_cb _hidl_cb) override {
    std::lock_guard<std::mutex> lk(mu_);
    if (staIface_ != nullptr && ifname == "wlan0") {
      _hidl_cb(okStatus(), staIface_);
    } else {
      _hidl_cb(WifiStatus{WifiStatusCode::ERROR_WIFI_IFACE_INVALID, ""}, nullptr);
    }
    return Void();
  }

  Return<void> removeStaIface(const hidl_string &ifname, removeStaIface_cb _hidl_cb) override {
    std::lock_guard<std::mutex> lk(mu_);
    if (staIface_ != nullptr && ifname == "wlan0") {
      staIface_ = nullptr;
      _hidl_cb(okStatus());
    } else {
      _hidl_cb(WifiStatus{WifiStatusCode::ERROR_WIFI_IFACE_INVALID, ""});
    }
    return Void();
  }

  Return<void> createRttController(const sp<V1_0::IWifiIface> &,
                                   createRttController_cb _hidl_cb) override {
    _hidl_cb(unsupportedStatus(), nullptr);
    return Void();
  }

  Return<void> createRttController_1_4(const sp<V1_0::IWifiIface> &,
                                       createRttController_1_4_cb _hidl_cb) override {
    _hidl_cb(unsupportedStatus(), nullptr);
    return Void();
  }

  Return<void> createRttController_1_6(const sp<V1_0::IWifiIface> &,
                                       createRttController_1_6_cb _hidl_cb) override {
    _hidl_cb(unsupportedStatus(), nullptr);
    return Void();
  }

  Return<void> getDebugRingBuffersStatus(getDebugRingBuffersStatus_cb _hidl_cb) override {
    _hidl_cb(okStatus(), hidl_vec<WifiDebugRingBufferStatus>{});
    return Void();
  }

  Return<void> startLoggingToDebugRingBuffer(const hidl_string &, WifiDebugRingBufferVerboseLevel,
                                             uint32_t, uint32_t,
                                             startLoggingToDebugRingBuffer_cb _hidl_cb) override {
    _hidl_cb(unsupportedStatus());
    return Void();
  }

  Return<void> forceDumpToDebugRingBuffer(const hidl_string &,
                                          forceDumpToDebugRingBuffer_cb _hidl_cb) override {
    _hidl_cb(unsupportedStatus());
    return Void();
  }

  Return<void> flushRingBufferToFile(flushRingBufferToFile_cb _hidl_cb) override {
    _hidl_cb(okStatus());
    return Void();
  }

  Return<void> stopLoggingToDebugRingBuffer(stopLoggingToDebugRingBuffer_cb _hidl_cb) override {
    _hidl_cb(okStatus());
    return Void();
  }

  Return<void> getDebugHostWakeReasonStats(getDebugHostWakeReasonStats_cb _hidl_cb) override {
    _hidl_cb(unsupportedStatus(), WifiDebugHostWakeReasonStats{});
    return Void();
  }

  Return<void> enableDebugErrorAlerts(bool, enableDebugErrorAlerts_cb _hidl_cb) override {
    _hidl_cb(okStatus());
    return Void();
  }

  Return<void> selectTxPowerScenario(V1_1::IWifiChip::TxPowerScenario,
                                     selectTxPowerScenario_cb _hidl_cb) override {
    _hidl_cb(okStatus());
    return Void();
  }

  Return<void> selectTxPowerScenario_1_2(V1_2::IWifiChip::TxPowerScenario,
                                         selectTxPowerScenario_1_2_cb _hidl_cb) override {
    _hidl_cb(okStatus());
    return Void();
  }

  Return<void> resetTxPowerScenario(resetTxPowerScenario_cb _hidl_cb) override {
    _hidl_cb(okStatus());
    return Void();
  }

  Return<void> setLatencyMode(V1_3::IWifiChip::LatencyMode, setLatencyMode_cb _hidl_cb) override {
    _hidl_cb(okStatus());
    return Void();
  }

  Return<void> setMultiStaPrimaryConnection(const hidl_string &,
                                            setMultiStaPrimaryConnection_cb _hidl_cb) override {
    _hidl_cb(okStatus());
    return Void();
  }

  Return<void> setMultiStaUseCase(V1_5::IWifiChip::MultiStaUseCase,
                                  setMultiStaUseCase_cb _hidl_cb) override {
    _hidl_cb(okStatus());
    return Void();
  }

  Return<void> setCoexUnsafeChannels(const hidl_vec<V1_5::IWifiChip::CoexUnsafeChannel> &,
                                     hidl_bitfield<V1_5::IWifiChip::CoexRestriction>,
                                     setCoexUnsafeChannels_cb _hidl_cb) override {
    _hidl_cb(okStatus());
    return Void();
  }

  Return<void> setCountryCode(const hidl_array<int8_t, 2> &,
                              setCountryCode_cb _hidl_cb) override {
    _hidl_cb(okStatus());
    return Void();
  }

  Return<void> getUsableChannels(V1_5::WifiBand, hidl_bitfield<V1_5::WifiIfaceMode>,
                                 hidl_bitfield<V1_5::IWifiChip::UsableChannelFilter>,
                                 getUsableChannels_cb _hidl_cb) override {
    std::vector<V1_5::WifiUsableChannel> chs;
    for (int i = 0; i < 11; ++i) {
      chs.push_back(V1_5::WifiUsableChannel{
          static_cast<uint32_t>(2412 + i * 5), V1_0::WifiChannelWidthInMhz::WIDTH_20,
          static_cast<uint32_t>(V1_5::WifiIfaceMode::IFACE_MODE_STA)});
    }
    _hidl_cb(okStatus(), chs);
    return Void();
  }

  Return<void> getUsableChannels_1_6(V1_5::WifiBand, hidl_bitfield<V1_5::WifiIfaceMode>,
                                     hidl_bitfield<V1_6::IWifiChip::UsableChannelFilter>,
                                     getUsableChannels_1_6_cb _hidl_cb) override {
    std::vector<V1_6::WifiUsableChannel> chs;
    for (int i = 0; i < 11; ++i) {
      chs.push_back(V1_6::WifiUsableChannel{
          static_cast<uint32_t>(2412 + i * 5), V1_6::WifiChannelWidthInMhz::WIDTH_20,
          static_cast<uint32_t>(V1_5::WifiIfaceMode::IFACE_MODE_STA)});
    }
    _hidl_cb(okStatus(), chs);
    return Void();
  }

  Return<void> triggerSubsystemRestart(triggerSubsystemRestart_cb _hidl_cb) override {
    _hidl_cb(okStatus());
    return Void();
  }

  Return<void> getSupportedRadioCombinationsMatrix(
      getSupportedRadioCombinationsMatrix_cb _hidl_cb) override {
    V1_6::WifiRadioCombinationMatrix matrix{};
    V1_6::WifiRadioConfiguration cfg{V1_5::WifiBand::BAND_24GHZ_5GHZ,
                                     V1_6::WifiAntennaMode::WIFI_ANTENNA_MODE_2X2};
    V1_6::WifiRadioCombination comb{{cfg}};
    matrix.radioCombinations = {comb};
    _hidl_cb(okStatus(), matrix);
    return Void();
  }

 private:
  std::mutex mu_;
  bool configured_ = false;
  ChipModeId modeId_ = 0;
  sp<WifiStaIfaceImpl> staIface_;
};

class WifiHidlImpl : public V1_6::IWifi {
 public:
  WifiHidlImpl() { hostChannel_.start(); }
  ~WifiHidlImpl() override { hostChannel_.stop(); }

  Return<void> registerEventCallback(const sp<V1_0::IWifiEventCallback> &cb,
                                     registerEventCallback_cb _hidl_cb) override {
    std::lock_guard<std::mutex> lk(mu_);
    if (cb != nullptr) cbs10_.push_back(cb);
    _hidl_cb(okStatus());
    return Void();
  }

  Return<void> registerEventCallback_1_5(const sp<V1_5::IWifiEventCallback> &cb,
                                         registerEventCallback_1_5_cb _hidl_cb) override {
    std::lock_guard<std::mutex> lk(mu_);
    if (cb != nullptr) cbs15_.push_back(cb);
    _hidl_cb(okStatus());
    return Void();
  }

  Return<bool> isStarted() override {
    std::lock_guard<std::mutex> lk(mu_);
    return started_;
  }

  Return<void> start(start_cb _hidl_cb) override {
    std::vector<sp<V1_0::IWifiEventCallback>> c10;
    std::vector<sp<V1_5::IWifiEventCallback>> c15;
    {
      std::lock_guard<std::mutex> lk(mu_);
      if (!started_) {
        started_ = true;
        chip_ = new WifiChipImpl();
        c10 = cbs10_;
        c15 = cbs15_;
      }
    }
    for (auto &cb : c10) (void)cb->onStart().isOk();
    for (auto &cb : c15) (void)cb->onStart().isOk();
    _hidl_cb(okStatus());
    return Void();
  }

  Return<void> stop(stop_cb _hidl_cb) override {
    std::vector<sp<V1_0::IWifiEventCallback>> c10;
    std::vector<sp<V1_5::IWifiEventCallback>> c15;
    {
      std::lock_guard<std::mutex> lk(mu_);
      if (started_) {
        started_ = false;
        chip_ = nullptr;
        c10 = cbs10_;
        c15 = cbs15_;
      }
    }
    for (auto &cb : c10) (void)cb->onStop().isOk();
    for (auto &cb : c15) (void)cb->onStop().isOk();
    _hidl_cb(okStatus());
    return Void();
  }

  Return<void> getChipIds(getChipIds_cb _hidl_cb) override {
    std::lock_guard<std::mutex> lk(mu_);
    if (!started_) {
      _hidl_cb(WifiStatus{WifiStatusCode::ERROR_NOT_STARTED, ""}, hidl_vec<ChipId>{});
      return Void();
    }
    _hidl_cb(okStatus(), hidl_vec<ChipId>{0});
    return Void();
  }

  Return<void> getChip(ChipId chipId, getChip_cb _hidl_cb) override {
    std::lock_guard<std::mutex> lk(mu_);
    if (!started_ || chip_ == nullptr) {
      _hidl_cb(WifiStatus{WifiStatusCode::ERROR_NOT_STARTED, ""}, nullptr);
      return Void();
    }
    if (chipId != 0) {
      _hidl_cb(WifiStatus{WifiStatusCode::ERROR_WIFI_CHIP_INVALID, ""}, nullptr);
      return Void();
    }
    _hidl_cb(okStatus(), chip_);
    return Void();
  }

 private:
  std::mutex mu_;
  bool started_ = false;
  sp<WifiChipImpl> chip_;
  std::vector<sp<V1_0::IWifiEventCallback>> cbs10_;
  std::vector<sp<V1_5::IWifiEventCallback>> cbs15_;
  cdh_wifi::HostChannel hostChannel_;
};

}  // namespace

int main() {
  ::android::hardware::configureRpcThreadpool(4, true);
  sp<V1_6::IWifi> svc = new WifiHidlImpl();
  if (svc->registerAsService("default") != ::android::OK) {
    ALOGE("Failed to register IWifi@1.6/default");
    return 1;
  }
  ALOGI("IWifi@1.6/default service ready");
  ::android::hardware::joinRpcThreadpool();
  return 1;
}
