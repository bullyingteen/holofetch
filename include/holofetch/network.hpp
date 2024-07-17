#pragma once

#include <string>
#include <vector>

namespace holofetch::network {

    struct adapter_info {
        std::string name;
        std::string description;
        std::string mac;
        std::vector<std::string> ipv4;
        std::vector<std::string> ipv6;
    };

    struct network_info {
        std::vector<adapter_info> adapters;
    };

    network_info query_network_info();

} // namespace holofetch::network
