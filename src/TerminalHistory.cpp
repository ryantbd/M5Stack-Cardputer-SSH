#include "TerminalHistory.h"

namespace app {

void TerminalHistory::clear() {
    for (TerminalRow& row : rows_) row = {};
    first_ = 0;
    count_ = 0;
    ++generation_;
}

void TerminalHistory::push(const TerminalRow& row) {
    if (count_ < kTerminalHistoryRows) {
        rows_[(first_ + count_) % kTerminalHistoryRows] = row;
        ++count_;
    } else {
        rows_[first_] = row;
        first_ = (first_ + 1) % kTerminalHistoryRows;
    }
    ++generation_;
}

size_t TerminalHistory::size() const { return count_; }

uint32_t TerminalHistory::generation() const { return generation_; }

const TerminalRow& TerminalHistory::row(size_t oldestBasedIndex) const {
    static const TerminalRow blank{};
    if (oldestBasedIndex >= count_) return blank;
    return rows_[(first_ + oldestBasedIndex) % kTerminalHistoryRows];
}

}  // namespace app
