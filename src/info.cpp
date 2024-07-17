#include "holofetch/info.hpp"

#ifndef WIN32_LEAN_AND_MEAN
#   define WIN32_LEAN_AND_MEAN 1
#endif

#ifndef UMDF_USING_NTSTATUS
#   define UMDF_USING_NTSTATUS 1
#endif

#include <windows.h>
#include <winternl.h>
#include <ntstatus.h>
#include <Lmcons.h>

#include <string>
#include <codecvt>
#include <iostream>
#include <thread>
#include <format>
#include <stdexcept>

#include <cassert>

#include "m4x1m1l14n/Registry.hpp"
#include "holofetch/subprocess.hpp"

namespace holofetch {
    
    std::string find_executable_path(std::string_view command) noexcept {
        auto cmd = std::vformat("WHERE.EXE {}", std::make_format_args(command));
        auto result = subprocess::run(cmd);
        
        if (result.has_value() && result.value().code == 0) {
            std::string path = std::move(result.value().out);
            return path;
        }

        return {};
    }

    std::string find_env_var(std::string_view name) noexcept {
        if (const char* key = getenv(name.data()); key) {
            return std::string{key};
        }
        return {};
    }

}

namespace {

    std::string get_msvc_version() {
        auto result = holofetch::subprocess::run("CL.EXE");
        constexpr std::string_view prefix{"Microsoft (R) C/C++ Optimizing Compiler Version "};
        
        if (result.has_value() && result.value().code == 0) {
            const auto& out = result.value().err; // Microsoft :facepalm:
            
            auto pos = out.find('\n', prefix.size());
            if (pos == std::string_view::npos) {
                return {};
            }

            return out.substr( prefix.size(), pos - prefix.size() - 1 );
        }
        
        return {};
    }

    std::string get_rustc_version() {
        auto result = holofetch::subprocess::run("rustc --version");
        if (result.has_value() && result.value().code == 0) {
            auto& out = result.value().out;
            auto pos = out.find('(');
            return out.substr(sizeof("rustc ") - 1, pos == std::string::npos ? pos : pos - sizeof("rustc "));
        }
        return {};
    }

    std::string get_python_version() {
        auto result = holofetch::subprocess::run("py --version");
        if (result.has_value() && result.value().code == 0) {
            auto& out = result.value().out;
            auto pos = out.find('\n', sizeof("Python ") - 1);
            auto s = out.substr(sizeof("Python ") - 1, pos == std::string::npos ? pos : pos - sizeof("Python "));
            return s;
        }
        return {};
    }

    std::string get_pwsh_version() {
        auto result = holofetch::subprocess::run("pwsh --version");
        if (result.has_value() && result.value().code == 0) {
            auto& out = result.value().out;
            auto pos = out.find('\n');
            return out.substr(sizeof("PowerShell ") - 1, pos == std::string::npos ? pos : pos - sizeof("PowerShell "));
        }
        return {};
    }

    
    inline holofetch::swap_info my_fetch_swap() {
        holofetch::swap_info si;

        SYSTEM_INFO sysInfo;
        GetNativeSystemInfo(&sysInfo);

        HMODULE m = LoadLibraryW(L"NTDLL.DLL");
        if (!m)
            throw std::runtime_error("could not load ntdll");
        
        using NtQuerySystemInformationType = NTSTATUS (NTAPI *)(
            IN SYSTEM_INFORMATION_CLASS SystemInformationClass,
            OUT PVOID SystemInformation,
            IN ULONG SystemInformationLength,
            OUT PULONG ReturnLength OPTIONAL
        );

        auto fp = (NtQuerySystemInformationType)GetProcAddress(m, "NtQuerySystemInformation");
        
        uint8_t buffer[1024];
        ULONG size = sizeof(buffer);
        
        
        struct __SYSTEM_PAGEFILE_INFORMATION {
            ULONG NextEntryOffset;
            ULONG TotalSize;
            ULONG TotalInUse;
            ULONG PeakUsage;
            UNICODE_STRING PageFileNmae;
        };
        __SYSTEM_PAGEFILE_INFORMATION* pstart = (__SYSTEM_PAGEFILE_INFORMATION*) buffer;
        if(!NT_SUCCESS(fp((SYSTEM_INFORMATION_CLASS)0x12, pstart, size, &size)))
            throw std::runtime_error("NtQuerySystemInformation(0x12, size) failed");

        FreeLibrary(m);

        si.usedMB = ((uint64_t)pstart->TotalInUse * sysInfo.dwPageSize) / 1024 / 1024;
        si.totalMB = ((uint64_t)pstart->TotalSize * sysInfo.dwPageSize) / 1024 / 1024;
        si.peakMB = ((uint64_t)pstart->PeakUsage * sysInfo.dwPageSize) / 1024 / 1024;
        si.percent = (si.usedMB * 100) / si.totalMB;

        return si;
    }

    inline holofetch::terminal_info my_fetch_terminal() {
        holofetch::terminal_info info_;

        char buffer[UNLEN + 1]{};

        DWORD size = UNLEN + 1;
        GetComputerNameA(buffer, &size);
        info_.hostname = std::string(buffer);
        
        size = UNLEN + 1;
        GetUserNameA(buffer, &size);
        info_.username = std::string(buffer);

        std::setlocale(LC_ALL, "en_US.UTF-8");
        WCHAR console[UNLEN + 1]{};
        size = GetConsoleTitleW((WCHAR*)console, 256);
        info_.tab = holofetch::convert_to_utf8(std::wstring_view{console});

        return info_;
    }

