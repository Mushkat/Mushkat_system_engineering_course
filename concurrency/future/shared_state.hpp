#pragma once

#include <mutex>
#include <condition_variable>
#include <variant>
#include <exception>
#include <stdexcept>

namespace stdlike {
    namespace detail {

        template<typename T>
        class SharedState {
        public:
            SharedState() = default;

            void SetValue(T value) {
                std::lock_guard lock(mutex_);
                if (ready_) {
                    throw std::runtime_error("Result already set");
                }
                result_ = std::move(value);
                ready_ = true;
                cv_.notify_all();
            }

            void SetException(std::exception_ptr exc) {
                std::lock_guard lock(mutex_);
                if (ready_) {
                    throw std::runtime_error("Result already set");
                }
                result_ = std::move(exc);
                ready_ = true;
                cv_.notify_all();
            }

            T Get() {
                std::unique_lock lock(mutex_);
                cv_.wait(lock, [this] { return ready_; });

                if (std::holds_alternative<T>(result_)) {
                    return std::get<T>(std::move(result_));
                } else if (std::holds_alternative<std::exception_ptr>(result_)) {
                    std::rethrow_exception(std::get<std::exception_ptr>(result_));
                } else {
                    throw std::runtime_error("No result available");
                }
            }

            bool IsReady() const {
                std::lock_guard lock(mutex_);
                return ready_;
            }

        private:
            std::variant<std::monostate, T, std::exception_ptr> result_;
            mutable std::mutex mutex_;
            std::condition_variable cv_;
            bool ready_ = false;
        };

    }  // namespace detail
}  // namespace stdlike