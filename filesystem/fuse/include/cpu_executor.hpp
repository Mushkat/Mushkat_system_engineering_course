#pragma once

#include "cpu_state.hpp"

namespace cpufs {

    class CpuExecutor {
    public:
        static void Execute(UnitState& unit);
    };

}  // namespace cpufs