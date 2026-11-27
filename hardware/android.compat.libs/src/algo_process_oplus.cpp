// Copyright (c) 2026 cloud-phone-hub
//
// Self-contained Oplus CameraUnit APS (Algorithm Process Service) engine
// (`libAlgoProcess.so`) for virtualized ColorOS / OxygenOS / RealmeUI cameras.
//
// Loaded via `android_load_sphal_library("libAlgoProcess.so", RTLD_NOW)` by
// `/product/lib64/libAPSClient-cmd-jni.so`. Compiled with NDK `libc++_static.a`
// (`std::__ndk1` ABI) so `DataMap` matches `libAPSClient-cmd-jni.so` bit-for-bit
// while keeping DT_NEEDED strictly within NDK / LLNDK public libraries.

#include <android/hardware_buffer.h>
#include <android/log.h>

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <deque>
#include <map>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

#define LOG_TAG "CDH_APS"
#define ALOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define ALOGW(...) __android_log_print(ANDROID_LOG_WARN, LOG_TAG, __VA_ARGS__)
#define ALOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

using DataMap = std::map<std::string, std::vector<std::string>>;
using OnTransactCb = int (*)(int code, DataMap& data, DataMap& reply);
using RequestActionCb = void (*)(int action, DataMap* data, DataMap* reply);

namespace {

std::atomic<OnTransactCb> g_transact_cb{nullptr};
std::atomic<RequestActionCb> g_action_cb{nullptr};
std::atomic<int> g_seq_id{1};

struct CachedFrame {
    std::string identity;
    std::string frame_number;
    std::string camera_id;
    std::string capture_mode;
    int width = 0;
    int height = 0;
    int rotation = 0;
    bool mirror = false;
    std::vector<uint8_t> y_plane;
    std::vector<uint8_t> u_plane;
    std::vector<uint8_t> v_plane;
};

std::mutex g_frames_mu;
std::deque<CachedFrame> g_frames;
std::string g_last_camera_id = "0";
std::string g_last_capture_mode = "common";
std::atomic<uint64_t> g_preview_cb_count{0};

// Parse alternating [k0, v0, k1, v1, ...] vector into unordered_map.
std::unordered_map<std::string, std::string> parseKvPairs(const DataMap* m, const char* key) {
    std::unordered_map<std::string, std::string> out;
    if (!m) return out;
    auto it = m->find(key);
    if (it == m->end()) return out;
    const auto& vec = it->second;
    for (size_t i = 0; i + 1 < vec.size(); i += 2) {
        out[vec[i]] = vec[i + 1];
    }
    return out;
}

std::string getFirst(const DataMap* m, const char* key) {
    if (!m) return "";
    auto it = m->find(key);
    if (it == m->end() || it->second.empty()) return "";
    return it->second[0];
}

int parseIntOr(const std::string& s, int defVal) {
    if (s.empty()) return defVal;
    char* end = nullptr;
    long v = std::strtol(s.c_str(), &end, 10);
    if (end == s.c_str()) return defVal;
    return static_cast<int>(v);
}

// Extract I420 (planar Y, U/Cb, V/Cr) from AHardwareBuffer.
bool extractI420FromHardwareBuffer(AHardwareBuffer* ahb, int fallbackW, int fallbackH, CachedFrame* out) {
    if (!ahb || !out) return false;
    AHardwareBuffer_Desc desc{};
    AHardwareBuffer_describe(ahb, &desc);
    int w = (desc.width > 0) ? static_cast<int>(desc.width) : fallbackW;
    int h = (desc.height > 0) ? static_cast<int>(desc.height) : fallbackH;
    if (w <= 0 || h <= 0) return false;
    // Ensure even dimensions for 4:2:0 subsampling.
    w &= ~1;
    h &= ~1;
    if (w <= 0 || h <= 0) return false;

    AHardwareBuffer_Planes planes{};
    int rc = AHardwareBuffer_lockPlanes(
        ahb, AHARDWAREBUFFER_USAGE_CPU_READ_OFTEN, -1, nullptr, &planes);
    if (rc == 0 && planes.planeCount >= 3 &&
        planes.planes[0].data && planes.planes[1].data && planes.planes[2].data) {
        out->width = w;
        out->height = h;
        out->y_plane.resize(static_cast<size_t>(w) * h);
        int cw = w / 2;
        int ch = h / 2;
        out->u_plane.resize(static_cast<size_t>(cw) * ch);
        out->v_plane.resize(static_cast<size_t>(cw) * ch);

        const auto* ySrc = static_cast<const uint8_t*>(planes.planes[0].data);
        uint32_t yRowStride = planes.planes[0].rowStride;
        uint32_t yPixStride = planes.planes[0].pixelStride ? planes.planes[0].pixelStride : 1;
        for (int r = 0; r < h; ++r) {
            const uint8_t* row = ySrc + static_cast<size_t>(r) * yRowStride;
            uint8_t* dst = out->y_plane.data() + static_cast<size_t>(r) * w;
            if (yPixStride == 1) {
                std::memcpy(dst, row, static_cast<size_t>(w));
            } else {
                for (int c = 0; c < w; ++c) dst[c] = row[c * yPixStride];
            }
        }

        // Per Android NDK AHardwareBuffer_lockPlanes spec for Y8Cb8Cr8_420:
        // plane 1 is Cb (U), plane 2 is Cr (V).
        const auto* uSrc = static_cast<const uint8_t*>(planes.planes[1].data);
        const auto* vSrc = static_cast<const uint8_t*>(planes.planes[2].data);
        uint32_t uRowStride = planes.planes[1].rowStride;
        uint32_t vRowStride = planes.planes[2].rowStride;
        uint32_t uPixStride = planes.planes[1].pixelStride ? planes.planes[1].pixelStride : 1;
        uint32_t vPixStride = planes.planes[2].pixelStride ? planes.planes[2].pixelStride : 1;

        for (int r = 0; r < ch; ++r) {
            const uint8_t* uRow = uSrc + static_cast<size_t>(r) * uRowStride;
            const uint8_t* vRow = vSrc + static_cast<size_t>(r) * vRowStride;
            uint8_t* uDst = out->u_plane.data() + static_cast<size_t>(r) * cw;
            uint8_t* vDst = out->v_plane.data() + static_cast<size_t>(r) * cw;
            for (int c = 0; c < cw; ++c) {
                uDst[c] = uRow[c * uPixStride];
                vDst[c] = vRow[c * vPixStride];
            }
        }
        AHardwareBuffer_unlock(ahb, nullptr);
        return true;
    }

    // Fallback to AHardwareBuffer_lock assuming NV12 / tight YUV420.
    void* raw = nullptr;
    if (AHardwareBuffer_lock(ahb, AHARDWAREBUFFER_USAGE_CPU_READ_OFTEN, -1, nullptr, &raw) == 0 && raw) {
        int stride = (desc.stride > 0) ? static_cast<int>(desc.stride) : w;
        out->width = w;
        out->height = h;
        out->y_plane.resize(static_cast<size_t>(w) * h);
        int cw = w / 2;
        int ch = h / 2;
        out->u_plane.resize(static_cast<size_t>(cw) * ch);
        out->v_plane.resize(static_cast<size_t>(cw) * ch);

        const auto* base = static_cast<const uint8_t*>(raw);
        for (int r = 0; r < h; ++r) {
            std::memcpy(out->y_plane.data() + static_cast<size_t>(r) * w,
                        base + static_cast<size_t>(r) * stride, static_cast<size_t>(w));
        }
        const uint8_t* uvBase = base + static_cast<size_t>(stride) * h;
        for (int r = 0; r < ch; ++r) {
            const uint8_t* uvRow = uvBase + static_cast<size_t>(r) * stride;
            for (int c = 0; c < cw; ++c) {
                out->u_plane[static_cast<size_t>(r) * cw + c] = uvRow[c * 2];
                out->v_plane[static_cast<size_t>(r) * cw + c] = uvRow[c * 2 + 1];
            }
        }
        AHardwareBuffer_unlock(ahb, nullptr);
        return true;
    }
    return false;
}

// ============================================================================
// Minimal, dependency-free Baseline JFIF JPEG Encoder (YCbCr 4:2:0, Quality ~92)
// Supports on-the-fly 0/90/180/270 degree rotation and horizontal mirror.
// ============================================================================
namespace jpeg {

constexpr uint8_t kZigZag[64] = {
    0,  1,  5,  6,  14, 15, 27, 28,
    2,  4,  7,  13, 16, 26, 29, 42,
    3,  8,  12, 17, 25, 30, 41, 43,
    9,  11, 18, 24, 31, 40, 44, 53,
    10, 19, 23, 32, 39, 45, 52, 54,
    20, 22, 33, 38, 46, 51, 55, 60,
    21, 34, 37, 47, 50, 56, 59, 61,
    35, 36, 48, 49, 57, 58, 62, 63
};

constexpr uint8_t kStdLumQuant[64] = {
    16, 11, 10, 16, 24,  40,  51,  61,
    12, 12, 14, 19, 26,  58,  60,  55,
    14, 13, 16, 24, 40,  57,  69,  56,
    14, 17, 22, 29, 51,  87,  80,  62,
    18, 22, 37, 56, 68,  109, 103, 77,
    24, 35, 55, 64, 81,  104, 113, 92,
    49, 64, 78, 87, 103, 121, 120, 101,
    72, 92, 95, 98, 112, 100, 103, 99
};

constexpr uint8_t kStdChromQuant[64] = {
    17, 18, 24, 47, 99, 99, 99, 99,
    18, 21, 26, 66, 99, 99, 99, 99,
    24, 26, 56, 99, 99, 99, 99, 99,
    47, 66, 99, 99, 99, 99, 99, 99,
    99, 99, 99, 99, 99, 99, 99, 99,
    99, 99, 99, 99, 99, 99, 99, 99,
    99, 99, 99, 99, 99, 99, 99, 99,
    99, 99, 99, 99, 99, 99, 99, 99
};

constexpr uint8_t kBitsDcLum[17] = {0, 0, 1, 5, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0};
constexpr uint8_t kValDcLum[12]  = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11};

constexpr uint8_t kBitsDcChrom[17] = {0, 0, 3, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0};
constexpr uint8_t kValDcChrom[12]  = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11};

