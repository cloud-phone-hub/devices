#include <android/binder_auto_utils.h>
#include <android/binder_ibinder.h>
#include <android/binder_status.h>

extern "C" {
// Android 15 NDK ships these libbinder_ndk symbols on device; this SDK layout
// does not expose binder_manager.h, so keep the exact C ABI declarations here.
void ABinderProcess_setThreadPoolMaxThreadCount(uint32_t numThreads);
void ABinderProcess_joinThreadPool(void);
binder_status_t AServiceManager_addService(AIBinder *binder,
                                           const char *instance);
AIBinder *AServiceManager_checkService(const char *instance);
}

#include <aidl/android/hardware/media/bufferpool2/IClientManager.h>
#include <aidl/android/hardware/media/c2/Buffer.h>
#include <aidl/android/hardware/media/c2/BnComponentInterface.h>
#include <aidl/android/hardware/media/c2/BnComponentStore.h>
#include <aidl/android/hardware/media/c2/BnConfigurable.h>
#include <aidl/android/hardware/media/c2/FieldSupportedValues.h>
#include <aidl/android/hardware/media/c2/FieldSupportedValuesQuery.h>
#include <aidl/android/hardware/media/c2/FieldSupportedValuesQueryResult.h>
#include <aidl/android/hardware/media/c2/IComponent.h>
#include <aidl/android/hardware/media/c2/IComponentInterface.h>
#include <aidl/android/hardware/media/c2/IComponentListener.h>
#include <aidl/android/hardware/media/c2/IComponentStore.h>
#include <aidl/android/hardware/media/c2/IConfigurable.h>
#include <aidl/android/hardware/media/c2/IInputSurface.h>
#include <aidl/android/hardware/media/c2/ParamDescriptor.h>
#include <aidl/android/hardware/media/c2/ParamField.h>
#include <aidl/android/hardware/media/c2/Params.h>
#include <aidl/android/hardware/media/c2/Status.h>
#include <aidl/android/hardware/media/c2/StructDescriptor.h>
#if CDH_ANDROID_VERSION >= 16
#include <aidl/android/hardware/media/c2/ValueRange.h>
#endif

#include <unistd.h>

#include <android/log.h>

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include <libxml/parser.h>
#include <libxml/tree.h>

using aidl::android::hardware::media::bufferpool2::IClientManager;
using aidl::android::hardware::media::c2::BnComponentInterface;
using aidl::android::hardware::media::c2::BnComponentStore;
using aidl::android::hardware::media::c2::BnConfigurable;
using aidl::android::hardware::media::c2::Buffer;
using aidl::android::hardware::media::c2::FieldSupportedValues;
using aidl::android::hardware::media::c2::FieldSupportedValuesQuery;
using aidl::android::hardware::media::c2::FieldSupportedValuesQueryResult;
using aidl::android::hardware::media::c2::IComponent;
using aidl::android::hardware::media::c2::IComponentInterface;
using aidl::android::hardware::media::c2::IComponentListener;
using aidl::android::hardware::media::c2::IComponentStore;
using aidl::android::hardware::media::c2::IConfigurable;
using aidl::android::hardware::media::c2::IInputSurface;
using aidl::android::hardware::media::c2::ParamDescriptor;
using aidl::android::hardware::media::c2::ParamField;
using aidl::android::hardware::media::c2::Params;
using aidl::android::hardware::media::c2::Status;
using aidl::android::hardware::media::c2::StructDescriptor;
#if CDH_ANDROID_VERSION >= 16
using aidl::android::hardware::media::c2::ValueRange;
#endif
using ndk::ScopedAStatus;

static constexpr const char *kServicePrefix =
    "android.hardware.media.c2.IComponentStore/";
static constexpr const char *kSoftwareServiceName =
    "android.hardware.media.c2.IComponentStore/software";

// 设备值外置（对齐 audio/gnss）：真机每个 vendor 二进制只注册一个 store 实例，
// rc 不传参数、由二进制名选实例。这里把「二进制名→实例」与「实例→组件表」从
// 编译期 codec2_targets.inc 改为运行时 XML，改机型数值不重编驱动：
//   <media-c2-config>
//     <instance binary="<二进制 basename>" store="<实例名>"/>
//     <component store="<实例名>" name="..." kind="..." domain="..."
//                media-type="..." rank="..." aliases="a,b"
//                profiles="1,2" levels="3"/>
//   </media-c2-config>
struct Codec2Target {
  std::string instance;
  std::string name;
  std::string kind;
  std::string domain;
  std::string mediaType;
  int32_t rank = 0;
  std::vector<std::string> aliases;
  std::vector<int32_t> profiles;
  std::vector<int32_t> levels;
};

struct MediaConfig {
  std::vector<Codec2Target> targets;
  std::vector<std::string> instances;  // 去重、保持出现顺序
  std::map<std::string, std::string> binaryToInstance;  // basename -> 实例
};

// ---- libxml2 读取便利层（本模块自用，不进公共头）----
// 读整个文件成 DOM；失败返回 nullptr（调用方回退内置默认）。
static xmlDocPtr loadXmlDoc(const std::string &path) {
  return xmlReadFile(path.c_str(), nullptr,
                     XML_PARSE_NONET | XML_PARSE_NOBLANKS);
}

// 取属性值；缺失返回 false（值拷进 std::string，属性内存即时释放）。
static bool readProp(const xmlNode *node, const char *name, std::string *out) {
  if (node == nullptr) return false;
  xmlChar *v = xmlGetProp(const_cast<xmlNode *>(node), BAD_CAST name);
  if (v == nullptr) return false;
  out->assign(reinterpret_cast<const char *>(v));
  xmlFree(v);
  return true;
}

