#pragma once

#include <vector>
#include <string>
#include <Windows.h>
#include "emulator_interface.hpp"

class RetroarchMemInterface : public EmulatorInterface
{
private:
    DWORD _process_id;
    HANDLE _process_handle = nullptr;
    uint64_t _base_address = UINT64_MAX;
    uint64_t _module_size = UINT64_MAX;
    uint64_t _game_ram_base_address = UINT64_MAX;

public:
    RetroarchMemInterface();
    ~RetroarchMemInterface() override;

    [[nodiscard]] uint8_t read_game_byte(uint16_t address) const override;
    [[nodiscard]] uint16_t read_game_word(uint16_t address) const override;
    [[nodiscard]] uint32_t read_game_long(uint16_t address) const override;

    void write_game_byte(uint16_t address, uint8_t value) override;
    void write_game_word(uint16_t address, uint16_t value) override;
    void write_game_long(uint16_t address, uint32_t value) override;

private:
    uint32_t read_uint32(uint64_t address);
    uint64_t read_uint64(uint64_t address);
    void write_byte(uint64_t address, char value);
    uint64_t find_signature(const std::vector<uint16_t>& signature);
    bool read_module_information(HANDLE processHandle, const std::string& module_name);
    uint64_t find_gpgx_ram_base_addr();

    static DWORD find_process_id(const std::string& process_name);
};