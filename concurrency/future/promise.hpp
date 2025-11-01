#pragma once

#include "future.hpp"

#include <memory>

namespace stdlike {

    template<typename T>
    class Promise {
    public:
        Promise() : state_(std::make_shared<detail::SharedState<T>>()) {
        }

        // Non-copyable
        Promise(const Promise &) = delete;

        Promise &operator=(const Promise &) = delete;

        // Movable
        Promise(Promise &&) = default;

        Promise &operator=(Promise &&) = default;

        // One-shot
        Future<T> MakeFuture() {
            if (!state_) {
                throw std::runtime_error("Promise already used");
            }
            return Future<T>(state_);
        }

        // One-shot
        // Fulfill promise with value
        void SetValue(T value) {
            if (!state_) {
                throw std::runtime_error("Promise already used");
            }
            state_->SetValue(std::move(value));
            state_.reset();
        }

        // One-shot
        // Fulfill promise with exception
        void SetException(std::exception_ptr ex) {
            if (!state_) {
                throw std::runtime_error("Promise already used");
            }
            state_->SetException(std::move(ex));
            state_.reset();
        }

    private:
        std::shared_ptr<detail::SharedState<T>> state_;
    };

}  // namespace stdlike