constexpr uint8_t kBitsAcLum[17] = {0, 0, 2, 1, 3, 3, 2, 4, 3, 5, 5, 4, 4, 0, 0, 1, 0x7d};
constexpr uint8_t kValAcLum[162] = {
    0x01, 0x02, 0x03, 0x00, 0x04, 0x11, 0x05, 0x12, 0x21, 0x31, 0x41, 0x06, 0x13, 0x51, 0x61, 0x07,
    0x22, 0x71, 0x14, 0x32, 0x81, 0x91, 0xa1, 0x08, 0x23, 0x42, 0xb1, 0xc1, 0x15, 0x52, 0xd1, 0xf0,
    0x24, 0x33, 0x62, 0x72, 0x82, 0x09, 0x0a, 0x16, 0x17, 0x18, 0x19, 0x1a, 0x25, 0x26, 0x27, 0x28,
    0x29, 0x2a, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39, 0x3a, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48, 0x49,
    0x4a, 0x53, 0x54, 0x55, 0x56, 0x57, 0x58, 0x59, 0x5a, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68, 0x69,
    0x6a, 0x73, 0x74, 0x75, 0x76, 0x77, 0x78, 0x79, 0x7a, 0x83, 0x84, 0x85, 0x86, 0x87, 0x88, 0x89,
    0x8a, 0x92, 0x93, 0x94, 0x95, 0x96, 0x97, 0x98, 0x99, 0x9a, 0xa2, 0xa3, 0xa4, 0xa5, 0xa6, 0xa7,
    0xa8, 0xa9, 0xaa, 0xb2, 0xb3, 0xb4, 0xb5, 0xb6, 0xb7, 0xb8, 0xb9, 0xba, 0xc2, 0xc3, 0xc4, 0xc5,
    0xc6, 0xc7, 0xc8, 0xc9, 0xca, 0xd2, 0xd3, 0xd4, 0xd5, 0xd6, 0xd7, 0xd8, 0xd9, 0xda, 0xe1, 0xe2,
    0xe3, 0xe4, 0xe5, 0xe6, 0xe7, 0xe8, 0xe9, 0xea, 0xf1, 0xf2, 0xf3, 0xf4, 0xf5, 0xf6, 0xf7, 0xf8,
    0xf9, 0xfa
};

constexpr uint8_t kBitsAcChrom[17] = {0, 0, 2, 1, 2, 4, 4, 3, 4, 7, 5, 4, 4, 0, 1, 2, 0x77};
constexpr uint8_t kValAcChrom[162] = {
    0x00, 0x01, 0x02, 0x03, 0x11, 0x04, 0x05, 0x21, 0x31, 0x06, 0x12, 0x41, 0x51, 0x07, 0x61, 0x71,
    0x13, 0x22, 0x32, 0x81, 0x08, 0x14, 0x42, 0x91, 0xa1, 0xb1, 0xc1, 0x09, 0x23, 0x33, 0x52, 0xf0,
    0x15, 0x62, 0x72, 0xd1, 0x0a, 0x16, 0x24, 0x34, 0xe1, 0x25, 0xf1, 0x17, 0x18, 0x19, 0x1a, 0x26,
    0x27, 0x28, 0x29, 0x2a, 0x35, 0x36, 0x37, 0x38, 0x39, 0x3a, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48,
    0x49, 0x4a, 0x53, 0x54, 0x55, 0x56, 0x57, 0x58, 0x59, 0x5a, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68,
    0x69, 0x6a, 0x73, 0x74, 0x75, 0x76, 0x77, 0x78, 0x79, 0x7a, 0x82, 0x83, 0x84, 0x85, 0x86, 0x87,
    0x88, 0x89, 0x8a, 0x92, 0x93, 0x94, 0x95, 0x96, 0x97, 0x98, 0x99, 0x9a, 0xa2, 0xa3, 0xa4, 0xa5,
    0xa6, 0xa7, 0xa8, 0xa9, 0xaa, 0xb2, 0xb3, 0xb4, 0xb5, 0xb6, 0xb7, 0xb8, 0xb9, 0xba, 0xc2, 0xc3,
    0xc4, 0xc5, 0xc6, 0xc7, 0xc8, 0xc9, 0xca, 0xd2, 0xd3, 0xd4, 0xd5, 0xd6, 0xd7, 0xd8, 0xd9, 0xda,
    0xe2, 0xe3, 0xe4, 0xe5, 0xe6, 0xe7, 0xe8, 0xe9, 0xea, 0xf2, 0xf3, 0xf4, 0xf5, 0xf6, 0xf7, 0xf8,
    0xf9, 0xfa
};

struct HuffCode {
    uint16_t code = 0;
    uint8_t len = 0;
};

void buildHuffTable(const uint8_t bits[17], const uint8_t* vals, HuffCode out[256]) {
    std::memset(out, 0, sizeof(HuffCode) * 256);
    uint16_t code = 0;
    int k = 0;
    for (int len = 1; len <= 16; ++len) {
        for (int i = 0; i < bits[len]; ++i) {
            uint8_t sym = vals[k++];
            out[sym].code = code++;
            out[sym].len = static_cast<uint8_t>(len);
        }
        code <<= 1;
    }
}

