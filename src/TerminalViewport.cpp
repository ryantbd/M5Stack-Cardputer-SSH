#include "TerminalViewport.h"

namespace app {
namespace {

size_t negativeMagnitude(int value) {
    return static_cast<size_t>(-(value + 1)) + 1u;
}

}  // namespace

void TerminalViewport::enterLocal(const TerminalEmulator& terminal) {
    localActive_ = true;
    distanceFromBottom_ = 0;
    historyGeneration_ = terminal.history().generation();
    terminalGeneration_ = terminal.generation();
    alternateSnapshot_ = terminal.alternateActive();
    hasNewOutput_ = false;

    if (!alternateSnapshot_) return;
    for (size_t row = 0; row < kTerminalRows; ++row) {
        for (size_t column = 0; column < kTerminalColumns; ++column) {
            snapshot_[row][column] = terminal.activeScreen().cell(row, column);
        }
    }
}

void TerminalViewport::leaveLocal() {
    localActive_ = false;
    distanceFromBottom_ = 0;
    alternateSnapshot_ = false;
    hasNewOutput_ = false;
}

void TerminalViewport::onTerminalChanged(const TerminalEmulator& terminal) {
    if (!localActive_) return;

    if (alternateSnapshot_) {
        if (terminal.generation() != terminalGeneration_) hasNewOutput_ = true;
    } else {
        const uint32_t appended = terminal.history().generation() - historyGeneration_;
        if (distanceFromBottom_ != 0) {
            scrollAway(static_cast<size_t>(appended), terminal.history().size());
        } else {
            distanceFromBottom_ = 0;
        }
    }

    historyGeneration_ = terminal.history().generation();
    terminalGeneration_ = terminal.generation();
}

void TerminalViewport::scrollAway(size_t amount, size_t maximum) {
    if (distanceFromBottom_ > maximum) distanceFromBottom_ = maximum;
    if (amount > maximum - distanceFromBottom_) {
        distanceFromBottom_ = maximum;
    } else {
        distanceFromBottom_ += amount;
    }
}

void TerminalViewport::scrollToward(size_t amount) {
    if (amount >= distanceFromBottom_) {
        distanceFromBottom_ = 0;
    } else {
        distanceFromBottom_ -= amount;
    }
}

void TerminalViewport::scrollLines(int amount, const TerminalEmulator& terminal) {
    if (!localActive_ || alternateSnapshot_ || amount == 0) return;
    onTerminalChanged(terminal);
    if (amount > 0) {
        scrollAway(static_cast<size_t>(amount), terminal.history().size());
    } else {
        scrollToward(negativeMagnitude(amount));
    }
}

void TerminalViewport::scrollPages(int pages, const TerminalEmulator& terminal) {
    if (!localActive_ || alternateSnapshot_ || pages == 0) return;
    onTerminalChanged(terminal);

    const size_t maximum = terminal.history().size();
    if (pages > 0) {
        const size_t count = static_cast<size_t>(pages);
        const size_t pageLimit = maximum / kTerminalRows +
                                 (maximum % kTerminalRows == 0 ? 0u : 1u);
        if (count >= pageLimit) {
            distanceFromBottom_ = maximum;
        } else {
            scrollAway(count * kTerminalRows, maximum);
        }
    } else {
        const size_t count = negativeMagnitude(pages);
        const size_t pageLimit = distanceFromBottom_ / kTerminalRows +
                                 (distanceFromBottom_ % kTerminalRows == 0 ? 0u : 1u);
        if (count >= pageLimit) {
            distanceFromBottom_ = 0;
        } else {
            scrollToward(count * kTerminalRows);
        }
    }
}

void TerminalViewport::jumpToBottom() { distanceFromBottom_ = 0; }

const TerminalCell& TerminalViewport::cell(const TerminalEmulator& terminal,
                                           size_t row, size_t column) const {
    static const TerminalCell blank;
    if (row >= kTerminalRows || column >= kTerminalColumns) return blank;
    if (!localActive_) return terminal.activeScreen().cell(row, column);
    if (alternateSnapshot_) return snapshot_[row][column];

    const size_t historySize = terminal.history().size();
    const size_t distance = distanceFromBottom_ > historySize
                                ? historySize
                                : distanceFromBottom_;
    const size_t combinedIndex = historySize - distance + row;
    if (combinedIndex < historySize) {
        return terminal.history().row(combinedIndex)[column];
    }
    return terminal.primaryScreen().cell(combinedIndex - historySize, column);
}

size_t TerminalViewport::distanceFromBottom() const { return distanceFromBottom_; }

bool TerminalViewport::alternateSnapshot() const { return alternateSnapshot_; }

bool TerminalViewport::hasNewOutput() const { return hasNewOutput_; }

}  // namespace app
