// Copyright (c) 2025-2026 fei_cong(https://github.com/feicong/feicong-course)
//
// cuttlefish `common/libs/fs/shared_fd.h` 的裁剪声明：官方头把 epoll/inotify/socket
// 等一大批 guest 用不到的 API 一起带进来，还依赖 cuttlefish 内部的线程/日志头。
// Remote KeyMint HAL 只用到 Open/Fd/SetTerminalRaw/StrError，故只声明这一小块，
// 实现见 src/posix_channel.cpp。本目录须排在 `@cuttlefish/source/device-cuttlefish`
// 之前（见 modules/android.hardware.keymint.json 的 includeDirs 顺序），才能遮蔽官方头。
#pragma once

#include <fcntl.h>
#include <memory>
#include <string>

namespace cuttlefish {

class SharedFD {
 public:
    class State {
     public:
        explicit State(int fd);
        ~State();
        bool IsOpen() const;
        int SetTerminalRaw();
        std::string StrError() const;
        int Fd() const;

     private:
        int fd_;
        int error_;
    };

    SharedFD() = default;
    explicit SharedFD(std::shared_ptr<State> state);
    static SharedFD Open(const std::string& path, int flags);
    State* operator->() const;

 private:
    std::shared_ptr<State> state_;
};

}  // namespace cuttlefish
