#include "sorter.hpp"

#include "mmap_file.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <iostream>
#include <string>
#include <vector>

namespace filemapper {
    namespace {

        constexpr std::uint64_t kValueSize = sizeof(std::int64_t);

        std::uint64_t AlignDown(std::uint64_t value, std::uint64_t alignment) {
            return (value / alignment) * alignment;
        }

        std::uint64_t MaxOrDefaultLimit(std::uint64_t file_size, std::uint64_t requested_limit) {
            std::uint64_t limit = requested_limit;
            if (limit == 0) {
                limit = std::max<std::uint64_t>(file_size / 10, 3 * kValueSize);
            }
            limit = std::max<std::uint64_t>(limit, 3 * kValueSize);
            return AlignDown(limit, kValueSize);
        }

        std::filesystem::path TempDirFor(const std::filesystem::path& input_file) {
            return input_file.parent_path() / ".filemapper_tmp";
        }

        std::filesystem::path MakeRunPath(const std::filesystem::path& temp_dir, std::size_t pass, std::size_t index) {
            return temp_dir / ("run_pass_" + std::to_string(pass) + "_" + std::to_string(index) + ".bin");
        }

        struct RunInfo {
            std::filesystem::path path;
            std::uint64_t bytes = 0;
        };

        std::vector<RunInfo> CreateInitialRuns(const std::filesystem::path& input_file,
                                               std::uint64_t file_size,
                                               std::uint64_t memory_limit,
                                               const std::filesystem::path& temp_dir) {
            const std::uint64_t chunk_bytes = std::max<std::uint64_t>(AlignDown(memory_limit, kValueSize), kValueSize);

            MappedFile input(input_file, MapMode::ReadOnly);
            std::vector<RunInfo> runs;

            std::uint64_t offset = 0;
            std::size_t run_index = 0;

            while (offset < file_size) {
                const std::uint64_t bytes_left = file_size - offset;
                const std::uint64_t current_chunk = std::min<std::uint64_t>(chunk_bytes, bytes_left);

                auto in_window = input.Map(offset, static_cast<std::size_t>(current_chunk));

                const std::size_t value_count = static_cast<std::size_t>(current_chunk / kValueSize);
                std::vector<std::int64_t> values(value_count);
                std::memcpy(values.data(), in_window.data, static_cast<std::size_t>(current_chunk));

                std::sort(values.begin(), values.end());

                const auto run_path = MakeRunPath(temp_dir, 0, run_index++);
                MappedFile output(run_path, MapMode::ReadWrite, current_chunk);
                auto out_window = output.Map(0, static_cast<std::size_t>(current_chunk));
                std::memcpy(out_window.data, values.data(), static_cast<std::size_t>(current_chunk));

                runs.push_back({run_path, current_chunk});
                offset += current_chunk;
            }

            return runs;
        }

        class WindowedInt64Reader {
        public:
            WindowedInt64Reader(const std::filesystem::path& path, std::uint64_t file_size, std::uint64_t window_bytes)
                    : file_(path, MapMode::ReadOnly),
                      file_size_(file_size),
                      window_bytes_(std::max<std::uint64_t>(AlignDown(window_bytes, kValueSize), kValueSize)) {}

            [[nodiscard]] bool HasValue() const {
                return global_index_ < file_size_ / kValueSize;
            }

            std::int64_t Peek() {
                EnsureWindowLoaded();
                return current_ptr_[index_in_window_];
            }

            void Advance() {
                EnsureWindowLoaded();
                ++index_in_window_;
                ++global_index_;
            }

        private:
            void EnsureWindowLoaded() {
                if (!HasValue()) {
                    throw std::out_of_range("Reader exhausted");
                }

                if (window_.Empty() || index_in_window_ >= values_in_window_) {
                    const std::uint64_t byte_offset = global_index_ * kValueSize;
                    const std::uint64_t left_bytes = file_size_ - byte_offset;
                    const std::uint64_t current_bytes = std::min<std::uint64_t>(window_bytes_, left_bytes);

                    window_ = file_.Map(byte_offset, static_cast<std::size_t>(current_bytes));
                    current_ptr_ = reinterpret_cast<const std::int64_t*>(window_.data);
                    values_in_window_ = static_cast<std::size_t>(current_bytes / kValueSize);
                    index_in_window_ = 0;
                }
            }

        private:
            MappedFile file_;
            std::uint64_t file_size_;
            std::uint64_t window_bytes_;
            std::uint64_t global_index_ = 0;

            MappedWindow window_{};
            const std::int64_t* current_ptr_ = nullptr;
            std::size_t values_in_window_ = 0;
            std::size_t index_in_window_ = 0;
        };

        class WindowedInt64Writer {
        public:
            WindowedInt64Writer(const std::filesystem::path& path, std::uint64_t file_size, std::uint64_t window_bytes)
                    : file_(path, MapMode::ReadWrite, file_size),
                      file_size_(file_size),
                      window_bytes_(std::max<std::uint64_t>(AlignDown(window_bytes, kValueSize), kValueSize)) {}

