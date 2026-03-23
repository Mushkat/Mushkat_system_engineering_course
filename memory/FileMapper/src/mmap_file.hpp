#pragma once

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <stdexcept>
#include <string>

namespace filemapper {

    enum class MapMode {
        ReadOnly,
        ReadWrite
    };

    struct MappedWindow {
        std::byte* data = nullptr;
        std::size_t requested_size = 0;
        std::uint64_t requested_offset = 0;

        [[nodiscard]] bool Empty() const {
            return data == nullptr || requested_size == 0;
        }
    };

    class MappedFile {
    public:
        MappedFile(const std::filesystem::path& path, MapMode mode, std::uint64_t file_size = 0);
        ~MappedFile();

        MappedFile(const MappedFile&) = delete;
        MappedFile& operator=(const MappedFile&) = delete;

        MappedFile(MappedFile&& other) noexcept;
        MappedFile& operator=(MappedFile&& other) noexcept;

        [[nodiscard]] std::uint64_t Size() const;
        [[nodiscard]] static std::size_t AllocationGranularity();

        MappedWindow Map(std::uint64_t offset, std::size_t size);
        void Unmap();

    private:
        void Close();
        void EnsureSize(std::uint64_t file_size);

    private:
#ifdef _WIN32
        void* file_handle_ = nullptr;
  void* mapping_handle_ = nullptr;
#else
        int fd_ = -1;
#endif

        MapMode mode_;
        std::uint64_t file_size_ = 0;

        void* mapped_base_ = nullptr;
        std::size_t mapped_size_ = 0;
    };

    std::uint64_t GetFileSize(const std::filesystem::path& path);
    std::string LastSystemErrorMessage();

}  // namespace filemapper