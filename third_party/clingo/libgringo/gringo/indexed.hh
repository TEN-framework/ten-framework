// {{{ MIT License

// Copyright 2017 Roland Kaminski

// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to
// deal in the Software without restriction, including without limitation the
// rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
// sell copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:

// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.

// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
// FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
// IN THE SOFTWARE.

// }}}

#ifndef GRINGO_INDEXED_HH
#define GRINGO_INDEXED_HH

#include <vector>

namespace Gringo {

// {{{ declaration of Indexed

template <class T, class R = unsigned>
struct Indexed {
public:
    using ValueType = T;
    using IndexType = R;
    template <class... Args>
    IndexType emplace(Args&&... args);
    IndexType insert(ValueType &&value);
    ValueType erase(IndexType uid);
    ValueType &operator[](IndexType uid);
private:
    std::vector<ValueType> values_;
    std::vector<IndexType> free_;
};

// }}}

// {{{ defintion of Indexed<T>

template <class T, class R>
template <class... Args>
typename Indexed<T, R>::IndexType Indexed<T, R>::emplace(Args&&... args) {
    if (free_.empty()) {
        values_.emplace_back(std::forward<Args>(args)...);
        return IndexType(values_.size() - 1);
    }
    IndexType uid = free_.back();
    values_[uid] = ValueType(std::forward<Args>(args)...);
    free_.pop_back();
    return uid;
}

template <class T, class R>
typename Indexed<T, R>::IndexType Indexed<T, R>::insert(ValueType &&value) {
    if (free_.empty()) {
        values_.push_back(std::move(value));
        return IndexType(values_.size() - 1);
    }
    IndexType uid = free_.back();
    values_[uid] = std::move(value);
    free_.pop_back();
    return uid;
}

template <class T, class R>
typename Indexed<T, R>::ValueType Indexed<T, R>::erase(IndexType uid) {
    ValueType val(std::move(values_[uid]));
    if (uid + 1 == values_.size()) { values_.pop_back(); }
    else { free_.push_back(uid); }
    return val;
}

template <class T, class R>
typename Indexed<T, R>::ValueType &Indexed<T, R>::operator[](typename Indexed<T, R>::IndexType uid) {
    return values_[uid];
}

// }}}

} // namespace Gringo

#endif  // GRINGO_INDEXED_HH
