//
// Copyright (c) 2014-2017 Benjamin Kaufmann
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

#ifndef TEST_LPCOMPARE_H_INCLUDED
#define TEST_LPCOMPARE_H_INCLUDED

#include <string>
#include <vector>
#include <sstream>
#include <algorithm>
#include <clasp/logic_program.h>
#include <clasp/parser.h>
#include <potassco/aspif_text.h>

namespace Clasp { namespace Test {
inline std::string& trimR(std::string& r) {
	std::string::size_type p = r.find_last_not_of(" ");
	if (p != r.size() - 1) {
		r.erase(p+1);
	}
	return r;
}
inline bool compareProgram(std::stringstream& exp, std::stringstream& actual) {
	std::vector<std::string> lines;
	if (!exp) return !actual;
	while (exp) {
		for (std::string r; std::getline(exp, r) && r != "0";) {
			lines.push_back(trimR(r));
		}
		for (std::string r; std::getline(actual, r) && r != "0";) {
			std::vector<std::string>::iterator it = std::find(lines.begin(), lines.end(), trimR(r));
			if (it == lines.end()) { return false; }
			lines.erase(it);
		}
		if (!lines.empty()) { return false; }
	}
	return true;
}
inline bool findProgram(std::stringstream& what, std::stringstream& actual) {
	std::vector<std::string> lines;
	for (std::string r; std::getline(what, r);) {
		if (r != "0") { lines.push_back(trimR(r)); }
	}
	for (std::string r; std::getline(actual, r) && !lines.empty();) {
		std::vector<std::string>::iterator it;
		if (r != "0" && (it = std::find(lines.begin(), lines.end(), trimR(r))) != lines.end()) {
			lines.erase(it);
		}
	}
	return lines.empty();
}
inline bool compareSmodels(std::stringstream& exp, Asp::LogicProgram& prg) {
	std::stringstream str;
	AspParser::write(prg, str, AspParser::format_smodels);
	return compareProgram(exp, str);
}
inline bool findSmodels(std::stringstream& exp, Asp::LogicProgram& prg) {
	std::stringstream str;
	AspParser::write(prg, str, AspParser::format_smodels);
	return findProgram(exp, str);
}
struct NoStep : Asp::LogicProgramAdapter {
	NoStep(Asp::LogicProgram& p) : Asp::LogicProgramAdapter(p) {}
	void initProgram(bool) {}
	void beginStep()       {}
	void endStep()         {}
};
inline void lpAdd(Asp::LogicProgram& lp, const char* prg) {
	NoStep adapt(lp);
	Potassco::AspifTextInput input(&adapt);
	std::stringstream str;
	str << prg << "\n";
	POTASSCO_REQUIRE(input.accept(str) && input.parse(), "invalid program");
}

}}
#endif
