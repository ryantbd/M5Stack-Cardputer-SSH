#pragma once

#include <stddef.h>

namespace app {

class ScrollbackViewport {
public:
    void scrollLines(int amount, size_t totalLines, size_t visibleRows);
    void scrollPages(int pages, size_t totalLines, size_t visibleRows);
    void jumpToBottom();
    void onLinesAppended(size_t count, size_t totalLines, size_t visibleRows);

    size_t distanceFromBottom() const;
    bool isScrolled() const;

private:
    static size_t maximumOffset(size_t totalLines, size_t visibleRows);
    void clamp(size_t totalLines, size_t visibleRows);

    size_t offset_ = 0;
};

}  // namespace app