// 第一个同名子元素（之后用 nextNamed 顺序遍历）。
static xmlNode *firstChild(xmlNode *parent, const char *name) {
  if (parent == nullptr) return nullptr;
  for (xmlNode *c = parent->children; c != nullptr; c = c->next) {
    if (c->type == XML_ELEMENT_NODE && xmlStrcmp(c->name, BAD_CAST name) == 0) {
      return c;
    }
  }
  return nullptr;
}

// 下一个同名兄弟元素（跳过文本/注释与异名元素）。
static xmlNode *nextNamed(xmlNode *node, const char *name) {
  if (node == nullptr) return nullptr;
  for (xmlNode *c = node->next; c != nullptr; c = c->next) {
    if (c->type == XML_ELEMENT_NODE && xmlStrcmp(c->name, BAD_CAST name) == 0) {
      return c;
    }
  }
  return nullptr;
}

static std::string trim(const std::string &value) {
  const size_t begin = value.find_first_not_of(" \t\r\n");
  if (begin == std::string::npos) {
    return std::string();
  }
  const size_t end = value.find_last_not_of(" \t\r\n");
  return value.substr(begin, end - begin + 1);
}

// 逗号分隔子表（aliases/profiles/levels）；整段为空视作空列表。
// 不用 std::istringstream：android14 线链接的 lineage-21.0 libc++.so 不导出
// basic_stringbuf（实测 2159 个导出符号里没有 stringstream / stringbuf），
// 用流会在链接期报 undefined symbol，故按 find 手拆。
static std::vector<std::string> splitComma(const std::string &value) {
  std::vector<std::string> out;
  if (trim(value).empty()) {
    return out;
  }
  size_t begin = 0;
  while (true) {
    const size_t comma = value.find(',', begin);
    const std::string piece = trim(value.substr(
        begin, comma == std::string::npos ? std::string::npos : comma - begin));
    if (!piece.empty()) {
      out.push_back(piece);
    }
    if (comma == std::string::npos) {
      break;
    }
    begin = comma + 1;
  }
  return out;
}

static int32_t parseInt(const std::string &value) {
  return static_cast<int32_t>(strtol(value.c_str(), nullptr, 10));
}

static std::string cfgPath() {
  const char *env = getenv("MEDIA_CONFIG_XML");
  if (env != nullptr && *env != '\0') {
    return env;
  }
  return "/vendor/etc/media_config.xml";
}

static void addInstanceIfAbsent(MediaConfig &cfg, const std::string &instance) {
  if (std::find(cfg.instances.begin(), cfg.instances.end(), instance) ==
      cfg.instances.end()) {
    cfg.instances.push_back(instance);
  }
}

// 逗号分隔子表里的整数表（profiles/levels）；整段为空视作空列表。
static std::vector<int32_t> splitInts(const std::string &value) {
  std::vector<int32_t> out;
  for (const std::string &piece : splitComma(value)) {
    out.push_back(parseInt(piece));
  }
  return out;
}

static MediaConfig loadConfig(const std::string &path) {
  MediaConfig cfg;
  xmlDocPtr doc = loadXmlDoc(path);
  if (doc == nullptr) {
    return cfg;
  }
  xmlNode *root = xmlDocGetRootElement(doc);
  if (root == nullptr) {
    xmlFreeDoc(doc);
    return cfg;
  }

  std::string binary;
  std::string store;
  for (xmlNode *e = firstChild(root, "instance"); e != nullptr;
       e = nextNamed(e, "instance")) {
    if (!readProp(e, "binary", &binary) || !readProp(e, "store", &store)) {
      continue;
    }
    cfg.binaryToInstance[binary] = store;
    addInstanceIfAbsent(cfg, store);
  }

  std::string name;
  std::string v;
  for (xmlNode *e = firstChild(root, "component"); e != nullptr;
       e = nextNamed(e, "component")) {
    if (!readProp(e, "store", &store) || !readProp(e, "name", &name)) {
      continue;
    }
    Codec2Target target;
    target.instance = store;
    target.name = name;
    if (readProp(e, "kind", &v)) {
      target.kind = v;
    }
    if (readProp(e, "domain", &v)) {
      target.domain = v;
    }
    if (readProp(e, "media-type", &v)) {
      target.mediaType = v;
    }
    if (readProp(e, "rank", &v)) {
      target.rank = parseInt(v);
    }
    if (readProp(e, "aliases", &v)) {
      target.aliases = splitComma(v);
    }
    if (readProp(e, "profiles", &v)) {
      target.profiles = splitInts(v);
    }
    if (readProp(e, "levels", &v)) {
      target.levels = splitInts(v);
    }
    cfg.targets.push_back(std::move(target));
    addInstanceIfAbsent(cfg, store);
  }

  xmlFreeDoc(doc);
  return cfg;
}

