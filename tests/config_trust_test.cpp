#include <doctest/doctest.h>

#include <fstream>
#include <sstream>
#include <string>

#include "domain/config/config_trust.hpp"

using izan::config::Trust;

namespace {

std::string slurp(const std::string& path)
{
    std::ifstream f(path, std::ios::binary);
    REQUIRE(f);
    std::ostringstream out;
    out << f.rdbuf();
    return out.str();
}

}

// This doubles as the drift guard: editing a shipped config without
// updating the pinned digest table turns the suite red.
TEST_CASE("config trust: shipped defaults verify, any drift is Modified")
{
    const std::string dir = std::string(IZAN_SOURCE_DIR) + "/data/";
    for (const char* name : { "chains.json", "tokens.json" }) {
        std::string contents = slurp(dir + name);
        CHECK(izan::config::classify(name, contents) == Trust::ShippedDefault);

        contents += " "; // one byte of drift
        CHECK(izan::config::classify(name, contents) == Trust::Modified);
    }

    // A filename we never shipped can vouch for nothing.
    CHECK(izan::config::classify("extra.json", "{}") == Trust::Modified);
}
