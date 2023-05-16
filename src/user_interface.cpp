#include <Windows.h>

#include <SFML/Graphics.hpp>
#include <imgui-SFML.h>
#include <fstream>
#include <filesystem>
#include <landstalker_lib/constants/item_codes.hpp>
#include <Shlobj_core.h>

#include "user_interface.hpp"
#include "client.hpp"
#include "randstalker_invoker.hpp"
#include "data/trackable_items.json.hxx"
#include "data/trackable_regions.json.hxx"

// ===== WINDOWS SPECIFIC TOOL FUNCTIONS ======================================================================

#include <commdlg.h>

/**
 * Opens a "Select a file" dialog and returns the path chosen by the user
 */
static std::string ask_for_file()
{
    char filename[256];

    OPENFILENAME ofn;
    ZeroMemory(&ofn, sizeof(ofn));
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = nullptr;
    ofn.lpstrFile = filename;
    ofn.lpstrFile[0] = '\0';
    ofn.nMaxFile = sizeof(filename);
    ofn.lpstrFilter = "All\0*.*\0Text\0*.TXT\0";
    ofn.nFilterIndex = 1;
    ofn.lpstrFileTitle = nullptr;
    ofn.nMaxFileTitle = 0 ;
    ofn.lpstrInitialDir = nullptr;
    ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST;

    GetOpenFileName(&ofn);

    return ofn.lpstrFile;
}

/**
 * Opens an explorer process if needed, and select the file pointed by the given path
 */
void show_file_in_explorer(const std::string& filename)
{
    std::string abs_path = std::filesystem::absolute(std::filesystem::path(filename)).string();
    PIDLIST_ABSOLUTE pidl = ILCreateFromPath(abs_path.c_str());
    if(pidl)
    {
        SHOpenFolderAndSelectItems(pidl, 0, nullptr, 0);
        ILFree(pidl);
    }
}

// ============================================================================================================

constexpr uint32_t FRAMERATE_LIMIT_FOCUS = 60;
constexpr uint32_t FRAMERATE_LIMIT_NO_FOCUS = 10;

constexpr uint32_t MIN_WINDOW_WIDTH = 800;
constexpr uint32_t MIN_WINDOW_HEIGHT = 800;

/// When the width of the client window becomes bigger than this value, the UI swap into a 3-columns mode where the
/// map tracker gets placed next to the console log, instead of being one on top of the other.
constexpr uint32_t THREE_COLUMNS_MODE_THRESHOLD = 1200;
constexpr uint32_t MAP_TRACKER_WINDOW_MIN_W = 410;

constexpr uint32_t MARGIN = 8;
constexpr uint32_t LEFT_PANEL_WIDTH = 340;
constexpr uint32_t STATUS_WINDOW_H = 104;

constexpr uint32_t CONSOLE_INPUT_HEIGHT = 35;

const auto WINDOW_FLAGS = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize
                        | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings;


void UserInterface::draw_archipelago_connection_window()
{
    ImGui::SetNextWindowPos(ImVec2(MARGIN, MARGIN));
    ImGui::SetNextWindowSize(ImVec2(LEFT_PANEL_WIDTH, 0.f));

    ImGui::Begin("AP Connection", nullptr, WINDOW_FLAGS);
    {
        ImGui::PushItemWidth(-1);
        ImGui::Text("Host");
        ImGui::InputText("##Host", _host, IM_ARRAYSIZE(_host));
        ImGui::Text("Slot name");
        ImGui::InputText("##Slot name", _slot_name, IM_ARRAYSIZE(_slot_name));
        ImGui::Text("Password");
        ImGui::InputText("##Password", _password, IM_ARRAYSIZE(_password));
        ImGui::PopItemWidth();

        ImGui::Dummy(ImVec2(0.f, 2.f));
        ImGui::Separator(); // --------------------------------------------------------
        ImGui::Dummy(ImVec2(0.f, 1.f));

        if(!archipelago)
        {
            if(ImGui::Button("Connect to Archipelago"))
                connect_ap(_host, _slot_name, _password);
        }
        else
        {
            if(ImGui::Button("Stop"))
                disconnect_ap();
        }
    }
    ImGui::End();
}

