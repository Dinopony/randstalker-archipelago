#include "retroarch_mem_interface.hpp"

#include <TlHelp32.h>
#include <Psapi.h>
#include <utility>
#include <iostream>
#include "logger.hpp"

RetroarchMemInterface::RetroarchMemInterface()
{
    //get_debug_privileges();
    _process_id = find_process_id("retroarch.exe");
    if(_process_id == -1)
        throw EmulatorException("Could not find a Retroarch process currently running.");

    _process_handle = OpenProcess(PROCESS_VM_READ | PROCESS_VM_WRITE | PROCESS_QUERY_INFORMATION, false, _process_id);
    if(!read_module_information(_process_handle, "genesis_plus_gx_libretro.dll"))
    {
        CloseHandle(_process_handle);
        throw EmulatorException("Could not find Genesis Plus GX core running.");
    }

    Logger::debug("Hooked on Genesis Plus GX core.");

    std::ostringstream oss;
    oss << "Addr = 0x" << std::hex << _base_address;
    Logger::debug(oss.str());

    std::ostringstream oss2;
    oss2 << "Size = 0x" << std::hex << _module_size;
    Logger::debug(oss2.str());

    _game_ram_base_address = this->find_gpgx_ram_base_addr();
    if(_game_ram_base_address == UINT64_MAX)
    {
        Logger::error("Could not find any known signature on this core version.\n"
                      "Please notify the author with your version of Retroarch & Genesis Plus GX.");
        return;
    }

    std::ostringstream oss3;
    oss3 << "Game RAM = 0x" << std::hex << _game_ram_base_address;
    Logger::debug(oss3.str());

    Logger::info("Successfully connected to Retroarch.");
}

RetroarchMemInterface::~RetroarchMemInterface()
{
    CloseHandle(_process_handle);
}

uint8_t RetroarchMemInterface::read_game_byte(uint16_t address) const
{
    uint16_t even_address = address - (address % 2);
    uint16_t word = this->read_game_word(even_address);
    if(even_address == address)
        return static_cast<uint8_t>(word >> 8);
    return static_cast<uint8_t>(word);
}

uint16_t RetroarchMemInterface::read_game_word(uint16_t address) const
{
    uint16_t buffer = 0;
    SIZE_T bytes_read;

    LPCVOID addr = reinterpret_cast<LPCVOID>(_game_ram_base_address + address);
    BOOL success = ReadProcessMemory(_process_handle, addr, &buffer, sizeof(uint16_t), &bytes_read);
    if (!success || bytes_read != sizeof(uint16_t))
        throw EmulatorException("Failed to read data from the emulator's memory");

    return buffer;
}

uint32_t RetroarchMemInterface::read_game_long(uint16_t address) const
{
    uint32_t msw = this->read_game_word(address);
    uint32_t lsw = this->read_game_word(address + 2);
    return (msw << 16) + lsw;
}

void RetroarchMemInterface::write_game_byte(uint16_t address, uint8_t value)
{
    uint16_t even_address = address - (address % 2);
    if(address == even_address)
        address += 1;
    else
        address -= 1;

    SIZE_T written_count;
    LPVOID addr = reinterpret_cast<LPVOID>(_game_ram_base_address + address);
    BOOL success = WriteProcessMemory(_process_handle, addr, &value, sizeof(value), &written_count);
    if (!success || written_count != sizeof(value))
        throw EmulatorException("Failed to write data into the emulator's memory");
}

void RetroarchMemInterface::write_game_word(uint16_t address, uint16_t value)
{
    SIZE_T written_count;
    LPVOID addr = reinterpret_cast<LPVOID>(_game_ram_base_address + address);
    BOOL success = WriteProcessMemory(_process_handle, addr, &value, sizeof(value), &written_count);
    if (!success || written_count != sizeof(value))
        throw EmulatorException("Failed to write data into the emulator's memory");
}

void RetroarchMemInterface::write_game_long(uint16_t address, uint32_t value)
{
    uint16_t msw = value >> 16;
    uint16_t lsw = static_cast<uint16_t>(value);
    this->write_game_word(address, msw);
    this->write_game_word(address + 2, lsw);
}

