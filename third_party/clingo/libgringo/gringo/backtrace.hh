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

#ifndef GRINGO_BACKTRACE_HH
#define GRINGO_BACKTRACE_HH

#include <libunwind.h>
#include <cstring>
#include <cxxabi.h>
#include <cstdio>
#include <unistd.h>
#include <unordered_map>
#include <string>
#include <cstdlib>

// TODO: given a little time this could be cleaned up
//       to remove the ugly fixed size strings
//       anyway this is just debug code...

namespace Gringo {

// NOLINTBEGIN(cppcoreguidelines-avoid-c-arrays,modernize-avoid-c-arrays,cppcoreguidelines-no-malloc,cppcoreguidelines-pro-type-vararg,cppcoreguidelines-avoid-magic-numbers,cppcoreguidelines-owning-memory,cppcoreguidelines-pro-bounds-pointer-arithmetic)

inline const char* getExecutableName() {
    static char const * exe = nullptr;
    if (exe == nullptr) {
        char tmp[4096];
        static char buf[4096];
        buf[0] = '\0';
        snprintf(tmp, sizeof(tmp), "/proc/%d/exe", getpid());
        // NOLINTNEXTLINE(bugprone-unused-return-value)
        readlink(tmp, buf, sizeof(tmp));
        exe = buf;
    }
    return exe;
}

inline int getFileAndLine (unw_word_t addr, char *file, int *line) {
    char buf[4096];
    static std::unordered_map<unw_word_t, std::string> cache;
    std::string &s = cache[addr];
    if (!s.empty()) {
        strncpy(buf, s.c_str(), sizeof(buf));
    }
    else {
        snprintf (buf, sizeof(buf), "/home/wv/bin/linux/64/binutils-2.23.1/bin/addr2line -C -e %s -f -i %lx", getExecutableName(), addr);
        FILE* f = popen (buf, "r");
        if (f == nullptr) {
            perror (buf);
            return 0;
        }
        fgets (buf, sizeof(buf), f);
        fgets (buf, sizeof(buf), f);
        pclose(f);
        s = buf;
        const char *pref = "/home/kaminski/svn/wv/Programming/gringo/trunk/";
        if (strncmp(pref, s.c_str(), strlen(pref)) == 0) {
            s = buf + strlen(pref);
            strncpy(buf, s.c_str(), sizeof(buf));
        }
    }
    if (buf[0] != '?') {
        char *p = buf;
        while (*p != ':') { p++; }
        *p++ = 0;
        strncpy (file , buf, 4096);
        sscanf (p,"%d", line);
    }
    else {
        strncpy (file,"unkown", 4096);
        *line = 0;
    }
    return 1;
}

inline void showBacktrace() {
    char name[4096];
    int  status{0};
    unw_cursor_t cursor; unw_context_t uc;
    unw_word_t ip{0};
    unw_word_t sp{0};
    unw_word_t offp{0};

    unw_getcontext(&uc);
    unw_init_local(&cursor, &uc);

    while (unw_step(&cursor) > 0) {
        char file[4096];
        int line = 0;

        name[0] = '\0';
        unw_get_proc_name(&cursor, name, sizeof(name), &offp);
        unw_get_reg(&cursor, UNW_REG_IP, &ip);
        unw_get_reg(&cursor, UNW_REG_SP, &sp);

        if (strcmp(name, "__libc_start_main") == 0 || strcmp(name, "_start") == 0) {
            continue;
        }

        char *realname = abi::__cxa_demangle(name, nullptr, nullptr, &status);
        getFileAndLine((long)ip, file, &line);
        printf("%s in %s:%d\n", status == 0 ? realname : name, file, line);
        free(realname);
    }
}

} // namespace Gringo

// NOLINTEND(cppcoreguidelines-avoid-c-arrays,modernize-avoid-c-arrays,cppcoreguidelines-no-malloc,cppcoreguidelines-pro-type-vararg,cppcoreguidelines-avoid-magic-numbers,cppcoreguidelines-owning-memory,cppcoreguidelines-pro-bounds-pointer-arithmetic)

#endif // GRINGO_BACKTRACE_HH
