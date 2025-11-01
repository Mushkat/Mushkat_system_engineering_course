#include <promise.hpp>

#include <string>
#include <thread>
#include <variant>

using namespace std::chrono_literals;

TEST_CASE("Future", "[future,unit]") {
  SECTION("get value") {
    stdlike::Promise<int> p;
    stdlike::Future<int> f = p.MakeFuture();

    p.SetValue(42);
    REQUIRE(42 == f.Get());
  }

  SECTION("Throw Exception") {
    class TestException : public std::runtime_error {
     public:
      TestException() : std::runtime_error("Test") {
      }
    };

    stdlike::Promise<int> p;
    auto f = p.MakeFuture();

    try {
      throw TestException();
    } catch (...) {
      p.SetException(std::current_exception());
    }

    REQUIRE_THROWS_AS(f.Get(), TestException);
  }

  SECTION("wait for value") {
    stdlike::Promise<std::string> p;
    auto f = p.MakeFuture();

    std::thread producer([p = std::move(p)]() mutable {
      std::this_thread::sleep_for(2s);
      p.SetValue("Hi");
    });

    REQUIRE("Hi" == f.Get());

    producer.join();
  }

  SECTION("wait for exception") {
    stdlike::Promise<std::string> p;
    auto f = p.MakeFuture();

    std::thread producer([p = std::move(p)]() mutable {
      std::this_thread::sleep_for(2s);
      try {
        throw TestException();
      } catch (...) {
        p.SetException(std::current_exception());
      }
    });

    REQUIRE_THROWS_AS(f.Get(), TestException);

    producer.join();
  }

  template <typename T>
  void Drop(T value) {
    (void)value;
  }
  SECTION("drop future") {
    stdlike::Promise<std::string> p;
    auto f = p.MakeFuture();

    Drop(std::move(f));

    std::thread producer([p = std::move(p)]() mutable {
      p.SetValue("Hi");
    });

    producer.join();
    REQUIRE("" == f.Get());
  }

  SECTION("drop promise") {
    Promise<std::string> p;
    auto f = p.MakeFuture();

    p.SetValue("Test");
    Drop(std::move(p));

    REQUIRE("Test" == f.Get());
  }

  SECTION("futures") {
    stdlike::Promise<int> p0;
    stdlike::Promise<int> p1;
    stdlike::Promise<int> p2;

    auto f0 = p0.MakeFuture();
    auto f1 = p1.MakeFuture();
    auto f2 = p2.MakeFuture();

    std::thread producer0([&]() {
      std::this_thread::sleep_for(3s);
      p0.SetValue(0);
    });

    std::thread producer1([&]() {
      std::this_thread::sleep_for(1s);
      p1.SetValue(1);
    });

    std::thread producer2([&]() {
      std::this_thread::sleep_for(2s);
      p2.SetValue(2);
    });

    REQUIRE(0 == f0.Get());
    REQUIRE(1 == f1.Get());
    REQUIRE(2 == f2.Get());

    producer0.join();
    producer1.join();
    producer2.join();
  }
}