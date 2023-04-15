#include "retroarch_interface.hpp"

#include <iostream>

RetroarchInterface::RetroarchInterface()
{
    get_debug_privileges();
    _process_id = find_process_id("retroarch.exe");
    if(_process_id == -1)
        throw EmulatorException("Could not find a Retroarch process currently running.");

    _process_handle = OpenProcess(PROCESS_ALL_ACCESS, false, _process_id);
    if(!read_module_information(_process_handle, "genesis_plus_gx_libretro.dll"))
    {
        CloseHandle(_process_handle);
        throw EmulatorException("Could not find Genesis Plus GX core running.");
    }

    std::cout << "Hooked on Genesis Plus GX core." << std::endl;
    std::cout << "Addr = 0x" << std::hex << _base_address << std::dec << std::endl;
    std::cout << "Size = 0x" << std::hex << _module_size << std::dec << std::endl;

    _game_ram_base_address = this->find_gpgx_ram_base_addr();
    std::cout << "Game RAM = 0x" << std::hex << _game_ram_base_address << std::dec << std::endl;
}

RetroarchInterface::~RetroarchInterface()
{
    CloseHandle(_process_handle);
}

uint8_t RetroarchInterface::read_game_byte(uint16_t address) const
{
    uint16_t even_address = address - (address % 2);
    uint16_t word = this->read_game_word(even_address);
    if(even_address == address)
        return static_cast<uint8_t>(word >> 8);
    return static_cast<uint8_t>(word);
}

uint16_t RetroarchInterface::read_game_word(uint16_t address) const
{
    uint16_t buffer = 0;
    SIZE_T bytes_read;

    LPCVOID addr = reinterpret_cast<LPCVOID>(_game_ram_base_address + address);
    BOOL success = ReadProcessMemory(_process_handle, addr, &buffer, sizeof(uint16_t), &bytes_read);
    if (!success || bytes_read != sizeof(uint16_t))
        throw EmulatorException("read_game_word failed");

    return buffer;
}

uint32_t RetroarchInterface::read_game_long(uint16_t address) const
{
    uint32_t msw = this->read_game_word(address);
    uint32_t lsw = this->read_game_word(address + 2);
    return (msw << 16) + lsw;
}

void RetroarchInterface::write_game_byte(uint16_t address, uint8_t value)
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
        throw EmulatorException("write_game_byte failed");
}

void RetroarchInterface::write_game_word(uint16_t address, uint16_t value)
{
    SIZE_T written_count;
    LPVOID addr = reinterpret_cast<LPVOID>(_game_ram_base_address + address);
    BOOL success = WriteProcessMemory(_process_handle, addr, &value, sizeof(value), &written_count);
    if (!success || written_count != sizeof(value))
        throw EmulatorException("write_game_word failed");
}

void RetroarchInterface::write_game_long(uint16_t address, uint32_t value)
{
    uint16_t msw = value >> 16;
    uint16_t lsw = static_cast<uint16_t>(value);
    this->write_game_word(address, msw);
    this->write_game_word(address + 2, lsw);
}

bool RetroarchInterface::get_debug_privileges()
{
    HANDLE hToken = nullptr;
    if(!OpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES, &hToken))
        throw EmulatorException("OpenProcessToken failed");

    if(!set_privilege(hToken, SE_DEBUG_NAME, TRUE))
        throw EmulatorException("Failed to enable privilege");

    return true;
}

bool RetroarchInterface::set_privilege(HANDLE hToken, LPCTSTR lpszPrivilege, BOOL bEnablePrivilege)
{
    LUID luid;
    if (!LookupPrivilegeValue(nullptr, lpszPrivilege, &luid))
        throw EmulatorException("LookupPrivilegeValue error");

    TOKEN_PRIVILEGES tp;
    tp.PrivilegeCount = 1;
    tp.Privileges[0].Luid = luid;
    tp.Privileges[0].Attributes = (bEnablePrivilege) ? SE_PRIVILEGE_ENABLED : 0;

    if (!AdjustTokenPrivileges(hToken, FALSE, &tp, sizeof(TOKEN_PRIVILEGES), (PTOKEN_PRIVILEGES) nullptr, (PDWORD) nullptr))
        throw EmulatorException("AdjustTokenPrivileges error");

    if (GetLastError() == ERROR_NOT_ALL_ASSIGNED)
        throw EmulatorException("The token does not have the specified privilege.");

    return true;
}

DWORD RetroarchInterface::find_process_id(const std::string& process_name)
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

bool RetroarchInterface::read_module_information(HANDLE processHandle, const std::string& module_name)
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

uint32_t RetroarchInterface::read_int(uint64_t address)
{
    uint32_t buffer = 0;
    SIZE_T NumberOfBytesToRead = sizeof(buffer); //this is equal to 4
    SIZE_T NumberOfBytesActuallyRead;

    BOOL success = ReadProcessMemory(_process_handle, reinterpret_cast<LPCVOID>(address), &buffer, NumberOfBytesToRead, &NumberOfBytesActuallyRead);
    if (!success || NumberOfBytesActuallyRead != NumberOfBytesToRead)
        throw EmulatorException("read_int failed");

    return buffer;
}

void RetroarchInterface::write_byte(uint64_t address, char value)
{
    SIZE_T written_count;

    BOOL success = WriteProcessMemory(_process_handle, reinterpret_cast<LPVOID>(_base_address + address), &value, 1, &written_count);
    if (!success || written_count != 1)
        throw EmulatorException("write_byte failed");
}

uint64_t RetroarchInterface::find_signature(const std::vector<uint16_t>& signature)
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

uint64_t RetroarchInterface::find_gpgx_ram_base_addr()
{
    constexpr uint16_t ANY = 0xFFFF;

    // Signature 1
    uint64_t addr = this->find_signature({
        0x85, 0xC9, 0x74, ANY, 0x83, 0xF9, 0x02, 0xB8, 0x00, 0x00, 0x00, 0x00, 0x48, 0x0F, 0x44, 0x05,
        ANY, ANY, ANY, ANY, 0xC3
    });
    if(addr != UINT64_MAX)
    {
        addr += 16;
        std::cout << "Found signature 1 -> 0x" << std::hex << addr << std::dec << std::endl;
        uint32_t offset = this->read_int(addr);
        return this->read_int(addr + 4 + offset);
    }

    // Signature 2
    addr = this->find_signature({
        0x85, 0xC9, 0x74, ANY, 0x31, 0xC0, 0x83, 0xF9, 0x02, 0x48, 0x0F, 0x44, 0x05,
        ANY, ANY, ANY, ANY, 0xC3
    });
    if(addr != UINT64_MAX)
    {
        addr += 13;
        std::cout << "Found signature 2 -> 0x" << std::hex << addr << std::dec << std::endl;
        uint32_t offset = this->read_int(addr);
        return this->read_int(addr + 4 + offset);
    }

    // Signature 3
    addr = this->find_signature({
        0x8B, 0x44, 0x24, 0x04, 0x85, 0xC0, 0x74, 0x18, 0x83, 0xF8, 0x02, 0xBA, 0x00, 0x00, 0x00, 0x00, 0xB8,
        ANY, ANY, ANY, ANY, 0x0F, 0x45, 0xC2, 0xC3, 0x8D, 0xB4, 0x26, 0x00, 0x00, 0x00, 0x00
    });
    if(addr != UINT64_MAX)
    {
        addr += 17;
        std::cout << "Found signature 3 -> 0x" << std::hex << addr << std::dec << std::endl;
        uint32_t offset = this->read_int(addr);
        return this->read_int(addr + 4 + offset);
    }

    return UINT64_MAX;
}
