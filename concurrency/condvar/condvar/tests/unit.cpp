#include "../condvar.hpp"

#include <atomic>
#include <thread>
#include <mutex>
#include <chrono>

#include <catch2/catch_all.hpp>

using namespace std::chrono_literals;

namespace {
class Event {
 public:
  void Await() {
    std::unique_lock lock(mutex_);
    while (!set_) {
      set_cond_.Wait(lock);
    }
  }

  void Set() {
    std::lock_guard guard(mutex_);
    set_ = true;
    set_cond_.NotifyOne();
  }

  void Reset() {
    std::lock_guard guard(mutex_);
    set_ = false;
  }

 private:
  bool set_{false};
  std::mutex mutex_;
  stdlike::CondVar set_cond_;
};

class Latch {
 public:
  void Await() {
    std::unique_lock lock(mutex_);
    while (!released_) {
      released_cond_.Wait(lock);
    }
  }

  void Release() {
    std::lock_guard guard(mutex_);
    released_ = true;
    released_cond_.NotifyAll();
  }

  void Reset() {
    std::lock_guard guard(mutex_);
    released_ = false;
  }

 private:
  bool released_{false};
  std::mutex mutex_;
  stdlike::CondVar released_cond_;
};
}  // namespace

TEST_CASE("notify one", "[condvar,unit]") {
  Event pass;

  for (size_t i = 0; i < 3; ++i) {
    pass.Reset();

    bool passed = false;

    std::thread waiter([&]() {
      {
        twist::test::util::ThreadCPUTimer cpu_timer;
        pass.Await();
        REQUIRE(cpu_timer.Elapsed() < 200ms);
      }
      passed = true;
    });

    std::this_thread::sleep_for(1s);
    REQUIRE(!passed);

    pass.Set();
    waiter.join();

    REQUIRE(passed);
  }
}

TEST_CASE("notify all", "[condvar]") {
  Latch latch;

  for (size_t i = 0; i < 3; ++i) {
    latch.Reset();

    std::atomic<size_t> passed{0};

    auto wait_routine = [&]() {
      latch.Await();
      ++passed;
    };

    std::thread t1(wait_routine);
    std::thread t2(wait_routine);

    std::this_thread::sleep_for(1s);
    REQUIRE(0 == passed.load());

    latch.Release();

    t1.join();
    t2.join();

    REQUIRE(2 == passed.load());
  }
}

TEST_CASE("notify many times", "[condvar]") {
  static const size_t kIterations = 1000'000;

  stdlike::CondVar cv;
  for (size_t i = 0; i < kIterations; ++i) {
    cv.NotifyOne();
  }
}