// Fast AAN 2D FDCT with integrated quantization scaling.
void fdct8x8(float block[64]) {
    for (int i = 0; i < 8; ++i) {
        float* row = block + i * 8;
        float tmp0 = row[0] + row[7];
        float tmp7 = row[0] - row[7];
        float tmp1 = row[1] + row[6];
        float tmp6 = row[1] - row[6];
        float tmp2 = row[2] + row[5];
        float tmp5 = row[2] - row[5];
        float tmp3 = row[3] + row[4];
        float tmp4 = row[3] - row[4];

        float tmp10 = tmp0 + tmp3;
        float tmp13 = tmp0 - tmp3;
        float tmp11 = tmp1 + tmp2;
        float tmp12 = tmp1 - tmp2;

        row[0] = tmp10 + tmp11;
        row[4] = tmp10 - tmp11;

        float z1 = (tmp12 + tmp13) * 0.707106781f;
        row[2] = tmp13 + z1;
        row[6] = tmp13 - z1;

        tmp10 = tmp4 + tmp5;
        tmp11 = tmp5 + tmp6;
        tmp12 = tmp6 + tmp7;

        float z5 = (tmp10 - tmp12) * 0.382683433f;
        float z2 = 0.541196100f * tmp10 + z5;
        float z4 = 1.306562965f * tmp12 + z5;
        float z3 = tmp11 * 0.707106781f;

        float z11 = tmp7 + z3;
        float z13 = tmp7 - z3;

        row[5] = z13 + z2;
        row[3] = z13 - z2;
        row[1] = z11 + z4;
        row[7] = z11 - z4;
    }

    for (int i = 0; i < 8; ++i) {
        float* col = block + i;
        float tmp0 = col[0 * 8] + col[7 * 8];
        float tmp7 = col[0 * 8] - col[7 * 8];
        float tmp1 = col[1 * 8] + col[6 * 8];
        float tmp6 = col[1 * 8] - col[6 * 8];
        float tmp2 = col[2 * 8] + col[5 * 8];
        float tmp5 = col[2 * 8] - col[5 * 8];
        float tmp3 = col[3 * 8] + col[4 * 8];
        float tmp4 = col[3 * 8] - col[4 * 8];

        float tmp10 = tmp0 + tmp3;
        float tmp13 = tmp0 - tmp3;
        float tmp11 = tmp1 + tmp2;
        float tmp12 = tmp1 - tmp2;

        col[0 * 8] = tmp10 + tmp11;
        col[4 * 8] = tmp10 - tmp11;

        float z1 = (tmp12 + tmp13) * 0.707106781f;
        col[2 * 8] = tmp13 + z1;
        col[6 * 8] = tmp13 - z1;

        tmp10 = tmp4 + tmp5;
        tmp11 = tmp5 + tmp6;
        tmp12 = tmp6 + tmp7;

        float z5 = (tmp10 - tmp12) * 0.382683433f;
        float z2 = 0.541196100f * tmp10 + z5;
        float z4 = 1.306562965f * tmp12 + z5;
        float z3 = tmp11 * 0.707106781f;

        float z11 = tmp7 + z3;
        float z13 = tmp7 - z3;

        col[5 * 8] = z13 + z2;
        col[3 * 8] = z13 - z2;
        col[1 * 8] = z11 + z4;
        col[7 * 8] = z11 - z4;
    }
}

class BitWriter {
public:
    explicit BitWriter(std::vector<uint8_t>& out) : out_(out) {}

    void putByte(uint8_t b) { out_.push_back(b); }
    void putWord(uint16_t w) {
        out_.push_back(static_cast<uint8_t>(w >> 8));
        out_.push_back(static_cast<uint8_t>(w & 0xff));
    }

    void putBits(uint16_t bits, uint8_t len) {
        if (len == 0) return;
        bitBuf_ = (bitBuf_ << len) | (bits & ((1u << len) - 1u));
        bitCount_ += len;
        while (bitCount_ >= 8) {
            bitCount_ -= 8;
            uint8_t b = static_cast<uint8_t>((bitBuf_ >> bitCount_) & 0xffu);
            out_.push_back(b);
            if (b == 0xff) out_.push_back(0x00);
        }
    }

    void flushBits() {
        if (bitCount_ > 0) {
            putBits(0x7f, static_cast<uint8_t>(8 - bitCount_));
            bitCount_ = 0;
            bitBuf_ = 0;
        }
    }

private:
    std::vector<uint8_t>& out_;
    uint32_t bitBuf_ = 0;
    int bitCount_ = 0;
};

inline int bitCategory(int v) {
    int a = (v < 0) ? -v : v;
    int n = 0;
    while (a > 0) {
        a >>= 1;
        ++n;
    }
    return n;
}

void encodeBlock(BitWriter& bw, float block[64], const float fdtbl[64], int& lastDC,
                 const HuffCode dcHuff[256], const HuffCode acHuff[256]) {
    fdct8x8(block);
    int zz[64];
    for (int i = 0; i < 64; ++i) {
        float val = block[i] * fdtbl[i];
        zz[kZigZag[i]] = static_cast<int>(std::lrintf(val));
    }

    int diff = zz[0] - lastDC;
    lastDC = zz[0];
    if (diff == 0) {
        bw.putBits(dcHuff[0].code, dcHuff[0].len);
    } else {
        int cat = bitCategory(diff);
        bw.putBits(dcHuff[cat].code, dcHuff[cat].len);
        uint16_t mag = static_cast<uint16_t>((diff > 0) ? diff : (diff + (1 << cat) - 1));
        bw.putBits(mag, static_cast<uint8_t>(cat));
    }

    int end0pos = 63;
    while (end0pos > 0 && zz[end0pos] == 0) --end0pos;
    if (end0pos == 0) {
        bw.putBits(acHuff[0x00].code, acHuff[0x00].len);
        return;
    }

    for (int i = 1; i <= end0pos; ++i) {
        int startpos = i;
        while (zz[i] == 0 && i <= end0pos) ++i;
        int nrzeroes = i - startpos;
        while (nrzeroes >= 16) {
            bw.putBits(acHuff[0xf0].code, acHuff[0xf0].len);
            nrzeroes -= 16;
        }
        int val = zz[i];
        int cat = bitCategory(val);
        uint8_t sym = static_cast<uint8_t>((nrzeroes << 4) | cat);
        bw.putBits(acHuff[sym].code, acHuff[sym].len);
        uint16_t mag = static_cast<uint16_t>((val > 0) ? val : (val + (1 << cat) - 1));
        bw.putBits(mag, static_cast<uint8_t>(cat));
    }
    if (end0pos < 63) {
        bw.putBits(acHuff[0x00].code, acHuff[0x00].len);
    }
}

// Map output pixel (ox, oy) in rotated/mirrored image back to source (sx, sy).
inline void mapToSource(int ox, int oy, int srcW, int srcH, int rot, bool mirror, int& sx, int& sy) {
    int outW = (rot == 90 || rot == 270) ? srcH : srcW;
    int mx = mirror ? (outW - 1 - ox) : ox;
    switch (rot) {
        case 90:
            sx = oy;
            sy = srcH - 1 - mx;
            break;
        case 180:
            sx = srcW - 1 - mx;
            sy = srcH - 1 - oy;
            break;
        case 270:
            sx = srcW - 1 - oy;
            sy = mx;
            break;
        default:
            sx = mx;
            sy = oy;
            break;
    }
    if (sx < 0) sx = 0;
    else if (sx >= srcW) sx = srcW - 1;
    if (sy < 0) sy = 0;
    else if (sy >= srcH) sy = srcH - 1;
}

