//
// Copyright (c) 2017 Benjamin Kaufmann
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
#include "catch.hpp"
#include <potassco/application.h>
#include <potassco/program_opts/typed_value.h>
#include <signal.h>
namespace Potassco { namespace ProgramOptions {
namespace Test {
namespace Po = ProgramOptions;

struct MyApp : public Potassco::Application {
	const char* getName()    const { return "TestApp"; }
	const char* getVersion() const { return "1.0"; }
	const char* getUsage()   const { return "[options] [files]"; }
	HelpOpt       getHelpOption() const { return HelpOpt("Print {1=basic|2=extended} help and exit", 2); }
	Po::PosOption getPositional() const { return parsePos; }
	void          error(const char* m) const { err = m; }
	void          info(const char*)    const {}
	void run() { setExitCode(0); }
	void setup() {}
	void initOptions(ProgramOptions::OptionContext& root) {
		Po::OptionGroup g("Basic Options");
		g.addOptions()
			("foo,@,@1", Po::storeTo(foo), "Option on level 1")
			;
		root.add(g);
		Po::OptionGroup g2("E1 Options");
		g2.setDescriptionLevel(Po::desc_level_e1);
		g2.addOptions()
			("file,f", Po::storeTo(input)->composing(), "Input files")
			;
		root.add(g2);
	}
	void validateOptions(const Po::OptionContext&, const Po::ParsedOptions&, const Po::ParsedValues&) {}
	void printHelp(const Po::OptionContext& ctx) {
		desc.clear();
		Po::StringOut out(desc);
		ctx.description(out);
	}
	static bool parsePos(const std::string&, std::string& opt) {
		opt = "file";
		return true;
	}
	using Potassco::Application::verbose;
	typedef std::vector<std::string> StringSeq;
	int         foo;
	StringSeq   input;
	std::string desc;
	mutable std::string err;
};

TEST_CASE("Test application", "[app]") {
	char* argv[] = {(char*)"app", (char*)"-h", (char*)"-V3", (char*)"--vers", (char*)"hallo", 0};
	int   argc = 5;
	MyApp app;
	REQUIRE(app.main(argc, argv) == EXIT_SUCCESS);
	REQUIRE(!app.desc.empty());
	REQUIRE(app.verbose() == 3);
	REQUIRE((!app.input.empty() && app.input[0] == "hallo"));
	REQUIRE(app.desc.find("verbose") != std::string::npos);
	REQUIRE(app.desc.find("file") == std::string::npos);
	REQUIRE(app.desc.find("foo") == std::string::npos);
	REQUIRE(app.desc.find("E1") == std::string::npos);

	argv[1] = (char*)"-h3";
	REQUIRE(app.main(2, argv) == EXIT_FAILURE);
	REQUIRE(app.err.find("'help'") != std::string::npos);
}
TEST_CASE("Test alarm", "[app]") {
	struct TimedApp : MyApp {
		TimedApp() : stop(0) {}
		void run() {
			int i = 0;
			while (!stop) { ++i;  }
			setExitCode(i);
		}
		virtual bool onSignal(int) {
			stop = 1;
			return true;
		}
		volatile sig_atomic_t stop;
	};

	TimedApp app;
	char* argv[] = {(char*)"app", (char*)"--time-limit=1", 0};
	int argc = 2;
	app.main(argc, argv);
	REQUIRE(app.stop == 1);
}
}}}