void UserInterface::draw_rom_generation_window()
{
    ImGui::SetNextWindowPos(ImVec2(MARGIN, MARGIN));
    ImGui::SetNextWindowSize(ImVec2(LEFT_PANEL_WIDTH, 0.f));

    ImGui::Begin("ROM Generation", nullptr, WINDOW_FLAGS);
    {
        ImGui::Text("Input ROM file");
        ImGui::PushItemWidth(-35);
        ImGui::InputText("##InputPath", _input_rom_path, IM_ARRAYSIZE(_input_rom_path));
        ImGui::PopItemWidth();
        ImGui::SameLine();
        if(ImGui::Button("... "))
        {
            std::string picked_path = ask_for_file();
            if(!picked_path.empty())
                sprintf(_input_rom_path, "%s", picked_path.c_str());
        }

        ImGui::Text("Output ROM directory");
        ImGui::PushItemWidth(-1);
        ImGui::InputText("##OutputPath", _output_rom_path, IM_ARRAYSIZE(_output_rom_path));
        ImGui::PopItemWidth();

        ImGui::Dummy(ImVec2(0.f, 2.f));
        ImGui::Separator(); // --------------------------------------------------------
        ImGui::Dummy(ImVec2(0.f, 1.f));

        ImGui::Text("HUD color");
        if(ImGui::ColorEdit3("##HUD Color", _hud_color, ImGuiColorEditFlags_InputRGB))
        {
            for(float& component_float : _hud_color)
            {
                uint8_t component = (uint8_t)(255.f * component_float);
                component -= component % 0x20;
                component_float = (float)component / 255.f;
            }
        }

        ImGui::Dummy(ImVec2(0.f, 2.f));

        ImGui::Text("Nigel colors");
        if(ImGui::ColorEdit3("##Nigel Color 1", _nigel_color_light, ImGuiColorEditFlags_InputRGB))
        {
            for(float& component_float : _nigel_color_light)
            {
                uint8_t component = (uint8_t)(255.f * component_float);
                component -= component % 0x20;
                component_float = (float)component / 255.f;
            }
        }

        if(ImGui::ColorEdit3("##Nigel Color 2", _nigel_color_dark, ImGuiColorEditFlags_InputRGB))
        {
            for(float& component_float : _nigel_color_dark)
            {
                uint8_t component = (uint8_t)(255.f * component_float);
                component -= component % 0x20;
                component_float = (float)component / 255.f;
            }
        }

        ImGui::Dummy(ImVec2(0.f, 2.f));
        ImGui::Separator(); // --------------------------------------------------------
        ImGui::Dummy(ImVec2(0.f, 1.f));

        ImGui::Checkbox("Remove music", &_remove_music);
        if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
            ImGui::SetTooltip("Removes in-game music, but keeps sound effects untouched");

        ImGui::Checkbox("Swap overworld music", &_swap_overworld_music);
        if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
            ImGui::SetTooltip("Swap the music before and after taking boat to Verla");

        ImGui::Dummy(ImVec2(0.f, 2.f));
        ImGui::Separator(); // --------------------------------------------------------
        ImGui::Dummy(ImVec2(0.f, 1.f));

        if(ImGui::Button("Build ROM"))
        {
            std::string rom_path = build_rom();
            game_state.built_rom_path(rom_path);
        }
    }
    ImGui::End();
}

void UserInterface::draw_emulator_connection_window()
{
    ImGui::SetNextWindowPos(ImVec2(MARGIN, MARGIN));
    ImGui::SetNextWindowSize(ImVec2(LEFT_PANEL_WIDTH, 0.f));

    ImGui::Begin("Emulator Connection", nullptr, WINDOW_FLAGS);
    {
        if(ImGui::Button("Show ROM file in explorer"))
            show_file_in_explorer(game_state.built_rom_path());

        if(ImGui::Button("Rebuild ROM with other settings"))
            game_state.built_rom_path("");

        ImGui::Dummy(ImVec2(0.f, 2.f));
        ImGui::Separator();
        ImGui::Dummy(ImVec2(0.f, 1.f));

        ImGui::TextWrapped("Open the built ROM with Retroarch using Genesis Plus GX core, then click on the \"Connect to emulator\" button below.");

        ImGui::Dummy(ImVec2(0.f, 2.f));
        ImGui::Separator();
        ImGui::Dummy(ImVec2(0.f, 1.f));

        if(ImGui::Button("Connect to emulator"))
            connect_emu();
    }
    ImGui::End();
}

