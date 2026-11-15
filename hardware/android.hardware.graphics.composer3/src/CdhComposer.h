#pragma once

#include <cstdint>
#include <memory>
#include <mutex>
#include <optional>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include <aidl/android/hardware/common/NativeHandle.h>
#include <aidl/android/hardware/drm/HdcpLevels.h>
#include <aidl/android/hardware/graphics/common/Dataspace.h>
#include <aidl/android/hardware/graphics/common/DisplayDecorationSupport.h>
#include <aidl/android/hardware/graphics/common/DisplayHotplugEvent.h>
#include <aidl/android/hardware/graphics/common/Hdr.h>
#include <aidl/android/hardware/graphics/common/HdrConversionCapability.h>
#include <aidl/android/hardware/graphics/common/HdrConversionStrategy.h>
#include <aidl/android/hardware/graphics/common/PixelFormat.h>
#include <aidl/android/hardware/graphics/common/Transform.h>
#include <aidl/android/hardware/graphics/composer3/BnComposer.h>
#include <aidl/android/hardware/graphics/composer3/BnComposerCallback.h>
#include <aidl/android/hardware/graphics/composer3/BnComposerClient.h>

#include "CdhDisplayConfig.h"

namespace aidl::android::hardware::graphics::composer3::cdh {

class CdhComposerClient;

// CdhComposerCallback 拦截底层 VSync 与热插拔通知：
//   - onVsync: 将物理屏的 vsyncPeriodNanos 替换为当前虚拟高刷档位周期；
//   - onHotplug / onHotplugEvent: 屏幕断开时清理缓存配置。
class CdhComposerCallback : public BnComposerCallback {
 public:
  CdhComposerCallback(std::shared_ptr<IComposerCallback> downstream,
                      std::weak_ptr<CdhComposerClient> owner);

  ndk::ScopedAStatus onHotplug(int64_t display, bool connected) override;
  ndk::ScopedAStatus onRefresh(int64_t display) override;
  ndk::ScopedAStatus onSeamlessPossible(int64_t display) override;
  ndk::ScopedAStatus onVsync(int64_t display, int64_t timestamp,
                             int32_t vsync_period_nanos) override;
  ndk::ScopedAStatus onVsyncPeriodTimingChanged(
      int64_t display, const VsyncPeriodChangeTimeline& updated_timeline) override;
  ndk::ScopedAStatus onVsyncIdle(int64_t display) override;
  ndk::ScopedAStatus onRefreshRateChangedDebug(
      const RefreshRateChangedDebugData& data) override;
  ndk::ScopedAStatus onHotplugEvent(
      int64_t display, common::DisplayHotplugEvent event) override;
  ndk::ScopedAStatus onHdcpLevelsChanged(
      int64_t display, const ::aidl::android::hardware::drm::HdcpLevels& levels) override;

 private:
  std::shared_ptr<IComposerCallback> downstream_;
  std::weak_ptr<CdhComposerClient> owner_;
};

// CdhComposerClient 包裹上游 impl::ComposerClient：
// 帧合成热路径直接虚函数转发，面板配置/色域/DPI/EDID 按 CdhDisplayConfig 合成与对齐。
class CdhComposerClient : public BnComposerClient {
 public:
  CdhComposerClient(std::shared_ptr<IComposerClient> inner, CdhDisplayConfig cfg);
  ~CdhComposerClient() override = default;

  void OnDisplayDisconnected(int64_t display);
  int32_t OverrideVsyncPeriodIfPhysical(int64_t display, int32_t fallback_period);

