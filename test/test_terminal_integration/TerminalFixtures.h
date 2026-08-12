#pragma once

#include <stddef.h>

namespace app {
namespace terminal_fixtures {

constexpr char kVimFrame[] =
    "shell$ vim file.txt\r\n"
    "\x1b[?1049h\x1b[H\x1b[2J"
    "file.txt"
    "\x1b[2;1H\x1b[38;5;196;48;5;17mhello\x1b[0m"
    "\x1b[13;1H-- INSERT --\x1b[?25h\x1b[?1h";

constexpr char kTopFrame[] =
    "\x1b[?1049h\x1b[H\x1b[2J"
    "top - 02:40"
    "\x1b[2;1HTasks: 123 total\x1b[K"
    "\x1b[Htop - 02:41"
    "\x1b[2;1HTasks: 124 total\x1b[K";

constexpr char kTmuxFrame[] =
    "\x1b[?1049h\x1b[H\x1b[2J"
    "\x1b(0lqqqqqqqqqqk"
    "\x1b[2;1Hx          x"
    "\x1b[3;1Hmqqqqqqqqqqj\x1b(B";

constexpr char kNanoFrame[] =
    "\x1b[?1049h\x1b[H\x1b[2J"
    "\x1b[7m GNU nano 7.2 \x1b[0m"
    "\x1b[3;1Hhello world"
    "\x1b[3;3H\x1b[?25h";

constexpr char kHtopFrame[] =
    "\x1b[?1049h\x1b[H\x1b[2J"
    "\x1b[38;5;46mCPU\x1b[0m "
    "\x1b[38;5;196m\xe2\x96\x88\xe2\x96\x88\x1b[0m"
    "\x1b[2;1H\xe2\x86\x92 \xe2\xa0\x81 \xc3\xa9 \xe4\xb8\xad";

static_assert(sizeof(kVimFrame) < 256, "Vim fixture must remain bounded");
static_assert(sizeof(kTopFrame) < 256, "top fixture must remain bounded");
static_assert(sizeof(kTmuxFrame) < 256, "tmux fixture must remain bounded");
static_assert(sizeof(kNanoFrame) < 256, "nano fixture must remain bounded");
static_assert(sizeof(kHtopFrame) < 256, "htop fixture must remain bounded");

}  // namespace terminal_fixtures
}  // namespace app
