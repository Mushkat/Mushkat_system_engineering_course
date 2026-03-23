#pragma once

#include <atomic>
#include <cstdint>
#include <cstdlib>

namespace stdlike {

    class Mutex {
    public:
        void Lock() {
            uint32_t expected = 0;

            if (state_.compare_exchange_strong(expected, 1)) {
                return;
            }

            while (true) {
                if (expected == 2 || state_.exchange(2) != 0) {
                    state_.wait(2);
                    expected = 0;
                } else {
                    return;
                }
            }
        }

        void Unlock() {
            if (state_.fetch_sub(1) == 1) {
                return;
            }

            state_.store(0);
            state_.notify_one();
        }

    private:
        std::atomic<uint32_t> state_{0};
    };

}  // namespace stdlike
