#include "cpu_fs.hpp"

#include "cpu_executor.hpp"
#include "path_utils.hpp"

#include <cerrno>
#include <cstring>
#include <stdexcept>
#include <string>
#include <sys/stat.h>
#include <vector>

namespace cpufs {
    namespace {

        std::string TrimCommand(const std::string& s) {
            std::size_t begin = 0;
            while (begin < s.size() && (s[begin] == ' ' || s[begin] == '\n' || s[begin] == '\r' || s[begin] == '\t')) {
                ++begin;
            }

            std::size_t end = s.size();
            while (end > begin && (s[end - 1] == ' ' || s[end - 1] == '\n' || s[end - 1] == '\r' || s[end - 1] == '\t')) {
                --end;
            }

            return s.substr(begin, end - begin);
        }

        std::string SliceString(const std::string& s, size_t size, off_t offset) {
            if (offset < 0) {
                return {};
            }
            const auto off = static_cast<std::size_t>(offset);
            if (off >= s.size()) {
                return {};
            }
            return s.substr(off, size);
        }

        size_t CopyVectorSlice(const std::vector<std::uint8_t>& src, char* dst, size_t size, off_t offset) {
            if (offset < 0) {
                return 0;
            }
            const auto off = static_cast<std::size_t>(offset);
            if (off >= src.size()) {
                return 0;
            }

            const std::size_t n = std::min<std::size_t>(size, src.size() - off);
            std::memcpy(dst, src.data() + off, n);
            return n;
        }

    }  // namespace

    CpuFs::CpuFs(std::size_t unit_count)
            : state_(unit_count) {
    }

    DeviceState& CpuFs::State() {
        return state_;
    }

    const DeviceState& CpuFs::State() const {
        return state_;
    }

    CpuFs* CpuFs::FromContext() {
        auto* ctx = fuse_get_context();
        return static_cast<CpuFs*>(ctx->private_data);
    }

    fuse_operations CpuFs::CreateOperations() {
        fuse_operations ops{};
        ops.getattr = &CpuFs::GetAttr;
        ops.readdir = &CpuFs::ReadDir;
        ops.open = &CpuFs::Open;
        ops.read = &CpuFs::Read;
        ops.write = &CpuFs::Write;
        ops.truncate = &CpuFs::Truncate;
        return ops;
    }

    int CpuFs::GetAttr(const char* path, struct stat* stbuf, struct fuse_file_info* /*fi*/) {
        std::memset(stbuf, 0, sizeof(struct stat));
        const auto parsed = ParsePath(path);
        auto* fs = FromContext();

        switch (parsed.kind) {
            case PathKind::Root:
            case PathKind::UnitDir:
                stbuf->st_mode = S_IFDIR | 0755;
                stbuf->st_nlink = 2;
                return 0;

            case PathKind::CtrlFile: {
                const auto content = fs->State().ReadCtrl();
                stbuf->st_mode = S_IFREG | 0644;
                stbuf->st_nlink = 1;
                stbuf->st_size = static_cast<off_t>(content.size());
                return 0;
            }

            case PathKind::PramFile: {
                if (parsed.unit_index >= fs->State().UnitCount()) {
                    return -ENOENT;
                }
                const auto content = fs->State().ReadPram(parsed.unit_index);
                stbuf->st_mode = S_IFREG | 0644;
                stbuf->st_nlink = 1;
                stbuf->st_size = static_cast<off_t>(content.size());
                return 0;
            }

            case PathKind::LramFile: {
                if (parsed.unit_index >= fs->State().UnitCount()) {
                    return -ENOENT;
                }
                const auto content = fs->State().ReadLram(parsed.unit_index);
                stbuf->st_mode = S_IFREG | 0644;
                stbuf->st_nlink = 1;
                stbuf->st_size = static_cast<off_t>(content.size());
                return 0;
            }

            case PathKind::Invalid:
            default:
                return -ENOENT;
        }
    }

    int CpuFs::ReadDir(const char* path, void* buf, fuse_fill_dir_t filler, off_t /*off*/,
                       struct fuse_file_info* /*fi*/, enum fuse_readdir_flags /*flags*/) {
        const auto parsed = ParsePath(path);
        auto* fs = FromContext();

        if (parsed.kind == PathKind::Root) {
            filler(buf, ".", nullptr, 0, static_cast<fuse_fill_dir_flags>(0));
            filler(buf, "..", nullptr, 0, static_cast<fuse_fill_dir_flags>(0));
            filler(buf, "ctrl", nullptr, 0, static_cast<fuse_fill_dir_flags>(0));

            for (std::size_t i = 0; i < fs->State().UnitCount(); ++i) {
                const std::string name = "unit" + std::to_string(i);
                filler(buf, name.c_str(), nullptr, 0, static_cast<fuse_fill_dir_flags>(0));
            }
            return 0;
        }