  ndk::ScopedAStatus createLayer(int64_t display, int32_t buffer_slot_count,
                                 int64_t* layer) override;
  ndk::ScopedAStatus createVirtualDisplay(
      int32_t width, int32_t height, common::PixelFormat format_hint,
      int32_t output_buffer_slot_count, VirtualDisplay* display) override;
  ndk::ScopedAStatus destroyLayer(int64_t display, int64_t layer) override;
  ndk::ScopedAStatus destroyVirtualDisplay(int64_t display) override;
  ndk::ScopedAStatus executeCommands(
      const std::vector<DisplayCommand>& commands,
      std::vector<CommandResultPayload>* results) override;
  ndk::ScopedAStatus getActiveConfig(int64_t display, int32_t* config) override;
  ndk::ScopedAStatus getColorModes(
      int64_t display, std::vector<ColorMode>* color_modes) override;
  ndk::ScopedAStatus getDataspaceSaturationMatrix(
      common::Dataspace dataspace, std::vector<float>* matrix) override;
  ndk::ScopedAStatus getDisplayAttribute(int64_t display, int32_t config,
                                         DisplayAttribute attribute,
                                         int32_t* value) override;
  ndk::ScopedAStatus getDisplayCapabilities(
      int64_t display, std::vector<DisplayCapability>* caps) override;
  ndk::ScopedAStatus getDisplayConfigs(int64_t display,
                                       std::vector<int32_t>* configs) override;
  ndk::ScopedAStatus getDisplayConnectionType(
      int64_t display, DisplayConnectionType* type) override;
  ndk::ScopedAStatus getDisplayIdentificationData(
      int64_t display, DisplayIdentification* id) override;
  ndk::ScopedAStatus getDisplayName(int64_t display, std::string* name) override;
  ndk::ScopedAStatus getDisplayVsyncPeriod(int64_t display,
                                           int32_t* vsync_period) override;
  ndk::ScopedAStatus getDisplayedContentSample(
      int64_t display, int64_t max_frames, int64_t timestamp,
      DisplayContentSample* samples) override;
  ndk::ScopedAStatus getDisplayedContentSamplingAttributes(
      int64_t display, DisplayContentSamplingAttributes* attrs) override;
  ndk::ScopedAStatus getDisplayPhysicalOrientation(
      int64_t display, common::Transform* orientation) override;
  ndk::ScopedAStatus getHdrCapabilities(int64_t display,
                                        HdrCapabilities* caps) override;
  ndk::ScopedAStatus getMaxVirtualDisplayCount(int32_t* count) override;
  ndk::ScopedAStatus getPerFrameMetadataKeys(
      int64_t display, std::vector<PerFrameMetadataKey>* keys) override;
  ndk::ScopedAStatus getReadbackBufferAttributes(
      int64_t display, ReadbackBufferAttributes* attrs) override;
  ndk::ScopedAStatus getReadbackBufferFence(
      int64_t display, ndk::ScopedFileDescriptor* acquire_fence) override;
  ndk::ScopedAStatus getRenderIntents(
      int64_t display, ColorMode mode,
      std::vector<RenderIntent>* intents) override;
  ndk::ScopedAStatus getSupportedContentTypes(
      int64_t display, std::vector<ContentType>* types) override;
  ndk::ScopedAStatus getDisplayDecorationSupport(
      int64_t display,
      std::optional<common::DisplayDecorationSupport>* support) override;
  ndk::ScopedAStatus registerCallback(
      const std::shared_ptr<IComposerCallback>& callback) override;
  ndk::ScopedAStatus setActiveConfig(int64_t display, int32_t config) override;
  ndk::ScopedAStatus setActiveConfigWithConstraints(
      int64_t display, int32_t config,
      const VsyncPeriodChangeConstraints& constraints,
      VsyncPeriodChangeTimeline* timeline) override;
  ndk::ScopedAStatus setBootDisplayConfig(int64_t display, int32_t config) override;
  ndk::ScopedAStatus clearBootDisplayConfig(int64_t display) override;
  ndk::ScopedAStatus getPreferredBootDisplayConfig(int64_t display,
                                                   int32_t* config) override;
  ndk::ScopedAStatus setAutoLowLatencyMode(int64_t display, bool on) override;
  ndk::ScopedAStatus setClientTargetSlotCount(int64_t display,
                                              int32_t count) override;
  ndk::ScopedAStatus setColorMode(int64_t display, ColorMode mode,
                                  RenderIntent intent) override;
  ndk::ScopedAStatus setContentType(int64_t display, ContentType type) override;
  ndk::ScopedAStatus setDisplayedContentSamplingEnabled(
      int64_t display, bool enable, FormatColorComponent component_mask,
      int64_t max_frames) override;
  ndk::ScopedAStatus setPowerMode(int64_t display, PowerMode mode) override;
  ndk::ScopedAStatus setReadbackBuffer(
      int64_t display, const ::aidl::android::hardware::common::NativeHandle& buffer,
      const ndk::ScopedFileDescriptor& release_fence) override;
  ndk::ScopedAStatus setVsyncEnabled(int64_t display, bool enabled) override;
  ndk::ScopedAStatus setIdleTimerEnabled(int64_t display, int32_t timeout) override;
  ndk::ScopedAStatus getOverlaySupport(
      OverlayProperties* out_overlay_properties) override;
  ndk::ScopedAStatus getHdrConversionCapabilities(
      std::vector<common::HdrConversionCapability>* out_capabilities) override;
  ndk::ScopedAStatus setHdrConversionStrategy(
      const common::HdrConversionStrategy& conversion_strategy,
      common::Hdr* out_hdr) override;
  ndk::ScopedAStatus setRefreshRateChangedCallbackDebugEnabled(
      int64_t display, bool enabled) override;
  ndk::ScopedAStatus getDisplayConfigurations(
      int64_t display, int32_t max_frame_interval_ns,
      std::vector<DisplayConfiguration>* configurations) override;
  ndk::ScopedAStatus notifyExpectedPresent(
      int64_t display, const ClockMonotonicTimestamp& expected_present_time,
      int32_t frame_interval_ns) override;
  ndk::ScopedAStatus startHdcpNegotiation(
      int64_t display,
      const ::aidl::android::hardware::drm::HdcpLevels& levels) override;
  ndk::ScopedAStatus getMaxLayerPictureProfiles(int64_t display,
                                                int32_t* max_profiles) override;
  ndk::ScopedAStatus getLuts(int64_t display, const std::vector<Buffer>& buffers,
                             std::vector<Luts>* out_luts) override;

