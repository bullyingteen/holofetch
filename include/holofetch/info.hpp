#pragma once

#include <string>
#include <vector>
#include <chrono>

namespace holofetch {

    std::string convert_to_utf8(std::wstring_view wstr);

    struct cpu_info {
        std::string name;
        uint32_t cores{0};
        uint32_t rate{0};
    };

    struct gpu_info {
        std::string name;
    };

    struct memory_info {
        uint64_t usedMB{0};
        uint64_t totalMB{0};
        uint32_t percent{0};
    };

    struct disk_info {
        uint64_t usedMB{0};
        uint64_t totalMB{0};
        uint32_t percent{0};
        char id{'C'};
    };

    struct swap_info {
        uint64_t usedMB{0};
        uint64_t totalMB{0};
        uint64_t peakMB{0};
        uint32_t percent{0};
    };

    struct display_info {
        uint32_t index{0};
        std::string name;
        uint32_t width{0};
        uint32_t height{0};
        uint32_t frequency{0};
    };

    struct hardware_info {
        cpu_info cpu;
        gpu_info gpu;
        memory_info mem;
        swap_info swap;
        std::vector<disk_info> disks;
        std::vector<display_info> displays;
    };

    struct software_info {
        std::string os;
        std::string pwsh;
        std::string c;
        std::string py;
        std::string rust;
    };

    struct terminal_info {
        std::string tab;
        std::string hostname;
        std::string username;
    };

    struct host_info {
        terminal_info terminal;
        hardware_info hardware;
        software_info software;
        std::chrono::milliseconds uptime{0};
    };

    std::string find_executable_path(std::string_view command) noexcept;
    std::string find_env_var(std::string_view name) noexcept;

    host_info query_host_info();

} // namespace holofetch
