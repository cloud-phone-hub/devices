#define LOG_TAG "drmhwc"

#include "CdhComposer.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <ctime>
#include <utility>

#include <android/binder_ibinder_platform.h>
#include <log/log.h>

namespace aidl::android::hardware::graphics::composer3::cdh {
namespace {

constexpr std::array<float, 16> kIdentityMatrix = {
    1.0f, 0.0f, 0.0f, 0.0f,
    0.0f, 1.0f, 0.0f, 0.0f,
    0.0f, 0.0f, 1.0f, 0.0f,
    0.0f, 0.0f, 0.0f, 1.0f,
};

int64_t NowMonotonicNs() {
  struct timespec ts = {0, 0};
  clock_gettime(CLOCK_MONOTONIC, &ts);
  return static_cast<int64_t>(ts.tv_sec) * 1000000000LL +
         static_cast<int64_t>(ts.tv_nsec);
}

bool IsSupportedWideColorMode(ColorMode mode) {
  return mode == ColorMode::NATIVE || mode == ColorMode::SRGB ||
         mode == ColorMode::DISPLAY_P3;
}

const DisplayConfiguration* FindDisplayConfigById(
    const std::vector<DisplayConfiguration>& configs, int32_t config_id) {
  for (const auto& item : configs) {
    if (item.configId == config_id) {
      return &item;
    }
  }
  return nullptr;
}

}  // namespace

// ---------------- CdhComposerCallback ----------------

CdhComposerCallback::CdhComposerCallback(
    std::shared_ptr<IComposerCallback> downstream,
    std::weak_ptr<CdhComposerClient> owner)
    : downstream_(std::move(downstream)), owner_(std::move(owner)) {}

ndk::ScopedAStatus CdhComposerCallback::onHotplug(int64_t display,
                                                  bool connected) {
  if (!connected) {
    if (auto owner = owner_.lock()) {
      owner->OnDisplayDisconnected(display);
    }
  }
  return downstream_ ? downstream_->onHotplug(display, connected)
                     : ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus CdhComposerCallback::onRefresh(int64_t display) {
  return downstream_ ? downstream_->onRefresh(display)
                     : ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus CdhComposerCallback::onSeamlessPossible(int64_t display) {
  return downstream_ ? downstream_->onSeamlessPossible(display)
                     : ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus CdhComposerCallback::onVsync(int64_t display,
                                                int64_t timestamp,
                                                int32_t vsync_period_nanos) {
  if (auto owner = owner_.lock()) {
    vsync_period_nanos =
        owner->OverrideVsyncPeriodIfPhysical(display, vsync_period_nanos);
  }
  return downstream_
             ? downstream_->onVsync(display, timestamp, vsync_period_nanos)
             : ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus CdhComposerCallback::onVsyncPeriodTimingChanged(
    int64_t display, const VsyncPeriodChangeTimeline& updated_timeline) {
  return downstream_
             ? downstream_->onVsyncPeriodTimingChanged(display, updated_timeline)
             : ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus CdhComposerCallback::onVsyncIdle(int64_t display) {
  return downstream_ ? downstream_->onVsyncIdle(display)
                     : ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus CdhComposerCallback::onRefreshRateChangedDebug(
    const RefreshRateChangedDebugData& data) {
  return downstream_ ? downstream_->onRefreshRateChangedDebug(data)
                     : ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus CdhComposerCallback::onHotplugEvent(
    int64_t display, common::DisplayHotplugEvent event) {
  if (event == common::DisplayHotplugEvent::DISCONNECTED) {
    if (auto owner = owner_.lock()) {
      owner->OnDisplayDisconnected(display);
    }
  }
  return downstream_ ? downstream_->onHotplugEvent(display, event)
                     : ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus CdhComposerCallback::onHdcpLevelsChanged(
    int64_t display, const ::aidl::android::hardware::drm::HdcpLevels& levels) {
  return downstream_ ? downstream_->onHdcpLevelsChanged(display, levels)
                     : ndk::ScopedAStatus::ok();
}

// ---------------- CdhComposerClient ----------------

CdhComposerClient::CdhComposerClient(std::shared_ptr<IComposerClient> inner,
                                     CdhDisplayConfig cfg)
    : inner_(std::move(inner)), cfg_(std::move(cfg)) {}

void CdhComposerClient::OnDisplayDisconnected(int64_t display) {
  std::lock_guard<std::mutex> lock(mutex_);
  physical_displays_.erase(display);
}

int32_t CdhComposerClient::OverrideVsyncPeriodIfPhysical(
    int64_t display, int32_t fallback_period) {
  std::lock_guard<std::mutex> lock(mutex_);
  auto it = physical_displays_.find(display);
  if (it == physical_displays_.end()) {
    return fallback_period;
  }
  return it->second.active_vsync_period;
}

bool CdhComposerClient::IsVirtualDisplay(int64_t display) const {
  std::lock_guard<std::mutex> lock(mutex_);
  return virtual_displays_.count(display) > 0;
}

ndk::ScopedAStatus CdhComposerClient::EnsurePhysicalDisplay(
    int64_t display, PhysicalDisplayState* out_state) {
  {
    std::lock_guard<std::mutex> lock(mutex_);
    if (virtual_displays_.count(display) > 0) {
      return ndk::ScopedAStatus::fromServiceSpecificError(
          IComposerClient::EX_BAD_DISPLAY);
    }
    auto it = physical_displays_.find(display);
    if (it != physical_displays_.end()) {
      if (out_state != nullptr) {
        *out_state = it->second;
      }
      return ndk::ScopedAStatus::ok();
    }
  }

  std::vector<DisplayConfiguration> inner_cfgs;
  auto st = inner_->getDisplayConfigurations(display, 0, &inner_cfgs);
  if (!st.isOk()) {
    return st;
  }
  if (inner_cfgs.empty()) {
    return ndk::ScopedAStatus::fromServiceSpecificError(
        IComposerClient::EX_BAD_CONFIG);
  }

  int32_t base_active_id = inner_cfgs.front().configId;
  int32_t active_tmp = 0;
  if (inner_->getActiveConfig(display, &active_tmp).isOk()) {
    base_active_id = active_tmp;
  }

  const DisplayConfiguration* base_cfg = &inner_cfgs.front();
  for (const auto& candidate : inner_cfgs) {
    if (candidate.configId == base_active_id) {
      base_cfg = &candidate;
      break;
    }
  }

  PhysicalDisplayState built;
  const auto& rates = cfg_.refresh_rates.empty()
                          ? std::vector<int32_t>{60, 120, 90}
                          : cfg_.refresh_rates;
  built.configs.reserve(rates.size());
  for (size_t i = 0; i < rates.size(); ++i) {
    const int32_t rate = rates[i];
    DisplayConfiguration item;
    item.configId = static_cast<int32_t>(i + 1);
    item.width = base_cfg->width;
    item.height = base_cfg->height;
    item.configGroup = 1;
    if (base_cfg->vsyncPeriod > 0 &&
        std::abs(1000000000.0 / static_cast<double>(base_cfg->vsyncPeriod) -
                 static_cast<double>(rate)) < 0.5) {
      item.vsyncPeriod = base_cfg->vsyncPeriod;
    } else {
      item.vsyncPeriod = 1000000000 / rate;
    }
    if (cfg_.HasCustomDpi()) {
      item.dpi = DisplayConfiguration::Dpi{.x = cfg_.xdpi, .y = cfg_.ydpi};
    } else {
      item.dpi = base_cfg->dpi;
    }
    built.configs.push_back(item);
  }
  const DisplayConfiguration* initial_cfg = &built.configs.front();
  if (cfg_.default_refresh_rate > 0) {
    for (const auto& candidate : built.configs) {
      if (candidate.vsyncPeriod > 0 &&
          std::lround(1000000000.0 / static_cast<double>(candidate.vsyncPeriod)) ==
              cfg_.default_refresh_rate) {
        initial_cfg = &candidate;
        break;
      }
    }
  }
  built.default_config_id = initial_cfg->configId;
  built.active_config_id = initial_cfg->configId;
  built.active_vsync_period = initial_cfg->vsyncPeriod;

  std::lock_guard<std::mutex> lock(mutex_);
  auto [it, _] = physical_displays_.emplace(display, std::move(built));
  if (out_state != nullptr) {
    *out_state = it->second;
  }
  return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus CdhComposerClient::createLayer(int64_t display,
                                                  int32_t buffer_slot_count,
                                                  int64_t* layer) {
  return inner_->createLayer(display, buffer_slot_count, layer);
}

ndk::ScopedAStatus CdhComposerClient::createVirtualDisplay(
    int32_t width, int32_t height, common::PixelFormat format_hint,
    int32_t output_buffer_slot_count, VirtualDisplay* display) {
  auto st = inner_->createVirtualDisplay(width, height, format_hint,
                                         output_buffer_slot_count, display);
  if (st.isOk() && display != nullptr) {
    std::lock_guard<std::mutex> lock(mutex_);
    virtual_displays_.insert(display->display);
  }
  return st;
}

ndk::ScopedAStatus CdhComposerClient::destroyLayer(int64_t display,
                                                   int64_t layer) {
  return inner_->destroyLayer(display, layer);
}

ndk::ScopedAStatus CdhComposerClient::destroyVirtualDisplay(int64_t display) {
  {
    std::lock_guard<std::mutex> lock(mutex_);
    virtual_displays_.erase(display);
  }
  return inner_->destroyVirtualDisplay(display);
}

ndk::ScopedAStatus CdhComposerClient::executeCommands(
    const std::vector<DisplayCommand>& commands,
    std::vector<CommandResultPayload>* results) {
  return inner_->executeCommands(commands, results);
}

ndk::ScopedAStatus CdhComposerClient::getActiveConfig(int64_t display,
                                                      int32_t* config) {
  if (IsVirtualDisplay(display)) {
    return inner_->getActiveConfig(display, config);
  }
  if (config == nullptr) {
    return ndk::ScopedAStatus::fromServiceSpecificError(
        IComposerClient::EX_BAD_PARAMETER);
  }
  PhysicalDisplayState state;
  auto st = EnsurePhysicalDisplay(display, &state);
  if (!st.isOk()) {
    return st;
  }
  *config = state.active_config_id;
  return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus CdhComposerClient::getColorModes(
    int64_t display, std::vector<ColorMode>* color_modes) {
  if (!cfg_.wide_color_gamut || IsVirtualDisplay(display)) {
    return inner_->getColorModes(display, color_modes);
  }
  if (color_modes == nullptr) {
    return ndk::ScopedAStatus::fromServiceSpecificError(
        IComposerClient::EX_BAD_PARAMETER);
  }
  auto st = EnsurePhysicalDisplay(display, nullptr);
  if (!st.isOk()) {
    return st;
  }
  *color_modes = {ColorMode::NATIVE, ColorMode::SRGB, ColorMode::DISPLAY_P3};
  return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus CdhComposerClient::getDataspaceSaturationMatrix(
    common::Dataspace dataspace, std::vector<float>* matrix) {
  if (!cfg_.wide_color_gamut) {
    return inner_->getDataspaceSaturationMatrix(dataspace, matrix);
  }
  if (matrix == nullptr) {
    return ndk::ScopedAStatus::fromServiceSpecificError(
        IComposerClient::EX_BAD_PARAMETER);
  }
  matrix->assign(kIdentityMatrix.begin(), kIdentityMatrix.end());
  return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus CdhComposerClient::getDisplayAttribute(
    int64_t display, int32_t config, DisplayAttribute attribute,
    int32_t* value) {
  if (IsVirtualDisplay(display)) {
    return inner_->getDisplayAttribute(display, config, attribute, value);
  }
  if (value == nullptr) {
    return ndk::ScopedAStatus::fromServiceSpecificError(
        IComposerClient::EX_BAD_PARAMETER);
  }
  PhysicalDisplayState state;
  auto st = EnsurePhysicalDisplay(display, &state);
  if (!st.isOk()) {
    return st;
  }
  const DisplayConfiguration* found = nullptr;
  for (const auto& item : state.configs) {
    if (item.configId == config) {
      found = &item;
      break;
    }
  }
  if (found == nullptr) {
    return ndk::ScopedAStatus::fromServiceSpecificError(
        IComposerClient::EX_BAD_CONFIG);
  }

  switch (attribute) {
    case DisplayAttribute::WIDTH:
      *value = found->width;
      break;
    case DisplayAttribute::HEIGHT:
      *value = found->height;
      break;
    case DisplayAttribute::VSYNC_PERIOD:
      *value = found->vsyncPeriod;
      break;
    case DisplayAttribute::DPI_X:
      *value = found->dpi ? static_cast<int32_t>(std::lround(found->dpi->x * 1000.0f))
                          : -1;
      break;
    case DisplayAttribute::DPI_Y:
      *value = found->dpi ? static_cast<int32_t>(std::lround(found->dpi->y * 1000.0f))
                          : -1;
      break;
    case DisplayAttribute::CONFIG_GROUP:
      *value = found->configGroup;
      break;
    default:
      return ndk::ScopedAStatus::fromServiceSpecificError(
          IComposerClient::EX_UNSUPPORTED);
  }
  return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus CdhComposerClient::getDisplayCapabilities(
    int64_t display, std::vector<DisplayCapability>* caps) {
  return inner_->getDisplayCapabilities(display, caps);
}

ndk::ScopedAStatus CdhComposerClient::getDisplayConfigs(
    int64_t display, std::vector<int32_t>* configs) {
  if (IsVirtualDisplay(display)) {
    return inner_->getDisplayConfigs(display, configs);
  }
  if (configs == nullptr) {
    return ndk::ScopedAStatus::fromServiceSpecificError(
        IComposerClient::EX_BAD_PARAMETER);
  }
  PhysicalDisplayState state;
  auto st = EnsurePhysicalDisplay(display, &state);
  if (!st.isOk()) {
    return st;
  }
  configs->clear();
  configs->reserve(state.configs.size());
  for (const auto& item : state.configs) {
    configs->push_back(item.configId);
  }
  return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus CdhComposerClient::getDisplayConnectionType(
    int64_t display, DisplayConnectionType* type) {
  return inner_->getDisplayConnectionType(display, type);
}

ndk::ScopedAStatus CdhComposerClient::getDisplayIdentificationData(
    int64_t display, DisplayIdentification* id) {
  if (IsVirtualDisplay(display) || !cfg_.HasCustomEdid()) {
    return inner_->getDisplayIdentificationData(display, id);
  }
  if (id == nullptr) {
    return ndk::ScopedAStatus::fromServiceSpecificError(
        IComposerClient::EX_BAD_PARAMETER);
  }
  auto st = EnsurePhysicalDisplay(display, nullptr);
  if (!st.isOk()) {
    return st;
  }
  id->port = static_cast<int8_t>(static_cast<uint8_t>(display));
  id->data = cfg_.edid_blob;
  return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus CdhComposerClient::getDisplayName(int64_t display,
                                                     std::string* name) {
  return inner_->getDisplayName(display, name);
}

ndk::ScopedAStatus CdhComposerClient::getDisplayVsyncPeriod(
    int64_t display, int32_t* vsync_period) {
  if (IsVirtualDisplay(display)) {
    return inner_->getDisplayVsyncPeriod(display, vsync_period);
  }
  if (vsync_period == nullptr) {
    return ndk::ScopedAStatus::fromServiceSpecificError(
        IComposerClient::EX_BAD_PARAMETER);
  }
  PhysicalDisplayState state;
  auto st = EnsurePhysicalDisplay(display, &state);
  if (!st.isOk()) {
    return st;
  }
  *vsync_period = state.active_vsync_period;
  return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus CdhComposerClient::getDisplayedContentSample(
    int64_t display, int64_t max_frames, int64_t timestamp,
    DisplayContentSample* samples) {
  return inner_->getDisplayedContentSample(display, max_frames, timestamp,
                                           samples);
}

ndk::ScopedAStatus CdhComposerClient::getDisplayedContentSamplingAttributes(
    int64_t display, DisplayContentSamplingAttributes* attrs) {
  return inner_->getDisplayedContentSamplingAttributes(display, attrs);
}

ndk::ScopedAStatus CdhComposerClient::getDisplayPhysicalOrientation(
    int64_t display, common::Transform* orientation) {
  return inner_->getDisplayPhysicalOrientation(display, orientation);
}

ndk::ScopedAStatus CdhComposerClient::getHdrCapabilities(
    int64_t display, HdrCapabilities* caps) {
  return inner_->getHdrCapabilities(display, caps);
}

ndk::ScopedAStatus CdhComposerClient::getMaxVirtualDisplayCount(
    int32_t* count) {
  return inner_->getMaxVirtualDisplayCount(count);
}

ndk::ScopedAStatus CdhComposerClient::getPerFrameMetadataKeys(
    int64_t display, std::vector<PerFrameMetadataKey>* keys) {
  return inner_->getPerFrameMetadataKeys(display, keys);
}

ndk::ScopedAStatus CdhComposerClient::getReadbackBufferAttributes(
    int64_t display, ReadbackBufferAttributes* attrs) {
  return inner_->getReadbackBufferAttributes(display, attrs);
}

ndk::ScopedAStatus CdhComposerClient::getReadbackBufferFence(
    int64_t display, ndk::ScopedFileDescriptor* acquire_fence) {
  return inner_->getReadbackBufferFence(display, acquire_fence);
}

ndk::ScopedAStatus CdhComposerClient::getRenderIntents(
    int64_t display, ColorMode mode, std::vector<RenderIntent>* intents) {
  if (!cfg_.wide_color_gamut || IsVirtualDisplay(display)) {
    return inner_->getRenderIntents(display, mode, intents);
  }
  if (intents == nullptr) {
    return ndk::ScopedAStatus::fromServiceSpecificError(
        IComposerClient::EX_BAD_PARAMETER);
  }
  auto st = EnsurePhysicalDisplay(display, nullptr);
  if (!st.isOk()) {
    return st;
  }
  if (!IsSupportedWideColorMode(mode)) {
    return ndk::ScopedAStatus::fromServiceSpecificError(
        IComposerClient::EX_BAD_PARAMETER);
  }
  *intents = {RenderIntent::COLORIMETRIC};
  return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus CdhComposerClient::getSupportedContentTypes(
    int64_t display, std::vector<ContentType>* types) {
  return inner_->getSupportedContentTypes(display, types);
}

ndk::ScopedAStatus CdhComposerClient::getDisplayDecorationSupport(
    int64_t display,
    std::optional<common::DisplayDecorationSupport>* support) {
  return inner_->getDisplayDecorationSupport(display, support);
}

ndk::ScopedAStatus CdhComposerClient::registerCallback(
    const std::shared_ptr<IComposerCallback>& callback) {
  std::shared_ptr<CdhComposerCallback> wrapper;
  if (callback != nullptr) {
    wrapper = ndk::SharedRefBase::make<CdhComposerCallback>(
        callback, this->ref<CdhComposerClient>());
  }
  {
    std::lock_guard<std::mutex> lock(mutex_);
    callback_wrapper_ = wrapper;
  }
  // 不持锁调用 inner_->registerCallback：底层 ResourceManager::Init 会同步触发
  // onHotplugEvent，SurfaceFlinger 在回调内会重入查询 getDisplayConfigurations。
  return inner_->registerCallback(wrapper);
}

ndk::ScopedAStatus CdhComposerClient::setActiveConfig(int64_t display,
                                                      int32_t config) {
  if (IsVirtualDisplay(display)) {
    return inner_->setActiveConfig(display, config);
  }
  VsyncPeriodChangeTimeline timeline;
  VsyncPeriodChangeConstraints constraints = {
      .desiredTimeNanos = NowMonotonicNs(),
      .seamlessRequired = false,
  };
  return setActiveConfigWithConstraints(display, config, constraints, &timeline);
}

ndk::ScopedAStatus CdhComposerClient::setActiveConfigWithConstraints(
    int64_t display, int32_t config,
    const VsyncPeriodChangeConstraints& constraints,
    VsyncPeriodChangeTimeline* timeline) {
  if (IsVirtualDisplay(display)) {
    return inner_->setActiveConfigWithConstraints(display, config, constraints,
                                                  timeline);
  }
  if (timeline == nullptr) {
    return ndk::ScopedAStatus::fromServiceSpecificError(
        IComposerClient::EX_BAD_PARAMETER);
  }
  auto st = EnsurePhysicalDisplay(display, nullptr);
  if (!st.isOk()) {
    return st;
  }
  if (constraints.seamlessRequired) {
    return ndk::ScopedAStatus::fromServiceSpecificError(
        IComposerClient::EX_SEAMLESS_NOT_ALLOWED);
  }

  {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = physical_displays_.find(display);
    if (it == physical_displays_.end()) {
      return ndk::ScopedAStatus::fromServiceSpecificError(
          IComposerClient::EX_BAD_DISPLAY);
    }
    const DisplayConfiguration* target =
        FindDisplayConfigById(it->second.configs, config);
    if (target == nullptr) {
      return ndk::ScopedAStatus::fromServiceSpecificError(
          IComposerClient::EX_BAD_CONFIG);
    }
    it->second.active_config_id = config;
    it->second.active_vsync_period = target->vsyncPeriod;
  }

  timeline->newVsyncAppliedTimeNanos =
      std::max(constraints.desiredTimeNanos, NowMonotonicNs());
  timeline->refreshTimeNanos = 0;
  timeline->refreshRequired = false;
  return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus CdhComposerClient::setBootDisplayConfig(int64_t display,
                                                           int32_t config) {
  if (IsVirtualDisplay(display)) {
    return inner_->setBootDisplayConfig(display, config);
  }
  auto st = EnsurePhysicalDisplay(display, nullptr);
  if (!st.isOk()) {
    return st;
  }
  std::lock_guard<std::mutex> lock(mutex_);
  auto it = physical_displays_.find(display);
  if (it == physical_displays_.end()) {
    return ndk::ScopedAStatus::fromServiceSpecificError(
        IComposerClient::EX_BAD_DISPLAY);
  }
  if (FindDisplayConfigById(it->second.configs, config) == nullptr) {
    return ndk::ScopedAStatus::fromServiceSpecificError(
        IComposerClient::EX_BAD_CONFIG);
  }
  it->second.boot_config_override = config;
  return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus CdhComposerClient::clearBootDisplayConfig(int64_t display) {
  if (IsVirtualDisplay(display)) {
    return inner_->clearBootDisplayConfig(display);
  }
  auto st = EnsurePhysicalDisplay(display, nullptr);
  if (!st.isOk()) {
    return st;
  }
  std::lock_guard<std::mutex> lock(mutex_);
  auto it = physical_displays_.find(display);
  if (it != physical_displays_.end()) {
    it->second.boot_config_override.reset();
  }
  return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus CdhComposerClient::getPreferredBootDisplayConfig(
    int64_t display, int32_t* config) {
  if (IsVirtualDisplay(display)) {
    return inner_->getPreferredBootDisplayConfig(display, config);
  }
  if (config == nullptr) {
    return ndk::ScopedAStatus::fromServiceSpecificError(
        IComposerClient::EX_BAD_PARAMETER);
  }
  PhysicalDisplayState state;
  auto st = EnsurePhysicalDisplay(display, &state);
  if (!st.isOk()) {
    return st;
  }
  *config = state.boot_config_override.value_or(state.default_config_id);
  return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus CdhComposerClient::setAutoLowLatencyMode(int64_t display,
                                                            bool on) {
  return inner_->setAutoLowLatencyMode(display, on);
}

ndk::ScopedAStatus CdhComposerClient::setClientTargetSlotCount(int64_t display,
                                                               int32_t count) {
  return inner_->setClientTargetSlotCount(display, count);
}

ndk::ScopedAStatus CdhComposerClient::setColorMode(int64_t display,
                                                   ColorMode mode,
                                                   RenderIntent intent) {
  if (!cfg_.wide_color_gamut || IsVirtualDisplay(display)) {
    return inner_->setColorMode(display, mode, intent);
  }
  auto st = EnsurePhysicalDisplay(display, nullptr);
  if (!st.isOk()) {
    return st;
  }
  if (!IsSupportedWideColorMode(mode) || intent != RenderIntent::COLORIMETRIC) {
    return ndk::ScopedAStatus::fromServiceSpecificError(
        IComposerClient::EX_BAD_PARAMETER);
  }
  if (mode == ColorMode::NATIVE) {
    return inner_->setColorMode(display, mode, intent);
  }
  return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus CdhComposerClient::setContentType(int64_t display,
                                                     ContentType type) {
  return inner_->setContentType(display, type);
}

ndk::ScopedAStatus CdhComposerClient::setDisplayedContentSamplingEnabled(
    int64_t display, bool enable, FormatColorComponent component_mask,
    int64_t max_frames) {
  return inner_->setDisplayedContentSamplingEnabled(
      display, enable, component_mask, max_frames);
}

ndk::ScopedAStatus CdhComposerClient::setPowerMode(int64_t display,
                                                   PowerMode mode) {
  return inner_->setPowerMode(display, mode);
}

ndk::ScopedAStatus CdhComposerClient::setReadbackBuffer(
    int64_t display,
    const ::aidl::android::hardware::common::NativeHandle& buffer,
    const ndk::ScopedFileDescriptor& release_fence) {
  return inner_->setReadbackBuffer(display, buffer, release_fence);
}

ndk::ScopedAStatus CdhComposerClient::setVsyncEnabled(int64_t display,
                                                      bool enabled) {
  return inner_->setVsyncEnabled(display, enabled);
}

ndk::ScopedAStatus CdhComposerClient::setIdleTimerEnabled(int64_t display,
                                                          int32_t timeout) {
  return inner_->setIdleTimerEnabled(display, timeout);
}

ndk::ScopedAStatus CdhComposerClient::getOverlaySupport(
    OverlayProperties* out_overlay_properties) {
  return inner_->getOverlaySupport(out_overlay_properties);
}

ndk::ScopedAStatus CdhComposerClient::getHdrConversionCapabilities(
    std::vector<common::HdrConversionCapability>* out_capabilities) {
  return inner_->getHdrConversionCapabilities(out_capabilities);
}

ndk::ScopedAStatus CdhComposerClient::setHdrConversionStrategy(
    const common::HdrConversionStrategy& conversion_strategy,
    common::Hdr* out_hdr) {
  return inner_->setHdrConversionStrategy(conversion_strategy, out_hdr);
}

ndk::ScopedAStatus CdhComposerClient::setRefreshRateChangedCallbackDebugEnabled(
    int64_t display, bool enabled) {
  return inner_->setRefreshRateChangedCallbackDebugEnabled(display, enabled);
}

ndk::ScopedAStatus CdhComposerClient::getDisplayConfigurations(
    int64_t display, int32_t max_frame_interval_ns,
    std::vector<DisplayConfiguration>* configurations) {
  if (IsVirtualDisplay(display)) {
    return inner_->getDisplayConfigurations(display, max_frame_interval_ns,
                                            configurations);
  }
  if (configurations == nullptr) {
    return ndk::ScopedAStatus::fromServiceSpecificError(
        IComposerClient::EX_BAD_PARAMETER);
  }
  PhysicalDisplayState state;
  auto st = EnsurePhysicalDisplay(display, &state);
  if (!st.isOk()) {
    return st;
  }
  *configurations = state.configs;
  return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus CdhComposerClient::notifyExpectedPresent(
    int64_t display, const ClockMonotonicTimestamp& expected_present_time,
    int32_t frame_interval_ns) {
  return inner_->notifyExpectedPresent(display, expected_present_time,
                                       frame_interval_ns);
}

ndk::ScopedAStatus CdhComposerClient::startHdcpNegotiation(
    int64_t display,
    const ::aidl::android::hardware::drm::HdcpLevels& levels) {
  return inner_->startHdcpNegotiation(display, levels);
}

ndk::ScopedAStatus CdhComposerClient::getMaxLayerPictureProfiles(
    int64_t display, int32_t* max_profiles) {
  return inner_->getMaxLayerPictureProfiles(display, max_profiles);
}

ndk::ScopedAStatus CdhComposerClient::getLuts(
    int64_t display, const std::vector<Buffer>& buffers,
    std::vector<Luts>* out_luts) {
  return inner_->getLuts(display, buffers, out_luts);
}

::ndk::SpAIBinder CdhComposerClient::createBinder() {
  auto binder = BnComposerClient::createBinder();
  AIBinder_setInheritRt(binder.get(), true);
  return binder;
}

// ---------------- CdhComposer ----------------

CdhComposer::CdhComposer(std::shared_ptr<IComposer> inner, CdhDisplayConfig cfg)
    : inner_(std::move(inner)), cfg_(std::move(cfg)) {}

binder_status_t CdhComposer::dump(int fd, const char** args,
                                  uint32_t num_args) {
  return inner_ ? inner_->dump(fd, args, num_args) : STATUS_OK;
}

ndk::ScopedAStatus CdhComposer::createClient(
    std::shared_ptr<IComposerClient>* out_client) {
  if (out_client == nullptr) {
    return ndk::ScopedAStatus::fromServiceSpecificError(
        IComposer::EX_NO_RESOURCES);
  }
  std::shared_ptr<IComposerClient> inner_client;
  auto st = inner_->createClient(&inner_client);
  if (!st.isOk() || inner_client == nullptr) {
    *out_client = nullptr;
    return st;
  }
  *out_client =
      ndk::SharedRefBase::make<CdhComposerClient>(std::move(inner_client), cfg_);
  return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus CdhComposer::getCapabilities(std::vector<Capability>* caps) {
  return inner_->getCapabilities(caps);
}

::ndk::SpAIBinder CdhComposer::createBinder() {
  auto binder = BnComposer::createBinder();
  AIBinder_setInheritRt(binder.get(), true);
  return binder;
}

}  // namespace aidl::android::hardware::graphics::composer3::cdh
