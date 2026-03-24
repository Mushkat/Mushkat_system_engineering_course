#include "cpu_executor.hpp"

#include <algorithm>
#include <string>

namespace cpufs {

    void CpuExecutor::Execute(UnitState& unit) {
        const bool should_sort =
                unit.pram.find("std::sort") != std::string::npos ||
                unit.pram.find("sort(") != std::string::npos ||
                unit.pram.find("sort") != std::string::npos;

        if (should_sort) {
            std::sort(unit.lram.begin(), unit.lram.end());
        }
    }

}  // namespace cpufs