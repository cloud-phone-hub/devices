// Copyright (c) 2025-2026 fei_cong(https://github.com/feicong/feicong-course)
//
// Remote KeyMint 的 guest 侧通道实现：把 /dev/hvc3（virtio-console，host 对端是
// daemon 拉起的 secure_env）当成一对裸 fd，按 keymaster_message 帧头 + payload
// 同步收发。cuttlefish 原本用 vhost/用户态 socket 封装同一协议，这里只需 POSIX
// read/write，故不依赖 cuttlefish 的线程与日志设施。
//
// 上层用到两个符号：cuttlefish::SharedFD（src/shims 的裁剪声明）与
// cuttlefish::SharedFdKeymasterChannel；协议层 CreateKeymasterMessage 来自官方
// `@cuttlefish/source/device-cuttlefish/common/libs/security/keymaster_channel.cpp`。

#include "common/libs/fs/shared_fd.h"
#include "common/libs/security/keymaster_channel_sharedfd.h"

#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <termios.h>
#include <unistd.h>

namespace cuttlefish {

namespace {

constexpr size_t kMaxMessagePayload = 16 * 1024 * 1024;

}

SharedFD::State::State(int fd) : fd_(fd), error_(fd < 0 ? errno : 0) {}
SharedFD::State::~State() {
    if (fd_ >= 0) close(fd_);
}
bool SharedFD::State::IsOpen() const { return fd_ >= 0; }
int SharedFD::State::SetTerminalRaw() {
    struct termios settings {};
    if (tcgetattr(fd_, &settings) < 0) return -1;
    cfmakeraw(&settings);
    return tcsetattr(fd_, TCSANOW, &settings);
}
std::string SharedFD::State::StrError() const { return std::strerror(error_ == 0 ? errno : error_); }
int SharedFD::State::Fd() const { return fd_; }
SharedFD::SharedFD(std::shared_ptr<State> state) : state_(std::move(state)) {}
SharedFD SharedFD::Open(const std::string& path, int flags) { return SharedFD(std::make_shared<State>(open(path.c_str(), flags))); }
SharedFD::State* SharedFD::operator->() const { return state_.get(); }

static bool transferAll(int fd, void* data, size_t size, bool writeMode) {
    auto* bytes = static_cast<unsigned char*>(data);
    size_t offset = 0;
    while (offset < size) {
        ssize_t count = writeMode ? write(fd, bytes + offset, size - offset) : read(fd, bytes + offset, size - offset);
        if (count < 0 && errno == EINTR) continue;
        if (count <= 0) return false;
        offset += static_cast<size_t>(count);
    }
    return true;
}

SharedFdKeymasterChannel::SharedFdKeymasterChannel(SharedFD input, SharedFD output) : input_(std::move(input)), output_(std::move(output)) {}
bool SharedFdKeymasterChannel::SendRequest(AndroidKeymasterCommand command, const keymaster::Serializable& message) { return SendMessage(command, false, message); }
bool SharedFdKeymasterChannel::SendResponse(AndroidKeymasterCommand command, const keymaster::Serializable& message) { return SendMessage(command, true, message); }
bool SharedFdKeymasterChannel::SendMessage(AndroidKeymasterCommand command, bool response, const keymaster::Serializable& message) {
    const size_t payloadSize = message.SerializedSize();
    if (payloadSize > kMaxMessagePayload) return false;
    auto outgoing = CreateKeymasterMessage(command, response, payloadSize);
    message.Serialize(outgoing->payload, outgoing->payload + payloadSize);
    return transferAll(output_->Fd(), outgoing.get(), sizeof(keymaster::keymaster_message) + payloadSize, true);
}
ManagedKeymasterMessage SharedFdKeymasterChannel::ReceiveMessage() {
    keymaster::keymaster_message header {};
    if (!transferAll(input_->Fd(), &header, sizeof(header), false)) return {};
    if (header.payload_size > kMaxMessagePayload) return {};
    auto incoming = CreateKeymasterMessage(header.cmd, header.is_response, header.payload_size);
    if (!transferAll(input_->Fd(), incoming->payload, header.payload_size, false)) return {};
    return incoming;
}

}  // namespace cuttlefish