            void Push(std::int64_t value) {
                EnsureWindowLoaded();
                current_ptr_[index_in_window_] = value;
                ++index_in_window_;
                ++global_index_;
            }

        private:
            void EnsureWindowLoaded() {
                if (global_index_ >= file_size_ / kValueSize) {
                    throw std::out_of_range("Writer overflow");
                }

                if (window_.Empty() || index_in_window_ >= values_in_window_) {
                    const std::uint64_t byte_offset = global_index_ * kValueSize;
                    const std::uint64_t left_bytes = file_size_ - byte_offset;
                    const std::uint64_t current_bytes = std::min<std::uint64_t>(window_bytes_, left_bytes);

                    window_ = file_.Map(byte_offset, static_cast<std::size_t>(current_bytes));
                    current_ptr_ = reinterpret_cast<std::int64_t*>(window_.data);
                    values_in_window_ = static_cast<std::size_t>(current_bytes / kValueSize);
                    index_in_window_ = 0;
                }
            }

        private:
            MappedFile file_;
            std::uint64_t file_size_;
            std::uint64_t window_bytes_;
            std::uint64_t global_index_ = 0;

            MappedWindow window_{};
            std::int64_t* current_ptr_ = nullptr;
            std::size_t values_in_window_ = 0;
            std::size_t index_in_window_ = 0;
        };

        RunInfo MergeTwoRuns(const RunInfo& left,
                             const RunInfo& right,
                             const std::filesystem::path& out_path,
                             std::uint64_t memory_limit) {
            const std::uint64_t total_size = left.bytes + right.bytes;
            const std::uint64_t per_window = std::max<std::uint64_t>(AlignDown(memory_limit / 3, kValueSize), kValueSize);

            WindowedInt64Reader lreader(left.path, left.bytes, per_window);
            WindowedInt64Reader rreader(right.path, right.bytes, per_window);
            WindowedInt64Writer writer(out_path, total_size, per_window);

            while (lreader.HasValue() && rreader.HasValue()) {
                if (lreader.Peek() <= rreader.Peek()) {
                    writer.Push(lreader.Peek());
                    lreader.Advance();
                } else {
                    writer.Push(rreader.Peek());
                    rreader.Advance();
                }
            }

            while (lreader.HasValue()) {
                writer.Push(lreader.Peek());
                lreader.Advance();
            }

            while (rreader.HasValue()) {
                writer.Push(rreader.Peek());
                rreader.Advance();
            }

            return {out_path, total_size};
        }

        std::vector<RunInfo> MergePass(const std::vector<RunInfo>& runs,
                                       std::uint64_t memory_limit,
                                       const std::filesystem::path& temp_dir,
                                       std::size_t pass_index) {
            std::vector<RunInfo> next_runs;

            for (std::size_t i = 0; i < runs.size(); i += 2) {
                if (i + 1 >= runs.size()) {
                    next_runs.push_back(runs[i]);
                    continue;
                }

                const auto out_path = MakeRunPath(temp_dir, pass_index, i / 2);
                next_runs.push_back(MergeTwoRuns(runs[i], runs[i + 1], out_path, memory_limit));
            }

            return next_runs;
        }

        void RemoveFiles(const std::vector<RunInfo>& runs) {
            for (const auto& run : runs) {
                std::error_code ec;
                std::filesystem::remove(run.path, ec);
            }
        }

    }  // namespace

    void SortFile(const SortOptions& options) {
        if (options.input_file.empty()) {
            throw std::invalid_argument("Input path is empty");
        }
        if (!std::filesystem::exists(options.input_file)) {
            throw std::runtime_error("Input file does not exist");
        }

        const std::uint64_t file_size = GetFileSize(options.input_file);
        if (file_size == 0) {
            return;
        }
        if (file_size % kValueSize != 0) {
            throw std::runtime_error("Input file size must be divisible by sizeof(int64_t)");
        }

        const std::uint64_t memory_limit = MaxOrDefaultLimit(file_size, options.memory_limit_bytes);
        const auto temp_dir = TempDirFor(options.input_file);
        std::filesystem::create_directories(temp_dir);

        auto runs = CreateInitialRuns(options.input_file, file_size, memory_limit, temp_dir);

        if (runs.empty()) {
            return;
        }

        std::size_t pass = 1;
        while (runs.size() > 1) {
            auto previous_runs = runs;
            runs = MergePass(previous_runs, memory_limit, temp_dir, pass++);
            RemoveFiles(previous_runs);
        }

        const auto final_sorted = runs.front();

        {
            std::error_code ec;
            std::filesystem::remove(options.input_file, ec);
        }
        std::filesystem::rename(final_sorted.path, options.input_file);

        {
            std::error_code ec;
            std::filesystem::remove_all(temp_dir, ec);
        }

        std::cout << "Sorting finished: " << options.input_file << '\n';
        std::cout << "File size: " << file_size << " bytes\n";
        std::cout << "Mapping limit: " << memory_limit << " bytes\n";
    }

}  // namespace filemapper