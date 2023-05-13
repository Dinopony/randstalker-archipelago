#pragma once

#include <SFML/Graphics.hpp>
#include <vector>
#include <string>
#include "logger.hpp"
#include "trackable_item.hpp"

class UserInterface {
private:
    char _host[512] = "archipelago.gg:12345";
    char _slot_name[256] = "Player";
    char _password[256] = "";
    char _console_input[512] = "";
    char _input_rom_path[1024] = "./LandStalker_USA.SGD";
    char _output_rom_path[1024] = "./seeds/";
    float _hud_color[3] = { 0.f, 0.f, 0.f };
    float _nigel_color_light[3] = { 0.f, 0.f, 0.f };
    float _nigel_color_dark[3] = { 0.f, 0.f, 0.f };

    uint32_t _window_width = 1000;
    uint32_t _window_height = 600;
    uint32_t _last_message_count = 0;

    std::vector<TrackableItem*> _trackable_items;

public:
    ~UserInterface();

    void open();
    void load_client_settings();
    void save_client_settings();
    void load_personal_settings();
    void save_personal_settings();
    void init_tracker();

    [[nodiscard]] const char* input_rom_path() const { return _input_rom_path; }
    [[nodiscard]] const char* output_rom_path() const { return _output_rom_path; }

private:
    float draw_archipelago_connection_window(float y);
    float draw_rom_generation_window(float y);
    float draw_emulator_connection_window(float y);
    float draw_hint_window(float y) const;
    float draw_tracker_window(float y) const;
    void draw_status_window();

    void draw_console_window();
    void draw_console_input();
};
