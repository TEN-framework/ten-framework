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

#ifndef GRINGO_OUTPUT_BACKENDS_HH
#define GRINGO_OUTPUT_BACKENDS_HH

#include <gringo/backend.hh>
#include <potassco/convert.h>
#include <potassco/aspif.h>
#include <potassco/smodels.h>
#include <potassco/theory_data.h>

namespace Gringo { namespace Output {

class SmodelsFormatBackend : public Potassco::SmodelsConvert {
public:
    SmodelsFormatBackend(std::ostream& os)
    : Potassco::SmodelsConvert(out_, true)
    , out_(os, true, 0) { }
private:
    Potassco::SmodelsOutput out_;
};

using IntermediateFormatBackend = Potassco::AspifOutput;

} } // namespace Output Gringo

#endif // GRINGO_OUTPUT_BACKENDS_HH

