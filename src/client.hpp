#pragma once

#include "archipelago_interface.hpp"
#include "retroarch_interface.hpp"
#include "game_state.hpp"
#include "logger.hpp"

extern GameState game_state;
extern ArchipelagoInterface* archipelago;
extern RetroarchInterface* emulator;
extern Logger logger;

void connect_ap(std::string host, const std::string& slot_name, const std::string& password);
void disconnect_ap();
void connect_emu();
void disconnect_emu();
void process_console_input(const std::string& input);
