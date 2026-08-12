#include "TextEditModel.h"

#include <string.h>

#include "AppConfig.h"

namespace app {

TextEditModel::TextEditModel(char* storage, size_t capacity) : storage_(storage), capacity_(capacity) {
    if (storage_ == nullptr || capacity_ == 0) {
        storage_ = nullptr;
        capacity_ = 0;
        return;
    }
    while (length_ + 1 < capacity_ && storage_[length_] != '\0') {
        ++length_;
    }
    storage_[length_] = '\0';
    cursor_ = length_;
}

bool TextEditModel::assign(const char* value) {
    if (storage_ == nullptr || value == nullptr) {
        return false;
    }
    const size_t valueLength = strlen(value);
    if (valueLength >= capacity_) {
        return false;
    }
    memmove(storage_, value, valueLength + 1);
    length_ = valueLength;
    cursor_ = length_;
    return true;
}

bool TextEditModel::insert(char value) {
    const unsigned char byte = static_cast<unsigned char>(value);
    if (storage_ == nullptr || byte < 0x20U || byte == 0x7fU || length_ + 1 >= capacity_) {
        return false;
    }
    memmove(storage_ + cursor_ + 1, storage_ + cursor_, length_ - cursor_ + 1);
    storage_[cursor_] = value;
    ++cursor_;
    ++length_;
    return true;
}

bool TextEditModel::moveLeft() {
    if (cursor_ == 0) {
        return false;
    }
    --cursor_;
    return true;
}

bool TextEditModel::moveRight() {
    if (cursor_ >= length_) {
        return false;
    }
    ++cursor_;
    return true;
}

bool TextEditModel::eraseBeforeCursor() {
    if (storage_ == nullptr || cursor_ == 0) {
        return false;
    }
    memmove(storage_ + cursor_ - 1, storage_ + cursor_, length_ - cursor_ + 1);
    --cursor_;
    --length_;
    return true;
}

bool TextEditModel::eraseAtCursor() {
    if (storage_ == nullptr || cursor_ >= length_) {
        return false;
    }
    memmove(storage_ + cursor_, storage_ + cursor_ + 1, length_ - cursor_);
    --length_;
    return true;
}

void TextEditModel::clearSensitive() {
    if (storage_ != nullptr && capacity_ > 0) {
        secureClear(storage_, capacity_);
        storage_[0] = '\0';
    }
    length_ = 0;
    cursor_ = 0;
}

const char* TextEditModel::data() const {
    return storage_ == nullptr ? "" : storage_;
}

size_t TextEditModel::length() const {
    return length_;
}

size_t TextEditModel::cursor() const {
    return cursor_;
}

size_t TextEditModel::capacity() const {
    return capacity_;
}

size_t TextEditModel::visibleSlice(char* output,
                                   size_t outputCapacity,
                                   size_t maximumVisible,
                                   bool masked,
                                   size_t& viewportStart,
                                   size_t& cursorColumn) const {
    viewportStart = 0;
    cursorColumn = 0;
    if (output == nullptr || outputCapacity == 0 || maximumVisible == 0 || storage_ == nullptr) {
        if (output != nullptr && outputCapacity > 0) {
            output[0] = '\0';
        }
        return 0;
    }

    const size_t outputLimit = outputCapacity - 1;
    const size_t visibleLimit = maximumVisible < outputLimit ? maximumVisible : outputLimit;
    viewportStart = cursor_ > visibleLimit ? cursor_ - visibleLimit : 0;
    if (viewportStart + visibleLimit > length_ && length_ > visibleLimit) {
        viewportStart = length_ - visibleLimit;
    }
    const size_t remaining = length_ - viewportStart;
    const size_t copied = remaining < visibleLimit ? remaining : visibleLimit;
    for (size_t index = 0; index < copied; ++index) {
        output[index] = masked ? '*' : storage_[viewportStart + index];
    }
    output[copied] = '\0';
    cursorColumn = cursor_ - viewportStart;
    return copied;
}

}  // namespace app