void UserInterface::draw_map_tracker_details_window(float y) const
{
    ImGui::SetNextWindowPos(ImVec2(MARGIN, y));
    ImGui::SetNextWindowSize(ImVec2(LEFT_PANEL_WIDTH, (float)_window_height - y - STATUS_WINDOW_H - 2*MARGIN));
    ImGui::Begin("Locations details", nullptr, WINDOW_FLAGS);
    {
        // Display unchecked locations
        for(const Location* loc : _selected_region->locations())
        {
            if(loc->was_checked())
                continue;

            float initial_cursor_y = ImGui::GetCursorPosY();
            ImGui::Image((ImTextureID)(uintptr_t)_tex_location->getNativeHandle(), ImVec2(39.f, 39.f));
            float cursor_x_after_image = ImGui::GetCursorStartPos().x + 39.f + 6.f;
            float cursor_y_after_image = ImGui::GetCursorPos().y;

            ImGui::SetCursorPos(ImVec2(cursor_x_after_image, initial_cursor_y));
            ImGui::TextWrapped("%s", loc->name().c_str());

            ImGui::SetCursorPosX(cursor_x_after_image);
            if(loc->reachable())
            {
                ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(100, 255, 100, 255));
                ImGui::TextWrapped("Can be checked");
            }
            else
            {
                ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(255, 100, 100, 255));
                ImGui::TextWrapped("Cannot be checked");
            }
            ImGui::PopStyleColor();

            ImGui::SetCursorPosX(cursor_x_after_image);

            std::string hint_btn_id = "Hint contents##" + loc->name();
            if(ImGui::Button(hint_btn_id.c_str()))
                process_console_input("!hint_location " + loc->name());

            ImGui::SameLine();
            std::string where_is_it_btn_id = "Where is it?##" + loc->name();
            if(ImGui::Button(where_is_it_btn_id.c_str()))
            {
                std::string url = "https://imgur.com/a/FnN7Akx";
                ShellExecuteA(nullptr, "open", url.c_str(), nullptr, nullptr, SW_SHOWDEFAULT);
            }

            if(ImGui::GetCursorPosY() < cursor_y_after_image)
                ImGui::SetCursorPosY(cursor_y_after_image);
            ImGui::Separator();
        }

        // Display already checked locations
        for(const Location* loc : _selected_region->locations())
        {
            if(!loc->was_checked())
                continue;

            float initial_cursor_y = ImGui::GetCursorPosY();
            ImGui::Image((ImTextureID)(uintptr_t)_tex_location_checked->getNativeHandle(), ImVec2(39.f, 39.f));
            float cursor_x_after_image = ImGui::GetCursorStartPos().x + 39.f + 6.f;
            float cursor_y_after_image = ImGui::GetCursorPos().y;

            ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(255,255,255,128));
            ImGui::SetCursorPos(ImVec2(cursor_x_after_image, initial_cursor_y));
            ImGui::TextWrapped("%s", loc->name().c_str());
            ImGui::PopStyleColor();

            ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(100,255,100,128));
            ImGui::SetCursorPosX(cursor_x_after_image);
            ImGui::TextWrapped("Already checked");
            ImGui::PopStyleColor();

            if(ImGui::GetCursorPosY() < cursor_y_after_image)
                ImGui::SetCursorPosY(cursor_y_after_image);
            ImGui::Separator();
        }
    }
    ImGui::End();
}

