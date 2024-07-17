#pragma once

#include <cstdio>
#include <cstdint>
#include <string>
#include <sstream>
#include <string_view>
#include <thread>
#include <expected>
#include <stdexcept>
#include <filesystem>

namespace holofetch::subprocess {

    struct result {
        std::string out;
        std::string err;
        int code;
    };

    /*
     * Passing command by value is intentional
     */
    auto run(std::string_view command, const std::filesystem::path& working_directory = std::filesystem::current_path()) noexcept 
        -> std::expected<result, std::errc>;

} // namespace holofetch::subprocess
