// Golden parity: replay every tests/golden/*.json (produced by the web build) through sim::step and compare.
#include <doctest/doctest.h>

#include <algorithm>
#include <filesystem>
#include <fstream>

#include "golden_driver.h"

namespace fs = std::filesystem;
using nlohmann::json;

TEST_CASE("golden fixtures replay to the recorded results") {
    const fs::path dir = fs::path(testkit::repoRoot()) / "tests" / "golden";
    std::vector<fs::path> files;
    for (const auto& e : fs::directory_iterator(dir))
        if (e.path().extension() == ".json") files.push_back(e.path());
    std::sort(files.begin(), files.end());
    REQUIRE(files.size() >= 25);

    for (const auto& f : files) {
        SUBCASE(f.stem().string().c_str()) {
            std::ifstream in(f);
            const json fx = json::parse(in);
            const json got = testkit::runScenario(fx, testkit::repoConfig());
            json want;
            for (const char* k : {"inputs", "samples", "final"}) want[k] = fx.at(k);
            const auto d = testkit::firstDiff(got, want);
            INFO(d.value_or("ok"));
            CHECK_FALSE(d.has_value());
        }
    }
}
