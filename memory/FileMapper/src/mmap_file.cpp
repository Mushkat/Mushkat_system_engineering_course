#include "mmap_file.hpp"

#include <algorithm>
#include <cstring>
#include <system_error>

#ifdef _WIN32
#include <windows.h>
#else
#include <cerrno>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

namespace filemapper {

    namespace {

        std::runtime_error MakeError(const std::string& message) {
            return std::runtime_error(message + ": " + LastSystemErrorMessage());
        }

    }  // namespace

    std::string LastSystemErrorMessage() {
#ifdef _WIN32
        DWORD error_code = GetLastError();
  if (error_code == 0) {
    return "no error";
  }

  LPSTR buffer = nullptr;
  const DWORD size = FormatMessageA(
      FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
      nullptr,
      error_code,
      MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
      reinterpret_cast<LPSTR>(&buffer),
      0,
      nullptr);

  std::string message = (size != 0 && buffer != nullptr) ? std::string(buffer, size) : "unknown error";
  if (buffer != nullptr) {
    LocalFree(buffer);
  }
  return message;
#else
        return std::strerror(errno);
#endif
    }

    std::uint64_t GetFileSize(const std::filesystem::path& path) {
        return static_cast<std::uint64_t>(std::filesystem::file_size(path));
    }

    std::size_t MappedFile::AllocationGranularity() {
#ifdef _WIN32
        SYSTEM_INFO sys_info{};
  GetSystemInfo(&sys_info);
  return static_cast<std::size_t>(sys_info.dwAllocationGranularity);
#else
        long page_size = sysconf(_SC_PAGESIZE);
        if (page_size <= 0) {
            throw std::runtime_error("Failed to get page size");
        }
        return static_cast<std::size_t>(page_size);
#endif
    }

    MappedFile::MappedFile(const std::filesystem::path& path, MapMode mode, std::uint64_t file_size)
            : mode_(mode) {
#ifdef _WIN32
        const DWORD access = (mode == MapMode::ReadOnly) ? GENERIC_READ : (GENERIC_READ | GENERIC_WRITE);
  const DWORD creation = (mode == MapMode::ReadOnly) ? OPEN_EXISTING : OPEN_ALWAYS;

  HANDLE file = CreateFileW(
      path.wstring().c_str(),
      access,
      FILE_SHARE_READ,
      nullptr,
      creation,
      FILE_ATTRIBUTE_NORMAL,
      nullptr);

  if (file == INVALID_HANDLE_VALUE) {
    throw MakeError("CreateFile failed");
  }
  file_handle_ = file;

  if (mode == MapMode::ReadOnly) {
    file_size_ = GetFileSize(path);
  } else {
    EnsureSize(file_size);
    file_size_ = file_size;
  }

  const DWORD protect = (mode == MapMode::ReadOnly) ? PAGE_READONLY : PAGE_READWRITE;
  HANDLE mapping = CreateFileMappingW(
      static_cast<HANDLE>(file_handle_),
      nullptr,
      protect,
      static_cast<DWORD>(file_size_ >> 32),
      static_cast<DWORD>(file_size_ & 0xffffffffULL),
      nullptr);

  if (mapping == nullptr) {
    Close();
    throw MakeError("CreateFileMapping failed");
  }
  mapping_handle_ = mapping;
#else
        const int flags = (mode == MapMode::ReadOnly) ? O_RDONLY : (O_RDWR | O_CREAT);
        fd_ = open(path.c_str(), flags, 0644);
        if (fd_ < 0) {
            throw MakeError("open failed");
        }

        if (mode == MapMode::ReadOnly) {
            file_size_ = GetFileSize(path);
        } else {
            EnsureSize(file_size);
            file_size_ = file_size;
        }
#endif
    }

    MappedFile::~MappedFile() {
        Close();
    }