std::vector<uint8_t> encodeI420ToJpeg(const CachedFrame& frame, int quality, int rot, bool mirror,
                                      int* outWidth, int* outHeight) {
    int srcW = frame.width;
    int srcH = frame.height;
    rot = ((rot % 360) + 360) % 360;
    if (rot != 90 && rot != 180 && rot != 270) rot = 0;

    int dstW = (rot == 90 || rot == 270) ? srcH : srcW;
    int dstH = (rot == 90 || rot == 270) ? srcW : srcH;
    if (outWidth) *outWidth = dstW;
    if (outHeight) *outHeight = dstH;

    int q = std::clamp(quality, 1, 100);
    int scale = (q < 50) ? (5000 / q) : (200 - q * 2);

    uint8_t qLum[64], qChrom[64];
    for (int i = 0; i < 64; ++i) {
        int l = (static_cast<int>(kStdLumQuant[i]) * scale + 50) / 100;
        int c = (static_cast<int>(kStdChromQuant[i]) * scale + 50) / 100;
        qLum[kZigZag[i]] = static_cast<uint8_t>(std::clamp(l, 1, 255));
        qChrom[kZigZag[i]] = static_cast<uint8_t>(std::clamp(c, 1, 255));
    }

    constexpr float kAanScale[8] = {
        1.0f, 1.387039845f, 1.306562965f, 1.175875602f,
        1.0f, 0.785694958f, 0.541196100f, 0.275899379f
    };
    float fdLum[64], fdChrom[64];
    for (int r = 0; r < 8; ++r) {
        for (int c = 0; c < 8; ++c) {
            int idx = r * 8 + c;
            float denom = kAanScale[r] * kAanScale[c] * 8.0f;
            fdLum[idx] = 1.0f / (static_cast<float>(qLum[kZigZag[idx]]) * denom);
            fdChrom[idx] = 1.0f / (static_cast<float>(qChrom[kZigZag[idx]]) * denom);
        }
    }

    HuffCode dcLum[256], acLum[256], dcChrom[256], acChrom[256];
    buildHuffTable(kBitsDcLum, kValDcLum, dcLum);
    buildHuffTable(kBitsAcLum, kValAcLum, acLum);
    buildHuffTable(kBitsDcChrom, kValDcChrom, dcChrom);
    buildHuffTable(kBitsAcChrom, kValAcChrom, acChrom);

    std::vector<uint8_t> out;
    out.reserve(static_cast<size_t>(dstW) * dstH / 4 + 1024);
    BitWriter bw(out);

    // SOI + APP0 (JFIF 1.01)
    bw.putWord(0xFFD8);
    bw.putWord(0xFFE0);
    bw.putWord(16);
    const uint8_t jfif[14] = {'J', 'F', 'I', 'F', 0, 1, 1, 0, 0, 1, 0, 1, 0, 0};
    for (uint8_t b : jfif) bw.putByte(b);

    // DQT (Luminance = 0, Chrominance = 1)
    bw.putWord(0xFFDB);
    bw.putWord(2 + 65 + 65);
    bw.putByte(0x00);
    for (int i = 0; i < 64; ++i) bw.putByte(qLum[i]);
    bw.putByte(0x01);
    for (int i = 0; i < 64; ++i) bw.putByte(qChrom[i]);

    // SOF0 (Baseline DCT, YCbCr 4:2:0)
    bw.putWord(0xFFC0);
    bw.putWord(17);
    bw.putByte(8);
    bw.putWord(static_cast<uint16_t>(dstH));
    bw.putWord(static_cast<uint16_t>(dstW));
    bw.putByte(3);
    bw.putByte(1); bw.putByte(0x22); bw.putByte(0);  // Y: 2x2, Q=0
    bw.putByte(2); bw.putByte(0x11); bw.putByte(1);  // Cb: 1x1, Q=1
    bw.putByte(3); bw.putByte(0x11); bw.putByte(1);  // Cr: 1x1, Q=1

    // DHT (4 tables combined)
    bw.putWord(0xFFC4);
    bw.putWord(2 + (1 + 16 + 12) + (1 + 16 + 162) + (1 + 16 + 12) + (1 + 16 + 162));
    auto writeHuff = [&](uint8_t tcTh, const uint8_t bits[17], const uint8_t* vals, int nvals) {
        bw.putByte(tcTh);
        for (int i = 1; i <= 16; ++i) bw.putByte(bits[i]);
        for (int i = 0; i < nvals; ++i) bw.putByte(vals[i]);
    };
    writeHuff(0x00, kBitsDcLum, kValDcLum, 12);
    writeHuff(0x10, kBitsAcLum, kValAcLum, 162);
    writeHuff(0x01, kBitsDcChrom, kValDcChrom, 12);
    writeHuff(0x11, kBitsAcChrom, kValAcChrom, 162);

    // SOS
    bw.putWord(0xFFDA);
    bw.putWord(12);
    bw.putByte(3);
    bw.putByte(1); bw.putByte(0x00);
    bw.putByte(2); bw.putByte(0x11);
    bw.putByte(3); bw.putByte(0x11);
    bw.putByte(0);
    bw.putByte(63);
    bw.putByte(0);

    int dcY = 0, dcU = 0, dcV = 0;
    int srcCW = srcW / 2;
    const uint8_t* yData = frame.y_plane.data();
    const uint8_t* uData = frame.u_plane.data();
    const uint8_t* vData = frame.v_plane.data();

    float blkY00[64], blkY01[64], blkY10[64], blkY11[64], blkU[64], blkV[64];

    for (int mcuY = 0; mcuY < dstH; mcuY += 16) {
        for (int mcuX = 0; mcuX < dstW; mcuX += 16) {
            for (int r = 0; r < 16; ++r) {
                int oy = std::min(mcuY + r, dstH - 1);
                for (int c = 0; c < 16; ++c) {
                    int ox = std::min(mcuX + c, dstW - 1);
                    int sx, sy;
                    mapToSource(ox, oy, srcW, srcH, rot, mirror, sx, sy);
                    float yVal = static_cast<float>(yData[static_cast<size_t>(sy) * srcW + sx]) - 128.0f;
                    int subIdx = (r & 7) * 8 + (c & 7);
                    if (r < 8) {
                        if (c < 8) blkY00[subIdx] = yVal;
                        else       blkY01[subIdx] = yVal;
                    } else {
                        if (c < 8) blkY10[subIdx] = yVal;
                        else       blkY11[subIdx] = yVal;
                    }
                    if ((r & 1) == 0 && (c & 1) == 0) {
                        int cIdx = (r >> 1) * 8 + (c >> 1);
                        size_t uvOff = static_cast<size_t>(sy >> 1) * srcCW + (sx >> 1);
                        blkU[cIdx] = static_cast<float>(uData[uvOff]) - 128.0f;
                        blkV[cIdx] = static_cast<float>(vData[uvOff]) - 128.0f;
                    }
                }
            }
            encodeBlock(bw, blkY00, fdLum, dcY, dcLum, acLum);
            encodeBlock(bw, blkY01, fdLum, dcY, dcLum, acLum);
            encodeBlock(bw, blkY10, fdLum, dcY, dcLum, acLum);
            encodeBlock(bw, blkY11, fdLum, dcY, dcLum, acLum);
            encodeBlock(bw, blkU, fdChrom, dcU, dcChrom, acChrom);
            encodeBlock(bw, blkV, fdChrom, dcV, dcChrom, acChrom);
        }
    }

    bw.flushBits();
    bw.putWord(0xFFD9);
    return out;
}

}  // namespace jpeg

