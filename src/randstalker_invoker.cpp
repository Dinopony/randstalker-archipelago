#include "randstalker_invoker.hpp"

#include <Windows.h>
#include <nlohmann/json.hpp>
#include <fstream>
#include <filesystem>
#include "logger.hpp"

#define LOGIC_RESULT_FILE_PATH "./reachable_sources.json"

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

std::set<std::string> invoke_randstalker_to_solve_logic(const std::string& command)
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
        Logger::error("Couldn't update logic for map tracker.");
        return {};
    }

    // Wait until child process exits
    WaitForSingleObject(pi.hProcess, INFINITE);

    std::set<std::string> location_names;

    DWORD ec;
    GetExitCodeProcess(pi.hProcess, &ec);
    if(ec == 0)
    {
        nlohmann::json result;
        std::ifstream result_file(LOGIC_RESULT_FILE_PATH);
        if(result_file.is_open())
        {
            result_file >> result;
            result_file.close();
            for(std::string location_name : result)
                location_names.insert(location_name);
        }
    }
    else
    {
        Logger::error("Couldn't update logic for map tracker.");
    }

    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);

    std::filesystem::path(LOGIC_RESULT_FILE_PATH).remove_filename();

    return location_names;
}