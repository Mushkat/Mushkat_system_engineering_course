#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <mutex>
#include <string>
#include <vector>

namespace cpufs {

    struct UnitState {
        std::string pram;
        std::vector<std::uint8_t> lram;
    };

    class DeviceState {
    public:
        explicit DeviceState(std::size_t unit_count);

        std::size_t UnitCount() const;

        std::string ReadPram(std::size_t unit_index) const;
        std::vector<std::uint8_t> ReadLram(std::size_t unit_index) const;
        std::string ReadCtrl() const;

        void WritePram(std::size_t unit_index, const char* data, std::size_t size, std::size_t offset);
        void WriteLram(std::size_t unit_index, const char* data, std::size_t size, std::size_t offset);
        void WriteCtrlCommand(const std::string& command);

        void TruncatePram(std::size_t unit_index, std::size_t size);
        void TruncateLram(std::size_t unit_index, std::size_t size);
        void ClearCtrl();

        void ExecuteForUnit(std::size_t unit_index, const std::function<void(UnitState&)>& fn);

        const UnitState& GetUnit(std::size_t unit_index) const;

    private:
        void CheckUnitIndex(std::size_t unit_index) const;

    private:
        std::vector<UnitState> units_;
        std::vector<std::string> ctrl_log_;
        mutable std::mutex mutex_;
    };

}  // namespace cpufs