int handlePreviewDecision(DataMap* data, DataMap* reply) {
    auto kv = parseKvPairs(data, "previewdecision_params_STRING_BASIC_ARRAYVALUE_");
    std::string camId = kv.count("camera_id") ? kv["camera_id"] : "0";
    std::string capMode = kv.count("capture_mode") ? kv["capture_mode"] : "common";
    if (camId.empty()) camId = "0";
    if (capMode.empty()) capMode = "common";

    {
        std::lock_guard<std::mutex> lk(g_frames_mu);
        g_last_camera_id = camId;
        g_last_capture_mode = capMode;
    }

    int seq = g_seq_id.fetch_add(1, std::memory_order_relaxed);
    std::string seqStr = std::to_string(seq);

    std::vector<std::string> res = {
        "previewdecision_cameraid", camId,
        "previewdecision_capture_mode", capMode,
        "previewdecision_request_format", "35",
        "previewdecision_request_format_list", "35",
        "previewdecision_multiframecnt", "1",
        "previewdecision_thumbnail_index", "1",
        "previewdecision_meta_index", "0",
        "previewdecision_supernight_scene", "0",
        "previewdecision_turboraw_scene", "0",
        "previewdecision_motion_capture_enable", "0",
        "previewdecision_ancraw_scene", "0",
        "previewdecision_handshake_state", "0",
        "previewdecision_night_total_exptime", "0",
        "previewdecision_turbo_fusion_night_total_exptime", "0",
        "previewdecision_turbo_fusion_night_shutter_wait_time", "0",
        "previewdecision_scene_mode", "0",
        "previewdecision_feature_type", "0",
        "previewdecision_algolist", "none",
        "previewdecision_evlist", "0",
        "previewdecision_pre_evlist", "0",
        "doc_vertices", "0,0,0,0,0,0,0,0",
        "rectified_size", "0,0",
        "doc_edge_color", "0",
        "previewdecision_etlist", "0",
        "previewdecision_flash_type_list", "0",
        "previewdecision_capture_need_flash", "0",
        "previewdecision_naturetone_state", "0",
        "previewdecision_sensormask", "0,1,0,0",
        "previewdecision_master_pipeline", "1",
        "resultinfo_frame_id", seqStr,
        "previewdecision_bracket_mode", "0",
        "previewdecision_mfsr_frame_count", "1",
        "previewdecision_sequenceid", seqStr,
        "previewdecision_precollect_frame_count", "0",
        "previewdecision_specificprocessalgo", "0",
        "previewdecision_zoom_feature", "0",
        "previewdecision_previewzoom_feature", "0",
        "drawtext_bracket_mode", "",
        "drawtext_scene_mode", "",
        "drawtext_feature_type", "",
        "drawtext_hdr_type", "",
        "preview_asd", "0",
        "preview_antibanding", "0",
        "preview_rectify", "0",
        "preview_pf", "0",
        "preview_lsd", "0",
        "preview_rtb", "0",
        "preview_aicolor", "0",
        "preview_video_hyperlapse", "0",
        "algo_visualization_key", "",
        "algo_visualization_value", "",
        "previewdecision_exposure_time_from_app", "0",
        "previewdecision_burstalgolist", "none",
        "previewdecision_asd_moving_object", "0",
        "previewdecision_capture_intent", "2",
        "previewdecision_show_hdr_icon", "0",
        "previewdecision_capture_interval", "0",
        "previewdecision_capture_surface", "0",
        "previewdecision_quick_jpeg", "0",
        "previewdecision_tr_bypass_cmc", "0",
        "previewdecision_interpolate_frame_camera_mode", "0",
        "previewdecision_portrait_hdr_type", "0",
        "previewdecision_asd_msg_scene_value", "0",
        "previewdecision_aishutter_enable", "false",
        "previewdecision_request_mixed_format", "false",
        "previewdecision_hdr_brighten_index", "0",
        "previewdecision_capture_noneed_match_meta", "0",
    };
    (*reply)["previewdecision_result_STRING_BASIC_ARRAYVALUE_"] = std::move(res);
    return 0;
}

int handleAddFrameBuff(DataMap* data, DataMap* reply) {
    (*reply)["addframebuff_result_INT_BASIC_"] = {"0"};
    auto actionCb = g_action_cb.load(std::memory_order_acquire);
    auto transactCb = g_transact_cb.load(std::memory_order_acquire);
    std::string envStr = getFirst(data, "jni_callback_env");
    if (!actionCb || envStr.empty()) {
        ALOGW("addFrameBuff: missing actionCb or jni_callback_env");
        return 0;
    }

    auto kv = parseKvPairs(data, "addframebuff_params_STRING_BASIC_ARRAYVALUE_");
    std::string identity = kv.count("identity") ? kv["identity"] :
                           (kv.count("time_stamp") ? kv["time_stamp"] : "0");
    std::string frameNum = kv.count("frame_number") ? kv["frame_number"] : "0";
    int inW = parseIntOr(kv.count("buffer_input_width") ? kv["buffer_input_width"] : "", 0);
    int inH = parseIntOr(kv.count("buffer_input_height") ? kv["buffer_input_height"] : "", 0);
    int rot = parseIntOr(kv.count("jpeg_orientation") ? kv["jpeg_orientation"] :
                         (kv.count("orientation") ? kv["orientation"] :
                         (kv.count("rotation") ? kv["rotation"] : "")), 0);
    std::string mirrorVal = kv.count("mirror_enable") ? kv["mirror_enable"] :
                            (kv.count("mirror") ? kv["mirror"] : "");
    bool mirror = (mirrorVal == "1" || mirrorVal == "true" || mirrorVal == "on");

    std::string hwBufObj = getFirst(data, "addframebuff_hardwarebufferobj_HARDWAREBUFFER_OBJECT_");
    bool extracted = false;
    CachedFrame frame;
    frame.identity = identity;
    frame.frame_number = frameNum;
    frame.rotation = rot;
    frame.mirror = mirror;

    if (!hwBufObj.empty()) {
        DataMap req, rep;
        req["jni_callback_env"] = {envStr};
        req["jni_data_hardwarebuffer_object"] = {hwBufObj};
        actionCb(/*actionGetHardwareBufferPtr=*/5, &req, &rep);
        std::string ptrStr = getFirst(&rep, "jni_reply_hardwarebuffer_ptr");
        long ptrVal = ptrStr.empty() ? 0 : std::strtol(ptrStr.c_str(), nullptr, 10);
        if (ptrVal != 0) {
            auto* ahb = reinterpret_cast<AHardwareBuffer*>(ptrVal);
            extracted = extractI420FromHardwareBuffer(ahb, inW, inH, &frame);
        }
    }

    if (!extracted) {
        int w = (inW > 0) ? (inW & ~1) : 1280;
        int h = (inH > 0) ? (inH & ~1) : 960;
        frame.width = w;
        frame.height = h;
        frame.y_plane.assign(static_cast<size_t>(w) * h, 128);
        frame.u_plane.assign(static_cast<size_t>(w / 2) * (h / 2), 128);
        frame.v_plane.assign(static_cast<size_t>(w / 2) * (h / 2), 128);
        ALOGW("addFrameBuff: fallback neutral frame %dx%d identity=%s", w, h, identity.c_str());
    } else {
        ALOGI("addFrameBuff: captured I420 %dx%d identity=%s frame=%s rot=%d mirror=%d",
              frame.width, frame.height, identity.c_str(), frameNum.c_str(), rot, mirror ? 1 : 0);
    }

    {
        std::lock_guard<std::mutex> lk(g_frames_mu);
        frame.camera_id = kv.count("camera_id") ? kv["camera_id"] : g_last_camera_id;
        frame.capture_mode = kv.count("capture_mode") ? kv["capture_mode"] : g_last_capture_mode;
        g_frames.push_back(std::move(frame));
        while (g_frames.size() > 16) {
            g_frames.pop_front();
        }
    }

    // Immediately close Java ApsResult$ImageBuffer so ImageReader buffer slot is recycled!
    std::string imgBufObj = getFirst(data, "addframebuff_imagebufferobj_IMAGEBUFFER_OBJECT_");
    if (transactCb && !imgBufObj.empty() && imgBufObj != "0") {
        DataMap closeData, closeReply;
        closeData["jni_callback_env"] = {envStr};
        closeData["close_object_IMAGEBUFFER_OBJECT_"] = {imgBufObj};
        transactCb(/*TRANSACTION_CLOSE_OBJECT=*/5, closeData, closeReply);
    }

    return 0;
}

