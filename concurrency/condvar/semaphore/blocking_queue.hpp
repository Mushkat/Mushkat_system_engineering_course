#pragma once

#include "tagged_semaphore.hpp"
#include <utility>
#include <deque>

namespace solutions {

// Bounded Blocking Multi-Producer/Multi-Consumer (MPMC) Queue
    struct SlotTag {
    };
    struct ItemTag {
    };
    struct MutexTag {
    };

    template<typename T>
    class BlockingQueue {
    public:
        explicit BlockingQueue(size_t capacity)
                : capacity_(capacity),
                  free_slots_(capacity),
                  items_(0),
                  mutex_(1) {
        }

        // Inserts the specified element into this queue,
        // waiting if necessary for space to become available.
        void Put(T value) {
            auto slot_token = free_slots_.Acquire();

            auto mutex_token = mutex_.Acquire();

            buffer_.push_back(std::move(value));

            mutex_.Release(std::move(mutex_token));

            auto item_token = items_.Acquire();
            items_.Release(std::move(item_token));
        }

        // Retrieves and removes the head of this queue,
        // waiting if necessary until an element becomes available
        T Take() {
            auto item_token = items_.Acquire();

            auto mutex_token = mutex_.Acquire();

            T value = std::move(buffer_.front());
            buffer_.pop_front();

            mutex_.Release(std::move(mutex_token));

            auto slot_token = free_slots_.Acquire();
            free_slots_.Release(std::move(slot_token));

            return value;
        }

    private:
        size_t capacity_;
        std::deque<T> buffer_;

        TaggedSemaphore<SlotTag> free_slots_;
        TaggedSemaphore<ItemTag> items_;
        TaggedSemaphore<MutexTag> mutex_;
    };

}  // namespace solutions
