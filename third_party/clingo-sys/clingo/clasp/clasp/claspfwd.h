//
// Copyright (c) 2013-2017 Benjamin Kaufmann
//
// This file is part of Clasp. See http://www.cs.uni-potsdam.de/clasp/
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to
// deal in the Software without restriction, including without limitation the
// rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
// sell copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
// FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
// IN THE SOFTWARE.
//
#ifndef CLASP_CLASP_FWD_H_INCLUDED
#define CLASP_CLASP_FWD_H_INCLUDED
/*!
 * \file
 * \brief Forward declarations of important clasp and potassco types.
 */

namespace Potassco {
class TheoryAtom;
class TheoryTerm;
class TheoryData;
template <class T> struct Span;
struct Heuristic_t;
class BufferedStream;
class AbstractStatistics;
}
//! Root namespace for all types and functions of libclasp.
namespace Clasp {
class SharedContext;
class MinimizeBuilder;
class SharedMinimizeData;
class Configuration;
class Constraint;
class ConstraintInfo;
class Solver;
struct Model;
//! Supported problem types.
struct Problem_t {
	enum Type {Sat = 0, Pb = 1, Asp = 2};
};
typedef Problem_t::Type ProblemType;
class ProgramBuilder;
class ProgramParser;
class SatBuilder;
class PBBuilder;
class ExtDepGraph;
class ConstString;
typedef Potassco::Span<char> StrView;
typedef Potassco::Heuristic_t DomModType;
//! Namespace for types and functions used to define ASP programs.
namespace Asp {
class LogicProgram;
class Preprocessor;
class LpStats;
class PrgAtom;
class PrgBody;
class PrgDisj;
class PrgHead;
class PrgNode;
class PrgDepGraph;
struct PrgEdge;
}}

#endif
