#pragma once

#include <stddef.h>
#include <stdint.h>

namespace app {

enum class SshReadStatus : uint8_t { Data, WouldBlock, Closed, Error };

struct SshReadResult {
    constexpr SshReadResult(SshReadStatus readStatus = SshReadStatus::Error,
                            size_t readLength = 0)
        : status(readStatus), length(readLength) {}

    SshReadStatus status;
    size_t length;
};

constexpr SshReadResult classifySshReadResult(int rawResult, size_t capacity,
                                               int againCode, int eofCode) {
    return rawResult > 0 && static_cast<size_t>(rawResult) <= capacity
               ? SshReadResult{SshReadStatus::Data,
                               static_cast<size_t>(rawResult)}
           : rawResult == 0 || rawResult == againCode
               ? SshReadResult{SshReadStatus::WouldBlock, 0}
           : rawResult == eofCode
               ? SshReadResult{SshReadStatus::Closed, 0}
               : SshReadResult{SshReadStatus::Error, 0};
}

constexpr bool sshReadTerminatesSession(SshReadStatus status) {
    return status == SshReadStatus::Closed || status == SshReadStatus::Error;
}

}  // namespace app
