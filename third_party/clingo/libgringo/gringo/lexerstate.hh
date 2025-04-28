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

#ifndef GRINGO_LEXERSTATE_HH
#define GRINGO_LEXERSTATE_HH

#include <cstring>
#include <cstdlib>
#include <cstdio>
#include <iterator>
#include <string>
#include <vector>
#include <iostream>
#include <fstream>
#include <cassert>
#include <memory>
#include <potassco/basic_types.h>

namespace Gringo {

using Potassco::StringSpan;

// {{{ declaration of LexerState

template <class T>
class LexerState {
private:
    struct State;

public:
    using Data = T;
    LexerState();
    void start();
    bool eof() const;
    bool push(char const *file, T &&data);
    bool push(std::unique_ptr<std::istream> in, T &&data);
    void pop();
    bool empty() const;
    StringSpan string(int start = 0, int end = 0);
    void step(char s);
    void step();
    int32_t signed_integer() const;
    uint32_t unsigned_integer() const;
    int clingo_number() const;
    int line() const;
    int column() const;
    T const &data() const;
    char *&cursor();
    char *&marker();
    char *&ctxmarker();
    char const *limit() const;
    void fill(size_t n);
    void seek(int offset);
private:
    State const &state() const;
    State &state();

    std::vector<State> states_;
};

// }}}
// {{{ declaration of LexerState<T>::State

template <class T>
struct LexerState<T>::State {
    State(T &&data);
    State(State const &other) = default;
    State(State &&other) noexcept;
    State &operator=(State const &other) = default;
    State &operator=(State &&other) noexcept;
    ~State() noexcept;

    void fill(size_t n);
    void step();
    void next();

