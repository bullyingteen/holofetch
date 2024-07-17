#include "holofetch/network.hpp"
#include "holofetch/info.hpp"
#include <stdexcept>

#include <winsock2.h>
#include <iphlpapi.h>
#include <stdio.h>
#include <ws2tcpip.h>

// Link with Iphlpapi.lib and ws2_32.lib
#pragma comment(lib, "Iphlpapi.lib")
#pragma comment(lib, "ws2_32.lib")

inline char halfbyte_to_hex(uint8_t v) {
    return v < 10 ? ('0' + v) : ('A' + v - 10);
}

inline void append_byte_hex(std::string& s, uint8_t b) {
    s += halfbyte_to_hex((b >> 4) & 0xf);
    s += halfbyte_to_hex(b & 0xf);
}

namespace holofetch::network {
    
    network_info query_network_info() {
        network_info net_info;

        IP_ADAPTER_ADDRESSES* adapter_addresses(NULL);
        IP_ADAPTER_ADDRESSES* adapter(NULL);
        
        DWORD adapter_addresses_buffer_size = 16 * 1024;
        
        // Get adapter addresses
        for (int attempts = 0; attempts != 3; ++attempts) {
            adapter_addresses = (IP_ADAPTER_ADDRESSES*) malloc(adapter_addresses_buffer_size);

            DWORD error = ::GetAdaptersAddresses(AF_UNSPEC, 
                GAA_FLAG_SKIP_ANYCAST | 
                GAA_FLAG_SKIP_MULTICAST | 
                GAA_FLAG_SKIP_DNS_SERVER | 
                GAA_FLAG_SKIP_FRIENDLY_NAME,
                NULL, 
                adapter_addresses,
                &adapter_addresses_buffer_size
            );
            
            if (ERROR_SUCCESS == error) {
                break;
            } else if (ERROR_BUFFER_OVERFLOW == error) {
                // Try again with the new size
                free(adapter_addresses);
                adapter_addresses = NULL;
                continue;
            } else {
                // Unexpected error code - log and throw
                free(adapter_addresses);
                adapter_addresses = NULL;
                throw std::runtime_error("could not get network adapters: " + std::to_string(::WSAGetLastError()));
            }
        }

        // Iterate through all of the adapters
        for (adapter = adapter_addresses; NULL != adapter; adapter = adapter->Next) {
            // Skip loopback adapters
            if (IF_TYPE_SOFTWARE_LOOPBACK == adapter->IfType) continue;
            // Skip WSL/Hyper-V adapters
            if (wcsncmp(adapter->Description, L"Hyper-V", 7) == 0) continue;

            adapter_info adapter_info;
            
            if (adapter->FriendlyName) {
                adapter_info.name = holofetch::convert_to_utf8(std::wstring_view{adapter->FriendlyName, wcslen(adapter->FriendlyName)});
            } else {
                adapter_info.name = "N/A";
            }

            if (adapter->Description) {
                adapter_info.description = holofetch::convert_to_utf8(std::wstring_view{adapter->Description, wcslen(adapter->Description)});
            } else {
                adapter_info.description = "N/A";
            }
            
            // printf("D: %S\n", adapter->Description);
            // printf("F: %S\n", adapter->FriendlyName);

            for (ULONG i = 0; i < adapter->PhysicalAddressLength; ++i) {
                append_byte_hex(adapter_info.mac, adapter->PhysicalAddress[i]);
                adapter_info.mac += '-';
            }
            adapter_info.mac.resize(adapter_info.mac.size() - 1);

            // Parse all IPv4 addresses
            for (IP_ADAPTER_UNICAST_ADDRESS* address = adapter->FirstUnicastAddress; NULL != address; address = address->Next) {
                auto family = address->Address.lpSockaddr->sa_family;
                if (AF_INET == family) {
                    SOCKADDR_IN* ipv4 = reinterpret_cast<SOCKADDR_IN*>(address->Address.lpSockaddr);
                    char str_buffer[17] = {0};
                    inet_ntop(AF_INET, &(ipv4->sin_addr), str_buffer, 16);
                    adapter_info.ipv4.push_back(std::string{str_buffer});
                } else if (AF_INET6 == family) {
                    SOCKADDR_IN6* ipv6 = reinterpret_cast<SOCKADDR_IN6*>(address->Address.lpSockaddr);
                    char str_buffer[25] = {0};
                    inet_ntop(AF_INET6, &(ipv6->sin6_addr), str_buffer, 24);
                    adapter_info.ipv6.push_back(std::string{str_buffer});
                }
            }

            net_info.adapters.push_back(adapter_info);
        }

        free(adapter_addresses);
        adapter_addresses = NULL;

        return net_info;
    }

}