float UserInterface::draw_item_tracker_window() const
{
    ImGui::SetNextWindowPos(ImVec2(MARGIN, MARGIN));
    ImGui::SetNextWindowSize(ImVec2(LEFT_PANEL_WIDTH, 0.f));

    ImGui::Begin("Tracker", nullptr, WINDOW_FLAGS);
    {
        ImVec2 wsize(46.f, 46.f);
        for(TrackableItem* item : _trackable_items)
        {
            if(!game_state.item_exists_in_game(item->item_id()))
                continue;

            bool item_owned = (game_state.owned_item_quantity(item->item_id()) > 0);
            ImVec4 color_multipler(1, 1, 1, 1);
            if(!item_owned)
                color_multipler = ImVec4(0.4, 0.4, 0.4, 0.6);

            ImGui::SetCursorPos(ImVec2(MARGIN + item->x(), MARGIN + item->y()));
            ImGui::Image((ImTextureID) item->get_texture_id(), wsize, ImVec2(0,0), ImVec2(1,1), color_multipler);
            if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
            {
                std::string suffix = (!item_owned) ? "\n\n(Right-click to get a hint for this item)" : "";
                if(item_owned)
                    ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(100,255,100,255));
                else
                    ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(255,100,100,255));

                ImGui::SetTooltip("%s%s", item->name().c_str(), suffix.c_str());
                ImGui::PopStyleColor();

                if(!item_owned && ImGui::IsMouseReleased(1))
                    process_console_input("!hint " + item->name());
            }
        }
    }
    float window_height = ImGui::GetWindowHeight();
    ImGui::End();

    return window_height + MARGIN*2;
}

void UserInterface::draw_status_window() const
{
    ImGui::SetNextWindowPos(ImVec2(MARGIN, (float)_window_height - (MARGIN + STATUS_WINDOW_H)));
    ImGui::SetNextWindowSize(ImVec2(LEFT_PANEL_WIDTH, STATUS_WINDOW_H));
    ImGui::Begin("Status", nullptr, WINDOW_FLAGS);
    {
        ImGui::Text("Archipelago Server:");
        ImGui::SameLine();
        if(archipelago && archipelago->is_connected())
        {
            ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(0, 255, 0, 255));
            ImGui::Text("Connected");
            ImGui::PopStyleColor();
        }
        else if(archipelago)
        {
            ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(255, 200, 0, 255));
            ImGui::Text("Connecting...");
            ImGui::PopStyleColor();
        }
        else
        {
            ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(255, 0, 0, 255));
            ImGui::Text("Not connected");
            ImGui::PopStyleColor();
        }

        ImGui::Separator(); // --------------------------------------------

        ImGui::Text("ROM build:");
        ImGui::SameLine();
        if(game_state.has_built_rom())
        {
            ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(0, 255, 0, 255));
            ImGui::Text("Built");
            ImGui::PopStyleColor();
        }
        else
        {
            ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(255, 0, 0, 255));
            ImGui::Text("Not built");
            ImGui::PopStyleColor();
        }

        ImGui::Separator(); // --------------------------------------------

        ImGui::Text("Emulator:");
        ImGui::SameLine();
        if(emulator)
        {
            ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(0, 255, 0, 255));
            ImGui::Text("Connected");
            ImGui::PopStyleColor();
        }
        else
        {
            ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(255, 0, 0, 255));
            ImGui::Text("Not connected");
            ImGui::PopStyleColor();
        }

        ImGui::Separator(); // --------------------------------------------
        ImGui::Dummy(ImVec2(0.f, 1.f));

        if(archipelago && archipelago->is_connected())
        {
            if(ImGui::Button("Disconnect from server"))
                disconnect_ap();
        }
        else
        {
            ImGui::BeginDisabled();
            ImGui::Button("Disconnect from server");
            ImGui::EndDisabled();
        }
    }
    ImGui::End();
}