    MappedFile::MappedFile(MappedFile&& other) noexcept
    :
#ifdef _WIN32
    file_handle_(other.file_handle_),
      mapping_handle_(other.mapping_handle_),
#else
    fd_(other.fd_),
#endif
    mode_(other.mode_),
    file_size_(other.file_size_),
    mapped_base_(other.mapped_base_),
    mapped_size_(other.mapped_size_) {
#ifdef _WIN32
    other.file_handle_ = nullptr;
  other.mapping_handle_ = nullptr;
#else
    other.fd_ = -1;
#endif
    other.mapped_base_ = nullptr;
    other.mapped_size_ = 0;
    other.file_size_ = 0;
}

MappedFile& MappedFile::operator=(MappedFile&& other) noexcept {
if (this != &other) {
Close();

#ifdef _WIN32
file_handle_ = other.file_handle_;
    mapping_handle_ = other.mapping_handle_;
    other.file_handle_ = nullptr;
    other.mapping_handle_ = nullptr;
#else
fd_ = other.fd_;
other.fd_ = -1;
#endif

mode_ = other.mode_;
file_size_ = other.file_size_;
mapped_base_ = other.mapped_base_;
mapped_size_ = other.mapped_size_;

other.file_size_ = 0;
other.mapped_base_ = nullptr;
other.mapped_size_ = 0;
}
return *this;
}

std::uint64_t MappedFile::Size() const {
    return file_size_;
}

void MappedFile::EnsureSize(std::uint64_t file_size) {
#ifdef _WIN32
    LARGE_INTEGER size{};
  size.QuadPart = static_cast<LONGLONG>(file_size);

  if (SetFilePointerEx(static_cast<HANDLE>(file_handle_), size, nullptr, FILE_BEGIN) == 0) {
    throw MakeError("SetFilePointerEx failed");
  }
  if (SetEndOfFile(static_cast<HANDLE>(file_handle_)) == 0) {
    throw MakeError("SetEndOfFile failed");
  }
#else
    if (ftruncate(fd_, static_cast<off_t>(file_size)) != 0) {
        throw MakeError("ftruncate failed");
    }
#endif
}

MappedWindow MappedFile::Map(std::uint64_t offset, std::size_t size) {
    if (offset + size > file_size_) {
        throw std::out_of_range("Map range exceeds file size");
    }
    if (size == 0) {
        return {};
    }

    Unmap();

    const std::size_t granularity = AllocationGranularity();
    const std::uint64_t aligned_offset = (offset / granularity) * granularity;
    const std::size_t delta = static_cast<std::size_t>(offset - aligned_offset);
    const std::size_t mapping_size = delta + size;

#ifdef _WIN32
    const DWORD access = (mode_ == MapMode::ReadOnly) ? FILE_MAP_READ : FILE_MAP_WRITE;
  void* base = MapViewOfFile(
      static_cast<HANDLE>(mapping_handle_),
      access,
      static_cast<DWORD>(aligned_offset >> 32),
      static_cast<DWORD>(aligned_offset & 0xffffffffULL),
      mapping_size);

  if (base == nullptr) {
    throw MakeError("MapViewOfFile failed");
  }

  mapped_base_ = base;
  mapped_size_ = mapping_size;

  auto* data = reinterpret_cast<std::byte*>(base) + delta;
  return {data, size, offset};
#else
    const int prot = (mode_ == MapMode::ReadOnly) ? PROT_READ : (PROT_READ | PROT_WRITE);
    void* base = mmap(nullptr, mapping_size, prot, MAP_SHARED, fd_, static_cast<off_t>(aligned_offset));
    if (base == MAP_FAILED) {
        mapped_base_ = nullptr;
        mapped_size_ = 0;
        throw MakeError("mmap failed");
    }

    mapped_base_ = base;
    mapped_size_ = mapping_size;

    auto* data = reinterpret_cast<std::byte*>(base) + delta;
    return {data, size, offset};
#endif
}

void MappedFile::Unmap() {
    if (mapped_base_ == nullptr) {
        return;
    }

#ifdef _WIN32
    FlushViewOfFile(mapped_base_, mapped_size_);
  UnmapViewOfFile(mapped_base_);
#else
    if (mode_ == MapMode::ReadWrite) {
        msync(mapped_base_, mapped_size_, MS_SYNC);
    }
    munmap(mapped_base_, mapped_size_);
#endif

    mapped_base_ = nullptr;
    mapped_size_ = 0;
}

void MappedFile::Close() {
    Unmap();

#ifdef _WIN32
    if (mapping_handle_ != nullptr) {
    CloseHandle(static_cast<HANDLE>(mapping_handle_));
    mapping_handle_ = nullptr;
  }
  if (file_handle_ != nullptr) {
    CloseHandle(static_cast<HANDLE>(file_handle_));
    file_handle_ = nullptr;
  }
#else
    if (fd_ >= 0) {
        close(fd_);
        fd_ = -1;
    }
#endif
}

}  // namespace filemapper