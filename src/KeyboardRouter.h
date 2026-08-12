#pragma once

#include <stddef.h>
#include <stdint.h>

#include "KeyboardLayout.h"

namespace app {

enum class InputContext : uint8_t { Menu, TextInput, Dialog, RemoteTerminal, LocalScroll };

enum class KeyEventType : uint8_t {
    None,
    MoveUp,
    MoveDown,
    MoveLeft,
    MoveRight,
    Confirm,
    Back,
    DeleteBackward,
    DeleteForward,
    ToggleTerminalMode,
    OpenSettings,
    EndSession,
    Text,
    Control,
    AltText,
    Function,
    Tab,
    Escape,
    JumpBottom,
};

struct KeyboardSnapshot {
    uint64_t pressed = 0;

    bool contains(PhysicalKeyId id) const;
    void set(PhysicalKeyId id, bool down);
    void press(PhysicalKeyId id) { set(id, true); }
    void release(PhysicalKeyId id) { set(id, false); }
    bool empty() const { return pressed == 0; }
};

struct KeyboardStatus {
    PhysicalKeyId lastPressed = 0;
    bool hasLastPressed = false;
    bool fnHeld = false;
    bool aaHeld = false;
    bool aaLocked = false;
    bool ctrlHeld = false;
    bool altHeld = false;
    bool optHeld = false;
};

struct KeyEvent {
    KeyEventType type = KeyEventType::None;
    uint8_t value = 0;
    bool repeat = false;
};

class KeyboardRouter {
public:
    static constexpr size_t kQueueCapacity = 8;
    static constexpr uint32_t kInitialRepeatMs = 350;
    static constexpr uint32_t kRepeatIntervalMs = 150;
    static constexpr uint32_t kAaTapMaximumMs = 300;

    void update(const KeyboardSnapshot& snapshot, InputContext context, uint32_t nowMs);
    bool pop(KeyEvent& event);
    void clear();
    void suppressUntilReleased();

    const KeyboardStatus& status() const { return status_; }
    bool aaHeld() const { return status_.aaHeld; }
    bool aaLocked() const { return status_.aaLocked; }

#ifdef ARDUINO
    void pollHardware(InputContext context, uint32_t nowMs);
#endif

private:
    KeyEvent routeKey(PhysicalKeyId id, InputContext context) const;
    static bool repeatable(const KeyEvent& event, InputContext context);
    static uint8_t controlValue(char character);
    bool push(const KeyEvent& event);
    bool removeOldestRepeat();
    void updateModifierStatus(const KeyboardSnapshot& snapshot);

    KeyboardSnapshot previous_{};
    KeyboardStatus status_{};
    KeyEvent queue_[kQueueCapacity]{};
    size_t queueHead_ = 0;
    size_t queueCount_ = 0;
    bool suppressing_ = false;
    uint32_t nextRepeatAt_[kPhysicalKeyCount]{};
    KeyEvent heldEvents_[kPhysicalKeyCount]{};
    uint32_t aaPressedAt_ = 0;
    bool aaUsedInChord_ = false;
};

}  // namespace app
