#include "randstalker_invoker.hpp"
#include <Windows.h>
#include <iostream>

bool invoke(char* command)
{
    STARTUPINFO si;
    ZeroMemory(&si, sizeof(si));
    si.cb = sizeof(si);

    PROCESS_INFORMATION pi;
    ZeroMemory(&pi, sizeof(pi));

    if(!CreateProcess(nullptr, command, nullptr, nullptr, FALSE, 0, nullptr, nullptr, &si, &pi))
    {
        std::cerr << "CreateProcess failed (" << GetLastError() << ").\n";
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