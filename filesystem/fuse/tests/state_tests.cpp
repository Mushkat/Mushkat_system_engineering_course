#include "cpu_state.hpp"

#include <catch2/catch.hpp>

using namespace cpufs;

TEST_CASE("write and read pram") {
DeviceState state(2);

const char text[] = "sort";
state.WritePram(0, text, 4, 0);

REQUIRE(state.ReadPram(0) == "sort");
}

TEST_CASE("write and read lram") {
DeviceState state(1);

const char data[] = {'c', 'b', 'a'};
state.WriteLram(0, data, 3, 0);

const auto lram = state.ReadLram(0);
REQUIRE(lram.size() == 3);
REQUIRE(lram[0] == static_cast<std::uint8_t>('c'));
REQUIRE(lram[1] == static_cast<std::uint8_t>('b'));
REQUIRE(lram[2] == static_cast<std::uint8_t>('a'));
}

TEST_CASE("ctrl log accumulates commands") {
DeviceState state(1);

state.WriteCtrlCommand("5");
state.WriteCtrlCommand("3");
state.WriteCtrlCommand("6");

REQUIRE(state.ReadCtrl() == "5\n3\n6\n");
}