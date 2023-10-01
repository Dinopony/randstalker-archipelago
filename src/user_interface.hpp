#pragma once

#include <SFML/Graphics.hpp>
#include <vector>
#include <string>
#include "logger.hpp"
#include "trackable_item.hpp"
#include "trackable_region.hpp"
#include "tracker_config.hpp"

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
    bool _remove_music = false;
    bool _swap_overworld_music = false;

    int _offline_generation_mode = 0; ///< 0 = preset, 1 = permalink
    int _selected_preset = 0;
    char _permalink[1024] = "";
    std::vector<std::string> _presets;

    int _window_x = -1;
    int _window_y = -1;
    uint32_t _window_width = 1000;
    uint32_t _window_height = 800;
    uint32_t _last_message_count = 0;

    std::vector<TrackableItem*> _trackable_items;
    std::vector<TrackableRegion*> _trackable_regions;
    TrackableRegion* _selected_region = nullptr;
    TrackerConfig _tracker_config;
    bool _map_tracker_open = false;

    sf::Texture* _tex_location = nullptr;
    sf::Texture* _tex_location_checked = nullptr;
    sf::Texture* _tex_tree = nullptr;
    uint64_t _tex_lantern_id = -1;
    sf::Texture* _tex_spell_book = nullptr;

public:
    void open();
    void load_client_settings();
    void save_client_settings();
    void load_personal_settings();
    void save_personal_settings();

    [[nodiscard]] const char* input_rom_path() const { return _input_rom_path; }
    [[nodiscard]] const char* output_rom_path() const { return _output_rom_path; }

    [[nodiscard]] const std::vector<TrackableItem*>& trackable_items() const { return _trackable_items; }
    [[nodiscard]] const std::vector<TrackableRegion*>& trackable_regions() const { return _trackable_regions; }
    [[nodiscard]] bool map_tracker_open() const { return _map_tracker_open; }

    [[nodiscard]] int offline_generation_mode() const { return _offline_generation_mode; }
    [[nodiscard]] const std::string& selected_preset() const { return _presets.at(_selected_preset); }
    [[nodiscard]] std::string permalink() const { return _permalink; }

    [[nodiscard]] TrackerConfig& tracker_config() { return _tracker_config; }

private:
    void init_item_tracker();
    void init_map_tracker();
    void init_presets_list();

    void loop(sf::RenderWindow& window);

    void draw_archipelago_connection_window();
    void draw_rom_generation_window();
    void draw_emulator_connection_window();
    float draw_item_tracker_window() const;
    float draw_tracker_config_window(float y);
    void draw_map_tracker_details_window(float y);
    void draw_status_window() const;

    float draw_map_tracker_window(float x, float y, float width, float height);
    void draw_console_window(float x, float y);
};