float UserInterface::draw_map_tracker_window(float x, float y, float width, float height)
{
    ImGui::SetNextWindowPos(ImVec2(x, y));
    ImGui::SetNextWindowSize(ImVec2(width, height));

    constexpr float SIZE_UNIT = 16.f;
    float map_width = SIZE_UNIT * 23;
    float map_height = SIZE_UNIT * 38;
    float map_origin_x = std::round((width - map_width) / 2.f);
    float map_origin_y = 8;
    if(height != 0.f)
        map_origin_y = std::round((height - map_height) / 2.f);

    ImGui::Begin("Map Tracker", nullptr, WINDOW_FLAGS);
    {
        ImGui::Text("Goal: %s", game_state.goal_string().c_str());

        for(TrackableRegion* region : _trackable_regions)
        {
            if(region->is_hidden_for_goal(game_state.goal_id()))
                continue;

            ImGui::SetCursorPos(ImVec2(map_origin_x, map_origin_y));

            sf::FloatRect rectangle((float)region->x() * SIZE_UNIT, (float)region->y() * SIZE_UNIT,
                                    (float)region->width() * SIZE_UNIT, (float)region->height() * SIZE_UNIT);

            uint32_t locations_count = region->locations().size();
            uint32_t checked_locations_count = region->checked_locations_count();

            bool draw_border = (locations_count > 0);

            bool contains_at_least_one_reachable = false;
            bool contains_at_least_one_unreachable = false;
            for(const Location* loc : region->locations())
            {
                if(loc->was_checked())
                    continue;

                if(loc->reachable())
                    contains_at_least_one_reachable = true;
                else
                    contains_at_least_one_unreachable = true;
            }

            sf::Color color(128,128,128);
            if(locations_count > 0)
            {
                if(locations_count == checked_locations_count)
                    color = sf::Color(20, 80, 20);
                else if(contains_at_least_one_unreachable)
                {
                    if(contains_at_least_one_reachable)
                        color = sf::Color(255, 160, 60); // Mixed reachable / unreachable => orange
                    else
                        color = sf::Color(200, 60, 60); // Fully unreachable => red
                }
                else if(contains_at_least_one_reachable)
                {
                    color = sf::Color(60, 200, 60); // Fully reachable => green
                }
            }

            ImGui::DrawRectFilled(rectangle, color, 0.f, 0);
            if(draw_border)
                ImGui::DrawRect(rectangle, sf::Color::Black);

            ImGui::SetCursorPos(ImVec2(map_origin_x + rectangle.left, map_origin_y + rectangle.top));
            ImGui::Dummy(ImVec2(rectangle.width, rectangle.height));
            if (!region->name().empty() && ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
            {
                ImGui::SetTooltip("%s\n%u/%u locations checked",
                                  region->name().c_str(),
                                  checked_locations_count,
                                  locations_count);

                if(locations_count > 0 && ImGui::IsMouseReleased(0))
                {
                    if(_selected_region != region)
                        _selected_region = region;
                    else
                        _selected_region = nullptr;
                }
            }
        }

        // Draw a yellow outline around the selected region if there is one
        if(_selected_region)
        {
            ImGui::SetCursorPos(ImVec2(map_origin_x, map_origin_y));
            sf::Color border_color =  sf::Color(255, 220, 0);
            sf::FloatRect rectangle((float)_selected_region->x() * SIZE_UNIT,
                                    (float)_selected_region->y() * SIZE_UNIT,
                                    (float)_selected_region->width() * SIZE_UNIT,
                                    (float)_selected_region->height() * SIZE_UNIT);
            sf::FloatRect bigger_rect(rectangle.left-1, rectangle.top-1,
                                      rectangle.width+2, rectangle.height+2);
            ImGui::DrawRect(rectangle, border_color);
            ImGui::DrawRect(bigger_rect, border_color);
        }
    }
    float window_height = ImGui::GetWindowHeight();
    ImGui::End();

    return y + window_height + MARGIN;
}

void UserInterface::draw_console_window(float x, float y)
{
    float width = (float)_window_width - x - MARGIN;
    float height = (float)_window_height - y - CONSOLE_INPUT_HEIGHT - MARGIN;

    ImGui::SetNextWindowPos(ImVec2(x, y));
    ImGui::SetNextWindowSize(ImVec2(width, height));

    ImGui::Begin("Console", nullptr, WINDOW_FLAGS | ImGuiWindowFlags_AlwaysVerticalScrollbar);
    {
        ImGui::PushStyleColor(ImGuiCol_Separator, IM_COL32(128,128,128,40));
        auto messages = Logger::get().messages();
        for(const Logger::Message& msg : messages)
        {
            std::string prefix;
            switch(msg.level) {
                case Logger::LOG_DEBUG:
                    ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(120,120,120,255));
                    prefix = "[DEBUG] ";
                    break;
                case Logger::LOG_MESSAGE:
                    ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(230,230,255,255));
                    break;
                case Logger::LOG_INFO:
                    ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(100,100,255,255));
                    prefix = "[INFO] ";
                    break;
                case Logger::LOG_HINT:
                    ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(100,255,100,(msg.text.ends_with("(found)")) ? 100 : 255));
                    break;
                case Logger::LOG_WARNING:
                    ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(255,180,0,255));
                    prefix = "[WARNING] ";
                    break;
                case Logger::LOG_ERROR:
                    ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(255,60,60,255));
                    prefix = "[ERROR] ";
                    break;
            }

            ImGui::TextWrapped("%s%s", prefix.c_str(), msg.text.c_str());
            ImGui::PopStyleColor();

            ImGui::Separator();
        }
        ImGui::PopStyleColor();

        if (messages.size() > _last_message_count)
        {
            _last_message_count = messages.size();
            ImGui::SetScrollHereY(1.0f);
        }
    }
    ImGui::End();

    ImGui::SetNextWindowPos(ImVec2(x, y + height - 1));
    ImGui::SetNextWindowSize(ImVec2(width, (float)CONSOLE_INPUT_HEIGHT));

    ImGui::Begin("ConsoleInputWindow", nullptr, WINDOW_FLAGS);
    {
        ImGui::PushItemWidth(-45);
        if(ImGui::InputText("##ConsoleInput", _console_input, IM_ARRAYSIZE(_console_input), ImGuiInputTextFlags_EnterReturnsTrue))
        {
            process_console_input(_console_input);
            sprintf(_console_input, "");
        }
        ImGui::SameLine();
        if(ImGui::Button("Send"))
        {
            process_console_input(_console_input);
            sprintf(_console_input, "");
        }
    }
    ImGui::End();
}