 protected:
  ::ndk::SpAIBinder createBinder() override;

 private:
  struct PhysicalDisplayState {
    std::vector<DisplayConfiguration> configs;
    int32_t active_config_id = 1;
    int32_t active_vsync_period = 16666666;
    int32_t default_config_id = 1;
    std::optional<int32_t> boot_config_override;
  };

  bool IsVirtualDisplay(int64_t display) const;
  ndk::ScopedAStatus EnsurePhysicalDisplay(int64_t display,
                                           PhysicalDisplayState* out_state);

  std::shared_ptr<IComposerClient> inner_;
  CdhDisplayConfig cfg_;

  mutable std::mutex mutex_;
  std::unordered_set<int64_t> virtual_displays_;
  std::unordered_map<int64_t, PhysicalDisplayState> physical_displays_;
  std::shared_ptr<CdhComposerCallback> callback_wrapper_;
};

// CdhComposer 包裹上游 impl::Composer 并注入 CdhComposerClient。
class CdhComposer : public BnComposer {
 public:
  CdhComposer(std::shared_ptr<IComposer> inner, CdhDisplayConfig cfg);
  ~CdhComposer() override = default;

  binder_status_t dump(int fd, const char** args, uint32_t num_args) override;
  ndk::ScopedAStatus createClient(
      std::shared_ptr<IComposerClient>* out_client) override;
  ndk::ScopedAStatus getCapabilities(std::vector<Capability>* caps) override;

 protected:
  ::ndk::SpAIBinder createBinder() override;

 private:
  std::shared_ptr<IComposer> inner_;
  CdhDisplayConfig cfg_;
};

}  // namespace aidl::android::hardware::graphics::composer3::cdh
