#include <filesystem>
#include "argparse.hpp"

#include "holofetch/info.hpp"
#include "holofetch/network.hpp"
#include "holofetch/renderer.hpp"


template <class T>
std::string format_memory_info(const T& mem) {
    double used_ram = static_cast<double>(mem.usedMB) / 1000.;
    double total_ram = static_cast<double>(mem.totalMB) / 1000.;
    
    std::string_view percent_color = holofetch::ANSI::fg_bright_green;
    if (mem.percent > 80) {
        percent_color = holofetch::ANSI::fg_bright_yellow;
    } else if (mem.percent > 90) {
        percent_color = holofetch::ANSI::fg_bright_red;
    }

    auto s = std::vformat("{:0>3} GB / {:0>3} GB [ {}{}{}% ]", std::make_format_args(
        used_ram, total_ram,
        percent_color, mem.percent, holofetch::ANSI::reset
    ));

    if constexpr (requires (T t) { { t.peakMB }; }) {
        double peak = static_cast<double>(mem.peakMB) / 1000.;
        s += std::vformat(" ^ {:0>3} GB", std::make_format_args(peak));
    }

    return s;
}

std::string format_disk_info(const holofetch::disk_info& disk) {
    double used = static_cast<double>(disk.usedMB) / 1000.;
    double total = static_cast<double>(disk.totalMB) / 1000.;
    
    std::string_view used_suffix = "GB";
    std::string_view total_suffix = "GB";
    
    if (used > 100.) {
        used = static_cast<double>(disk.usedMB / 1000) / 1000.;
        used_suffix = "TB";
    }
    if (total > 100.) {
        total = static_cast<double>(disk.totalMB / 1000) / 1000.;
        total_suffix = "TB";
    }

    std::string_view percent_color = holofetch::ANSI::fg_bright_green;
    if (disk.percent > 80) {
        percent_color = holofetch::ANSI::fg_bright_yellow;
    } else if (disk.percent > 90) {
        percent_color = holofetch::ANSI::fg_bright_red;
    }

    return std::vformat("{:0>3} {} / {:0>3} {} [ {}{}{}% ]", std::make_format_args(
        used, used_suffix, 
        total, total_suffix,
        percent_color, disk.percent, holofetch::ANSI::reset
    ));
}

std::vector<holofetch::section> format_host_info_sections() {
    auto host_info = holofetch::query_host_info();

    std::vector<holofetch::section> sections;

    const auto ms = host_info.uptime.count() % 1000;
    const auto ss = std::chrono::duration_cast<std::chrono::seconds>(host_info.uptime).count() % 60;
    const auto mm = std::chrono::duration_cast<std::chrono::minutes>(host_info.uptime).count() % 60;
    const auto hh = std::chrono::duration_cast<std::chrono::hours>(host_info.uptime).count() % 24;
    const auto days = std::chrono::duration_cast<std::chrono::days>(host_info.uptime).count();

    sections.emplace_back("Hardware", std::vector<std::pair<std::string, std::string>>{
        {"CPU", host_info.hardware.cpu.name},
        {"GPU", host_info.hardware.gpu.name},
        {"RAM", format_memory_info(host_info.hardware.mem)},
        {"Swap", format_memory_info(host_info.hardware.swap)}
    });

    auto& disks_section = sections.emplace_back("Disks", std::vector<std::pair<std::string, std::string>>{});
    for (const holofetch::disk_info& disk : host_info.hardware.disks) {
        disks_section.properties.emplace_back(std::string(1, disk.id), format_disk_info(disk));
    }

    auto& displays_section = sections.emplace_back("Displays", std::vector<std::pair<std::string, std::string>>{});
    for (const holofetch::display_info& display : host_info.hardware.displays) {
        displays_section.properties.emplace_back(std::to_string(display.index), std::vformat("{}x{} {}Hz @ {}", 
            std::make_format_args(display.width, display.height, display.frequency, display.name)
        ));
    }

    auto net_info = holofetch::network::query_network_info();
    for (const auto& adapter : net_info.adapters) {
        auto& adapter_section = sections.emplace_back("Network Adapter", std::vector<std::pair<std::string, std::string>>{
            {"Name", adapter.name},
            {"MAC", adapter.mac}
        });

        for (const auto& addr4 : adapter.ipv4) {
            if (addr4.empty())
                continue;
            adapter_section.properties.emplace_back("IPv4", addr4);
        }

        for (const auto& addr6 : adapter.ipv6) {
            if (addr6.empty())
                continue;
            adapter_section.properties.emplace_back("IPv6", addr6);
        }
    }

    auto& software_section = sections.emplace_back("Software", std::vector<std::pair<std::string, std::string>>{
        {"OS", host_info.software.os}
    });

    if (!host_info.software.pwsh.empty()) {
        software_section.properties.emplace_back("PowerShell", host_info.software.pwsh);
    }

    if (!host_info.software.c.empty()) {
        software_section.properties.emplace_back("C/C++", "cl " + host_info.software.c);
    }

    if (!host_info.software.py.empty()) {
        software_section.properties.emplace_back("Python", host_info.software.py);
    }

    if (!host_info.software.rust.empty()) {
        software_section.properties.emplace_back("Rust", host_info.software.rust);
    }

    sections.emplace_back("Terminal", std::vector<std::pair<std::string, std::string>>{
        {"Tab", host_info.terminal.tab},
        {"Host", host_info.terminal.hostname},
        {"User", host_info.terminal.username},
        {"Up", std::vformat("{}D {:0>2}h:{:0>2}m:{:0>2}s.{:0>3}ms", std::make_format_args(days, hh, mm, ss, ms))}
    });

    return sections;
}

int main(int ac, char** av) {
    auto argparser = argparse::ArgumentParser("holofetch");
    
    std::string template_path; // = "C:\\Development\\Projects\\holofetch\\assets\\prerendered_image_data.utf.ans";
    argparser.add_argument("template").store_into(template_path);

    std::string texture;
    argparser.add_argument("--texture").store_into(texture);
    
    // argparser.add_argument("--border-top");
    // argparser.add_argument("--border-left");
    // argparser.add_argument("--border-right");
    // argparser.add_argument("--border-bottom");

    try {
        argparser.parse_args(ac, av);
    } catch (const std::exception& e) {
        std::cerr << "ERROR: could not parse arguments: " << e.what() << std::endl;
        std::cerr << argparser;
        return 1;
    }

    try {
        
        holofetch::image avatar;
        std::filesystem::path template_file{template_path};
        
        avatar.data = holofetch::read_file(template_file);
        if (!texture.empty()) {
            avatar.texture = texture;
        }

        auto renderer_ = holofetch::renderer();
        renderer_.set_avatar(std::move(avatar));
        renderer_.draw( format_host_info_sections() );

    } catch (const std::exception& e) {
        std::cerr << "ERROR: could not render holofetch: " << e.what() << std::endl;
        return 2;
    }

    return 0;
}