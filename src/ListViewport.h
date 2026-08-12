#pragma once

#include <stddef.h>

namespace app {

struct ListViewportState {
    size_t first = 0;
    size_t rows = 0;
};

ListViewportState listViewport(size_t count,
                               size_t selected,
                               size_t previousFirst,
                               size_t visibleRows = 3);

}  // namespace app
