#include "path_utils.hpp"

#include <catch2/catch.hpp>

using namespace cpufs;

TEST_CASE("parse root path") {
const auto p = ParsePath("/");
REQUIRE(p.kind == PathKind::Root);
}

TEST_CASE("parse ctrl path") {
const auto p = ParsePath("/ctrl");
REQUIRE(p.kind == PathKind::CtrlFile);
}

TEST_CASE("parse unit dir path") {
const auto p = ParsePath("/unit3");
REQUIRE(p.kind == PathKind::UnitDir);
REQUIRE(p.unit_index == 3);
}

TEST_CASE("parse pram path") {
const auto p = ParsePath("/unit1/pram");
REQUIRE(p.kind == PathKind::PramFile);
REQUIRE(p.unit_index == 1);
}

TEST_CASE("parse lram path") {
const auto p = ParsePath("/unit2/lram");
REQUIRE(p.kind == PathKind::LramFile);
REQUIRE(p.unit_index == 2);
}

TEST_CASE("invalid path rejected") {
const auto p = ParsePath("/bad/path");
REQUIRE(p.kind == PathKind::Invalid);
}