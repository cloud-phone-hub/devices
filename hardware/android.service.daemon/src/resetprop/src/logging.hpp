#pragma once

#if NDK_DEBUG
#include <cstdio>
#define LOGI(...) std::printf(__VA_ARGS__)
#define LOGE(...) std::printf(__VA_ARGS__)
#define LOGD(...) std::printf(__VA_ARGS__)
#define LOGV(...) std::printf(__VA_ARGS__)
#define LOGW(...) std::printf(__VA_ARGS__)
#else
#define LOGI(...) do { } while (0)
#define LOGE(...) do { } while (0)
#define LOGD(...) do { } while (0)
#define LOGV(...) do { } while (0)
#define LOGW(...) do { } while (0)
#endif