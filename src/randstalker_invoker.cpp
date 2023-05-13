#include "randstalker_invoker.hpp"
#include <Windows.h>
#include "logger.hpp"

#ifdef DEBUG
    #define PROCESS_FLAGS 0
#else
    #define PROCESS_FLAGS CREATE_NO_WINDOW
#endif

bool invoke(const std::string& command)
{
    STARTUPINFO si;
    ZeroMemory(&si, sizeof(si));
    si.cb = sizeof(si);

    PROCESS_INFORMATION pi;
    ZeroMemory(&pi, sizeof(pi));

    char command_c_str[2048];
    sprintf(command_c_str, "%s", command.c_str());

    if(!CreateProcess(nullptr, command_c_str, nullptr, nullptr, FALSE, PROCESS_FLAGS, nullptr, nullptr, &si, &pi))
    {
        Logger::error("Couldn't create Randstalker process to build ROM.");
        return false;
    }

    // Wait until child process exits
    WaitForSingleObject(pi.hProcess, INFINITE);

    DWORD ec;
    GetExitCodeProcess(pi.hProcess, &ec);

    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    return ec == 0;
}