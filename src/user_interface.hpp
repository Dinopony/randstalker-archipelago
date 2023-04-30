#pragma once

#include <vector>
#include <string>
#include "logger.hpp"

class UserInterface {
private:
    char _host[512] = "archipelago.gg:12345";
    char _slot_name[256] = "Player";
    char _password[256] = "";
    char _console_input[512] = "";
    char _input_rom_path[1024] = "./Landstalker (USA).md";
    char _output_rom_path[1024] = "./seeds/";
    float _hud_color[3] = { 0.f, 0.f, 0.f };
    float _nigel_color_light[3] = { 0.f, 0.f, 0.f };
    float _nigel_color_dark[3] = { 0.f, 0.f, 0.f };

    uint32_t _window_width = 1000;
    uint32_t _window_height = 600;
    uint32_t _last_message_count = 0;

public:
    void open();
    void load_client_settings();
    void save_client_settings();
    void load_personal_settings();
    void save_personal_settings();

    [[nodiscard]] const char* input_rom_path() const { return _input_rom_path; }
    [[nodiscard]] const char* output_rom_path() const { return _output_rom_path; }

private:
    void draw_archipelago_connection_window();
    void draw_rom_generation_window();
    void draw_emulator_connection_window();
    void draw_hint_window();
    void draw_console_window();
    void draw_console_input();
};