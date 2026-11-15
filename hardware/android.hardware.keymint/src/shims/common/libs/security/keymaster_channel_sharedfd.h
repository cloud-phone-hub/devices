// Copyright (c) 2025-2026 fei_cong(https://github.com/feicong/feicong-course)
//
// cuttlefish 的 KeymasterChannel 在这里用「一对 SharedFD」实现（官方实现走
// vhost/用户态 socket 封装）。协议层（keymaster_message 帧）复用官方
// `common/libs/security/keymaster_channel.h`，只重写通道读写；实现见
// src/posix_channel.cpp。
#pragma once

#include "common/libs/fs/shared_fd.h"
#include "common/libs/security/keymaster_channel.h"

namespace cuttlefish {

class SharedFdKeymasterChannel : public KeymasterChannel {
 public:
    SharedFdKeymasterChannel(SharedFD input, SharedFD output);
    bool SendRequest(AndroidKeymasterCommand command, const keymaster::Serializable& message) override;
    bool SendResponse(AndroidKeymasterCommand command, const keymaster::Serializable& message) override;
    ManagedKeymasterMessage ReceiveMessage() override;

 private:
    bool SendMessage(AndroidKeymasterCommand command, bool response, const keymaster::Serializable& message);
    SharedFD input_;
    SharedFD output_;
};

}  // namespace cuttlefish
