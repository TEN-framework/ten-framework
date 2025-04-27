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

#ifdef CLINGO_WITH_LUA
#   include <luaclingo.h>
#endif
#include "clingo/clingo_app.hh"
#include "clingo/scripts.hh"
#include <iterator>

class ExitException : public std::exception {
public:
    ExitException(int status) : status_(status) {
        std::ostringstream oss;
        oss << "exited with status: " << status_;
        msg_ = oss.str();
    }
    int status() const { return status_; }
    char const *what() const noexcept { return msg_.c_str(); }
    ~ExitException() = default;
private:
    std::string msg_;
    int status_;
};

struct WebApp : Gringo::ClingoApp {
    void exit(int status) const {
        throw ExitException(status);
    }
};

extern "C" int run(char const *program, char const *options) {
    try {
#ifdef CLINGO_WITH_LUA
        Gringo::g_scripts() = Gringo::Scripts();
        clingo_register_lua_(nullptr);
#endif
        std::streambuf* orig = std::cin.rdbuf();
        auto exit(Gringo::onExit([orig]{ std::cin.rdbuf(orig); }));
        std::istringstream input(program);
        std::cin.rdbuf(input.rdbuf());
        std::vector<std::vector<char>> opts;
        opts.emplace_back(std::initializer_list<char>{'c','l','i','n','g','o','\0'});
        std::istringstream iss(options);
        for (std::istream_iterator<std::string> it(iss), ie; it != ie; ++it) {
            opts.emplace_back(it->c_str(), it->c_str() + it->size() + 1);
        }
        std::vector<char*> args;
        for (auto &opt : opts) {
            args.emplace_back(opt.data());
        }
        WebApp app;
        args.emplace_back(nullptr);
        return app.main(args.size()-2, args.data());
    }
    catch (ExitException const &e) {
        return e.status();
    }
}