// ---- C2 profile/level 应答（对齐真机 MTK store）----
// framework 的 Codec2InfoBuilder::addSupportedProfileLevels() 会按组件问
// C2StreamProfileLevelInfo 的 profile/level；答不出来时 MediaCodecInfo 退回 mime
// 默认上限（AVC 仅 Level1 级：maxBlocks=99 / 1485 MB/s / 64kbps），表现为能力塌缩，
// 且 c2.mtk.avc.encoder 会因 XML block-count 下限(100)>默认上限(99) 求交出空区间、
// Java 侧 parseFromInfo 抛异常而被整条丢弃（详见 README §9）。
//
// 组件级常量取自真机 dumpsys profile/levels（SDK 值）经 Codec2Mapper 反查：
//   kParamIndexProfileLevel=0x808；C2ProfileLevelStruct{profile,level} 各 4 字节，
//   但 framework 的 C2ParamField 以**整个 param 对象**为基准算字段偏移：
//   sizeof(C2StreamProfileLevelInfo)=16 = 8(C2Param 头) + 4(profile) + 4(level)，
//   故 profile 偏移 8、level 偏移 12（C2 头文件实测值，非推断）。
//   param index = INFO(0xC0000000) | dir(decoder=INPUT=0 / encoder=OUTPUT=0x10000000)
//                 | IS_STREAM_FLAG(0x02000000) | streamId(0) | coreIndex(0x808)
//   （framework 用 C2StreamProfileLevelInfo pl(encoder, 0u) 构造）。
// 匹配只认 core index + 字段偏移，kind/dir/stream 位放开。
static constexpr uint32_t kProfileLevelCoreIndex = 0x808u;
static constexpr int32_t kProfileFieldOffset = 8;
static constexpr int32_t kLevelFieldOffset = 12;

static bool isProfileLevelField(const ParamField &field, int32_t offset) {
  const uint32_t index = static_cast<uint32_t>(field.index);
  if ((index & 0x0000FFFFu) != kProfileLevelCoreIndex) {
    return false;
  }
  return field.fieldId.offset == offset;
}

static FieldSupportedValues makeEmptyValues() {
  return FieldSupportedValues::make<FieldSupportedValues::Tag::empty>(true);
}

static FieldSupportedValues makeValues(const std::vector<int32_t> &list) {
  std::vector<int64_t> values;
  values.reserve(list.size());
  for (const int32_t value : list) {
    values.push_back(value);
  }
  return FieldSupportedValues::make<FieldSupportedValues::Tag::values>(
      std::move(values));
}

static Status makeStatus(int32_t value) {
  Status status;
  status.status = value;
  return status;
}

#if CDH_ANDROID_VERSION >= 16
// ---- store 级分配参数（dmabuf-usage / ion-usage）----
// C2PlatformAllocatorStore 通过 preferred store 的 IConfigurable 构造 DMA-BUF
// 分配映射器：先 querySupportedValues(usage, capacity) 要求 C2_OK 才安装映射器，
// 之后每次分配都 config(C2StoreDmaBufUsageInfo) 取 heapName。AOSP 参考 store
// （hal/services/vendor.cpp 的 SetDmaBufUsage/SetIonUsage）会回填
// heapName="system"、allocFlags=0；本假 store 若只回 OMITTED，映射器虽装上但
// heapName 为空，Alloc("") 会把 /dev/dma_heap 当设备打开 -> EACCES -> NO_MEMORY，
// c2.android.* 软件解码全部无法启动（对标设备实测表现为 launcher 黑屏）。
//
// AIDL Params blob 布局（C2Param 头 8B + 结构体，8 字节对齐；见 C2Config.h）：
//   C2StoreDmaBufUsageInfo : usage@8(u64) capacity@16(u32) allocFlags@20(i32)
//                            heapName@24(char[flex])
//   C2StoreIonUsageInfo    : usage@8(u64) capacity@16(u32) heapMask@20(i32)
//                            allocFlags@24(i32) minAlignment@28(u32)
// typeIndex 为 C2Param index 低 16 位：ion-usage=0x4002、dmabuf-usage=0x4019。
static constexpr uint32_t kStoreIonUsageTypeIndex = 0x4002u;
static constexpr uint32_t kStoreDmaBufUsageTypeIndex = 0x4019u;

static constexpr size_t kParamHeaderSize = 8;
static constexpr size_t kAllocUsageOffset = 8;
static constexpr size_t kAllocCapacityOffset = 16;
static constexpr size_t kDmaBufAllocFlagsOffset = 20;
static constexpr size_t kDmaBufHeapNameOffset = 24;
static constexpr size_t kIonHeapMaskOffset = 20;
static constexpr size_t kIonAllocFlagsOffset = 24;
static constexpr size_t kIonMinAlignmentOffset = 28;

// C2MemoryUsage::CPU_READ(1<<0) | CPU_WRITE(1<<2)；FLAGS 首元素为最小标志集，
// 与 AOSP .flags({CPU_READ | CPU_WRITE}) 产生的 wire 值 {0, 5} 一致。
static constexpr int64_t kCpuUsageMinFlags = 0;
static constexpr int64_t kCpuUsageFlags = 0x1 | 0x4;
static constexpr int64_t kCapacityMax = 0xFFFFFFFFll;
static constexpr int64_t kCapacityRangeStep = 1024;
static constexpr char kDefaultDmaBufHeap[] = "system";

static bool isStoreUsageField(const ParamField &field, uint32_t typeIndex,
                              size_t offset) {
  const uint32_t index = static_cast<uint32_t>(field.index);
  if ((index & 0x0000FFFFu) != typeIndex) {
    return false;
  }
  return static_cast<size_t>(field.fieldId.offset) == offset;
}

static FieldSupportedValues makeFlags(std::vector<int64_t> flags) {
  return FieldSupportedValues::make<FieldSupportedValues::Tag::flags>(
      std::move(flags));
}

