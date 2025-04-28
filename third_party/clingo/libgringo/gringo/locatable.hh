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

#ifndef GRINGO_LOCATABLE_HH
#define GRINGO_LOCATABLE_HH

#include <gringo/symbol.hh>
#include <gringo/utility.hh>

namespace Gringo {

// {{{ declaration of Location

struct Location {
    Location(String beginFilename, unsigned beginLine, unsigned beginColumn, String endFilename, unsigned endLine, unsigned endColumn);
    Location(Location const &other) noexcept = default;
    Location(Location &&other) noexcept = default;
    Location &operator=(Location const &other) noexcept = default;
    Location &operator=(Location &&other) noexcept = default;
    ~Location() noexcept = default;

    String beginFilename;
    String endFilename;
    unsigned beginLine;
    unsigned endLine;
    unsigned beginColumn;
    unsigned endColumn;

    Location operator+(Location const &other) const;
    bool operator<(Location const &other) const;
    bool operator==(Location const &other) const;
};

std::ostream &operator<<(std::ostream &out, Location const &loc);

// }}}
// {{{ declaration of Locatable

class Locatable {
public:
    Locatable() = default;
    Locatable(Locatable const &other) = default;
    Locatable(Locatable && other) noexcept = default;
    Locatable &operator=(Locatable const &other) = default;
    Locatable &operator=(Locatable &&other) noexcept = default;
    virtual ~Locatable() noexcept = default;

    virtual Location const &loc() const = 0;
    virtual void loc(Location const &loc) = 0;
};

// }}}
// {{{ declaration of LocatableClass

template <class T>
class LocatableClass : public T {
public:
    template <typename... Args>
    LocatableClass(Location const &loc, Args&&... args);
    LocatableClass(LocatableClass const &other) = default;
    LocatableClass(LocatableClass &&other) noexcept = default;
    LocatableClass &operator=(LocatableClass const &other) = default;
    LocatableClass &operator=(LocatableClass &&other) noexcept = default;
    virtual ~LocatableClass() noexcept = default;

    virtual void loc(Location const &loc) final;
    virtual Location const &loc() const final;
private:
    Location loc_;
};

// }}}
// {{{ declaration of make_locatable<T,Args...>

template <class T, class... Args>
std::unique_ptr<T> make_locatable(Location const &loc, Args&&... args);

// }}}

// {{{ defintion of Location

inline Location::Location(String beginFilename, unsigned beginLine, unsigned beginColumn, String endFilename, unsigned endLine, unsigned endColumn)
    : beginFilename(beginFilename)
    , endFilename(endFilename)
    , beginLine(beginLine)
    , endLine(endLine)
    , beginColumn(beginColumn)
    , endColumn(endColumn) { }

inline Location Location::operator+(Location const &other) const {
    return {beginFilename, beginLine, beginColumn, other.endFilename, other.endLine, other.endColumn};
}

inline bool Location::operator<(Location const &other) const {
    if (beginFilename != other.beginFilename) { return beginFilename < other.beginFilename; }
    if (endFilename != other.endFilename) { return endFilename < other.endFilename; }
    if (beginLine != other.beginLine) { return beginLine < other.beginLine; }
    if (endLine != other.endLine) { return endLine < other.endLine; }
    if (beginColumn != other.beginColumn) { return beginColumn < other.beginColumn; }
    return endColumn < other.endColumn;
}

inline bool Location::operator==(Location const &other) const {
    return beginFilename == other.beginFilename &&
           endFilename == other.endFilename &&
           beginLine == other.beginLine &&
           endLine == other.endLine &&
           beginColumn == other.beginColumn &&
           endColumn == other.endColumn;
}

inline std::ostream &operator<<(std::ostream &out, Location const &loc) {
    out << loc.beginFilename << ":" << loc.beginLine << ":" << loc.beginColumn;
    if (loc.beginFilename != loc.endFilename) {
        out << "-" << loc.endFilename << ":" << loc.endLine << ":" << loc.endColumn;
    }
    else if(loc.beginLine != loc.endLine) {
        out << "-" << loc.endLine << ":" << loc.endColumn;
    }
    else if (loc.beginColumn != loc.endColumn) {
        out << "-" << loc.endColumn;
    }
    return out;
}

// }}}
// {{{ defintion of LocatableClass<T>

template <class T>
template <typename... Args>
LocatableClass<T>::LocatableClass(Location const &loc, Args&&... args)
    : T(std::forward<Args>(args)...)
    , loc_(loc) { }

template <class T>
Location const &LocatableClass<T>::loc() const {
    return loc_;
}

template <class T>
void LocatableClass<T>::loc(Location const &loc) {
    loc_ = loc;
}

// }}}
// {{{ defintion of make_locatable<T, Args>

template <class T, class... Args>
std::unique_ptr<T> make_locatable(Location const &loc, Args&&... args) {
    return gringo_make_unique<LocatableClass<T>>(loc, std::forward<Args>(args)...);
}

// }}}

} // namespace Gringo

#endif // GRINGO_LOCATABLE_HH
