#include "ListViewport.h"

namespace app {

ListViewportState listViewport(size_t count,
                               size_t selected,
                               size_t previousFirst,
                               size_t visibleRows) {
    ListViewportState state;
    if (count == 0 || visibleRows == 0) return state;
    if (selected >= count) selected = count - 1;

    state.rows = count < visibleRows ? count : visibleRows;
    const size_t maximumFirst = count - state.rows;
    state.first = previousFirst < maximumFirst ? previousFirst : maximumFirst;
    if (selected < state.first) {
        state.first = selected;
    } else if (selected >= state.first + state.rows) {
        state.first = selected - state.rows + 1;
    }
    return state;
}

}  // namespace app
