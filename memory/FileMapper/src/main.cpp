#include "sorter.hpp"

#include <cstdint>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>

namespace {

    std::uint64_t ParseMemoryLimitBytes(const std::string& arg_mb) {
        const std::uint64_t mb = std::stoull(arg_mb);
        return mb * 1024ULL * 1024ULL;
    }

    void PrintUsage(const char* exe_name) {
        std::cerr << "Usage:\n";
        std::cerr << "  " << exe_name << " <datafile.bin> [memory_limit_mb]\n";
    }

}  // namespace

int main(int argc, char* argv[]) {
    try {
        if (argc != 2 && argc != 3) {
            PrintUsage(argv[0]);
            return 1;
        }

        filemapper::SortOptions options;
        options.input_file = std::filesystem::path(argv[1]);

        if (argc == 3) {
            options.memory_limit_bytes = ParseMemoryLimitBytes(argv[2]);
        }

        filemapper::SortFile(options);
        return 0;
    } catch (const std::exception& ex) {
        std::cerr << "Error: " << ex.what() << '\n';
        return 1;
    }
}