/*
bool RetroarchMemInterface::get_debug_privileges()
{
    HANDLE hToken = nullptr;
    if(!OpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES, &hToken))
        throw EmulatorException("Failed to enable debug privileges");

    if(!set_privilege(hToken, SE_DEBUG_NAME, TRUE))
        throw EmulatorException("Failed to enable debug privileges");

    return true;
}

bool RetroarchMemInterface::set_privilege(HANDLE hToken, LPCTSTR lpszPrivilege, BOOL bEnablePrivilege)
{
    LUID luid;
    if (!LookupPrivilegeValue(nullptr, lpszPrivilege, &luid))
        throw EmulatorException("Failed to enable debug privileges");

    TOKEN_PRIVILEGES tp;
    tp.PrivilegeCount = 1;
    tp.Privileges[0].Luid = luid;
    tp.Privileges[0].Attributes = (bEnablePrivilege) ? SE_PRIVILEGE_ENABLED : 0;

    if (!AdjustTokenPrivileges(hToken, FALSE, &tp, sizeof(TOKEN_PRIVILEGES), (PTOKEN_PRIVILEGES) nullptr, (PDWORD) nullptr))
        throw EmulatorException("Failed to enable debug privileges");

    if (GetLastError() == ERROR_NOT_ALL_ASSIGNED)
        throw EmulatorException("Failed to enable debug privileges");

    return true;
}
*/

DWORD RetroarchMemInterface::find_process_id(const std::string& process_name)
{
    PROCESSENTRY32 pe32;
    pe32.dwSize = sizeof(PROCESSENTRY32);

    HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);

    if(Process32First(hSnapshot, &pe32))
    {
        do {
            if(process_name == pe32.szExeFile)
                break;
        } while(Process32Next(hSnapshot, &pe32));
    }

    if(hSnapshot != INVALID_HANDLE_VALUE)
        CloseHandle(hSnapshot);

    DWORD err = GetLastError();
    if (err != 0)
        return -1;

    return pe32.th32ProcessID;
}

bool RetroarchMemInterface::read_module_information(HANDLE processHandle, const std::string& module_name)
{
    constexpr uint16_t MAX_MODULE_COUNT = 512;
    HMODULE* modules = new HMODULE[MAX_MODULE_COUNT];
    DWORD modules_size;
    bool found = false;

    if(EnumProcessModulesEx(processHandle, modules, MAX_MODULE_COUNT * sizeof(HMODULE), &modules_size, LIST_MODULES_ALL))
    {
        for(size_t i = 0; i < modules_size/sizeof(HMODULE); i++)
        {
            char buffer[64];
            if(GetModuleBaseName(processHandle, modules[i], buffer, sizeof(buffer)))
            {
                if(module_name == buffer)
                {
                    HMODULE module_base_address = modules[i];
                    MODULEINFO module_info;
                    GetModuleInformation(processHandle, modules[i], &module_info, sizeof(MODULEINFO));
                    _base_address = reinterpret_cast<uint64_t>(module_base_address);
                    _module_size = module_info.SizeOfImage;
                    found = true;
                    break;
                }
            }
        }
    }

    delete[] modules;
    return found;
}

uint32_t RetroarchMemInterface::read_uint32(uint64_t address)
{
    uint32_t buffer = 0;
    SIZE_T NumberOfBytesToRead = sizeof(buffer); //this is equal to 4
    SIZE_T NumberOfBytesActuallyRead;

    BOOL success = ReadProcessMemory(_process_handle, reinterpret_cast<LPCVOID>(address), &buffer, NumberOfBytesToRead, &NumberOfBytesActuallyRead);
    if (!success || NumberOfBytesActuallyRead != NumberOfBytesToRead)
        throw EmulatorException("Failed to read data from the emulator's memory");

    return buffer;
}

uint64_t RetroarchMemInterface::read_uint64(uint64_t address)
{
    uint64_t buffer = 0;
    SIZE_T NumberOfBytesToRead = sizeof(buffer); //this is equal to 8
    SIZE_T NumberOfBytesActuallyRead;

    BOOL success = ReadProcessMemory(_process_handle, reinterpret_cast<LPCVOID>(address), &buffer, NumberOfBytesToRead, &NumberOfBytesActuallyRead);
    if (!success || NumberOfBytesActuallyRead != NumberOfBytesToRead)
        throw EmulatorException("Failed to read data from the emulator's memory");

    return buffer;
}

