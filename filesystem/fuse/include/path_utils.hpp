#pragma once

#include <cstddef>
#include <string>

namespace cpufs {

    enum class PathKind {
        Invalid,
        Root,
        CtrlFile,
        UnitDir,
        PramFile,
        LramFile
    };

    struct ParsedPath {
        PathKind kind = PathKind::Invalid;
        std::size_t unit_index = 0;
    };

    ParsedPath ParsePath(const std::string& path);

}  // namespace cpufs