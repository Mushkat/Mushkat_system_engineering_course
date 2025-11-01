#include <catch2/catch.hpp>

#include "semaphore.hpp"
#include "blocking_queue.hpp"

#include <atomic>
#include <thread>
#include <deque>
#include <chrono>
#include <string>

TEST_CASE("Semaphore", "[semaphore,unit]") {
  SECTION("non blocking") {
    solutions::Semaphore semaphore(2);

    semaphore.Acquire();  // -1
    semaphore.Release();  // +1

    semaphore.Acquire();  // -1
    semaphore.Acquire();  // -1
    semaphore.Release();  // +1
    semaphore.Release();  // +1
  }

  SECTION("blocking") {
    solutions::Semaphore semaphore(0);

    bool touched = false;

    std::thread touch([&]() {
      semaphore.Acquire();
      touched = true;
    });

    std::this_thread::sleep_for(250ms);

    REQUIRE(!touched);

    semaphore.Release();
    touch.join();

    REQUIRE(touched);
  }

  SECTION("PingPong") {
    solutions::Semaphore my{1};
    solutions::Semaphore that{0};

    int step = 0;

    thread opponent([&]() {
      that.Acquire();
      REQUIRE(1 == step);
      step = 0;
      my.Release();
    });

    my.Acquire();
    REQUIRE(0 == step);
    step = 1;
    that.Release();

    my.Acquire();
    REQUIRE(step == 0);

    opponent.join();
  }
}

TEST_CASE("Blocking Queue", "[semaphore,unit]") {
  SECTION("Put then take") {
    solutions::BlockingQueue<int> queue{1};
    queue.Put(42);
    REQUIRE(42 == queue.Take());
  }

  SECTION("Move only") {
    struct MoveOnly {
      MoveOnly() = default;

      MoveOnly(const MoveOnly& that) = delete;
      MoveOnly& operator=(const MoveOnly& that) = delete;

      MoveOnly(MoveOnly&& that) = default;
      MoveOnly& operator=(MoveOnly&& that) = default;
    };
    solutions::BlockingQueue<MoveOnly> queue{1};

    queue.Put(MoveOnly{});
    queue.Take();
  }

  SECTION("Buffer") {
    solutions::BlockingQueue<std::string> queue{2};

    queue.Put("hello");
    queue.Put("world");
  }

  SECTION("fifo small") {
    solutions::BlockingQueue<std::string> queue{2};

    std::thread producer([&queue]() {
      queue.Put("hello");
      queue.Put("world");
      queue.Put("!");
    });

    REQUIRE("hello" == queue.Take());
    REQUIRE("world" == queue.Take());
    REQUIRE("!" == queue.Take());

    producer.join();
  }

  SECTION("fifo ") {
    solutions::BlockingQueue<int> queue{3};

    static const int kItems = 1024;

    std::thread producer([&]() {
      for (int i = 0; i < kItems; ++i) {
        queue.Put(i);
      }
      queue.Put(-1);  // Poison pill
    });

    // Consumer

    for (int i = 0; i < kItems; ++i) {
      REQIURE(i == queue.Take());
    }
    REQIURE(-1 == queue.Take());

    producer.join();
  }

  SECTION("capacity ") {
    solutions::BlockingQueue<int> queue{3};
    std::atomic<size_t> send_count{0};

    std::thread producer([&]() {
      for (size_t i = 0; i < 100; ++i) {
        queue.Put(i);
        send_count.store(i);
      }
      queue.Put(-1);
    });

    std::this_thread::sleep_for(100ms);

    REQUIRE(send_count.load() <= 3);

    for (size_t i = 0; i < 14; ++i) {
      (void)queue.Take();
    }

    std::this_thread::sleep_for(100ms);

    REQUIRE(send_count.load() <= 17);

    while (queue.Take() != -1) {
      // Pass
    }

    producer.join();
  }

  SECTION("pill") {
    static const size_t kThreads = 10;
    solutions::BlockingQueue<int> queue{1};

    std::vector<std::thread> threads;

    std::random_device dev;
    std::mt19937 rng(dev());
    std::uniform_int_distribution<std::mt19937::result_type> dist(1, 1000);

    for (size_t i = 0; i < kThreads; ++i) {
      threads.emplace_back([&]() {
        std::this_thread::sleep_for(dist(rng));

        REQUIRE(-1 == queue.Take());
        queue.Put(-1);
      });
    }

    queue.Put(-1);

    for (auto& t : threads) {
      t.join();
    }
  }
}
