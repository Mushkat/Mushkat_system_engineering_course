#include <algorithm>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <memory>
#include <numeric>
#include <random>
#include <stdexcept>
#include <string>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#endif

namespace {

    struct Result {
        std::size_t pages = 0;
        std::size_t footprint_kb = 0;
        double ns_per_access = 0.0;
    };

    std::size_t GetPageSize() {
#ifdef _WIN32
        SYSTEM_INFO info{};
    GetSystemInfo(&info);
    return static_cast<std::size_t>(info.dwPageSize);
#else
        long page_size = sysconf(_SC_PAGESIZE);
        if (page_size <= 0) {
            throw std::runtime_error("Failed to get page size");
        }
        return static_cast<std::size_t>(page_size);
#endif
    }

    struct AlignedBuffer {
        std::unique_ptr<std::byte[]> storage;
        std::byte* aligned = nullptr;
        std::size_t size = 0;
    };

    AlignedBuffer AllocateAligned(std::size_t size, std::size_t alignment) {
        auto raw = std::make_unique<std::byte[]>(size + alignment);
        void* ptr = raw.get();
        std::size_t space = size + alignment;

        void* aligned = std::align(alignment, size, ptr, space);
        if (aligned == nullptr) {
            throw std::runtime_error("Failed to align buffer");
        }

        return {std::move(raw), static_cast<std::byte*>(aligned), size};
    }

    void WriteLink(std::byte* base, std::size_t page_size, std::size_t page_index, std::size_t next_page) {
        auto* slot = reinterpret_cast<std::size_t*>(base + page_index * page_size);
        *slot = next_page;
    }

    std::size_t ReadLink(const std::byte* base, std::size_t page_size, std::size_t page_index) {
        const auto* slot = reinterpret_cast<const std::size_t*>(base + page_index * page_size);
        return *slot;
    }

    std::vector<std::size_t> BuildPageCounts() {
        return {
                1, 2, 4, 8, 16, 24, 32, 48, 64, 96, 128, 192, 256, 384, 512,
                768, 1024, 1536, 2048, 3072, 4096, 6144, 8192, 12288, 16384
        };
    }

    void BuildRandomCycle(std::byte* base, std::size_t page_size, std::size_t pages, std::mt19937_64& rng) {
        std::vector<std::size_t> order(pages);
        std::iota(order.begin(), order.end(), 0);
        std::shuffle(order.begin(), order.end(), rng);

        for (std::size_t i = 0; i < pages; ++i) {
            const std::size_t current = order[i];
            const std::size_t next = order[(i + 1) % pages];
            WriteLink(base, page_size, current, next);
        }
    }

    std::size_t Traverse(const std::byte* base, std::size_t page_size, std::size_t start, std::uint64_t accesses) {
        std::size_t idx = start;
        for (std::uint64_t i = 0; i < accesses; ++i) {
            idx = ReadLink(base, page_size, idx);
        }
        return idx;
    }

    double Median(std::vector<double> values) {
        std::sort(values.begin(), values.end());
        if (values.empty()) {
            return 0.0;
        }
        const std::size_t n = values.size();
        if (n % 2 == 1) {
            return values[n / 2];
        }
        return 0.5 * (values[n / 2 - 1] + values[n / 2]);
    }

    Result Measure(std::size_t pages, std::size_t page_size, std::uint64_t target_accesses, int repeats) {
        const std::size_t total_bytes = pages * page_size;
        auto buffer = AllocateAligned(total_bytes, page_size);

        std::mt19937_64 rng(42 + static_cast<std::uint64_t>(pages));
        BuildRandomCycle(buffer.aligned, page_size, pages, rng);

        const std::uint64_t rounds = std::max<std::uint64_t>(1, target_accesses / pages);
        const std::uint64_t accesses = rounds * pages;

        volatile std::size_t sink = 0;

        // warm-up
        sink = Traverse(buffer.aligned, page_size, 0, accesses);

        std::vector<double> samples;
        samples.reserve(repeats);

        for (int r = 0; r < repeats; ++r) {
            const auto start = std::chrono::steady_clock::now();
            sink = Traverse(buffer.aligned, page_size, sink % pages, accesses);
            const auto finish = std::chrono::steady_clock::now();

            const auto elapsed_ns =
                    std::chrono::duration_cast<std::chrono::nanoseconds>(finish - start).count();

            samples.push_back(static_cast<double>(elapsed_ns) / static_cast<double>(accesses));
        }

        (void)sink;

        return Result{
                pages,
                (pages * page_size) / 1024,
                Median(samples)
        };
    }

    std::size_t EstimateTlbPages(const std::vector<Result>& results) {
        if (results.size() < 2) {
            return 0;
        }

        for (std::size_t i = 1; i < results.size(); ++i) {
            if (results[i - 1].pages < 16) {
                continue;
            }
            if (results[i].ns_per_access > results[i - 1].ns_per_access * 1.20) {
                return results[i - 1].pages;
            }
        }
        return results.back().pages;
    }

    void SaveCsv(const std::filesystem::path& path, const std::vector<Result>& results) {
        std::ofstream out(path);
        if (!out) {
            throw std::runtime_error("Failed to open CSV file for writing");
        }

        out << "pages,footprint_kb,ns_per_access\n";
        for (const auto& row : results) {
            out << row.pages << ','
                << row.footprint_kb << ','
                << std::fixed << std::setprecision(4) << row.ns_per_access << '\n';
        }
    }

    void PrintUsage(const char* exe_name) {
        std::cout << "Usage:\n";
        std::cout << "  " << exe_name << " [output_csv]\n";
        std::cout << "\n";
        std::cout << "Default output: tlb_results.csv\n";
    }

}  // namespace

int main(int argc, char* argv[]) {
    try {
        if (argc > 2) {
            PrintUsage(argv[0]);
            return 1;
        }

        const std::filesystem::path output_csv =
                (argc == 2) ? std::filesystem::path(argv[1]) : std::filesystem::path("tlb_results.csv");

        const std::size_t page_size = GetPageSize();
        const std::uint64_t target_accesses = 16ull * 1024ull * 1024ull;
        const int repeats = 7;

        std::cout << "Page size: " << page_size << " bytes\n";
        std::cout << "Running TLB benchmark...\n\n";

        const auto page_counts = BuildPageCounts();
        std::vector<Result> results;
        results.reserve(page_counts.size());

        std::cout << std::left
                  << std::setw(12) << "pages"
                  << std::setw(18) << "footprint_kb"
                  << std::setw(18) << "ns/access"
                  << '\n';

        for (std::size_t pages : page_counts) {
            const auto row = Measure(pages, page_size, target_accesses, repeats);
            results.push_back(row);

            std::cout << std::left
                      << std::setw(12) << row.pages
                      << std::setw(18) << row.footprint_kb
                      << std::setw(18) << std::fixed << std::setprecision(4) << row.ns_per_access
                      << '\n';
        }

        SaveCsv(output_csv, results);

        const std::size_t estimated_pages = EstimateTlbPages(results);
        const std::size_t estimated_kb = (estimated_pages * page_size) / 1024;

        std::cout << "\nResults saved to: " << output_csv << '\n';
        std::cout << "Approximate TLB capacity knee: ~"
                  << estimated_pages << " pages (~"
                  << estimated_kb << " KB mapped)\n";

        return 0;
    } catch (const std::exception& ex) {
        std::cerr << "Error: " << ex.what() << '\n';
        return 1;
    }
}