int handleProcessImages(DataMap* data, DataMap* reply) {
    (*reply)["processimages_result_INT_BASIC_"] = {"0"};
    auto actionCb = g_action_cb.load(std::memory_order_acquire);
    auto transactCb = g_transact_cb.load(std::memory_order_acquire);
    std::string envStr = getFirst(data, "jni_callback_env");
    if (!actionCb || !transactCb || envStr.empty()) {
        ALOGE("processImages: missing callbacks or jni_callback_env");
        return 0;
    }

    auto kv = parseKvPairs(data, "processimages_params_STRING_BASIC_ARRAYVALUE_");
    std::string reqIdentity = kv.count("identity") ? kv["identity"] :
                              (kv.count("time_stamp") ? kv["time_stamp"] : "");

    CachedFrame frame;
    bool found = false;
    {
        std::lock_guard<std::mutex> lk(g_frames_mu);
        if (!reqIdentity.empty()) {
            for (auto it = g_frames.begin(); it != g_frames.end(); ++it) {
                if (it->identity == reqIdentity) {
                    frame = std::move(*it);
                    g_frames.erase(it);
                    found = true;
                    break;
                }
            }
        }
        if (!found && !g_frames.empty()) {
            frame = std::move(g_frames.front());
            g_frames.pop_front();
            found = true;
        }
    }

    if (!found) {
        ALOGW("processImages: no cached frame for identity=%s, synthesizing 1280x960", reqIdentity.c_str());
        frame.identity = reqIdentity.empty() ? "0" : reqIdentity;
        frame.frame_number = "0";
        frame.camera_id = "0";
        frame.capture_mode = "common";
        frame.width = 1280;
        frame.height = 960;
        frame.y_plane.assign(1280 * 960, 128);
        frame.u_plane.assign(640 * 480, 128);
        frame.v_plane.assign(640 * 480, 128);
    }

    if (!reqIdentity.empty()) {
        frame.identity = reqIdentity;
    }

    int rot = frame.rotation;
    for (const char* rk : {"jpeg_orientation", "orientation", "rotation"}) {
        if (kv.count(rk) && !kv[rk].empty()) {
            rot = parseIntOr(kv[rk], rot);
            break;
        }
    }
    bool mirror = frame.mirror;
    for (const char* mk : {"mirror_enable", "mirror", "flip"}) {
        if (kv.count(mk) && !kv[mk].empty()) {
            const std::string& mv = kv[mk];
            mirror = (mv == "1" || mv == "true" || mv == "enable" || mv == "on");
            break;
        }
    }

    auto kvOr = [&](const char* k, const char* def) -> std::string {
        auto it = kv.find(k);
        return (it != kv.end() && !it->second.empty()) ? it->second : std::string(def);
    };
    ALOGI("processImages kv: id=%s title=%s jobType=%s deferGrp=%s hwBufCb=%s deferSup=%s heif=%s pipe=%s ori=%s cam=%s mode=%s totalKv=%zu",
          reqIdentity.c_str(),
          kvOr("picture_title", "").c_str(),
          kvOr("capture_job_type", "").c_str(),
          kvOr("defer_job_group_id", "").c_str(),
          kvOr("jpeg_callback_hardware_buf", "").c_str(),
          kvOr("capture_defer_support", "").c_str(),
          kvOr("heif_encode_in_aps", "").c_str(),
          kvOr("pipeline", kvOr("pipeline_name", "").c_str()).c_str(),
          kvOr("jpeg_orientation", kvOr("orientation", "").c_str()).c_str(),
          kvOr("camera_id", "").c_str(),
          kvOr("capture_mode", "").c_str(),
          kv.size());

    int outW = frame.width;
    int outH = frame.height;
    std::vector<uint8_t> jpegBytes = jpeg::encodeI420ToJpeg(frame, 92, rot, mirror, &outW, &outH);
    ALOGI("processImages: encoded JPEG %dx%d (%zu bytes) identity=%s rot=%d mirror=%d",
          outW, outH, jpegBytes.size(), frame.identity.c_str(), rot, mirror ? 1 : 0);

    DataMap arrReq, arrRep;
    arrReq["jni_callback_env"] = {envStr};
    arrReq["jni_data_array"] = {std::to_string(reinterpret_cast<long>(jpegBytes.data()))};
    arrReq["jni_data_type"] = {"_BYTE_BASIC_ARRAY_"};
    arrReq["jni_data_array_size"] = {std::to_string(static_cast<int>(jpegBytes.size()))};
    actionCb(/*actionNewArray=*/13, &arrReq, &arrRep);

    std::string jByteArrRef = getFirst(&arrRep, "jni_reply_array_object");
    if (jByteArrRef.empty() || jByteArrRef == "0") {
        ALOGE("processImages: actionNewArray failed");
        return 0;
    }

    std::string wStr = std::to_string(outW);
    std::string hStr = std::to_string(outH);
    std::string capMode = kv.count("capture_mode") ? kv["capture_mode"] :
                          (frame.capture_mode.empty() ? "common" : frame.capture_mode);
    std::string pipeName = kvOr("pipeline", kvOr("pipeline_name", "pipeline_default").c_str());
    std::string jobType = kvOr("capture_job_type", "0");
    std::string picTitle = kvOr("picture_title", "");
    std::string deferGrp = kvOr("defer_job_group_id", "");
    std::string hwBufCb = kvOr("jpeg_callback_hardware_buf", "");

    // When OplusCamera sets jpeg_callback_hardware_buf=true, it reads the JPEG
    // bytes from ApsResult.getHwBuffer() (BLOB HardwareBuffer) + mDataSize.
    AHardwareBuffer* jpegAhb = nullptr;
    if (hwBufCb == "true" || hwBufCb == "1") {
        AHardwareBuffer_Desc desc{};
        desc.width = static_cast<uint32_t>(jpegBytes.size());
        desc.height = 1;
        desc.layers = 1;
        desc.format = AHARDWAREBUFFER_FORMAT_BLOB;
        desc.usage = AHARDWAREBUFFER_USAGE_CPU_READ_OFTEN | AHARDWAREBUFFER_USAGE_CPU_WRITE_OFTEN;
        int allocRc = AHardwareBuffer_allocate(&desc, &jpegAhb);
        if (allocRc == 0 && jpegAhb != nullptr) {
            void* dst = nullptr;
            if (AHardwareBuffer_lock(jpegAhb, AHARDWAREBUFFER_USAGE_CPU_WRITE_OFTEN, -1, nullptr, &dst) == 0 && dst != nullptr) {
                std::memcpy(dst, jpegBytes.data(), jpegBytes.size());
                AHardwareBuffer_unlock(jpegAhb, nullptr);
            } else {
                ALOGE("processImages: AHardwareBuffer_lock failed for JPEG blob");
                AHardwareBuffer_release(jpegAhb);
                jpegAhb = nullptr;
            }
        } else {
            ALOGE("processImages: AHardwareBuffer_allocate(%zu) failed rc=%d", jpegBytes.size(), allocRc);
            jpegAhb = nullptr;
        }
    }

    DataMap cbData, cbReply;
    cbData["jni_callback_env"] = {envStr};
    // Note: convertNMap2JMap automatically deletes the GlobalRef for _BASIC_ARRAY_ keys!
    cbData["bufferPlanesData_BYTE_BASIC_ARRAY_"] = {jByteArrRef};
    if (jpegAhb != nullptr) {
        cbData["hwBuffer_HARDWAREBUFFER_OBJECT_"] = {std::to_string(reinterpret_cast<long>(jpegAhb))};
    }
    cbData["dataSize_LONG_BASIC_"] = {std::to_string(static_cast<long>(jpegBytes.size()))};
    cbData["exifData_needExifData_BOOLEAN_BASIC_"] = {"0"};
    cbData["extendInAps_BOOLEAN_BASIC_"] = {"0"};
    cbData["exifTagFlags_INT_BASIC_"] = {"0"};
    cbData["exifData_flashValue_INT_BASIC_"] = {"0"};
    cbData["exifData_exifDateTime_STRING_BASIC_"] = {kvOr("picture_date_time", "")};
    cbData["exifData_imageWidth_INT_BASIC_"] = {wStr};
    cbData["exifData_imageHeight_INT_BASIC_"] = {hStr};

    std::vector<std::string> resVec = {
        "result_using_string", "1",
        "resultinfo_message_type", "1",
        "resultinfo_buffer_type", "256",
        "resultinfo_identity", frame.identity.empty() ? "0" : frame.identity,
        "resultinfo_app_algoflag", "0",
        "resultinfo_width", wStr,
        "resultinfo_height", hStr,
        "resultinfo_stride", wStr,
        "resultinfo_scanline", hStr,
        "resultinfo_rotation", "0",
        "resultinfo_st_width", "0",
        "resultinfo_st_height", "0",
        "resultinfo_has_st_result", "false",
        "resultinfo_crop_left", "0",
        "resultinfo_crop_top", "0",
        "resultinfo_crop_right", wStr,
        "resultinfo_crop_bottom", hStr,
        "resultinfo_pipeline_name", pipeName,
        "resultinfo_is_last_videoframe", "0",
        "resultinfo_is_start_interoplation", "0",
        "resultinfo_is_motion_detected", "0",
        "resultinfo_is_need_detach", "false",
        "resultinfo_heif_encode_in_aps", "false",
        "resultinfo_dng_encode_in_aps", "false",
        "resultinfo_algoInterface_init_done", "true",
        "resultinfo_frame_id", frame.frame_number.empty() ? "0" : frame.frame_number,
        "capture_job_type", jobType,
        "jpeg_callback_for_defer_job", "0",
        "exif_tag_flags", "0",
        "job_level", "0",
        "aps_get_hal_service_time", "1",
        "capture_mode", capMode,
    };
    if (!picTitle.empty()) {
        resVec.push_back("picture_title");
        resVec.push_back(picTitle);
    }
    if (!deferGrp.empty()) {
        resVec.push_back("defer_job_group_id");
        resVec.push_back(deferGrp);
    }
    cbData["result_STRING_BASIC_ARRAYVALUE_"] = std::move(resVec);

    int cbRc = transactCb(/*TRANSACTION_CALLBACK_CAPTURE=*/3, cbData, cbReply);
    // Note: ownership of jpegAhb is transferred to Java HardwareBuffer via
    // AHardwareBuffer_toHardwareBuffer() and released by OplusCamera's HardwareBuffer.close().
    ALOGI("processImages: transactCb(3) rc=%d hwBuf=%d jByteArrRef=%s",
          cbRc, jpegAhb != nullptr ? 1 : 0, jByteArrRef.c_str());
    return 0;
}

