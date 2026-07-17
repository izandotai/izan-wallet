#define DOCTEST_CONFIG_IMPLEMENT
#include <doctest/doctest.h>

#include <string_view>

#ifdef _WIN32
#include "keyd/child.hpp"
#endif

int main(int argc, char** argv)
{
    // The keyd spawn tests re-execute this binary as the trust-plane
    // child; that mode must never enter the test runner.
#ifdef _WIN32
    if (argc > 1 && std::string_view(argv[1]) == "--keyd-child")
        return izan::keyd::child_main(argc, argv);
#endif
    return doctest::Context(argc, argv).run();
}
