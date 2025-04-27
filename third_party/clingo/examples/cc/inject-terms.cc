#include <clingo.hh>
#include <iostream>
#include <sstream>

using namespace Clingo;

// inject terms via a callback
void ground_callback(Location, char const *name, SymbolSpan args, SymbolSpanCallback ret) {
    if (strcmp(name, "c") == 0 && args.size() == 0) {
        ret({Number(42), Number(43)});
    }
}

int main(int argc, char const **argv) {
    try {
        Logger logger = [](Clingo::WarningCode, char const *message) {
            std::cerr << message << std::endl;
        };
        Control ctl{{argv+1, size_t(argc-1)}, logger, 20};

        // define a constant in string form
        std::ostringstream out;
        out << "#const d=" << Number(23) << ".";
        ctl.add("base", {}, out.str().c_str());

        // define a constant via the AST
        AST::with_builder(ctl, [](AST::ProgramBuilder &b) {
            Location loc{"<generated>", "<generated>", 1, 1, 1, 1};
            b.add(AST::Node(AST::Type::Definition, loc, "e", AST::Node(AST::Type::SymbolicTerm, loc, Number(24)), false));
        });

        ctl.add("base", {}, "p(@c()). p(d). p(e).");
        ctl.ground({{"base", {}}}, ground_callback);
        for (auto &m : ctl.solve()) {
            std::cout << "Model:";
            for (auto &atom : m.symbols()) {
                std::cout << " " << atom;
            }
            std::cout << "\n";
        }
    }
    catch (std::exception const &e) {
        std::cerr << "example failed with: " << e.what() << std::endl;
    }
}