void UserInterface::open()
{
    this->init_item_tracker();
    this->init_map_tracker();
    this->load_personal_settings();
    this->load_client_settings();

    sf::VideoMode video_settings(_window_width, _window_height, 32);
    sf::ContextSettings context_settings;
    context_settings.depthBits = 24;
    context_settings.stencilBits = 8;

    sf::RenderWindow window(video_settings, "Landstalker Archipelago Client", sf::Style::Default, context_settings);
    window.setFramerateLimit(FRAMERATE_LIMIT_FOCUS);
    ImGui::SFML::Init(window);

    auto pos = window.getPosition();
    if(_window_x != -1)
        pos.x = _window_x;
    if(_window_y != -1)
        pos.y = _window_y;
    window.setPosition(pos);

    sf::Image icon;
    if(icon.loadFromFile("icon.png"))
        window.setIcon(icon.getSize().x, icon.getSize().y, icon.getPixelsPtr());

    this->loop(window);

    _window_x = window.getPosition().x;
    _window_y = window.getPosition().y;

    ImGui::SFML::Shutdown();

    this->save_personal_settings();
    this->save_client_settings();
}

void UserInterface::loop(sf::RenderWindow& window)
{
    sf::Clock delta_clock;
    while(window.isOpen())
    {
        sf::Event event {};
        while(window.pollEvent(event))
        {
            ImGui::SFML::ProcessEvent(window, event);
            if(event.type == sf::Event::Closed)
            {
                window.close();
            }
            else if(event.type == sf::Event::Resized)
            {
                _window_width = event.size.width;
                if(_window_width < MIN_WINDOW_WIDTH)
                    _window_width = MIN_WINDOW_WIDTH;

                _window_height = event.size.height;
                if(_window_height < MIN_WINDOW_HEIGHT)
                    _window_height = MIN_WINDOW_HEIGHT;

                window.setSize(sf::Vector2u(_window_width, _window_height));
                window.setView(sf::View(sf::FloatRect(0, 0, (float)_window_width, (float)_window_height)));
            }
            else if(event.type == sf::Event::LostFocus)
                window.setFramerateLimit(FRAMERATE_LIMIT_NO_FOCUS);
            else if(event.type == sf::Event::GainedFocus)
                window.setFramerateLimit(FRAMERATE_LIMIT_FOCUS);
        }

        window.clear(sf::Color::Black);
        ImGui::SFML::Update(window, delta_clock.restart());

        // Draw left panel windows
        if(!archipelago || !archipelago->is_connected())
            draw_archipelago_connection_window();
        else if(!game_state.has_built_rom())
            draw_rom_generation_window();
        else if(!emulator)
            draw_emulator_connection_window();
        else
        {
            float y = draw_item_tracker_window();
            if(_selected_region)
                draw_map_tracker_details_window(y);
        }
        draw_status_window();

        // Draw right panel windows
        float right_panel_x_start = LEFT_PANEL_WIDTH + 2*MARGIN;
        if(!archipelago || !archipelago->is_connected() || !game_state.has_built_rom() || !emulator)
        {
            // Map tracker doesn't need to be drawn, just fill the remaining space with the console window
            draw_console_window(right_panel_x_start, MARGIN);
        }
        else
        {
            if(_window_width > THREE_COLUMNS_MODE_THRESHOLD)
            {
                // Three-columns mode: draw the map tracker as a full column, and the console as another column
                draw_map_tracker_window(right_panel_x_start,
                                        MARGIN,
                                        MAP_TRACKER_WINDOW_MIN_W,
                                        (float)_window_height - 2*MARGIN);
                draw_console_window(right_panel_x_start + MAP_TRACKER_WINDOW_MIN_W + MARGIN, MARGIN);
            }
            else
            {
                // Two-columns mode: draw the console below the map tracker
                float y = draw_map_tracker_window(right_panel_x_start,
                                                  MARGIN,
                                                  (float)_window_width - right_panel_x_start - MARGIN,
                                                  0.f);
                draw_console_window(right_panel_x_start, y);
            }
        }

        ImGui::SFML::Render(window);
        window.display();
    }
}

