#pragma once

#include "cpu_state.hpp"

#include <memory>
#include <string>

#define FUSE_USE_VERSION 31
#include <fuse3/fuse.h>

namespace cpufs {

    class CpuFs {
    public:
        explicit CpuFs(std::size_t unit_count);

        DeviceState& State();
        const DeviceState& State() const;

        static fuse_operations CreateOperations();

        static int GetAttr(const char* path, struct stat* stbuf, struct fuse_file_info* fi);
        static int ReadDir(const char* path, void* buf, fuse_fill_dir_t filler, off_t off,
                           struct fuse_file_info* fi, enum fuse_readdir_flags flags);
        static int Open(const char* path, struct fuse_file_info* fi);
        static int Read(const char* path, char* buf, size_t size, off_t offset,
                        struct fuse_file_info* fi);
        static int Write(const char* path, const char* buf, size_t size, off_t offset,
                         struct fuse_file_info* fi);
        static int Truncate(const char* path, off_t size, struct fuse_file_info* fi);

        static CpuFs* FromContext();

    private:
        DeviceState state_;
    };

}  // namespace cpufs