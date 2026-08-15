#include "gnc/foundation/version.hpp"

#include <cstdlib>
#include <iostream>

int main() {
    if (gnc::foundation::kProjectName.empty() ||
        gnc::foundation::kVersion.empty() ||
        gnc::foundation::kCurrentGate != "R1") {
        std::cerr << "bootstrap metadata is invalid\n";
        return EXIT_FAILURE;
    }

    std::cout << "foundation skeleton checks passed\n";
    return EXIT_SUCCESS;
}