static std::string color_to_string(const float color[3])
{
    std::ostringstream hud_color_str;
    hud_color_str << "#" << std::hex;
    for(uint8_t i=0 ; i<3 ; ++i)
    {
        uint8_t component_as_int = (uint8_t)(255.f * color[i]);
        component_as_int >>= 4;
        hud_color_str << (uint16_t)component_as_int;
    }

    return hud_color_str.str();
}

static std::array<float,3> color_from_string(const std::string& string)
{
    std::string r_str = string.substr(1, 1);
    std::string g_str = string.substr(2, 1);
    std::string b_str = string.substr(3, 1);

    uint8_t r = std::stoul(r_str, nullptr, 16) << 4;
    uint8_t g = std::stoul(g_str, nullptr, 16) << 4;
    uint8_t b = std::stoul(b_str, nullptr, 16) << 4;

    std::array<float,3> color {};
    color[0] = (float)r / 255.f;
    color[1] = (float)g / 255.f;
    color[2] = (float)b / 255.f;
    return color;
}

void UserInterface::load_personal_settings()
{
    json personal_settings;
    std::ifstream input_file("./personal_settings.json");
    if(input_file.is_open())
    {
        input_file >> personal_settings;
        input_file.close();
    }

    // Parse HUD color
    std::string color_string;
    if(personal_settings.contains("hudColor"))
        color_string = personal_settings.at("hudColor");
    else
        color_string = "#428";
    std::array<float,3> color = color_from_string(color_string);
    _hud_color[0] = color[0];
    _hud_color[1] = color[1];
    _hud_color[2] = color[2];

    // Parse Nigel colors
    if(personal_settings.contains("nigelColor"))
        color_string = personal_settings.at("nigelColor")[0];
    else
        color_string = "#0A8";
    color = color_from_string(color_string);
    _nigel_color_light[0] = color[0];
    _nigel_color_light[1] = color[1];
    _nigel_color_light[2] = color[2];

    if(personal_settings.contains("nigelColor"))
        color_string = personal_settings.at("nigelColor")[1];
    else
        color_string = "#042";
    color = color_from_string(color_string);
    _nigel_color_dark[0] = color[0];
    _nigel_color_dark[1] = color[1];
    _nigel_color_dark[2] = color[2];

    if(personal_settings.contains("removeMusic"))
        _remove_music = personal_settings.at("removeMusic");
    if(personal_settings.contains("swapOverworldMusic"))
        _swap_overworld_music = personal_settings.at("swapOverworldMusic");
}

