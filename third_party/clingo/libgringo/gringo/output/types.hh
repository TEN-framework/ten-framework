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

#ifndef GRINGO_OUTPUT_TYPES_HH
#define GRINGO_OUTPUT_TYPES_HH

#include <gringo/domain.hh>
#include <gringo/types.hh>

namespace Gringo { namespace Output {

using ClauseId = std::pair<Id_t, Id_t>;
using ClauseSpan = Potassco::Span<ClauseId>;
using FormulaId = std::pair<Id_t, Id_t>;
using Formula = std::vector<ClauseId>;
using AssignmentLookup = std::function<std::pair<bool, Potassco::Value_t>(unsigned)>; // (isExternal, truthValue)
using IsTrueLookup = std::function<bool(unsigned)>;
class OutputPredicates;

enum class OutputDebug { NONE, TEXT, TRANSLATE, ALL };
enum class OutputFormat { TEXT, INTERMEDIATE, SMODELS, REIFY };

} } // namespace Output Gringo

#endif // GRINGO_OUTPUT_TYPES_HH
