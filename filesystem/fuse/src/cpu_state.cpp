#include "cpu_state.hpp"

#include <algorithm>
#include <stdexcept>

namespace cpufs {

    DeviceState::DeviceState(std::size_t unit_count)
            : units_(unit_count) {
    }

    std::size_t DeviceState::UnitCount() const {
        return units_.size();
    }

    void DeviceState::CheckUnitIndex(std::size_t unit_index) const {
        if (unit_index >= units_.size()) {
            throw std::out_of_range("unit index out of range");
        }
    }

    std::string DeviceState::ReadPram(std::size_t unit_index) const {
        std::lock_guard<std::mutex> lock(mutex_);
        CheckUnitIndex(unit_index);
        return units_[unit_index].pram;
    }

    std::vector<std::uint8_t> DeviceState::ReadLram(std::size_t unit_index) const {
        std::lock_guard<std::mutex> lock(mutex_);
        CheckUnitIndex(unit_index);
        return units_[unit_index].lram;
    }

    std::string DeviceState::ReadCtrl() const {
        std::lock_guard<std::mutex> lock(mutex_);
        std::string result;
        for (const auto& line : ctrl_log_) {
            result += line;
            result += '\n';
        }
        return result;
    }

    void DeviceState::WritePram(std::size_t unit_index, const char* data, std::size_t size, std::size_t offset) {
        std::lock_guard<std::mutex> lock(mutex_);
        CheckUnitIndex(unit_index);

        auto& pram = units_[unit_index].pram;
        if (offset > pram.size()) {
            pram.resize(offset, '\0');
        }
        if (offset + size > pram.size()) {
            pram.resize(offset + size);
        }
        std::copy(data, data + size, pram.begin() + static_cast<std::ptrdiff_t>(offset));
    }

    void DeviceState::WriteLram(std::size_t unit_index, const char* data, std::size_t size, std::size_t offset) {
        std::lock_guard<std::mutex> lock(mutex_);
        CheckUnitIndex(unit_index);

        auto& lram = units_[unit_index].lram;
        if (offset > lram.size()) {
            lram.resize(offset, 0);
        }
        if (offset + size > lram.size()) {
            lram.resize(offset + size);
        }
        std::copy(reinterpret_cast<const std::uint8_t*>(data),
                  reinterpret_cast<const std::uint8_t*>(data) + size,
                  lram.begin() + static_cast<std::ptrdiff_t>(offset));
    }

    void DeviceState::WriteCtrlCommand(const std::string& command) {
        std::lock_guard<std::mutex> lock(mutex_);
        ctrl_log_.push_back(command);
    }

    void DeviceState::TruncatePram(std::size_t unit_index, std::size_t size) {
        std::lock_guard<std::mutex> lock(mutex_);
        CheckUnitIndex(unit_index);
        units_[unit_index].pram.resize(size, '\0');
    }

    void DeviceState::TruncateLram(std::size_t unit_index, std::size_t size) {
        std::lock_guard<std::mutex> lock(mutex_);
        CheckUnitIndex(unit_index);
        units_[unit_index].lram.resize(size, 0);
    }

    void DeviceState::ClearCtrl() {
        std::lock_guard<std::mutex> lock(mutex_);
        ctrl_log_.clear();
    }

    void DeviceState::ExecuteForUnit(std::size_t unit_index, const std::function<void(UnitState&)>& fn) {
        std::lock_guard<std::mutex> lock(mutex_);
        CheckUnitIndex(unit_index);
        fn(units_[unit_index]);
    }

    const UnitState& DeviceState::GetUnit(std::size_t unit_index) const {
        CheckUnitIndex(unit_index);
        return units_[unit_index];
    }

}  // namespace cpufs