#pragma once

#include <atomic>
#include <thread>
#include <chrono>
#include <mutex>
#include <condition_variable>
#include <cstdint>

namespace stdlike {

    class CondVar {
    public:
        // Mutex - BasicLockable
        // https://en.cppreference.com/w/cpp/named_req/BasicLockable
        template<class Mutex>
        void Wait(Mutex &mutex) {
            waiters_count_.fetch_add(1, std::memory_order_relaxed);

            uint32_t old_generation = generation_.load(std::memory_order_acquire);

            mutex.unlock();

            std::unique_lock<std::mutex> lock(internal_mutex_);
            while (generation_.load(std::memory_order_acquire) == old_generation) {
                internal_cv_.wait(lock);
            }

            waiters_count_.fetch_sub(1, std::memory_order_relaxed);
            mutex.lock();
        }

        void NotifyOne() {
            if (waiters_count_.load(std::memory_order_relaxed) > 0) {
                std::lock_guard<std::mutex> lock(internal_mutex_);
                generation_.fetch_add(1, std::memory_order_release);
                internal_cv_.notify_one();
            }
        }

        void NotifyAll() {
            if (waiters_count_.load(std::memory_order_relaxed) > 0) {
                std::lock_guard<std::mutex> lock(internal_mutex_);
                generation_.fetch_add(1, std::memory_order_release);
                internal_cv_.notify_all();
            }
        }

    private:
        std::atomic<uint32_t> generation_{0};
        std::atomic<uint32_t> waiters_count_{0};
        std::mutex internal_mutex_;
        std::condition_variable internal_cv_;
    };

}  // namespace stdlike