static FieldSupportedValues makeRange(int64_t min, int64_t max, int64_t step) {
  ValueRange range;
  range.min = min;
  range.max = max;
  range.step = step;
  range.num = 1;
  range.denom = 1;
  return FieldSupportedValues::make<FieldSupportedValues::Tag::range>(range);
}

// 就地回填 store 分配参数（对齐 AOSP SetDmaBufUsage/SetIonUsage）。blob 为
// C2Param 序列，逐条按 8 字节对齐前进；返回是否命中至少一个已知参数。
static bool applyStoreAllocParams(std::vector<uint8_t> *blob,
                                  const char **applied) {
  if (blob == nullptr) {
    return false;
  }
  bool touched = false;
  size_t ix = 0;
  const size_t size = blob->size();
  while (ix + kParamHeaderSize <= size) {
    uint32_t paramSize = 0;
    uint32_t index = 0;
    std::memcpy(&paramSize, blob->data() + ix, sizeof(paramSize));
    std::memcpy(&index, blob->data() + ix + sizeof(paramSize), sizeof(index));
    if (paramSize < kParamHeaderSize || paramSize > size - ix) {
      break;
    }
    uint8_t *base = blob->data() + ix;
    const uint32_t typeIndex = index & 0x0000FFFFu;
    if (typeIndex == kStoreDmaBufUsageTypeIndex) {
      if (paramSize >= kDmaBufAllocFlagsOffset + sizeof(int32_t)) {
        const int32_t zero = 0;
        std::memcpy(base + kDmaBufAllocFlagsOffset, &zero, sizeof(zero));
      }
      if (paramSize > kDmaBufHeapNameOffset) {
        const size_t room = paramSize - kDmaBufHeapNameOffset;
        std::memset(base + kDmaBufHeapNameOffset, 0, room);
        const size_t nameLen = std::strlen(kDefaultDmaBufHeap);
        std::memcpy(base + kDmaBufHeapNameOffset, kDefaultDmaBufHeap,
                    std::min(room, nameLen));
      }
      touched = true;
      if (applied != nullptr) {
        *applied = "dmabuf-usage";
      }
    } else if (typeIndex == kStoreIonUsageTypeIndex) {
      if (paramSize >= kIonMinAlignmentOffset + sizeof(uint32_t)) {
        const int32_t ones = -1;
        const int32_t zero = 0;
        const uint32_t uzero = 0;
        std::memcpy(base + kIonHeapMaskOffset, &ones, sizeof(ones));
        std::memcpy(base + kIonAllocFlagsOffset, &zero, sizeof(zero));
        std::memcpy(base + kIonMinAlignmentOffset, &uzero, sizeof(uzero));
      }
      touched = true;
      if (applied != nullptr) {
        *applied = "ion-usage";
      }
    }
    ix += (static_cast<size_t>(paramSize) + 7u) & ~static_cast<size_t>(7u);
  }
  return touched;
}
#endif  // CDH_ANDROID_VERSION >= 16

static ScopedAStatus serviceError(int32_t value) {
  return ScopedAStatus::fromServiceSpecificError(value);
}

static IComponentStore::ComponentTraits::Kind kindFor(const char *value) {
  if (std::strcmp(value, "decoder") == 0) {
    return IComponentStore::ComponentTraits::Kind::DECODER;
  }
  if (std::strcmp(value, "encoder") == 0) {
    return IComponentStore::ComponentTraits::Kind::ENCODER;
  }
  return IComponentStore::ComponentTraits::Kind::OTHER;
}

static IComponentStore::ComponentTraits::Domain domainFor(const char *value) {
  if (std::strcmp(value, "audio") == 0) {
    return IComponentStore::ComponentTraits::Domain::AUDIO;
  }
  if (std::strcmp(value, "video") == 0) {
    return IComponentStore::ComponentTraits::Domain::VIDEO;
  }
  if (std::strcmp(value, "image") == 0) {
    return IComponentStore::ComponentTraits::Domain::IMAGE;
  }
  return IComponentStore::ComponentTraits::Domain::OTHER;
}

static std::string serviceNameForInstance(const std::string &instance) {
  return std::string(kServicePrefix) + instance;
}

static bool hasArg(int argc, char **argv, const char *value) {
  for (int idx = 1; idx < argc; ++idx) {
    if (std::strcmp(argv[idx], value) == 0) {
      return true;
    }
  }
  return false;
}

static bool serviceIsSoftware(const char *serviceName) {
  return std::strcmp(serviceName, kSoftwareServiceName) == 0;
}

static void writeJSONString(std::string &out, const std::string &value) {
  out += '"';
  for (const char ch : value) {
    switch (ch) {
    case '\\':
      out += "\\\\";
      break;
    case '"':
      out += "\\\"";
      break;
    case '\n':
      out += "\\n";
      break;
    case '\r':
      out += "\\r";
      break;
    case '\t':
      out += "\\t";
      break;
    default:
      out += ch;
      break;
    }
  }
  out += '"';
}

static bool traitIsEncoder(const IComponentStore::ComponentTraits &traits) {
  return traits.kind == IComponentStore::ComponentTraits::Kind::ENCODER;
}

static bool traitIsHardwareAccelerated(
    const char *serviceName, const IComponentStore::ComponentTraits &traits) {
  // Android framework treats device Codec2 stores as hardware-backed codec
  // inputs; keep software store entries explicit so DevCheck-style summaries
  // can distinguish HW and SW directions without reading XML.
  return !serviceIsSoftware(serviceName) &&
         traits.domain == IComponentStore::ComponentTraits::Domain::VIDEO;
}