void UserInterface::load_client_settings()
{
    json client_settings;
    std::ifstream client_settings_file("./client_settings.json");
    if(client_settings_file.is_open())
    {
        client_settings_file >> client_settings;
        client_settings_file.close();
    }

    if(client_settings.contains("host"))
    {
        std::string host = client_settings["host"];
        sprintf(_host, "%s", host.c_str());
    }

    if(client_settings.contains("slot_name"))
    {
        std::string slot_name = client_settings["slot_name"];
        sprintf(_slot_name, "%s", slot_name.c_str());
    }

    if(client_settings.contains("input_rom_path"))
    {
        std::string input_rom_path = client_settings["input_rom_path"];
        sprintf(_input_rom_path, "%s", input_rom_path.c_str());
    }

    if(client_settings.contains("output_rom_path"))
    {
        std::string output_rom_path = client_settings["output_rom_path"];
        sprintf(_output_rom_path, "%s", output_rom_path.c_str());
    }

    if(client_settings.contains("window_x"))
        _window_x = client_settings["window_x"];
    if(client_settings.contains("window_y"))
        _window_y = client_settings["window_y"];

    if(client_settings.contains("window_width"))
    {
        _window_width = client_settings["window_width"];
        if(_window_width < MIN_WINDOW_WIDTH)
            _window_width = MIN_WINDOW_WIDTH;
    }

    if(client_settings.contains("window_height"))
    {
        _window_height = client_settings["window_height"];
        if(_window_height < MIN_WINDOW_HEIGHT)
            _window_height = MIN_WINDOW_HEIGHT;
    }
}

void UserInterface::save_personal_settings()
{
    json personal_settings;
    std::ifstream input_file("./personal_settings.json");
    if(input_file.is_open())
    {
        input_file >> personal_settings;
        input_file.close();
    }

    personal_settings["hudColor"] = color_to_string(_hud_color);

    personal_settings["nigelColor"] = json::array();
    personal_settings["nigelColor"].emplace_back(color_to_string(_nigel_color_light));
    personal_settings["nigelColor"].emplace_back(color_to_string(_nigel_color_dark));

    personal_settings["removeMusic"] = _remove_music;
    personal_settings["swapOverworldMusic"] = _swap_overworld_music;

    std::ofstream output_file("./personal_settings.json");
    if(output_file.is_open())
    {
        output_file << personal_settings.dump(4);
        output_file.close();
    }
}

void UserInterface::save_client_settings()
{
    json client_settings;
    client_settings["host"] = _host;
    client_settings["slot_name"] = _slot_name;
    client_settings["input_rom_path"] = _input_rom_path;
    client_settings["output_rom_path"] = _output_rom_path;
    client_settings["window_x"] = _window_x;
    client_settings["window_y"] = _window_y;
    client_settings["window_width"] = _window_width;
    client_settings["window_height"] = _window_height;

    std::ofstream client_settings_file("./client_settings.json");
    if(client_settings_file)
    {
        std::string dump = client_settings.dump(4);
        client_settings_file << dump;
        client_settings_file.close();
    }
}

void UserInterface::init_item_tracker()
{
    json input_json = json::parse(TRACKABLE_ITEMS_JSON);
    for(json& item_data : input_json)
        _trackable_items.emplace_back(new TrackableItem(item_data));
}

void UserInterface::init_map_tracker()
{
    json input_json = json::parse(TRACKABLE_REGIONS_JSON);
    for(json& region_data : input_json)
        _trackable_regions.emplace_back(new TrackableRegion(region_data));

    _tex_location = new sf::Texture();
    _tex_location->loadFromFile("images/chest.png");

    _tex_location_checked = new sf::Texture();
    _tex_location_checked->loadFromFile("images/chest_open.png");
}

#define SOLVE_LOGIC_PRESET_FILE_PATH "./_solve_logic.json"

void UserInterface::update_map_tracker_logic()
{
    nlohmann::json logic_solve_preset = game_state.preset_json();
    for(TrackableItem* item : _trackable_items)
        if(game_state.owned_item_quantity(item->item_id()) > 0)
            logic_solve_preset["gameSettings"]["startingItems"][item->name()] = 1;

    std::ofstream preset_file(SOLVE_LOGIC_PRESET_FILE_PATH);
    preset_file << logic_solve_preset.dump(4);
    preset_file.close();

    std::string command = "randstalker.exe";
    command += " --preset=\"" SOLVE_LOGIC_PRESET_FILE_PATH "\"";
    command += " --solvelogic";

    std::set<std::string> reachable_locations = invoke_randstalker_to_solve_logic(command);
    for(Location& loc : game_state.locations())
        loc.reachable(reachable_locations.contains(loc.name()));

    std::filesystem::path(SOLVE_LOGIC_PRESET_FILE_PATH).remove_filename();
}

UserInterface::~UserInterface()
{
    for(TrackableItem* ptr : _trackable_items)
        delete ptr;
}
