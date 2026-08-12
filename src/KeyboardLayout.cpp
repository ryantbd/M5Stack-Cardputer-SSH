#include "KeyboardLayout.h"

namespace app {

namespace {

constexpr KeyMeaning printable(char base, char aa, LayerValue fn = LayerValue::None) {
    return KeyMeaning{base, aa, fn, LayerValue::Printable};
}

constexpr KeyMeaning special(LayerValue role, LayerValue fn = LayerValue::None) {
    return KeyMeaning{0, 0, fn, role};
}

constexpr KeyMeaning kUndefined{0, 0, LayerValue::None, LayerValue::None};

constexpr KeyMeaning kLayout[kPhysicalKeyCount] = {
    printable('`', '~', LayerValue::Escape), printable('1', '!', LayerValue::F1),
    printable('2', '@', LayerValue::F2), printable('3', '#', LayerValue::F3),
    printable('4', '$', LayerValue::F4), printable('5', '%', LayerValue::F5),
    printable('6', '^', LayerValue::F6), printable('7', '&', LayerValue::F7),
    printable('8', '*', LayerValue::F8), printable('9', '(', LayerValue::F9),
    printable('0', ')', LayerValue::F10), printable('-', '_', LayerValue::F11),
    printable('=', '+', LayerValue::F12), special(LayerValue::Backspace, LayerValue::DeleteForward),

    special(LayerValue::Tab), printable('q', 'Q'), printable('w', 'W'), printable('e', 'E'),
    printable('r', 'R'), printable('t', 'T'), printable('y', 'Y'), printable('u', 'U'),
    printable('i', 'I'), printable('o', 'O'), printable('p', 'P'), printable('[', '{'),
    printable(']', '}'), printable('\\', '|'),

    special(LayerValue::Fn), special(LayerValue::Aa), printable('a', 'A'), printable('s', 'S'),
    printable('d', 'D'), printable('f', 'F'), printable('g', 'G'), printable('h', 'H'),
    printable('j', 'J'), printable('k', 'K'), printable('l', 'L'),
    printable(';', ':', LayerValue::Up), printable('\'', '"'), special(LayerValue::Enter),

    special(LayerValue::Ctrl), special(LayerValue::Opt), special(LayerValue::Alt),
    printable('z', 'Z'), printable('x', 'X'), printable('c', 'C'), printable('v', 'V'),
    printable('b', 'B'), printable('n', 'N'), printable('m', 'M'),
    printable(',', '<', LayerValue::Left), printable('.', '>', LayerValue::Down),
    printable('/', '?', LayerValue::Right), printable(' ', ' '),
};

static_assert(sizeof(kLayout) / sizeof(kLayout[0]) == kPhysicalKeyCount, "Incomplete keyboard table");

}  // namespace

const KeyMeaning& keyMeaning(PhysicalKeyId id) {
    return id < kPhysicalKeyCount ? kLayout[id] : kUndefined;
}

bool isModifierKey(PhysicalKeyId id) {
    const LayerValue role = keyMeaning(id).role;
    return role == LayerValue::Fn || role == LayerValue::Aa || role == LayerValue::Ctrl ||
           role == LayerValue::Opt || role == LayerValue::Alt;
}

}  // namespace app
