#include "KeyboardRouter.h"

#include <string.h>

#ifdef ARDUINO
#include <M5Cardputer.h>
#endif

namespace app {

namespace {

KeyEvent event(KeyEventType type, uint8_t value = 0) {
    KeyEvent result;
    result.type = type;
    result.value = value;
    return result;
}

bool due(uint32_t now, uint32_t deadline) {
    return static_cast<int32_t>(now - deadline) >= 0;
}

KeyEvent fnEvent(LayerValue value) {
    switch (value) {
        case LayerValue::Escape: return event(KeyEventType::Escape);
        case LayerValue::DeleteForward: return event(KeyEventType::DeleteForward);
        case LayerValue::Up: return event(KeyEventType::MoveUp);
        case LayerValue::Down: return event(KeyEventType::MoveDown);
        case LayerValue::Left: return event(KeyEventType::MoveLeft);
        case LayerValue::Right: return event(KeyEventType::MoveRight);
        default: break;
    }
    if (value >= LayerValue::F1 && value <= LayerValue::F12) {
        return event(KeyEventType::Function,
                     static_cast<uint8_t>(value) - static_cast<uint8_t>(LayerValue::F1) + 1);
    }
    return {};
}

bool terminalContext(InputContext context) {
    return context == InputContext::RemoteTerminal || context == InputContext::LocalScroll;
}

}  // namespace

bool KeyboardSnapshot::contains(PhysicalKeyId id) const {
    return id < kPhysicalKeyCount && (pressed & (uint64_t{1} << id)) != 0;
}

void KeyboardSnapshot::set(PhysicalKeyId id, bool down) {
    if (id >= kPhysicalKeyCount) {
        return;
    }
    const uint64_t mask = uint64_t{1} << id;
    pressed = down ? pressed | mask : pressed & ~mask;
}

void KeyboardRouter::update(const KeyboardSnapshot& snapshot, InputContext context, uint32_t nowMs) {
    updateModifierStatus(snapshot);
    if (suppressing_) {
        previous_ = snapshot;
        if (snapshot.empty()) {
            suppressing_ = false;
            previous_ = {};
        }
        return;
    }

    const uint64_t pressedEdges = snapshot.pressed & ~previous_.pressed;
    const uint64_t releasedEdges = previous_.pressed & ~snapshot.pressed;

    if ((pressedEdges & (uint64_t{1} << kKeyAa)) != 0) {
        aaPressedAt_ = nowMs;
        aaUsedInChord_ = false;
    }

    for (PhysicalKeyId id = 0; id < kPhysicalKeyCount; ++id) {
        const uint64_t mask = uint64_t{1} << id;
        if ((releasedEdges & mask) == 0) {
            continue;
        }
        nextRepeatAt_[id] = 0;
        heldEvents_[id] = {};
        if (id == kKeyAa && !aaUsedInChord_ && nowMs - aaPressedAt_ <= kAaTapMaximumMs) {
            status_.aaLocked = !status_.aaLocked;
        }
    }

    updateModifierStatus(snapshot);
    for (PhysicalKeyId id = 0; id < kPhysicalKeyCount; ++id) {
        const uint64_t mask = uint64_t{1} << id;
        if ((pressedEdges & mask) == 0 || isModifierKey(id)) {
            continue;
        }
        status_.lastPressed = id;
        status_.hasLastPressed = true;
        if (status_.aaHeld) {
            aaUsedInChord_ = true;
        }
        const KeyEvent next = routeKey(id, context);
        heldEvents_[id] = next;
        if (next.type != KeyEventType::None) {
            push(next);
            if (repeatable(next, context)) {
                nextRepeatAt_[id] = nowMs + kInitialRepeatMs;
            }
        }
    }

    for (PhysicalKeyId id = 0; id < kPhysicalKeyCount; ++id) {
        if (!snapshot.contains(id) || isModifierKey(id) || (pressedEdges & (uint64_t{1} << id)) != 0 ||
            heldEvents_[id].type == KeyEventType::None ||
            !repeatable(heldEvents_[id], context) || !due(nowMs, nextRepeatAt_[id])) {
            continue;
        }
        KeyEvent repeated = heldEvents_[id];
        repeated.repeat = true;
        push(repeated);
        nextRepeatAt_[id] = nowMs + kRepeatIntervalMs;
    }

    previous_ = snapshot;
}

bool KeyboardRouter::pop(KeyEvent& eventOut) {
    if (queueCount_ == 0) {
        return false;
    }
    eventOut = queue_[queueHead_];
    queueHead_ = (queueHead_ + 1) % kQueueCapacity;
    --queueCount_;
    return true;
}

void KeyboardRouter::clear() {
    queueCount_ = 0;
    queueHead_ = 0;
    memset(nextRepeatAt_, 0, sizeof(nextRepeatAt_));
    for (KeyEvent& held : heldEvents_) {
        held = {};
    }
}

void KeyboardRouter::suppressUntilReleased() {
    clear();
    suppressing_ = true;
    aaUsedInChord_ = true;
}

KeyEvent KeyboardRouter::routeKey(PhysicalKeyId id, InputContext context) const {
    const KeyMeaning& meaning = keyMeaning(id);

    if (status_.optHeld && terminalContext(context)) {
        if (id == kKeySpace) return event(KeyEventType::ToggleTerminalMode);
        if (id == kKeyEnter) return event(KeyEventType::OpenSettings);
        if (id == kKeyDelete) return event(KeyEventType::EndSession);
        return {};
    }
    if (status_.optHeld) {
        return {};
    }

    if (status_.fnHeld) {
        KeyEvent resolved = fnEvent(meaning.fn);
        if (resolved.type == KeyEventType::Escape && context != InputContext::RemoteTerminal) {
            resolved.type = KeyEventType::Back;
        }
        if (resolved.type == KeyEventType::DeleteForward &&
            context != InputContext::TextInput && context != InputContext::RemoteTerminal) {
            return {};
        }
        if (context == InputContext::LocalScroll && resolved.type != KeyEventType::MoveUp &&
            resolved.type != KeyEventType::MoveDown && resolved.type != KeyEventType::MoveLeft &&
            resolved.type != KeyEventType::MoveRight) {
            return {};
        }
        return resolved;
    }

    const bool aaLayer = status_.aaHeld || status_.aaLocked;
    const char printable = meaning.role == LayerValue::Printable ? (aaLayer ? meaning.aa : meaning.base) : 0;
    if (context == InputContext::RemoteTerminal && status_.ctrlHeld && printable != 0) {
        const uint8_t value = controlValue(printable);
        if (value != 0xffU) return event(KeyEventType::Control, value);
        return {};
    }
    if (context == InputContext::RemoteTerminal && status_.altHeld && printable != 0) {
        return event(KeyEventType::AltText, static_cast<uint8_t>(printable));
    }
    if (status_.ctrlHeld || status_.altHeld) {
        return {};
    }

    if (meaning.role == LayerValue::Enter) {
        return context == InputContext::LocalScroll ? KeyEvent{} : event(KeyEventType::Confirm);
    }
    if (meaning.role == LayerValue::Tab) {
        return context == InputContext::RemoteTerminal ? event(KeyEventType::Tab) : KeyEvent{};
    }
    if (meaning.role == LayerValue::Backspace) {
        if (context == InputContext::TextInput || context == InputContext::RemoteTerminal) {
            return event(KeyEventType::DeleteBackward);
        }
        if (context == InputContext::LocalScroll) return event(KeyEventType::JumpBottom);
        return event(KeyEventType::Back);
    }
    if (printable != 0 && (context == InputContext::TextInput || context == InputContext::RemoteTerminal)) {
        return event(KeyEventType::Text, static_cast<uint8_t>(printable));
    }
    return {};
}

bool KeyboardRouter::repeatable(const KeyEvent& eventValue, InputContext context) {
    switch (eventValue.type) {
        case KeyEventType::MoveUp:
        case KeyEventType::MoveDown:
        case KeyEventType::MoveLeft:
        case KeyEventType::MoveRight:
            return true;
        case KeyEventType::DeleteBackward:
        case KeyEventType::DeleteForward:
            return context == InputContext::TextInput || context == InputContext::RemoteTerminal;
        default:
            return false;
    }
}

uint8_t KeyboardRouter::controlValue(char character) {
    if (character >= 'a' && character <= 'z') return static_cast<uint8_t>(character - 'a' + 1);
    if (character >= 'A' && character <= 'Z') return static_cast<uint8_t>(character - 'A' + 1);
    if (character == ' ' || character == '2' || character == '@') return 0x00;
    if (character == '3' || character == '[') return 0x1b;
    if (character == '4' || character == '\\') return 0x1c;
    if (character == '5' || character == ']') return 0x1d;
    if (character == '6' || character == '^') return 0x1e;
    if (character == '7' || character == '_') return 0x1f;
    if (character == '8' || character == '?') return 0x7f;
    return 0xff;
}

bool KeyboardRouter::push(const KeyEvent& eventValue) {
    if (queueCount_ == kQueueCapacity) {
        if (eventValue.repeat || !removeOldestRepeat()) return false;
    }
    queue_[(queueHead_ + queueCount_) % kQueueCapacity] = eventValue;
    ++queueCount_;
    return true;
}

bool KeyboardRouter::removeOldestRepeat() {
    for (size_t index = 0; index < queueCount_; ++index) {
        if (!queue_[(queueHead_ + index) % kQueueCapacity].repeat) continue;
        for (size_t move = index + 1; move < queueCount_; ++move) {
            queue_[(queueHead_ + move - 1) % kQueueCapacity] = queue_[(queueHead_ + move) % kQueueCapacity];
        }
        --queueCount_;
        return true;
    }
    return false;
}

void KeyboardRouter::updateModifierStatus(const KeyboardSnapshot& snapshot) {
    status_.fnHeld = snapshot.contains(kKeyFn);
    status_.aaHeld = snapshot.contains(kKeyAa);
    status_.ctrlHeld = snapshot.contains(kKeyCtrl);
    status_.optHeld = snapshot.contains(kKeyOpt);
    status_.altHeld = snapshot.contains(kKeyAlt);
}

#ifdef ARDUINO
void KeyboardRouter::pollHardware(InputContext context, uint32_t nowMs) {
    M5Cardputer.update();
    KeyboardSnapshot snapshot;
    for (const Point2D_t& point : M5Cardputer.Keyboard.keyList()) {
        if (point.x >= 0 && point.x < static_cast<int>(kKeyboardColumns) && point.y >= 0 &&
            point.y < static_cast<int>(kKeyboardRows)) {
            snapshot.set(keyId(static_cast<uint8_t>(point.y), static_cast<uint8_t>(point.x)), true);
        }
    }
    update(snapshot, context, nowMs);
}
#endif

}  // namespace app
