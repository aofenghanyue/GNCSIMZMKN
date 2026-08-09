#include "gnc/foundation/version.hpp"

#include <iostream>
#include <string_view>

int main(int argc, char** argv) {
    if (argc == 2) {
        const std::string_view argument{argv[1]};
        if (argument == "--version") {
            std::cout << gnc::foundation::kProjectName << ' '
                      << gnc::foundation::kVersion << '\n';
            return 0;
        }
        if (argument == "--self-check") {
            std::cout << "skeleton-ok gate=" << gnc::foundation::kCurrentGate
                      << '\n';
            return 0;
        }
    }

    std::cout << "GNCZMKN Next is a greenfield bootstrap skeleton.\n"
                 "Available commands: --version, --self-check\n";
    return 0;
}
