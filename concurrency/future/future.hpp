#pragma once

#include <memory>
#include <cassert>
#include "shared_state.hpp"

namespace stdlike {

    template<typename T>
    class Future {
        template<typename U>
        friend
        class Promise;

    public:
        // Non-copyable
        Future(const Future &) = delete;

        Future &operator=(const Future &) = delete;

        // Movable
        Future(Future &&) = default;

        Future &operator=(Future &&) = default;

        // One-shot
        // Wait for result (value or exception)
        T Get() {
            if (!state_) {
                throw std::runtime_error("Future already used");
            }
            return state_->Get();
        }

        bool IsReady() const {
            return state_ && state_->IsReady();
        }

    private:
        explicit Future(std::shared_ptr<detail::SharedState<T>> state)
                : state_(std::move(state)) {
        }

    private:
        std::shared_ptr<detail::SharedState<T>> state_;
    };

}  // namespace stdlike
