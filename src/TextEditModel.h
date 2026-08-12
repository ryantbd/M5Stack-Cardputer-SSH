#pragma once

#include <stddef.h>

namespace app {

class TextEditModel {
public:
    TextEditModel(char* storage, size_t capacity);

    bool assign(const char* value);
    bool insert(char value);
    bool moveLeft();
    bool moveRight();
    bool eraseBeforeCursor();
    bool eraseAtCursor();
    void clearSensitive();

    const char* data() const;
    size_t length() const;
    size_t cursor() const;
    size_t capacity() const;

    size_t visibleSlice(char* output,
                        size_t outputCapacity,
                        size_t maximumVisible,
                        bool masked,
                        size_t& viewportStart,
                        size_t& cursorColumn) const;

private:
    char* storage_ = nullptr;
    size_t capacity_ = 0;
    size_t length_ = 0;
    size_t cursor_ = 0;
};

}  // namespace app
