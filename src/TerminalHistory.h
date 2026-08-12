#pragma once

#include <stddef.h>
#include <stdint.h>

#include "TerminalTypes.h"

namespace app {

class TerminalHistory {
public:
    void clear();
    void push(const TerminalRow& row);
    size_t size() const;
    uint32_t generation() const;
    const TerminalRow& row(size_t oldestBasedIndex) const;

private:
    TerminalRow rows_[kTerminalHistoryRows]{};
    size_t first_ = 0;
    size_t count_ = 0;
    uint32_t generation_ = 0;
};

}  // namespace app
