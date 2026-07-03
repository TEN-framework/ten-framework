//
// Copyright (c) 2015-2017 Benjamin Kaufmann
//
// This file is part of Potassco.
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

#include <potassco/aspif.h>
#include <potassco/aspif_text.h>
#include <potassco/convert.h>
#include <potassco/application.h>
#include <potassco/program_opts/typed_value.h>
#include <fstream>
#include <iostream>
#include <cctype>
#include <cstdlib>

using namespace Potassco::ProgramOptions;

class LpConvert : public Potassco::Application {
public:
	virtual const char* getName()       const { return "lpconvert"; }
	virtual const char* getVersion()    const { return "1.0.0"; }
	virtual PosOption   getPositional() const { return &positional; }
	virtual const char* getUsage()      const {
		return
			"[options] [<file>]\n"
			"Convert program in <file> or standard input";
	}
	virtual void initOptions(OptionContext& root);
	virtual void validateOptions(const OptionContext&, const ParsedOptions&, const ParsedValues&) {}
	virtual void setup() {}
	virtual void run();
	virtual void printVersion() {
		Potassco::Application::printVersion();
		printf("libpotassco version %s\n", LIB_POTASSCO_VERSION);
		printf("Copyright (C) Benjamin Kaufmann\n");
		printf("License: The MIT License <https://opensource.org/licenses/MIT>\n");
		fflush(stdout);
	}
private:
	static bool positional(const std::string&, std::string& optOut) {
		optOut = "input";
		return true;
	}
	static int error(int line, const char* what) {
		fprintf(stderr, "*** ERROR: In line %d: %s\n", line, what);
		static_cast<LpConvert*>(Application::getInstance())->exit(EXIT_FAILURE);
		return EXIT_FAILURE;
	}
	std::string input_;
	std::string output_;
	bool potassco_;
	bool filter_;
	bool text_;
};

void LpConvert::initOptions(OptionContext& root) {
	OptionGroup convert("Conversion Options");
	convert.addOptions()
		("input,i,@2", storeTo(input_),  "Input file")
		("potassco,p", flag(potassco_ = false), "Enable potassco extensions")
		("filter,f"  , flag(filter_ = false), "Hide converted potassco predicates")
		("output,o"  , storeTo(output_)->arg("<file>"), "Write output to <file> (default: stdout)")
		("text,t"    , flag(text_ = false), "Convert to ground text format")
	;
	root.add(convert);
}
void LpConvert::run() {
	std::ifstream iFile;
	std::ofstream oFile;
	if (!input_.empty() && input_ != "-") {
		iFile.open(input_.c_str());
		POTASSCO_EXPECT(iFile.is_open(), "Could not open input file!");
	}
	if (!output_.empty() && output_ != "-") {
		POTASSCO_EXPECT(input_ != output_, "Input and output must be different!");
		oFile.open(output_.c_str());
		POTASSCO_EXPECT(oFile.is_open(), "Could not open output file!");
	}
	std::istream& in = iFile.is_open() ? iFile : std::cin;
	std::ostream& os = oFile.is_open() ? oFile : std::cout;
	Potassco::AspifTextOutput text(os);
	POTASSCO_EXPECT(in.peek() == 'a' || std::isdigit(in.peek()), "Unrecognized input format!");
	if (in.peek() == 'a') {
		Potassco::SmodelsOutput  writer(os, potassco_, 0);
		Potassco::SmodelsConvert smodels(writer, potassco_);
		Potassco::readAspif(in, !text_ ? static_cast<Potassco::AbstractProgram&>(smodels) : text, &error);
	}
	else {
		Potassco::AspifOutput aspif(os);
		Potassco::SmodelsInput::Options opts;
		if (potassco_) {
			opts.enableClaspExt().convertEdges().convertHeuristic();
			if (filter_) { opts.dropConverted(); }
		}
		Potassco::readSmodels(in, !text_? static_cast<Potassco::AbstractProgram&>(aspif) : text, &error, opts);
	}
	iFile.close();
	oFile.close();
}

int main(int argc, char** argv) {
	LpConvert app;
	return app.main(argc, argv);
}
