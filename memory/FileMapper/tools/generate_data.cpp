#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <random>
#include <stdexcept>
#include <string>

namespace {

    void PrintUsage(const char* exe_name) {
        std::cerr << "Usage:\n";
        std::cerr << "  " << exe_name << " <output.bin> <count> [seed]\n";
    }

}  // namespace

int main(int argc, char* argv[]) {
    try {
        if (argc != 3 && argc != 4) {
            PrintUsage(argv[0]);
            return 1;
        }

        const std::filesystem::path output = argv[1];
        const std::uint64_t count = std::stoull(argv[2]);
        const std::uint64_t seed = (argc == 4) ? std::stoull(argv[3]) : std::random_device{}();

        std::mt19937_64 rng(seed);
        std::uniform_int_distribution<std::int64_t> dist(
                std::numeric_limits<std::int64_t>::min(),
                std::numeric_limits<std::int64_t>::max());

        std::ofstream out(output, std::ios::binary);
        if (!out) {
            throw std::runtime_error("Failed to open output file");
        }

        for (std::uint64_t i = 0; i < count; ++i) {
            const std::int64_t value = dist(rng);
            out.write(reinterpret_cast<const char*>(&value), sizeof(value));
        }

        std::cout << "Generated " << count << " numbers into " << output << '\n';
        std::cout << "Seed: " << seed << '\n';
        return 0;
    } catch (const std::exception& ex) {
        std::cerr << "Error: " << ex.what() << '\n';
        return 1;
    }
}