int handleAlgoInit(DataMap* data, DataMap* reply) {
    (*reply)["algoinit_result_INT_BASIC_"] = {"0"};
    auto kv = parseKvPairs(data, "algoinit_params_STRING_BASIC_ARRAYVALUE_");
    std::string camId;
    if (kv.count("previewcfg_camera_id") && !kv["previewcfg_camera_id"].empty()) {
        camId = kv["previewcfg_camera_id"];
    } else if (kv.count("camera_id") && !kv["camera_id"].empty()) {
        camId = kv["camera_id"];
    }
    std::string capMode;
    if (kv.count("previewcfg_camera_mode") && !kv["previewcfg_camera_mode"].empty()) {
        capMode = kv["previewcfg_camera_mode"];
    } else if (kv.count("capture_mode") && !kv["capture_mode"].empty()) {
        capMode = kv["capture_mode"];
    }
    if (!camId.empty() || !capMode.empty()) {
        std::lock_guard<std::mutex> lk(g_frames_mu);
        if (!camId.empty()) g_last_camera_id = camId;
        if (!capMode.empty()) g_last_capture_mode = capMode;
    }
    std::string apsMod = kv.count("aps_module") ? kv["aps_module"] : "";
    ALOGI("algoInit: module=%s camId=%s mode=%s",
          apsMod.c_str(), camId.c_str(), capMode.c_str());
    return 0;
}

int handleAddPreviewFrameBuff(DataMap* data, DataMap* reply) {
    (*reply)["addpreviewframebuff_result_INT_BASIC_"] = {"0"};
    auto actionCb = g_action_cb.load(std::memory_order_acquire);
    auto transactCb = g_transact_cb.load(std::memory_order_acquire);
    std::string envStr = getFirst(data, "jni_callback_env");
    if (!transactCb || envStr.empty()) {
        return 0;
    }

    auto kv = parseKvPairs(data, "addpreviewframebuff_params_STRING_BASIC_ARRAYVALUE_");
    std::string identity = kv.count("time_stamp") && !kv["time_stamp"].empty() ? kv["time_stamp"] :
                           (kv.count("identity") && !kv["identity"].empty() ? kv["identity"] : "0");
    std::string frameNum = kv.count("frameNumber") && !kv["frameNumber"].empty() ? kv["frameNumber"] :
                           (kv.count("frame_number") && !kv["frame_number"].empty() ? kv["frame_number"] : "0");
    std::string pipeName = kv.count("pipeline") && !kv["pipeline"].empty() ? kv["pipeline"] :
                           (kv.count("pipeline_name") && !kv["pipeline_name"].empty() ? kv["pipeline_name"] : "pipeline_default");

    std::string camId;
    std::string capMode;
    {
        std::lock_guard<std::mutex> lk(g_frames_mu);
        if (kv.count("camera_id") && !kv["camera_id"].empty()) {
            camId = kv["camera_id"];
            g_last_camera_id = camId;
        } else {
            camId = g_last_camera_id.empty() ? "0" : g_last_camera_id;
        }
        if (kv.count("capture_mode") && !kv["capture_mode"].empty()) {
            capMode = kv["capture_mode"];
            g_last_capture_mode = capMode;
        } else {
            capMode = g_last_capture_mode.empty() ? "common" : g_last_capture_mode;
        }
    }

    // Extract primary image (index 0) and close any secondary preview image buffers.
    std::string imgObj0 = getFirst(data, "addpreviewframebuff_imageBufferArray_0_IMAGE_OBJECT_");
    std::string hwBufObj0 = getFirst(data, "addpreviewframebuff_imageBufferArray_0_HARDWAREBUFFER_OBJECT_");
    std::string metaObj = getFirst(data, "addpreviewframebuff_metaObj_CAPTURERESULT_OBJECT_");

    for (int idx = 1; idx < 4; ++idx) {
        std::string extraKey = "addpreviewframebuff_imageBufferArray_" + std::to_string(idx) + "_IMAGEBUFFER_OBJECT_";
        std::string extraBuf = getFirst(data, extraKey.c_str());
        if (!extraBuf.empty() && extraBuf != "0") {
            DataMap closeData, closeReply;
            closeData["jni_callback_env"] = {envStr};
            closeData["close_object_IMAGEBUFFER_OBJECT_"] = {extraBuf};
            transactCb(/*TRANSACTION_CLOSE_OBJECT=*/5, closeData, closeReply);
        }
    }

    int w = parseIntOr(kv.count("preview_width") ? kv["preview_width"] : "", 0);
    int h = parseIntOr(kv.count("preview_height") ? kv["preview_height"] : "", 0);
    std::string nativeAhbPtrStr;
    if (actionCb && !hwBufObj0.empty() && hwBufObj0 != "0") {
        DataMap req, rep;
        req["jni_callback_env"] = {envStr};
        req["jni_data_hardwarebuffer_object"] = {hwBufObj0};
        actionCb(/*actionGetHardwareBufferPtr=*/5, &req, &rep);
        std::string ptrStr = getFirst(&rep, "jni_reply_hardwarebuffer_ptr");
        long ptrVal = ptrStr.empty() ? 0 : std::strtol(ptrStr.c_str(), nullptr, 10);
        if (ptrVal != 0) {
            nativeAhbPtrStr = ptrStr;
            if (w <= 0 || h <= 0) {
                AHardwareBuffer_Desc desc{};
                AHardwareBuffer_describe(reinterpret_cast<AHardwareBuffer*>(ptrVal), &desc);
                if (desc.width > 0 && desc.height > 0) {
                    w = static_cast<int>(desc.width);
                    h = static_cast<int>(desc.height);
                }
            }
        }
    }
    if (w <= 0 || h <= 0) {
        w = 1920;
        h = 1440;
    }

    int seq = g_seq_id.fetch_add(1, std::memory_order_relaxed);
    std::string seqStr = std::to_string(seq);
    std::string wStr = std::to_string(w);
    std::string hStr = std::to_string(h);

    DataMap cbData, cbReply;
    cbData["jni_callback_env"] = {envStr};
    if (!imgObj0.empty() && imgObj0 != "0") {
        cbData["imageobj_IMAGE_OBJECT_"] = {imgObj0};
    }
    if (!nativeAhbPtrStr.empty()) {
        cbData["hwBuffer_HARDWAREBUFFER_OBJECT_"] = {nativeAhbPtrStr};
    }
    if (!metaObj.empty() && metaObj != "0") {
        cbData["metadata_TOTALCAPTURERESULT_OBJECT_"] = {metaObj};
    }

    cbData["result_STRING_BASIC_ARRAYVALUE_"] = {
        "result_using_string", "1",
        "resultinfo_message_type", "2",
        "resultinfo_buffer_type", "35",
        "resultinfo_identity", identity,
        "resultinfo_app_algoflag", "0",
        "resultinfo_width", wStr,
        "resultinfo_height", hStr,
        "resultinfo_stride", wStr,
        "resultinfo_scanline", hStr,
        "resultinfo_rotation", "0",
        "resultinfo_st_width", "0",
        "resultinfo_st_height", "0",
        "resultinfo_has_st_result", "false",
        "resultinfo_crop_left", "0",
        "resultinfo_crop_top", "0",
        "resultinfo_crop_right", wStr,
        "resultinfo_crop_bottom", hStr,
        "resultinfo_pipeline_name", pipeName,
        "resultinfo_is_last_videoframe", "0",
        "resultinfo_is_start_interoplation", "0",
        "resultinfo_is_motion_detected", "0",
        "resultinfo_is_need_detach", "false",
        "resultinfo_heif_encode_in_aps", "false",
        "resultinfo_dng_encode_in_aps", "false",
        "resultinfo_algoInterface_init_done", "true",
        "resultinfo_frame_id", frameNum,
        "capture_mode", capMode,
        // Embedded previewDecision fields consumed by APSClient.fillDecisionResult()
        "previewdecision_cameraid", camId,
        "previewdecision_capture_mode", capMode,
        "previewdecision_request_format", "35",
        "previewdecision_request_format_list", "35",
        "previewdecision_multiframecnt", "1",
        "previewdecision_thumbnail_index", "1",
        "previewdecision_meta_index", "0",
        "previewdecision_supernight_scene", "0",
        "previewdecision_turboraw_scene", "0",
        "previewdecision_motion_capture_enable", "0",
        "previewdecision_ancraw_scene", "0",
        "previewdecision_handshake_state", "0",
        "previewdecision_night_total_exptime", "0",
        "previewdecision_turbo_fusion_night_total_exptime", "0",
        "previewdecision_turbo_fusion_night_shutter_wait_time", "0",
        "previewdecision_scene_mode", "0",
        "previewdecision_feature_type", "0",
        "previewdecision_algolist", "none",
        "previewdecision_evlist", "0",
        "previewdecision_pre_evlist", "0",
        "doc_vertices", "0,0,0,0,0,0,0,0",
        "rectified_size", "0,0",
        "doc_edge_color", "0",
        "previewdecision_etlist", "0",
        "previewdecision_flash_type_list", "0",
        "previewdecision_capture_need_flash", "0",
        "previewdecision_naturetone_state", "0",
        "previewdecision_sensormask", "0,1,0,0",
        "previewdecision_master_pipeline", "1",
        "previewdecision_bracket_mode", "0",
        "previewdecision_mfsr_frame_count", "1",
        "previewdecision_sequenceid", seqStr,
        "previewdecision_precollect_frame_count", "0",
        "previewdecision_specificprocessalgo", "0",
        "previewdecision_zoom_feature", "0",
        "previewdecision_previewzoom_feature", "0",
        "drawtext_bracket_mode", "",
        "drawtext_scene_mode", "",
        "drawtext_feature_type", "",
        "drawtext_hdr_type", "",
        "preview_asd", "0",
        "preview_antibanding", "0",
        "preview_rectify", "0",
        "preview_pf", "0",
        "preview_lsd", "0",
        "preview_rtb", "0",
        "preview_aicolor", "0",
        "preview_video_hyperlapse", "0",
        "algo_visualization_key", "",
        "algo_visualization_value", "",
        "previewdecision_exposure_time_from_app", "0",
        "previewdecision_burstalgolist", "none",
        "previewdecision_asd_moving_object", "0",
        "previewdecision_capture_intent", "2",
        "previewdecision_show_hdr_icon", "0",
        "previewdecision_capture_interval", "0",
        "previewdecision_capture_surface", "0",
        "previewdecision_quick_jpeg", "0",
        "previewdecision_tr_bypass_cmc", "0",
        "previewdecision_interpolate_frame_camera_mode", "0",
        "previewdecision_portrait_hdr_type", "0",
        "previewdecision_asd_msg_scene_value", "0",
        "previewdecision_aishutter_enable", "false",
        "previewdecision_request_mixed_format", "false",
        "previewdecision_hdr_brighten_index", "0",
        "previewdecision_capture_noneed_match_meta", "0",
    };

    uint64_t cnt = g_preview_cb_count.fetch_add(1, std::memory_order_relaxed);
    if (cnt < 3 || (cnt % 120) == 0) {
        ALOGI("addPreviewFrameBuff[%llu]: %dx%d cam=%s mode=%s pipe=%s id=%s",
              static_cast<unsigned long long>(cnt), w, h,
              camId.c_str(), capMode.c_str(), pipeName.c_str(), identity.c_str());
    }

    transactCb(/*TRANSACTION_CALLBACK_PREVIEW=*/4, cbData, cbReply);
    return 0;
}

}  // namespace