static void appendProbeServiceJSON(std::string &out, const char *serviceName,
                                   bool ok, const std::string &error,
                                   size_t componentCount,
                                   const std::string &configurableStatus = "",
                                   const std::string &configurableName = "") {
  out += "{\"name\":";
  writeJSONString(out, serviceName);
  out += ok ? ",\"ok\":true" : ",\"ok\":false";
  if (!error.empty()) {
    out += ",\"error\":";
    writeJSONString(out, error);
  }
  if (ok) {
    out += ",\"componentCount\":";
    out += std::to_string(componentCount);
  }
  if (!configurableStatus.empty()) {
    out += ",\"configurableStatus\":";
    writeJSONString(out, configurableStatus);
  }
  if (!configurableName.empty()) {
    out += ",\"configurableName\":";
    writeJSONString(out, configurableName);
  }
  out += "}";
}

static void appendProbeCodecJSON(
    std::string &out, const char *serviceName,
    const IComponentStore::ComponentTraits &traits) {
  const bool software = serviceIsSoftware(serviceName);
  out += "{\"name\":";
  writeJSONString(out, traits.name);
  out += traitIsEncoder(traits) ? ",\"encoder\":true" : ",\"encoder\":false";
  out += traitIsHardwareAccelerated(serviceName, traits)
             ? ",\"hardwareAccelerated\":true"
             : ",\"hardwareAccelerated\":false";
  out += software ? ",\"softwareOnly\":true" : ",\"softwareOnly\":false";
  out += software ? ",\"vendor\":false" : ",\"vendor\":true";
  out += ",\"supportedTypes\":[";
  writeJSONString(out, traits.mediaType);
  out += "]}";
}

static int runProbe(const std::vector<std::string> &instances) {
  // Probe the same Store service names that framework MediaCodecList resolves;
  // XML, dumpsys and app-side providers are intentionally outside this path.
  // 服务名由 cfg 实例表驱动（真机 vendor store）+ apex 提供的 software 实例。
  std::vector<std::string> serviceNames;
  for (const std::string &instance : instances) {
    serviceNames.push_back(serviceNameForInstance(instance));
  }
  serviceNames.push_back(kSoftwareServiceName);
  std::vector<std::string> serviceJSON;
  std::vector<std::string> codecJSON;
  bool ok = true;
  std::string firstError;

  for (const std::string &serviceNameStr : serviceNames) {
    const char *serviceName = serviceNameStr.c_str();
    ndk::SpAIBinder binder(AServiceManager_checkService(serviceName));
    if (binder.get() == nullptr) {
      ok = false;
      const std::string error = std::string(serviceName) + " missing";
      if (firstError.empty()) {
        firstError = error;
      }
      std::string serviceOut;
      appendProbeServiceJSON(serviceOut, serviceName, false, error, 0);
      serviceJSON.push_back(serviceOut);
      continue;
    }
    std::shared_ptr<IComponentStore> store = IComponentStore::fromBinder(binder);
    if (store == nullptr) {
      ok = false;
      const std::string error = std::string(serviceName) + " binder cast failed";
      if (firstError.empty()) {
        firstError = error;
      }
      std::string serviceOut;
      appendProbeServiceJSON(serviceOut, serviceName, false, error, 0);
      serviceJSON.push_back(serviceOut);
      continue;
    }

    std::vector<IComponentStore::ComponentTraits> traits;
    ScopedAStatus status = store->listComponents(&traits);
    if (!status.isOk()) {
      ok = false;
      const std::string error =
          std::string(serviceName) + " listComponents failed: " +
          status.getDescription();
      if (firstError.empty()) {
        firstError = error;
      }
      std::string serviceOut;
      appendProbeServiceJSON(serviceOut, serviceName, false, error, 0);
      serviceJSON.push_back(serviceOut);
      continue;
    }

    std::shared_ptr<IConfigurable> configurable;
    ScopedAStatus configurableCall = store->getConfigurable(&configurable);
    if (!configurableCall.isOk() || configurable == nullptr) {
      ok = false;
      const std::string error = std::string(serviceName) +
          " getConfigurable failed: " + configurableCall.getDescription();
      if (firstError.empty()) {
        firstError = error;
      }
      std::string serviceOut;
      appendProbeServiceJSON(serviceOut, serviceName, false, error,
                             traits.size(), "failed");
      serviceJSON.push_back(serviceOut);
      continue;
    }
    std::string configurableName;
    ScopedAStatus configurableNameCall = configurable->getName(&configurableName);
    if (!configurableNameCall.isOk()) {
      ok = false;
      const std::string error = std::string(serviceName) +
          " configurable getName failed: " + configurableNameCall.getDescription();
      if (firstError.empty()) {
        firstError = error;
      }
      std::string serviceOut;
      appendProbeServiceJSON(serviceOut, serviceName, false, error,
                             traits.size(), "failed");
      serviceJSON.push_back(serviceOut);
      continue;
    }

    std::string serviceOut;
    appendProbeServiceJSON(serviceOut, serviceName, true, "", traits.size(),
                           "ok", configurableName);
    serviceJSON.push_back(serviceOut);
    for (const auto &trait : traits) {
      std::string codecOut;
      appendProbeCodecJSON(codecOut, serviceName, trait);
      codecJSON.push_back(codecOut);
    }
  }

  std::string payload = "{\"source\":\"codec2-aidl-probe\",\"ok\":";
  payload += ok ? "true" : "false";
  if (!firstError.empty()) {
    payload += ",\"error\":";
    writeJSONString(payload, firstError);
  }
  payload += ",\"services\":[";
  for (size_t idx = 0; idx < serviceJSON.size(); ++idx) {
    if (idx != 0) {
      payload += ',';
    }
    payload += serviceJSON[idx];
  }
  payload += "],\"codecs\":[";
  for (size_t idx = 0; idx < codecJSON.size(); ++idx) {
    if (idx != 0) {
      payload += ',';
    }
    payload += codecJSON[idx];
  }
  payload += "]}\n";
  // 走 stdio 而不是 std::cout：流插入器要链 libc++ 的 __put_character_sequence，
  // Android 14 平台的 libc++.so 不导出（见 vendor/build/README android14 节）。
  fwrite(payload.data(), 1, payload.size(), stdout);
  fflush(stdout);
  return ok ? 0 : 2;
}

