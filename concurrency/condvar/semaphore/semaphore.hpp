#pragma once

// std::lock_guard, std::unique_lock
#include <mutex>
#include <cstdint>
#include <condition_variable>

namespace solutions {

// A Counting semaphore

// Semaphores are often used to restrict the number of threads
// than can access some (physical or logical) resource

    class Semaphore {
    public:
        // Creates a Semaphore with the given number of permits
        explicit Semaphore(size_t initial) : count_(initial) {}

        // Acquires a permit from this semaphore,
        // blocking until one is available
        void Acquire() {
            std::unique_lock lock(mutex_);
            while (count_ == 0) {
                cv_.wait(lock);
            }
            count_--;
        }

        // Releases a permit, returning it to the semaphore
        void Release() {
            std::lock_guard lock(mutex_);
            count_++;
            cv_.notify_one();
        }

    private:
        size_t count_;
        std::mutex mutex_;
        std::condition_variable cv_;
    };

}  // namespace solutions
