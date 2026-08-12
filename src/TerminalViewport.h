#pragma once

#include <stddef.h>
#include <stdint.h>

#include "TerminalEmulator.h"

namespace app {

class TerminalViewport {
public:
    void enterLocal(const TerminalEmulator& terminal);
    void leaveLocal();
    void onTerminalChanged(const TerminalEmulator& terminal);
    void scrollLines(int amount, const TerminalEmulator& terminal);
    void scrollPages(int pages, const TerminalEmulator& terminal);
    void jumpToBottom();
    const TerminalCell& cell(const TerminalEmulator& terminal, size_t row,
                             size_t column) const;
    size_t distanceFromBottom() const;
    bool alternateSnapshot() const;
    bool hasNewOutput() const;

private:
    void scrollAway(size_t amount, size_t maximum);
    void scrollToward(size_t amount);

    TerminalRow snapshot_[kTerminalRows]{};
    size_t distanceFromBottom_ = 0;
    uint32_t historyGeneration_ = 0;
    uint32_t terminalGeneration_ = 0;
    bool localActive_ = false;
    bool alternateSnapshot_ = false;
    bool hasNewOutput_ = false;
};

}  // namespace app
