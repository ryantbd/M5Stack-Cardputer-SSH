#include "ScrollbackViewport.h"

namespace app {

void ScrollbackViewport::scrollLines(int amount, size_t totalLines, size_t visibleRows) {
    const size_t maximum = maximumOffset(totalLines, visibleRows);
    if (amount > 0) {
        const size_t delta = static_cast<size_t>(amount);
        offset_ = delta > maximum - (offset_ < maximum ? offset_ : maximum) ? maximum : offset_ + delta;
    } else if (amount < 0) {
        const size_t delta = static_cast<size_t>(-amount);
        offset_ = delta > offset_ ? 0 : offset_ - delta;
    }
    clamp(totalLines, visibleRows);
}

void ScrollbackViewport::scrollPages(int pages, size_t totalLines, size_t visibleRows) {
    if (visibleRows == 0 || pages == 0) {
        return;
    }
    const int direction = pages > 0 ? 1 : -1;
    size_t count = static_cast<size_t>(pages > 0 ? pages : -pages);
    while (count-- > 0) {
        scrollLines(direction * static_cast<int>(visibleRows), totalLines, visibleRows);
    }
}

void ScrollbackViewport::jumpToBottom() {
    offset_ = 0;
}

void ScrollbackViewport::onLinesAppended(size_t count, size_t totalLines, size_t visibleRows) {
    if (offset_ > 0) {
        const size_t maximum = maximumOffset(totalLines, visibleRows);
        offset_ = count > maximum - (offset_ < maximum ? offset_ : maximum) ? maximum : offset_ + count;
    }
    clamp(totalLines, visibleRows);
}

size_t ScrollbackViewport::distanceFromBottom() const {
    return offset_;
}

bool ScrollbackViewport::isScrolled() const {
    return offset_ > 0;
}

size_t ScrollbackViewport::maximumOffset(size_t totalLines, size_t visibleRows) {
    return totalLines > visibleRows ? totalLines - visibleRows : 0;
}

void ScrollbackViewport::clamp(size_t totalLines, size_t visibleRows) {
    const size_t maximum = maximumOffset(totalLines, visibleRows);
    if (offset_ > maximum) {
        offset_ = maximum;
    }
}

}  // namespace app
