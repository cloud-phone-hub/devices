#include <stdint.h>
#include <android/log.h>

int __android_log_error_write(int tag, const char *subTag, int32_t uid, const char *data, uint32_t dataLen) {
  (void)uid;
  (void)data;
  (void)dataLen;
  return __android_log_print(ANDROID_LOG_ERROR, "camera_metadata", "snet:%d:%s", tag, subTag ? subTag : "");
}