static bool targetMatchesName(const Codec2Target &target,
                              const std::string &name) {
  if (target.name == name) {
    return true;
  }
  for (const std::string &alias : target.aliases) {
    if (alias == name) {
      return true;
    }
  }
  return false;
}

class Codec2StoreConfigurable : public BnConfigurable {
public:
#if CDH_ANDROID_VERSION >= 16
  Codec2StoreConfigurable(std::string name, std::vector<int32_t> profiles,
                          std::vector<int32_t> levels, bool storeParams = false)
      : name_(std::move(name)), profiles_(std::move(profiles)),
        levels_(std::move(levels)), storeParams_(storeParams) {}
#else
  Codec2StoreConfigurable(std::string name, std::vector<int32_t> profiles,
                          std::vector<int32_t> levels)
      : name_(std::move(name)), profiles_(std::move(profiles)),
        levels_(std::move(levels)) {}
#endif

  ScopedAStatus config(const Params &inParams, bool,
                       IConfigurable::ConfigResult *out) override {
    out->params = inParams;
    out->failures.clear();
#if CDH_ANDROID_VERSION >= 16
    if (storeParams_) {
      const char *applied = nullptr;
      if (applyStoreAllocParams(&out->params.params, &applied)) {
        __android_log_print(ANDROID_LOG_INFO, "cdh-codec2",
                            "%s: config %s -> heap=%s", name_.c_str(),
                            applied != nullptr ? applied : "?",
                            kDefaultDmaBufHeap);
      }
    }
#endif
    out->status = makeStatus(Status::OK);
    return ScopedAStatus::ok();
  }

  ScopedAStatus getId(int32_t *out) override {
    *out = 0;
    return ScopedAStatus::ok();
  }

  ScopedAStatus getName(std::string *out) override {
    *out = name_;
    return ScopedAStatus::ok();
  }

  ScopedAStatus query(const std::vector<int32_t> &, bool,
                      IConfigurable::QueryResult *out) override {
    out->params.params.clear();
    out->status = makeStatus(Status::OK);
    return ScopedAStatus::ok();
  }

  ScopedAStatus querySupportedParams(int32_t, int32_t,
                                     std::vector<ParamDescriptor> *out) override {
    out->clear();
    return ScopedAStatus::ok();
  }

  // profile/level 是 framework 拿到真实能力上限的唯一来源：按组件答
  // C2StreamProfileLevelInfo 的 profile（POSSIBLE）与 level（CURRENT）列表；
  // 其他字段仍回 OMITTED（本 store 不实例化组件、不承载其它参数语义）。
  ScopedAStatus querySupportedValues(
      const std::vector<FieldSupportedValuesQuery> &inFields, bool,
      IConfigurable::QuerySupportedValuesResult *out) override {
    out->values.clear();
    out->values.reserve(inFields.size());
    for (const FieldSupportedValuesQuery &field : inFields) {
      FieldSupportedValuesQueryResult result;
      const std::vector<int32_t> *answer = nullptr;
      if (isProfileLevelField(field.field, kProfileFieldOffset)) {
        answer = &profiles_;
      } else if (isProfileLevelField(field.field, kLevelFieldOffset)) {
        answer = &levels_;
      }
#if CDH_ANDROID_VERSION >= 16
      const bool usage = storeParams_ &&
          (isStoreUsageField(field.field, kStoreDmaBufUsageTypeIndex,
                             kAllocUsageOffset) ||
           isStoreUsageField(field.field, kStoreIonUsageTypeIndex,
                             kAllocUsageOffset));
      const bool capacity = storeParams_ &&
          (isStoreUsageField(field.field, kStoreDmaBufUsageTypeIndex,
                             kAllocCapacityOffset) ||
           isStoreUsageField(field.field, kStoreIonUsageTypeIndex,
                             kAllocCapacityOffset));
      const bool answered = (answer != nullptr && !answer->empty()) || usage ||
                            capacity;
      // 诊断：收到的每次查询都记一行（组件数量有限，量很小）
      __android_log_print(ANDROID_LOG_INFO, "cdh-codec2",
                          "%s: qv idx=0x%08x off=%d size=%d type=%d -> %s",
                          name_.c_str(), static_cast<uint32_t>(field.field.index),
                          field.field.fieldId.offset, field.field.fieldId.sizeBytes,
                          static_cast<int>(field.type),
                          answered ? (usage ? "flags" : (capacity ? "range" : "answer"))
                                   : "omit");
      if (answered) {
        result.status = makeStatus(Status::OK);
        if (usage) {
          result.values = makeFlags({kCpuUsageMinFlags, kCpuUsageFlags});
        } else if (capacity) {
          result.values = makeRange(0, kCapacityMax, kCapacityRangeStep);
        } else {
          result.values = makeValues(*answer);
        }
      } else {
#else
      // 诊断：收到的每次查询都记一行（组件数量有限，量很小）
      __android_log_print(ANDROID_LOG_INFO, "cdh-codec2",
                          "%s: qv idx=0x%08x off=%d size=%d type=%d -> %s",
                          name_.c_str(), static_cast<uint32_t>(field.field.index),
                          field.field.fieldId.offset, field.field.fieldId.sizeBytes,
                          static_cast<int>(field.type),
                          (answer != nullptr && !answer->empty()) ? "answer" : "omit");
      if (answer != nullptr && !answer->empty()) {
        result.status = makeStatus(Status::OK);
        result.values = makeValues(*answer);
      } else {
#endif
        result.status = makeStatus(Status::OMITTED);
        result.values = makeEmptyValues();
      }
      out->values.push_back(result);
    }
    out->status = makeStatus(Status::OK);
    return ScopedAStatus::ok();
  }

private:
  std::string name_;
  std::vector<int32_t> profiles_;
  std::vector<int32_t> levels_;
#if CDH_ANDROID_VERSION >= 16
  bool storeParams_ = false;
#endif
};

class Codec2StoreInterface : public BnComponentInterface {
public:
  Codec2StoreInterface(const Codec2Target &target)
      : configurable_(ndk::SharedRefBase::make<Codec2StoreConfigurable>(
            target.name, target.profiles, target.levels)) {}