    inline std::vector<holofetch::display_info> my_fetch_displays() {
        std::vector<holofetch::display_info> infos_;

        for (size_t i = 0;; ++i) {
            DISPLAY_DEVICEW dd{};
            dd.cb = sizeof(dd);
            if (!EnumDisplayDevicesW(NULL, i, &dd, 0))
                break;

            DEVMODEW dm{};
            dm.dmSize = sizeof(dm);
            dm.dmDriverExtra = 0;
            if (!EnumDisplaySettingsW(dd.DeviceName, ENUM_CURRENT_SETTINGS, &dm)) {
                continue;
            }
            
            infos_.push_back(holofetch::display_info{
                .index = static_cast<uint32_t>(i),
                .name = holofetch::convert_to_utf8( std::wstring(dd.DeviceString) ),
                .width = dm.dmPelsWidth,
                .height = dm.dmPelsHeight,
                .frequency = dm.dmDisplayFrequency
            });
        }

        return infos_;
    }

    inline std::vector<holofetch::disk_info> my_fetch_disks() {
        std::vector<holofetch::disk_info> info_;

        WCHAR drives[64];
        DWORD size = GetLogicalDriveStringsW(64, drives);

        WCHAR drive_path[] = L"A:\\";
        
        for (DWORD i = 0; i < size; ++i) {
            drive_path[0] = drives[i];
            
            ULARGE_INTEGER lpFreeBytesAvailableToCaller;
            ULARGE_INTEGER lpTotalNumberOfBytes;
            ULARGE_INTEGER lpTotalNumberOfFreeBytes;
            if (!GetDiskFreeSpaceExW(drive_path, &lpFreeBytesAvailableToCaller, &lpTotalNumberOfBytes, &lpTotalNumberOfFreeBytes))
                continue;
            
            uint64_t capacity_ = lpTotalNumberOfBytes.QuadPart / 1024 / 1024;
            uint64_t used_ = capacity_ - lpTotalNumberOfFreeBytes.QuadPart / 1024 / 1024;

            info_.push_back(holofetch::disk_info{
                .usedMB = used_,
                .totalMB = capacity_,
                .percent = static_cast<uint32_t>(used_ * 100 / capacity_),
                .id = static_cast<char>( 'A' + static_cast<int>(drives[i] - L'A') )
            });
        }

        return info_;
    }

    inline holofetch::hardware_info my_fetch_hardware() {
        holofetch::hardware_info info_;

        // CPU
        if (auto key = m4x1m1l14n::Registry::LocalMachine->Open(L"HARDWARE\\DESCRIPTION\\System\\CentralProcessor\\0"); key) {
            info_.cpu.name = holofetch::convert_to_utf8( key->GetString(L"ProcessorNameString") );
            info_.cpu.rate = key->GetInt32(L"~MHz");
        } else {
            info_.cpu.name = "N/A";
            info_.cpu.rate = 0;
        }
        info_.cpu.cores = std::thread::hardware_concurrency();

        // GPU
        if (auto key = m4x1m1l14n::Registry::LocalMachine->Open(L"SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion\\WinSAT"); key) {
            info_.gpu.name = holofetch::convert_to_utf8( key->GetString(L"PrimaryAdapterString") );
        } else {
            info_.gpu.name = "N/A";
        }

        // MEM
        if (MEMORYSTATUSEX mem{ .dwLength = sizeof(mem)}; GlobalMemoryStatusEx(&mem)) {
            info_.mem.totalMB = mem.ullTotalPhys / 1024 / 1024;
            info_.mem.usedMB = info_.mem.totalMB - mem.ullAvailPhys / 1024 / 1024;
            info_.mem.percent = mem.dwMemoryLoad;
        }

        info_.swap = my_fetch_swap();
        info_.disks = my_fetch_disks();
        info_.displays = my_fetch_displays();

        return info_;
    }

    inline holofetch::software_info my_fetch_software() {
        holofetch::software_info info_;

        if (auto key = m4x1m1l14n::Registry::LocalMachine->Open(L"SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion"); key) {
            auto productName = key->GetString(L"ProductName");
            auto displayVersion = key->GetString(L"DisplayVersion");
            auto currentBuildNumber = key->GetString(L"CurrentBuildNumber");

            if (std::stoi(currentBuildNumber) > 21999) {
                auto editionId = key->GetString(L"EditionId");
                productName = L"Windows 11 " + editionId.substr(0, 3);
            }

            info_.os = holofetch::convert_to_utf8( productName + L" " + displayVersion + L" (build " + currentBuildNumber + L")" );
        }

        info_.pwsh = get_pwsh_version();
        info_.c = get_msvc_version();
        info_.rust = get_rustc_version();
        info_.py = get_python_version();

        return info_;
    }

    inline holofetch::host_info my_fetch_host() {
        holofetch::host_info info_;

        info_.terminal = my_fetch_terminal();
        info_.hardware = my_fetch_hardware();
        info_.software = my_fetch_software();
        info_.uptime = std::chrono::milliseconds{ GetTickCount64() };

        return info_;
    }

} // namespace


std::string holofetch::convert_to_utf8(std::wstring_view wstr) {
    std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>> _UTF_Converter;
    return _UTF_Converter.to_bytes(wstr.data(), wstr.data() + wstr.size());
}

holofetch::host_info holofetch::query_host_info() {
    return my_fetch_host();
}
