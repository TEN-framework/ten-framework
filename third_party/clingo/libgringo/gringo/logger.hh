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

#ifndef GRINGO_LOGGER_HH
#define GRINGO_LOGGER_HH

#include <bitset>
#include <cstdio>
#include <sstream>
#include <stdexcept>
#include <memory>
#include <functional>

#include <iostream>

namespace Gringo {

// {{{1 declaration of GringoError

class GringoError : public std::runtime_error {
public:
    GringoError(char const *msg)
    : std::runtime_error(msg) { }
};

class MessageLimitError : public std::runtime_error {
public:
    MessageLimitError(char const *msg)
    : std::runtime_error(msg) { }
};

// {{{1 declaration of Logger

enum class Errors : int {
    Success   = 0,
    Runtime   = 1,
    Logic     = 2,
    Bad_alloc = 3,
    Unknown   = 4
};

enum class Warnings : int {
    OperationUndefined = 0,
    RuntimeError       = 1,
    AtomUndefined      = 2,
    FileIncluded       = 3,
    VariableUnbounded  = 4,
    GlobalVariable     = 5,
    Other              = 6,
};

class Logger {
public:
    using Printer = std::function<void (Warnings, char const *)>;
    Logger(Printer p = nullptr, unsigned limit = 20)
    : p_(std::move(p))
    , limit_(limit) { }

    bool check(Errors id);
    bool check(Warnings id);
    bool hasError() const;
    void enable(Warnings id, bool enable);
    void print(Warnings code, char const *msg);

private:
    Printer p_;
    unsigned limit_;
    std::bitset<static_cast<int>(Warnings::Other)+1> disabled_;
    bool error_ = false;
};

// }}}1

// {{{1 definition of Logger

inline bool Logger::check(Warnings id) {
    if (id == Warnings::RuntimeError) {
        if (limit_ == 0 && error_) {
            throw MessageLimitError("too many messages.");
        }
        if (limit_ > 0) {
            --limit_;
        }
        error_ = true;
        return true;
    }
    if (limit_  == 0 && error_) {
        throw MessageLimitError("too many messages.");
    }
    return !disabled_[static_cast<int>(id)] && limit_ > 0 && (--limit_, true);
}

inline bool Logger::hasError() const {
    return error_;
}

inline void Logger::enable(Warnings id, bool enabled) {
    disabled_[static_cast<int>(id)] = !enabled;
}

inline void Logger::print(Warnings code, char const *msg) {
    if (p_) {
        p_(code, msg);
    }
    else {
        // NOLINTNEXTLINE(cppcoreguidelines-pro-type-vararg)
        fprintf(stderr, "%s\n", msg);
        fflush(stderr);
    }
}

// {{{1 definition of Report

class Report {
public:
    Report(Logger &p, Warnings code)
    : p_(p), code_(code) { }
    Report(Report const &other) = delete;
    Report(Report &&other) = delete;
    Report &operator=(Report const &other) = delete;
    Report &operator=(Report &&other) = delete;
    ~Report() {
        p_.print(code_, out_.str().c_str());
    }
    std::ostringstream &out() {
        return out_;
    }

private:
    std::ostringstream out_;
    Logger &p_;
    Warnings code_;
};

// }}}1

} // namespace GRINGO

// NOLINTNEXTLINE(cppcoreguidelines-macro-usage)
#define GRINGO_REPORT(p, id) \
if (!(p).check(id)) { } \
else Gringo::Report(p, id).out()

#endif // GRINGO_LOGGER_HH