extern "C" __attribute__((visibility("default")))
void setOnTransactCallback(OnTransactCb cb) {
    g_transact_cb.store(cb, std::memory_order_release);
}

extern "C" __attribute__((visibility("default")))
void setRequestActionCallback(RequestActionCb cb) {
    g_action_cb.store(cb, std::memory_order_release);
}

extern "C" __attribute__((visibility("default")))
int onTransact(int code, DataMap* data, DataMap* reply) {
    if (!reply) return 0;
    switch (code) {
        case 2:  // create
            (*reply)["create_result_INT_BASIC_"] = {"0"};
            return 0;
        case 3:  // destroy
            {
                std::lock_guard<std::mutex> lk(g_frames_mu);
                g_frames.clear();
            }
            return 0;
        case 4:  // algoInit
            return handleAlgoInit(data, reply);
        case 5:  // uninit
            (*reply)["unint_result_INT_BASIC_"] = {"0"};
            return 0;
        case 6:  // addFrameBuff
            return handleAddFrameBuff(data, reply);
        case 7:  // processImages
            return handleProcessImages(data, reply);
        case 8:  // previewDecision
            return handlePreviewDecision(data, reply);
        case 9:  // addPreviewFrameBuff (FullApsImpl passthrough + DecisionResult)
            return handleAddPreviewFrameBuff(data, reply);
        case 14: // stop
            (*reply)["stop_result_INT_BASIC_"] = {"0"};
            return 0;
        case 15: // clear
            {
                std::lock_guard<std::mutex> lk(g_frames_mu);
                g_frames.clear();
            }
            return 0;
        case 16: // releaseBuffer
            (*reply)["releasebuff_result_INT_BASIC_"] = {"0"};
            return 0;
        case 17: // getRuntimeInfo
            (*reply)["runtimeinfo_mInputMemSize_INT_BASIC_"] = {"0"};
            (*reply)["runtimeinfo_mSingleAlgoMaxRunMem_INT_BASIC_"] = {"0"};
            (*reply)["runtimeinfo_mTotalTimeEstimate_INT_BASIC_"] = {"0"};
            (*reply)["runtimeinfo_mProcessCntPending_INT_BASIC_"] = {"0"};
            return 0;
        case 18: // setEnableAPSPipeline
            (*reply)["setenableapspipeline_result_INT_BASIC_"] = {"0"};
            return 0;
        case 19: // setEnableAPSAlgoNode
            (*reply)["setenableapsalgonode_result_INT_BASIC_"] = {"0"};
            return 0;
        case 20: // beforeCapture
            ALOGI("beforeCapture");
            (*reply)["beforecapture_result_INT_BASIC_"] = {"0"};
            return 0;
        case 22: // startCapture
            ALOGI("startCapture");
            (*reply)["startcapture_result_INT_BASIC_"] = {"0"};
            return 0;
        case 23: // getAPSVersion
            (*reply)["getapsversion_result_STRING_BASIC_"] = {"6.0"};
            return 0;
        case 24: // getDumpAlgoList
            (*reply)["getdumpalgolist_result_STRING_BASIC_ARRAYVALUE_"] = {};
            return 0;
        case 25: // setDumpFlag
            (*reply)["setdumpflag_result_INT_BASIC_"] = {"0"};
            return 0;
        case 26: // executeDeferJob
            (*reply)["executeDeferJob_result_STRING_BASIC_ARRAYVALUE_"] = {};
            return 0;
        case 27: // collectFluencyData
            (*reply)["collectFluencyData_result_STRING_BASIC_"] = {""};
            return 0;
        default:
            return 0;
    }
}
