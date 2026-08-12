#pragma once

#include <stdint.h>

namespace app {

using CancelCallback = bool (*)(void* context);
using ProgressCallback = void (*)(const char* stage, uint16_t current, uint16_t total, void* context);

struct OperationCallbacks {
    CancelCallback canceled = nullptr;
    ProgressCallback progress = nullptr;
    void* context = nullptr;

    bool isCanceled() const {
        return canceled != nullptr && canceled(context);
    }

    void report(const char* stage, uint16_t current = 0, uint16_t total = 0) const {
        if (progress != nullptr) {
            progress(stage, current, total, context);
        }
    }
};

}  // namespace app