  ScopedAStatus getConfigurable(std::shared_ptr<IConfigurable> *out) override {
    *out = configurable_;
    return ScopedAStatus::ok();
  }

private:
  std::shared_ptr<IConfigurable> configurable_;
};

class Codec2Store : public BnComponentStore {
public:
  Codec2Store(std::string instance, const std::vector<Codec2Target> &all)
      : instance_(std::move(instance)),
        configurable_(ndk::SharedRefBase::make<Codec2StoreConfigurable>(
            serviceNameForInstance(instance_), std::vector<int32_t>{},
            std::vector<int32_t>{}
#if CDH_ANDROID_VERSION >= 16
            , true
#endif
            )) {
    for (const auto &target : all) {
      if (target.instance == instance_) {
        targets_.push_back(&target);
      }
    }
  }

  size_t targetCount() const { return targets_.size(); }

  ScopedAStatus copyBuffer(const Buffer &, const Buffer &) override {
    return serviceError(Status::CANNOT_DO);
  }

  ScopedAStatus createComponent(
      const std::string &name,
      const std::shared_ptr<IComponentListener> &listener,
      const std::shared_ptr<IClientManager> &pool,
      std::shared_ptr<IComponent> *out) override {
    const std::string v4l2Name = mapToV4l2ComponentName(name);
    if (!v4l2Name.empty()) {
      std::shared_ptr<IComponentStore> v4l2Store = getDefaultV4l2Store();
      if (v4l2Store != nullptr) {
        return v4l2Store->createComponent(v4l2Name, listener, pool, out);
      }
    }
    return serviceError(Status::CANNOT_DO);
  }

  ScopedAStatus createInterface(
      const std::string &name,
      std::shared_ptr<IComponentInterface> *out) override {
    const Codec2Target *target = findTarget(name);
    if (target == nullptr) {
      return serviceError(Status::NOT_FOUND);
    }
    *out = ndk::SharedRefBase::make<Codec2StoreInterface>(*target);
    return ScopedAStatus::ok();
  }

  ScopedAStatus getConfigurable(std::shared_ptr<IConfigurable> *out) override {
    *out = configurable_;
    return ScopedAStatus::ok();
  }

  ScopedAStatus getPoolClientManager(
      std::shared_ptr<IClientManager> *out) override {
    std::shared_ptr<IComponentStore> v4l2Store = getDefaultV4l2Store();
    if (v4l2Store != nullptr) {
      return v4l2Store->getPoolClientManager(out);
    }
    out->reset();
    return serviceError(Status::CANNOT_DO);
  }

  ScopedAStatus getStructDescriptors(const std::vector<int32_t> &indices,
                                     std::vector<StructDescriptor> *out) override {
    std::shared_ptr<IComponentStore> v4l2Store = getDefaultV4l2Store();
    if (v4l2Store != nullptr) {
      ScopedAStatus status = v4l2Store->getStructDescriptors(indices, out);
      if (status.isOk() && !out->empty()) {
        return status;
      }
    }
    out->clear();
    return serviceError(Status::NOT_FOUND);
  }

  ScopedAStatus
  listComponents(std::vector<IComponentStore::ComponentTraits> *out) override {
    out->clear();
    out->reserve(targets_.size());
    for (const Codec2Target *target : targets_) {
      // DevCheck/AIDA64 codec fields depend on ComponentTraits; this service
      // publishes only the registry metadata and refuses actual codec creation.
      IComponentStore::ComponentTraits traits;
      traits.name = target->name;
      traits.domain = domainFor(target->domain.c_str());
      traits.kind = kindFor(target->kind.c_str());
      traits.rank = target->rank;
      traits.mediaType = target->mediaType;
      for (const std::string &alias : target->aliases) {
        traits.aliases.push_back(alias);
      }
      out->push_back(traits);
    }
    return ScopedAStatus::ok();
  }

