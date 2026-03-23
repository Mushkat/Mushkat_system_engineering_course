#pragma once

#include <cstdint>
#include <filesystem>
#include <vector>

namespace filemapper {

    struct SortOptions {
        std::filesystem::path input_file;
        std::uint64_t memory_limit_bytes = 0;
    };

    void SortFile(const SortOptions& options);

}  // namespace filemapper