    std::unique_ptr<std::istream> in_;
    T data_;
    size_t bufmin{4096};
    size_t bufsize{0};
    char *buffer{nullptr};
    char *start{nullptr};
    char *offset{nullptr};
    char *cursor{nullptr};
    char *limit{nullptr};
    char *marker{nullptr};
    char *ctxmarker{nullptr};
    char *eof{nullptr};
    int line{1};
    bool newline{false};
};

// }}}

// {{{ defintion of LexerState<T>::State

template <class T>
LexerState<T>::State::State(T &&data)
: data_(std::forward<T>(data)) { }

template <class T>
LexerState<T>::State::State(State &&other) noexcept
: data_(std::move(other.data_))
, bufmin(other.bufmin)
, bufsize(other.bufsize)
, start(other.start)
, offset(other.offset)
, cursor(other.cursor)
, limit(other.limit)
, marker(other.marker)
, ctxmarker(other.ctxmarker)
, eof(other.eof)
, line(other.line)
, newline(other.newline) {
    std::swap(other.in_, in_);
    std::swap(other.buffer, buffer);
}

template <class T>
typename LexerState<T>::State &LexerState<T>::State::operator=(State &&other) noexcept {
    data_ = std::move(other.data_);
    bufmin = other.bufmin;
    bufsize = other.bufsize;
    start = other.start;
    offset = other.offset;
    cursor = other.cursor;
    limit = other.limit;
    marker = other.marker;
    ctxmarker = other.ctxmarker;
    eof = other.eof;
    line = other.line;
    newline = other.newline;
    std::swap(other.in_, in_);
    std::swap(other.buffer, buffer);
}

template <class T>
void LexerState<T>::State::fill(size_t n) {
    // NOLINTBEGIN(cppcoreguidelines-pro-bounds-pointer-arithmetic)
    if (eof != nullptr) {
        return;
    }
    if (start > buffer) {
        size_t shift = start - buffer;
        memmove(buffer, start, limit - start);
        start      = buffer;
        offset    -= shift;
        marker    -= shift;
        ctxmarker -= shift;
        limit     -= shift;
        cursor    -= shift;
    }
    size_t inc = n < bufmin ? bufmin : n;
    if (bufsize < inc + (limit - buffer)) {
        bufsize   = inc + (limit - buffer);
        // NOLINTNEXTLINE(cppcoreguidelines-owning-memory,cppcoreguidelines-no-malloc)
        auto *buf = static_cast<char*>(realloc(buffer, bufsize * sizeof(char)));
        start     = buf + (start - buffer);
        cursor    = buf + (cursor - buffer);
        limit     = buf + (limit - buffer);
        marker    = buf + (marker - buffer);
        ctxmarker = buf + (ctxmarker - buffer);
        offset    = buf + (offset - buffer);
        buffer    = buf;
    }
    in_->read(limit, static_cast<std::streamsize>(inc));
    auto read = static_cast<size_t>(in_->gcount());
    limit+= read;
    if (read > 0) {
        newline = *(limit - 1) == '\n';
    }
    // make sure the last line is newline terminated
    if (read < inc && !newline) {
        newline = true;
        ++read;
        *limit++ = '\n';
    }
    if (read < inc) {
        eof = limit;
        *eof++ = '\n';
    }
    // NOLINTEND(cppcoreguidelines-pro-bounds-pointer-arithmetic)
}

template <class T>
void LexerState<T>::State::step() {
    offset = cursor;
    line++;
}

template <class T>
void LexerState<T>::State::next() {
    start = cursor;
}

template <class T>
LexerState<T>::State::~State() noexcept {
    if(buffer != nullptr) {
        // NOLINTNEXTLINE(cppcoreguidelines-owning-memory,cppcoreguidelines-no-malloc)
        free(buffer);
    }
}

// }}}
// {{{ defintion of LexerState

template <class T>
LexerState<T>::LexerState() = default;

template <class T>
void LexerState<T>::start() {
    state().next();
}

template <class T>
bool LexerState<T>::eof() const {
    return state().cursor == state().eof;
}

template <class T>
bool LexerState<T>::push(std::unique_ptr<std::istream> in, T &&data) {
    states_.emplace_back(std::forward<T>(data));
    state().in_ = std::move(in);
    return true;
}

template <class T>
bool LexerState<T>::push(char const *file, T &&data) {
    if (strcmp(file, "-") == 0) {
        states_.emplace_back(std::forward<T>(data));
        state().in_.reset(new std::istream(std::cin.rdbuf(nullptr)));
        return true;
    }
    std::unique_ptr<std::ifstream> ifs(new std::ifstream(file));
    if (ifs->is_open()) {
        states_.emplace_back(std::forward<T>(data));
        state().in_.reset(ifs.release());
        return true;
    }
    return false;
}

template <class T>
void LexerState<T>::pop() {
    states_.pop_back();
}

template <class T>
bool LexerState<T>::empty() const {
    return states_.empty();
}

template <class T>
StringSpan LexerState<T>::string(int start, int end) {
    auto b = state().start + start;
    auto e = state().cursor - end;
    return {b, static_cast<size_t>(e - b)};
}

template <class T>
void LexerState<T>::step(char s) {
    // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
    for (char *c = state().start; c != state().cursor; ++c) {
        if (*c == s) {
            step();
        }
    }
}

template <class T>
void LexerState<T>::step() {
    state().step();
}

template <class T>
int32_t LexerState<T>::signed_integer() const {
    int32_t r = 0;
    int32_t s = 1;
    char *i = state().start;
    if (i != state().cursor && *i == '-') {
        s = -1;
        // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
        ++i;
    }
    // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
    for (; i != state().cursor; i++) {
        r *= 10;
        r += *i - '0';
    }
    return s * r;
}

template <class T>
uint32_t LexerState<T>::unsigned_integer() const {
    uint32_t r = 0;
    // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
    for (char *i = state().start; i != state().cursor; i++) {
        r *= 10;
        r += *i - '0';
    }
    return r;
}

template <class T>
int LexerState<T>::clingo_number() const {
    int s = 0;
    int base = 10;
    if (state().cursor - state().start >= 2) {
        if (strncmp("0b", state().start, 2) == 0) {
            base = 2;
            s = 2;
        }
        else if (strncmp("0o", state().start, 2) == 0) {
            base = 8;
            s = 2;
        }
        else if (strncmp("0x", state().start, 2) == 0) {
            base = 16;
            s = 2;
        }
    }
    int r = 0;
    // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
    for (char *i = state().start + s; i != state().cursor; i++) {
        r *= base;
        if (*i <= '9') {
            r += *i - '0';
        }
        else if (*i <= 'F') {
            r += *i - 'A' + 10;
        }
        else {
            r += *i - 'a' + 10;
        }
    }
    return r;
}

template <class T>
int LexerState<T>::line() const {
    return state().line;
}

template <class T>
int LexerState<T>::column() const {
    return static_cast<int>(state().cursor - state().offset + 1);
}

template <class T>
T const &LexerState<T>::data() const {
    return state().data_;
}

template <class T>
char *&LexerState<T>::cursor() {
    return state().cursor;
}

template <class T>
char *&LexerState<T>::marker() {
    return state().marker;
}

template <class T>
char *&LexerState<T>::ctxmarker() {
    return state().ctxmarker;
}

template <class T>
char const *LexerState<T>::limit() const {
    return state().limit;
}

template <class T>
void LexerState<T>::fill(size_t n) {
    state().fill(n);
}

template <class T>
typename LexerState<T>::State const &LexerState<T>::state() const {
    assert(!empty());
    return states_.back();
}

template <class T>
typename LexerState<T>::State &LexerState<T>::state() {
    assert(!empty());
    return states_.back();
}

template <class T>
void LexerState<T>::seek(int offset) {
    state().cursor = state().start + offset;
    state().ctxmarker = state().cursor;
}

// }}}

} // namespace Gringo

#endif // GRINGO_LEXERSTATE_HH
