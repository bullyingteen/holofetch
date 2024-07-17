#include "holofetch/subprocess.hpp"

#ifndef WIN32_LEAN_AND_MEAN
#   define WIN32_LEAN_AND_MEAN 1
#endif
#include <windows.h>

namespace holofetch::subprocess {
    /* 
     * source:
     *  https://stackoverflow.com/questions/478898/how-do-i-execute-a-command-and-get-the-output-of-the-command-within-c-using-po
     */
    auto run(std::string_view command, const std::filesystem::path& working_directory) noexcept -> std::expected<result, std::errc> {
        int                  Success;
        SECURITY_ATTRIBUTES  security_attributes;
        HANDLE               stdout_rd = INVALID_HANDLE_VALUE;
        HANDLE               stdout_wr = INVALID_HANDLE_VALUE;
        HANDLE               stderr_rd = INVALID_HANDLE_VALUE;
        HANDLE               stderr_wr = INVALID_HANDLE_VALUE;
        PROCESS_INFORMATION  process_info;
        STARTUPINFOA         startup_info;
        std::jthread         stdout_thread;
        std::jthread         stderr_thread;

        std::ostringstream stdout_stream;
        std::ostringstream stderr_stream;
        int code;

        security_attributes.nLength              = sizeof(SECURITY_ATTRIBUTES);
        security_attributes.bInheritHandle       = TRUE;
        security_attributes.lpSecurityDescriptor = nullptr;

        if (!CreatePipe(&stdout_rd, &stdout_wr, &security_attributes, 0) ||
                !SetHandleInformation(stdout_rd, HANDLE_FLAG_INHERIT, 0)) {
            return std::unexpected(std::errc::broken_pipe);
        }

        if (!CreatePipe(&stderr_rd, &stderr_wr, &security_attributes, 0) ||
                !SetHandleInformation(stderr_rd, HANDLE_FLAG_INHERIT, 0)) {
            if (stdout_rd != INVALID_HANDLE_VALUE) CloseHandle(stdout_rd);
            if (stdout_wr != INVALID_HANDLE_VALUE) CloseHandle(stdout_wr);
            return std::unexpected(std::errc::broken_pipe);
        }

        ZeroMemory(&process_info, sizeof(PROCESS_INFORMATION));
        ZeroMemory(&startup_info, sizeof(STARTUPINFOA));

        startup_info.cb         = sizeof(STARTUPINFOA);
        startup_info.hStdInput  = 0;
        startup_info.hStdOutput = stdout_wr;
        startup_info.hStdError  = stderr_wr;

        if(stdout_rd || stderr_rd)
            startup_info.dwFlags |= STARTF_USESTDHANDLES;

        std::string command_copy{command.data(), command.size()};
        std::string directory = working_directory.string();

        Success = CreateProcessA(
            nullptr,
            command_copy.data(),
            nullptr,
            nullptr,
            TRUE,
            CREATE_NO_WINDOW,
            nullptr,
            directory.c_str(),
            &startup_info,
            &process_info
        );
        CloseHandle(stdout_wr);
        CloseHandle(stderr_wr);

        if(!Success) {
            CloseHandle(process_info.hProcess);
            CloseHandle(process_info.hThread);
            CloseHandle(stdout_rd);
            CloseHandle(stderr_rd);
            return std::unexpected(std::errc::no_child_process);
        }
        else {
            CloseHandle(process_info.hThread);
        }

        if(stdout_rd) {
            stdout_thread=std::jthread([&]() {
                DWORD n;
                char buffer[1024];
                for(;;) {
                    n = 0;
                    int Success = ReadFile(
                        stdout_rd,
                        buffer,
                        (DWORD)1024,
                        &n,
                        nullptr
                    );
                    if(!Success || n == 0)
                        break;
                    stdout_stream << std::string_view{buffer, n};
                }
            });
        }

        if(stderr_rd) {
            stderr_thread = std::jthread([&]() {
                DWORD n;
                char buffer[1024];
                for(;;) {
                    n = 0;
                    int Success = ReadFile(
                        stderr_rd,
                        buffer,
                        (DWORD)1024,
                        &n,
                        nullptr
                    );
                    if(!Success || n == 0)
                        break;
                    stderr_stream << std::string_view{buffer, n};
                }
            });
        }

        WaitForSingleObject(process_info.hProcess, INFINITE);
        if(!GetExitCodeProcess(process_info.hProcess, (DWORD*) &code))
            code = -1;

        CloseHandle(process_info.hProcess);

        if(stdout_thread.joinable())
            stdout_thread.join();

        if(stderr_thread.joinable())
            stderr_thread.join();

        CloseHandle(stdout_rd);
        CloseHandle(stderr_rd);

        return result{
            .out = stdout_stream.str(),
            .err = stderr_stream.str(),
            .code = code
        };
    }

} // namespace subprocess
