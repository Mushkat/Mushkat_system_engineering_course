#include "cpu_executor.hpp"

#include <catch2/catch.hpp>

using namespace cpufs;

TEST_CASE("executor sorts lram when pram contains sort") {
    UnitState unit;
    unit.pram = "std::sort(ram, ram + size);";
    unit.lram = {5, 1, 4, 2, 3};

    CpuExecutor::Execute(unit);

    REQUIRE(unit.lram == std::vector<std::uint8_t>{1, 2, 3, 4, 5});
}

TEST_CASE("executor leaves data unchanged when no sort requested") {
    UnitState unit;
    unit.pram = "noop";
    unit.lram = {5, 1, 4};

    CpuExecutor::Execute(unit);

    REQUIRE(unit.lram == std::vector<std::uint8_t>{5, 1, 4});
}