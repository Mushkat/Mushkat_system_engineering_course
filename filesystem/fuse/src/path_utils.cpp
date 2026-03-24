#include "path_utils.hpp"

#include <charconv>
#include <string>

namespace cpufs {
    namespace {

        bool ParseUnitName(const std::string& token, std::size_t& unit_index) {
            if (token.rfind("unit", 0) != 0) {
                return false;
            }

            const std::string suffix = token.substr(4);
            if (suffix.empty()) {
                return false;
            }

            std::size_t parsed = 0;
            const auto result = std::from_chars(suffix.data(), suffix.data() + suffix.size(), parsed);
            if (result.ec != std::errc{} || result.ptr != suffix.data() + suffix.size()) {
                return false;
            }

            unit_index = parsed;
            return true;
        }

    }  // namespace

    ParsedPath ParsePath(const std::string& path) {
        if (path == "/") {
            return {PathKind::Root, 0};
        }

        if (path == "/ctrl") {
            return {PathKind::CtrlFile, 0};
        }

        if (path.size() > 1 && path[0] == '/') {
            const std::string rest = path.substr(1);
            const auto slash = rest.find('/');

            if (slash == std::string::npos) {
                std::size_t unit_index = 0;
                if (ParseUnitName(rest, unit_index)) {
                    return {PathKind::UnitDir, unit_index};
                }
                return {PathKind::Invalid, 0};
            }

            const std::string unit_token = rest.substr(0, slash);
            const std::string leaf = rest.substr(slash + 1);

            std::size_t unit_index = 0;
            if (!ParseUnitName(unit_token, unit_index)) {
                return {PathKind::Invalid, 0};
            }

            if (leaf == "pram") {
                return {PathKind::PramFile, unit_index};
            }
            if (leaf == "lram") {
                return {PathKind::LramFile, unit_index};
            }
        }

        return {PathKind::Invalid, 0};
    }

}  // namespace cpufs