#include "bizhawk_mem_interface.hpp"

#include <TlHelp32.h>
#include <Psapi.h>
#include <utility>
#include <iostream>
#include <sstream>
#include "../logger.hpp"

BizhawkMemInterface::BizhawkMemInterface()
{
    _process_id = find_process_id("EmuHawk.exe");
    if(_process_id == -1)
        throw EmulatorException("Could not find a Bizhawk process currently running.");

    _process_handle = OpenProcess(PROCESS_VM_READ | PROCESS_VM_WRITE | PROCESS_QUERY_INFORMATION, false, _process_id);

    _game_ram_base_address = this->find_gpgx_ram_base_addr();
    if(_game_ram_base_address == UINT64_MAX)
    {
        CloseHandle(_process_handle);
        throw EmulatorException("Could not find any known signature on this core version.\n"
                                "Please notify the author with your version of Bizhawk.");
    }

    std::ostringstream oss2;
    oss2 << "Game RAM = 0x" << std::hex << _game_ram_base_address;
    Logger::debug(oss2.str());
}

BizhawkMemInterface::~BizhawkMemInterface()
{
    CloseHandle(_process_handle);
}

uint8_t BizhawkMemInterface::read_game_byte(uint16_t address) const
{
    uint16_t even_address = address - (address % 2);
    uint16_t word = this->read_game_word(even_address);
    if(even_address == address)
        return static_cast<uint8_t>(word >> 8);
    return static_cast<uint8_t>(word);
}

uint16_t BizhawkMemInterface::read_game_word(uint16_t address) const
{
    uint16_t buffer = 0;
    SIZE_T bytes_read;

    LPCVOID addr = reinterpret_cast<LPCVOID>(_game_ram_base_address + address);
    BOOL success = ReadProcessMemory(_process_handle, addr, &buffer, sizeof(uint16_t), &bytes_read);
    if (!success || bytes_read != sizeof(uint16_t))
        throw EmulatorException("Failed to read data from the emulator's memory");

    return buffer;
}

uint32_t BizhawkMemInterface::read_game_long(uint16_t address) const
{
    uint32_t msw = this->read_game_word(address);
    uint32_t lsw = this->read_game_word(address + 2);
    return (msw << 16) + lsw;
}

void BizhawkMemInterface::write_game_byte(uint16_t address, uint8_t value)
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

void BizhawkMemInterface::write_game_word(uint16_t address, uint16_t value)
{
    SIZE_T written_count;
    LPVOID addr = reinterpret_cast<LPVOID>(_game_ram_base_address + address);
    BOOL success = WriteProcessMemory(_process_handle, addr, &value, sizeof(value), &written_count);
    if (!success || written_count != sizeof(value))
        throw EmulatorException("Failed to write data into the emulator's memory");
}

void BizhawkMemInterface::write_game_long(uint16_t address, uint32_t value)
{
    uint16_t msw = value >> 16;
    uint16_t lsw = static_cast<uint16_t>(value);
    this->write_game_word(address, msw);
    this->write_game_word(address + 2, lsw);
}

DWORD BizhawkMemInterface::find_process_id(const std::string& process_name)
{
    PROCESSENTRY32 pe32;
    pe32.dwSize = sizeof(PROCESSENTRY32);

    HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);

    if(Process32First(hSnapshot, &pe32))
    {
        do
        {
            if(process_name == pe32.szExeFile)
            {
                CloseHandle(hSnapshot);
                return pe32.th32ProcessID;
            }
        }
        while(Process32Next(hSnapshot, &pe32));
    }

    if(hSnapshot != INVALID_HANDLE_VALUE)
        CloseHandle(hSnapshot);
    return -1;
}

bool BizhawkMemInterface::read_module_information(HANDLE processHandle, const std::string& module_name)
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

uint32_t BizhawkMemInterface::read_uint32(uint64_t address)
{
    uint32_t buffer = 0;
    SIZE_T NumberOfBytesToRead = sizeof(buffer); //this is equal to 4
    SIZE_T NumberOfBytesActuallyRead;

    BOOL success = ReadProcessMemory(_process_handle, reinterpret_cast<LPCVOID>(address), &buffer, NumberOfBytesToRead, &NumberOfBytesActuallyRead);
    if (!success || NumberOfBytesActuallyRead != NumberOfBytesToRead)
        throw EmulatorException("Failed to read data from the emulator's memory");

    return buffer;
}

uint64_t BizhawkMemInterface::read_uint64(uint64_t address)
{
    uint64_t buffer = 0;
    SIZE_T NumberOfBytesToRead = sizeof(buffer); //this is equal to 8
    SIZE_T NumberOfBytesActuallyRead;

    BOOL success = ReadProcessMemory(_process_handle, reinterpret_cast<LPCVOID>(address), &buffer, NumberOfBytesToRead, &NumberOfBytesActuallyRead);
    if (!success || NumberOfBytesActuallyRead != NumberOfBytesToRead)
        throw EmulatorException("Failed to read data from the emulator's memory");

    return buffer;
}

void BizhawkMemInterface::write_byte(uint64_t address, char value)
{
    SIZE_T written_count;

    BOOL success = WriteProcessMemory(_process_handle, reinterpret_cast<LPVOID>(_base_address + address), &value, 1, &written_count);
    if (!success || written_count != 1)
        throw EmulatorException("Failed to write data into the emulator's memory");
}

struct MemoryRegion
{
    uint64_t BaseAddress;
    int64_t Size;
    uint32_t Type;
};

uint64_t BizhawkMemInterface::find_gpgx_ram_base_addr()
{
    std::vector<MemoryRegion> regions;

    MEMORY_BASIC_INFORMATION info;
    uint64_t previous_region_addr = 0;

    while (VirtualQueryEx(_process_handle, (LPCVOID)previous_region_addr, &info, sizeof(MEMORY_BASIC_INFORMATION)))
    {
        bool validType = (info.Type == 0x40000); // MEM_MAPPED
        bool validSize = ((long)info.RegionSize == 0x2C000);

        if(validType && validSize)
        {
            regions.emplace_back(MemoryRegion {
                    .BaseAddress =  (uint64_t)info.BaseAddress,
                    .Size = (int64_t)info.RegionSize,
                    .Type = info.Type,
            });
        }

        previous_region_addr += info.RegionSize;
    }

    // If only one region matches in size, it *has* to be the right one
    if(regions.size() == 1)
        return regions[0].BaseAddress + 0x5D90;

    // If more are matching, try to find the one that really contains the RAM
    const uint32_t SIGNATURE = 0x6E264B61;
    for(MemoryRegion region : regions)
    {
        uint64_t ramBaseAddress = region.BaseAddress + 0x5D90;
        uint32_t readSignature = this->read_uint32(ramBaseAddress);
        if(readSignature == SIGNATURE)
        {
            std::cout << "Found! 0x" << std::hex << ramBaseAddress << std::endl;
            return ramBaseAddress;
        }
        else
        {
            std::cout << "Found block of good size, but has signature 0x" << std::hex << readSignature << std::endl;
        }
    }

    return UINT64_MAX;
}