        if (parsed.kind == PathKind::UnitDir) {
            if (parsed.unit_index >= fs->State().UnitCount()) {
                return -ENOENT;
            }
            filler(buf, ".", nullptr, 0, static_cast<fuse_fill_dir_flags>(0));
            filler(buf, "..", nullptr, 0, static_cast<fuse_fill_dir_flags>(0));
            filler(buf, "pram", nullptr, 0, static_cast<fuse_fill_dir_flags>(0));
            filler(buf, "lram", nullptr, 0, static_cast<fuse_fill_dir_flags>(0));
            return 0;
        }

        return -ENOENT;
    }

    int CpuFs::Open(const char* path, struct fuse_file_info* /*fi*/) {
        const auto parsed = ParsePath(path);
        auto* fs = FromContext();

        switch (parsed.kind) {
            case PathKind::CtrlFile:
                return 0;
            case PathKind::PramFile:
            case PathKind::LramFile:
                if (parsed.unit_index >= fs->State().UnitCount()) {
                    return -ENOENT;
                }
                return 0;
            default:
                return -ENOENT;
        }
    }

    int CpuFs::Read(const char* path, char* buf, size_t size, off_t offset,
                    struct fuse_file_info* /*fi*/) {
        const auto parsed = ParsePath(path);
        auto* fs = FromContext();

        try {
            switch (parsed.kind) {
                case PathKind::CtrlFile: {
                    const auto content = fs->State().ReadCtrl();
                    const auto slice = SliceString(content, size, offset);
                    std::memcpy(buf, slice.data(), slice.size());
                    return static_cast<int>(slice.size());
                }

                case PathKind::PramFile: {
                    const auto content = fs->State().ReadPram(parsed.unit_index);
                    const auto slice = SliceString(content, size, offset);
                    std::memcpy(buf, slice.data(), slice.size());
                    return static_cast<int>(slice.size());
                }

                case PathKind::LramFile: {
                    const auto content = fs->State().ReadLram(parsed.unit_index);
                    return static_cast<int>(CopyVectorSlice(content, buf, size, offset));
                }

                default:
                    return -ENOENT;
            }
        } catch (...) {
            return -EIO;
        }
    }

    int CpuFs::Write(const char* path, const char* buf, size_t size, off_t offset,
                     struct fuse_file_info* /*fi*/) {
        const auto parsed = ParsePath(path);
        auto* fs = FromContext();

        try {
            switch (parsed.kind) {
                case PathKind::PramFile:
                    fs->State().WritePram(parsed.unit_index, buf, size, static_cast<std::size_t>(offset));
                    return static_cast<int>(size);

                case PathKind::LramFile:
                    fs->State().WriteLram(parsed.unit_index, buf, size, static_cast<std::size_t>(offset));
                    return static_cast<int>(size);

                case PathKind::CtrlFile: {
                    std::string command(buf, buf + size);
                    command = TrimCommand(command);
                    if (command.empty()) {
                        return static_cast<int>(size);
                    }

                    const std::size_t unit_index = static_cast<std::size_t>(std::stoul(command));
                    if (unit_index >= fs->State().UnitCount()) {
                        return -EINVAL;
                    }

                    fs->State().WriteCtrlCommand(command);
                    fs->State().ExecuteForUnit(unit_index, [](UnitState& unit) {
                        CpuExecutor::Execute(unit);
                    });
                    return static_cast<int>(size);
                }

                default:
                    return -ENOENT;
            }
        } catch (const std::invalid_argument&) {
            return -EINVAL;
        } catch (const std::out_of_range&) {
            return -EINVAL;
        } catch (...) {
            return -EIO;
        }
    }

    int CpuFs::Truncate(const char* path, off_t size, struct fuse_file_info* /*fi*/) {
        if (size < 0) {
            return -EINVAL;
        }

        const auto parsed = ParsePath(path);
        auto* fs = FromContext();

        try {
            switch (parsed.kind) {
                case PathKind::PramFile:
                    fs->State().TruncatePram(parsed.unit_index, static_cast<std::size_t>(size));
                    return 0;

                case PathKind::LramFile:
                    fs->State().TruncateLram(parsed.unit_index, static_cast<std::size_t>(size));
                    return 0;

                case PathKind::CtrlFile:
                    if (size == 0) {
                        fs->State().ClearCtrl();
                        return 0;
                    }
                    return -EINVAL;

                default:
                    return -ENOENT;
            }
        } catch (...) {
            return -EIO;
        }
    }

}  // namespace cpufs