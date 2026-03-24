#include "cpu_fs.hpp"

#include <cstdlib>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

namespace {

    void PrintUsage(const char* exe_name) {
        std::cerr << "Usage:\n";
        std::cerr << "  " << exe_name << " --units=<count> <mountpoint> [fuse options]\n";
        std::cerr << "  " << exe_name << " --units <count> <mountpoint> [fuse options]\n";
    }

    bool ParseUnitsArg(const std::string& arg, std::size_t& value, bool& consumes_next) {
        const std::string prefix = "--units=";
        if (arg.rfind(prefix, 0) == 0) {
            value = static_cast<std::size_t>(std::stoul(arg.substr(prefix.size())));
            consumes_next = false;
            return true;
        }
        if (arg == "--units") {
            consumes_next = true;
            return true;
        }
        return false;
    }

}  // namespace

int main(int argc, char* argv[]) {
    if (argc < 3) {
        PrintUsage(argv[0]);
        return 1;
    }

    std::size_t unit_count = 0;
    std::vector<char*> fuse_args;
    fuse_args.push_back(argv[0]);

    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];

        bool consumes_next = false;
        if (ParseUnitsArg(arg, unit_count, consumes_next)) {
            if (consumes_next) {
                if (i + 1 >= argc) {
                    std::cerr << "Missing value after --units\n";
                    return 1;
                }
                unit_count = static_cast<std::size_t>(std::stoul(argv[++i]));
            }
            continue;
        }

        fuse_args.push_back(argv[i]);
    }

    if (unit_count == 0) {
        std::cerr << "unit_count must be positive\n";
        return 1;
    }

    auto fs = std::make_unique<cpufs::CpuFs>(unit_count);
    auto ops = cpufs::CpuFs::CreateOperations();

    struct fuse_args args = FUSE_ARGS_INIT(static_cast<int>(fuse_args.size()), fuse_args.data());
    const int result = fuse_main(args.argc, args.argv, &ops, fs.get());
    fuse_opt_free_args(&args);

    return result;
}