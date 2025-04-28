#include <clingo.hh>
#include <iostream>

using namespace Clingo;

int main(int argc, char const **argv) {
    try {
        Logger logger = [](Clingo::WarningCode, char const *message) {
            std::cerr << message << std::endl;
        };
        Control ctl{{argv+1, size_t(argc-1)}, logger, 20};
        ctl.add("base", {}, "a :- not b. b :- not a.");
        ctl.ground({{"base", {}}});
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

