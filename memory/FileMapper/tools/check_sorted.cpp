#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>

namespace {

    void PrintUsage(const char* exe_name) {
        std::cerr << "Usage:\n";
        std::cerr << "  " << exe_name << " <datafile.bin>\n";
    }

}  // namespace

int main(int argc, char* argv[]) {
    try {
        if (argc != 2) {
            PrintUsage(argv[0]);
            return 1;
        }

        const std::filesystem::path input = argv[1];
        if (!std::filesystem::exists(input)) {
            throw std::runtime_error("Input file does not exist");
        }

        const auto file_size = std::filesystem::file_size(input);
        if (file_size % sizeof(std::int64_t) != 0) {
            throw std::runtime_error("File size is not divisible by sizeof(int64_t)");
        }

        std::ifstream in(input, std::ios::binary);
        if (!in) {
            throw std::runtime_error("Failed to open input file");
        }

        std::int64_t prev = 0;
        std::int64_t current = 0;

        if (!in.read(reinterpret_cast<char*>(&prev), sizeof(prev))) {
            std::cout << "File is empty, considered sorted\n";
            return 0;
        }

        std::uint64_t index = 1;
        while (in.read(reinterpret_cast<char*>(&current), sizeof(current))) {
            if (current < prev) {
                std::cerr << "File is NOT sorted at index " << index
                          << ": prev=" << prev
                          << ", current=" << current << '\n';
                return 2;
            }
            prev = current;
            ++index;
        }

        std::cout << "File is sorted\n";
        return 0;
    } catch (const std::exception& ex) {
        std::cerr << "Error: " << ex.what() << '\n';
        return 1;
    }
}