void RetroarchMemInterface::write_byte(uint64_t address, char value)
{
    SIZE_T written_count;

    BOOL success = WriteProcessMemory(_process_handle, reinterpret_cast<LPVOID>(_base_address + address), &value, 1, &written_count);
    if (!success || written_count != 1)
        throw EmulatorException("Failed to write data into the emulator's memory");
}

uint64_t RetroarchMemInterface::find_signature(const std::vector<uint16_t>& signature)
{
    BYTE* data = new BYTE[_module_size];
    SIZE_T bytes_read;

    ReadProcessMemory(_process_handle, reinterpret_cast<LPVOID>(_base_address), data, _module_size, &bytes_read);

    for (DWORD i = 0; i < _module_size; i++)
    {
        // If we reached a point where the full signature cannot fit, we won't find a match so break out of this loop
        if(i+signature.size() >= _module_size)
            break;

        bool signature_match = true;
        for (DWORD j = 0; j < signature.size() ; ++j)
        {
            // 0xFFFF represents "any value", so don't bother comparing
            if(signature[j] == 0xFFFF)
                continue;

            // If any of the bytes don't match with the signature, signature was not recognized so we can just
            // stop trying
            if(data[i+j] != signature[j])
            {
                signature_match = false;
                break;
            }
        }

        if(signature_match)
        {
            delete[] data;
            return _base_address + i;
        }
    }

    delete[] data;
    return UINT64_MAX;
}

uint64_t RetroarchMemInterface::find_gpgx_ram_base_addr()
{
    constexpr uint16_t ANY = 0xFFFF;

    // Signature 1 (RA 1.9.0 - GPGX 1.7.4 [7fa34f2] - 64 bit)
    uint64_t addr = this->find_signature({
        0x85, 0xC9, 0x74, ANY, 0x83, 0xF9, 0x02, 0xB8, 0x00, 0x00, 0x00, 0x00, 0x48, 0x0F, 0x44, 0x05,
        ANY, ANY, ANY, ANY, 0xC3
    });
    if(addr != UINT64_MAX)
    {
        addr += 16;
        Logger::debug("Found GPGX signature 1");
        uint32_t offset = this->read_uint32(addr);
        return this->read_uint64(addr + 4 + offset);
    }

    // Signature 2 (RA 1.13.0 - GPGX 1.7.4 [7907766] - 64 bit)
    addr = this->find_signature({
        0x85, 0xC9, 0x74, ANY, 0x31, 0xC0, 0x83, 0xF9, 0x02, 0x48, 0x0F, 0x44, 0x05,
        ANY, ANY, ANY, ANY, 0xC3
    });
    if(addr != UINT64_MAX)
    {
        addr += 13;
        Logger::debug("Found GPGX signature 2");
        uint32_t offset = this->read_uint32(addr);
        return this->read_uint64(addr + 4 + offset);
    }

    // Signature 3 (RA 1.15.0 - GPGX 1.7.4 [9745432] - 32 bit)
    addr = this->find_signature({
        0x8B, 0x54, 0x24, 0x04, 0x85, 0xD2, 0x74, ANY, 0x83, 0xFA, 0x02, 0xB8, ANY, ANY, ANY, ANY, 0xBA,
        0x00, 0x00, 0x00, 0x00
    });
    if(addr != UINT64_MAX)
    {
        addr += 12;
        Logger::debug("Found GPGX signature 3");
        return this->read_uint32(addr);
    }

    // Signature 4 (???)
    addr = this->find_signature({
        0x8B, 0x44, 0x24, 0x04, 0x85, 0xC0, 0x74, 0x18, 0x83, 0xF8, 0x02, 0xBA, 0x00, 0x00, 0x00, 0x00, 0xB8,
        ANY, ANY, ANY, ANY, 0x0F, 0x45, 0xC2, 0xC3, 0x8D, 0xB4, 0x26, 0x00, 0x00, 0x00, 0x00
    });
    if(addr != UINT64_MAX)
    {
        addr += 17;
        Logger::debug("Found GPGX signature 4");
        uint32_t offset = this->read_uint32(addr);
        return this->read_uint32(addr + 4 + offset);
    }

    return UINT64_MAX;
}