  ScopedAStatus createInputSurface(
      std::shared_ptr<IInputSurface> *) override {
    return serviceError(Status::CANNOT_DO);
  }

private:
  std::string mapToV4l2ComponentName(const std::string &name) const {
    if (name.find(".secure") != std::string::npos ||
        name.find(".SECURE") != std::string::npos) {
      return std::string();
    }
    const Codec2Target *target = findTarget(name);
    if (target != nullptr && target->domain == "video") {
      if (target->kind == "decoder") {
        if (target->mediaType == "video/avc") {
          return "c2.v4l2.avc.decoder";
        }
        if (target->mediaType == "video/hevc") {
          return "c2.v4l2.hevc.decoder";
        }
        if (target->mediaType == "video/x-vnd.on2.vp8") {
          return "c2.v4l2.vp8.decoder";
        }
        if (target->mediaType == "video/x-vnd.on2.vp9") {
          return "c2.v4l2.vp9.decoder";
        }
      } else if (target->kind == "encoder") {
        if (target->mediaType == "video/avc") {
          return "c2.v4l2.avc.encoder";
        }
        if (target->mediaType == "video/x-vnd.on2.vp8") {
          return "c2.v4l2.vp8.encoder";
        }
        if (target->mediaType == "video/x-vnd.on2.vp9") {
          return "c2.v4l2.vp9.encoder";
        }
      }
    }
    if (name == "c2.mtk.avc.decoder" || name == "OMX.MTK.VIDEO.DECODER.AVC") {
      return "c2.v4l2.avc.decoder";
    }
    if (name == "c2.mtk.hevc.decoder" || name == "OMX.MTK.VIDEO.DECODER.HEVC") {
      return "c2.v4l2.hevc.decoder";
    }
    if (name == "c2.mtk.avc.encoder" || name == "OMX.MTK.VIDEO.ENCODER.AVC") {
      return "c2.v4l2.avc.encoder";
    }
    if (name == "c2.mtk.vpx.decoder" || name == "OMX.MTK.VIDEO.DECODER.VPX") {
      return "c2.v4l2.vp8.decoder";
    }
    if (name == "c2.mtk.vp9.decoder" || name == "OMX.MTK.VIDEO.DECODER.VP9") {
      return "c2.v4l2.vp9.decoder";
    }
    return std::string();
  }

  std::shared_ptr<IComponentStore> getDefaultV4l2Store() const {
    if (instance_ == "default") {
      return nullptr;
    }
    ndk::SpAIBinder binder(AServiceManager_checkService(
        "android.hardware.media.c2.IComponentStore/default"));
    if (binder.get() == nullptr) {
      return nullptr;
    }
    return IComponentStore::fromBinder(binder);
  }

  const Codec2Target *findTarget(const std::string &name) const {
    auto it = std::find_if(targets_.begin(), targets_.end(),
                           [&](const Codec2Target *target) {
                             return targetMatchesName(*target, name);
                           });
    if (it == targets_.end()) {
      return nullptr;
    }
    return *it;
  }

  std::string instance_;
  std::vector<const Codec2Target *> targets_;
  std::shared_ptr<IConfigurable> configurable_;
};

static binder_status_t
registerAsService(const std::shared_ptr<ndk::ICInterface> &service,
                  const char *name) {
  return AServiceManager_addService(service->asBinder().get(), name);
}

static std::vector<std::string> parseInstances(int argc, char **argv) {
  std::vector<std::string> instances;
  for (int idx = 1; idx + 1 < argc; ++idx) {
    if (std::strcmp(argv[idx], "--instance") == 0) {
      instances.emplace_back(argv[idx + 1]);
      ++idx;
    }
  }
  return instances;
}

static std::string executableBaseName() {
  char path[4096];
  const ssize_t length = readlink("/proc/self/exe", path, sizeof(path) - 1);
  if (length <= 0) {
    return std::string();
  }
  path[length] = '\0';
  const char *slash = std::strrchr(path, '/');
  return slash != nullptr ? std::string(slash + 1) : std::string(path);
}

int main(int argc, char **argv) {
  const MediaConfig cfg = loadConfig(cfgPath());
  if (hasArg(argc, argv, "--probe")) {
    return runProbe(cfg.instances);
  }
  std::vector<std::string> instances = parseInstances(argc, argv);
  if (instances.empty()) {
    // Real-device shape: rc passes no arguments, the binary name alone picks
    // the store instance —— 「二进制名→实例」映射由运行时 cfg 的 instance 行提供。
    const auto it = cfg.binaryToInstance.find(executableBaseName());
    if (it == cfg.binaryToInstance.end()) {
      return 1;
    }
    instances.push_back(it->second);
  }

  ABinderProcess_setThreadPoolMaxThreadCount(
      static_cast<uint32_t>(instances.size() * 2));
  std::vector<std::shared_ptr<Codec2Store>> stores;
  std::vector<std::string> registeredInstances;
  for (const std::string &instance : instances) {
    const std::string serviceName = serviceNameForInstance(instance);
    if (std::find(registeredInstances.begin(), registeredInstances.end(),
                  instance) != registeredInstances.end()) {
      return 1;
    }
    registeredInstances.push_back(instance);
    auto store = ndk::SharedRefBase::make<Codec2Store>(instance, cfg.targets);
    if (store->targetCount() == 0) {
      return 3;
    }
    if (registerAsService(store, serviceName.c_str()) != STATUS_OK) {
      return 2;
    }
    stores.push_back(store);
  }
  ABinderProcess_joinThreadPool();
  return 0;
}