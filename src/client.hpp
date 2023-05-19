#pragma once

#include "multiworld_interfaces/archipelago_interface.hpp"
#include "emulator_interfaces/emulator_interface.hpp"
#include "game_state.hpp"
#include "logger.hpp"

extern GameState game_state;
extern MultiworldInterface* multiworld;
extern EmulatorInterface* emulator;
extern Logger logger;

void update_map_tracker_logic();
void initiate_solo_session();
void connect_ap(std::string host, const std::string& slot_name, const std::string& password);
void disconnect_ap();
void connect_emu();
void check_rom_existence();
std::string build_rom();
void process_console_input(const std::string& input);
