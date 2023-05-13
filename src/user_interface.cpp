#include <Windows.h>

#include <SFML/Graphics.hpp>
#include <imgui-SFML.h>
#include <fstream>
#include <filesystem>
#include <landstalker_lib/constants/item_codes.hpp>

#include "user_interface.hpp"
#include "client.hpp"

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

// ============================================================================================================

constexpr uint32_t FRAMERATE_LIMIT_FOCUS = 60;
constexpr uint32_t FRAMERATE_LIMIT_NO_FOCUS = 10;

constexpr uint32_t MIN_WINDOW_WIDTH = 800;
constexpr uint32_t MIN_WINDOW_HEIGHT = 640;

constexpr uint32_t MARGIN = 8;
constexpr uint32_t LEFT_PANEL_WIDTH = 340;

constexpr uint32_t AP_WINDOW_H = 172;
constexpr uint32_t ROM_WINDOW_H = 260;
constexpr uint32_t EMU_WINDOW_H = 155;
constexpr uint32_t HINT_WINDOW_H = 62;
constexpr uint32_t TRACKER_WINDOW_H = 427;
constexpr uint32_t STATUS_WINDOW_H = 104;

constexpr uint32_t CONSOLE_INPUT_HEIGHT = 35;

const auto WINDOW_FLAGS = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize
                        | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings;


float UserInterface::draw_archipelago_connection_window(float y)
{
    if(archipelago && archipelago->is_connected())
        return 0.f;

    ImGui::SetNextWindowPos(ImVec2(MARGIN, y));
    ImGui::SetNextWindowSize(ImVec2(LEFT_PANEL_WIDTH, AP_WINDOW_H));

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

    return (float)(AP_WINDOW_H + MARGIN);
}

float UserInterface::draw_rom_generation_window(float y)
{
    if(!archipelago || !archipelago->is_connected() || game_state.has_built_rom())
        return 0.f;

    ImGui::SetNextWindowPos(ImVec2(MARGIN, y));
    ImGui::SetNextWindowSize(ImVec2(LEFT_PANEL_WIDTH, ROM_WINDOW_H));

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

        if(ImGui::Button("Build ROM"))
        {
            std::string rom_path = build_rom();
            game_state.built_rom_path(rom_path);
        }
    }
    ImGui::End();

    return (float)(ROM_WINDOW_H + MARGIN);
}

float UserInterface::draw_emulator_connection_window(float y)
{
    if(!archipelago || !archipelago->is_connected() || !game_state.has_built_rom() || emulator)
        return 0.f;

    ImGui::SetNextWindowPos(ImVec2(MARGIN, y));
    ImGui::SetNextWindowSize(ImVec2(LEFT_PANEL_WIDTH, EMU_WINDOW_H));

    ImGui::Begin("Emulator Connection", nullptr, WINDOW_FLAGS);
    {
        if(ImGui::Button("Show ROM file in explorer"))
        {
            std::string output_path = std::string(_output_rom_path);
            if(!output_path.ends_with("/"))
                output_path += "/";
            std::string path_str = std::filesystem::absolute(std::filesystem::path(output_path)).string();
            ShellExecuteA(nullptr, "explore", path_str.c_str(), nullptr, nullptr, SW_SHOWDEFAULT);
        }

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

    return (float)(EMU_WINDOW_H + MARGIN);
}

float UserInterface::draw_hint_window(float y) const
{
    if(!archipelago || !archipelago->is_connected() || !game_state.has_built_rom() || !emulator)
        return 0.f;

    ImGui::SetNextWindowPos(ImVec2(MARGIN, y));
    ImGui::SetNextWindowSize(ImVec2(LEFT_PANEL_WIDTH, HINT_WINDOW_H));
    ImGui::Begin("Hints", nullptr, WINDOW_FLAGS);
    {
        // LOCATION HINT COMBO /////////////////////////////////////////////////////////////////////

        static const char* current_location = game_state.locations()[0].name().c_str();
        if (ImGui::BeginCombo("##combolocations", current_location))
        {
            for (auto& location : game_state.locations())
            {
                if(location.was_checked())
                    continue;

                bool is_selected = (current_location == location.name().c_str());
                if (ImGui::Selectable(location.name().c_str(), is_selected))
                    current_location = location.name().c_str();
                if (is_selected)
                    ImGui::SetItemDefaultFocus();
            }
            ImGui::EndCombo();
        }
        ImGui::SameLine();

        if(ImGui::Button("Hint location"))
            process_console_input("!hint_location " + std::string(current_location));

        // LIST HINTS BUTTON /////////////////////////////////////////////////////////////////////

        if(ImGui::Button("List known hints"))
            process_console_input("!hint");
    }
    ImGui::End();

    return (float)(HINT_WINDOW_H + MARGIN);
}

float UserInterface::draw_tracker_window(float y) const
{
    if(!archipelago || !archipelago->is_connected() || !game_state.has_built_rom() || !emulator)
        return 0.f;

    ImGui::SetNextWindowPos(ImVec2(MARGIN, y));
    ImGui::SetNextWindowSize(ImVec2(LEFT_PANEL_WIDTH, TRACKER_WINDOW_H));
    ImGui::Begin("Tracker", nullptr, WINDOW_FLAGS);
    {
        ImVec2 wsize(46.f, 46.f);
        for(TrackableItem* ptr : _trackable_items)
        {
            bool item_owned = (game_state.owned_item_quantity(ptr->item_id()) > 0);
            ImVec4 color_multipler(1, 1, 1, 1);
            if(!item_owned)
                color_multipler = ImVec4(0.4, 0.4, 0.4, 0.6);

            ImGui::SetCursorPos(ImVec2(MARGIN + ptr->x(), MARGIN + ptr->y()));
            ImGui::Image((ImTextureID) ptr->get_texture_id(), wsize, ImVec2(0,0), ImVec2(1,1), color_multipler);
            if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
            {
                std::string suffix = (!item_owned) ? "\n\n(Right-click to get a hint for this item)" : "";
                if(item_owned)
                    ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(100,255,100,255));
                else
                    ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(255,100,100,255));

                ImGui::SetTooltip("%s%s", ptr->name().c_str(), suffix.c_str());
                ImGui::PopStyleColor();

                if(!item_owned && ImGui::IsMouseReleased(1))
                    process_console_input("!hint " + ptr->name());
            }
        }
    }
    ImGui::End();

    return (float)(TRACKER_WINDOW_H + MARGIN);
}

void UserInterface::draw_status_window()
{
    ImGui::SetNextWindowPos(ImVec2(MARGIN, _window_height - (MARGIN + STATUS_WINDOW_H)));
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

void UserInterface::draw_console_window()
{
    ImGui::SetNextWindowPos(ImVec2(LEFT_PANEL_WIDTH + 2*MARGIN, MARGIN));
    ImGui::SetNextWindowSize(ImVec2((float)_window_width-(LEFT_PANEL_WIDTH+3*MARGIN), (float)_window_height-(CONSOLE_INPUT_HEIGHT+2*MARGIN)));

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
                    ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(100,255,100,255));
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
}

void UserInterface::draw_console_input()
{
    ImGui::SetNextWindowPos(ImVec2(LEFT_PANEL_WIDTH + 2*MARGIN, _window_height-(CONSOLE_INPUT_HEIGHT+MARGIN)));
    ImGui::SetNextWindowSize(ImVec2((float)_window_width-(LEFT_PANEL_WIDTH+3*MARGIN), (float)CONSOLE_INPUT_HEIGHT));

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
    this->init_tracker();
    this->load_personal_settings();
    this->load_client_settings();

    sf::VideoMode video_settings(_window_width, _window_height, 32);
    sf::ContextSettings context_settings;
    context_settings.depthBits = 24;
    context_settings.stencilBits = 8;

    sf::RenderWindow window(video_settings, "Landstalker Archipelago Client", sf::Style::Default, context_settings);
    window.setFramerateLimit(FRAMERATE_LIMIT_FOCUS);
    ImGui::SFML::Init(window);

    sf::Image icon;
    if(icon.loadFromFile("icon.png"))
        window.setIcon(icon.getSize().x, icon.getSize().y, icon.getPixelsPtr());

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
                window.setView(sf::View(sf::FloatRect(0, 0, _window_width, _window_height)));
            }
            else if(event.type == sf::Event::LostFocus)
            {
                window.setFramerateLimit(FRAMERATE_LIMIT_NO_FOCUS);
            }
            else if(event.type == sf::Event::GainedFocus)
            {
                window.setFramerateLimit(FRAMERATE_LIMIT_FOCUS);
            }
        }

        window.clear(sf::Color::Black);
        ImGui::SFML::Update(window, delta_clock.restart());

        float y = MARGIN;
        y += draw_archipelago_connection_window(y);
        y += draw_rom_generation_window(y);
        y += draw_emulator_connection_window(y);
        y += draw_tracker_window(y);
        draw_hint_window(y);

        draw_status_window();

        draw_console_window();
        draw_console_input();

        ImGui::SFML::Render(window);
        window.display();
    }

    ImGui::SFML::Shutdown();

    this->save_personal_settings();
    this->save_client_settings();
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

void UserInterface::init_tracker()
{
    constexpr float ICON_SIZE = 55.f;
    _trackable_items = {
        new TrackableItem("Magic Sword",   "sword_1.gif", ITEM_MAGIC_SWORD,     sf::Vector2f(0.f,         0.f)),
        new TrackableItem("Thunder Sword", "sword_2.gif", ITEM_THUNDER_SWORD,   sf::Vector2f(ICON_SIZE,   0.f)),
        new TrackableItem("Sword of Ice",  "sword_3.gif", ITEM_ICE_SWORD,       sf::Vector2f(ICON_SIZE*2, 0.f)),
        new TrackableItem("Sword of Gaia", "sword_4.gif", ITEM_GAIA_SWORD,      sf::Vector2f(ICON_SIZE*3, 0.f)),

        new TrackableItem("Steel Breast",  "armor_1.gif", ITEM_STEEL_BREAST,    sf::Vector2f(0.f,         ICON_SIZE)),
        new TrackableItem("Chrome Breast", "armor_2.gif", ITEM_CHROME_BREAST,   sf::Vector2f(ICON_SIZE,   ICON_SIZE)),
        new TrackableItem("Shell Breast",  "armor_3.gif", ITEM_SHELL_BREAST,    sf::Vector2f(ICON_SIZE*2, ICON_SIZE)),
        new TrackableItem("Hyper Breast",  "armor_4.gif", ITEM_HYPER_BREAST,    sf::Vector2f(ICON_SIZE*3, ICON_SIZE)),

        new TrackableItem("Healing Boots", "boots_1.gif", ITEM_HEALING_BOOTS,   sf::Vector2f(0.f,         ICON_SIZE*2)),
        new TrackableItem("Fireproof",     "boots_2.gif", ITEM_FIREPROOF_BOOTS, sf::Vector2f(ICON_SIZE,   ICON_SIZE*2)),
        new TrackableItem("Iron Boots",    "boots_3.gif", ITEM_IRON_BOOTS,      sf::Vector2f(ICON_SIZE*2, ICON_SIZE*2)),
        new TrackableItem("Snow Spikes",   "boots_4.gif", ITEM_SPIKE_BOOTS,     sf::Vector2f(ICON_SIZE*3, ICON_SIZE*2)),

        new TrackableItem("Saturn Stone",  "ring_1.gif",  ITEM_SATURN_STONE,    sf::Vector2f(0.f,         ICON_SIZE*3)),
        new TrackableItem("Mars Stone",    "ring_2.gif",  ITEM_MARS_STONE,      sf::Vector2f(ICON_SIZE,   ICON_SIZE*3)),
        new TrackableItem("Moon Stone",    "ring_3.gif",  ITEM_MOON_STONE,      sf::Vector2f(ICON_SIZE*2, ICON_SIZE*3)),
        new TrackableItem("Venus Stone",   "ring_4.gif",  ITEM_VENUS_STONE,     sf::Vector2f(ICON_SIZE*3, ICON_SIZE*3)),

        new TrackableItem("Bell",            "bell.gif",         ITEM_BELL,         sf::Vector2f(ICON_SIZE*5, 0.f)),
        new TrackableItem("Lithograph",      "lithograph.gif",   ITEM_LITHOGRAPH,   sf::Vector2f(ICON_SIZE*5, ICON_SIZE)),
        new TrackableItem("Oracle Stone",    "oracle_stone.gif", ITEM_ORACLE_STONE, sf::Vector2f(ICON_SIZE*5, ICON_SIZE*2)),
        new TrackableItem("Statue of Jypta", "statue_jypta.gif", ITEM_STATUE_JYPTA, sf::Vector2f(ICON_SIZE*5, ICON_SIZE*3)),

        new TrackableItem("Idol Stone",  "idol_stone.gif",  ITEM_IDOL_STONE,    sf::Vector2f(0.f,         ICON_SIZE*4)),
        new TrackableItem("Safety Pass", "safety_pass.gif", ITEM_SAFETY_PASS,   sf::Vector2f(ICON_SIZE,   ICON_SIZE*4)),
        new TrackableItem("Armlet",      "armlet.gif",      ITEM_ARMLET,        sf::Vector2f(ICON_SIZE*2, ICON_SIZE*4)),
        new TrackableItem("Garlic",      "garlic.gif",      ITEM_GARLIC,        sf::Vector2f(ICON_SIZE*3, ICON_SIZE*4)),
        new TrackableItem("Key",         "key.gif",         ITEM_KEY,           sf::Vector2f(ICON_SIZE*4, ICON_SIZE*4)),
        new TrackableItem("Lantern",     "lantern.gif",     ITEM_LANTERN,       sf::Vector2f(ICON_SIZE*5, ICON_SIZE*4)),

        new TrackableItem("Einstein Whistle", "einstein_whistle.gif", ITEM_EINSTEIN_WHISTLE, sf::Vector2f(0.f,         ICON_SIZE*5)),
        new TrackableItem("Sun Stone",        "sun_stone.gif",        ITEM_SUN_STONE,        sf::Vector2f(ICON_SIZE,   ICON_SIZE*5)),
        new TrackableItem("Axe Magic",        "axe_magic.gif",        ITEM_AXE_MAGIC,        sf::Vector2f(ICON_SIZE*2, ICON_SIZE*5)),
        new TrackableItem("Logs",             "logs.gif",             ITEM_LOGS,             sf::Vector2f(ICON_SIZE*3, ICON_SIZE*5)),
        new TrackableItem("Buyer Card",       "buyer_card.gif",       ITEM_BUYER_CARD,       sf::Vector2f(ICON_SIZE*4, ICON_SIZE*5)),
        new TrackableItem("Casino Ticket",    "casino_ticket.gif",    ITEM_CASINO_TICKET,    sf::Vector2f(ICON_SIZE*5, ICON_SIZE*5)),

        new TrackableItem("Gola's Eye",       "gola_eye.gif",       ITEM_GOLA_EYE,      sf::Vector2f(0.f,           ICON_SIZE*6+15)),
        new TrackableItem("Red Jewel",        "red_jewel.gif",      ITEM_RED_JEWEL,     sf::Vector2f(ICON_SIZE,     ICON_SIZE*6)),
        new TrackableItem("Purple Jewel",     "purple_jewel.gif",   ITEM_PURPLE_JEWEL,  sf::Vector2f(ICON_SIZE+25,  ICON_SIZE*6+35)),
        new TrackableItem("Green Jewel",      "green_jewel.gif",    ITEM_GREEN_JEWEL,   sf::Vector2f(ICON_SIZE+50,  ICON_SIZE*6)),
        new TrackableItem("Yellow Jewel",     "yellow_jewel.gif",   ITEM_YELLOW_JEWEL,  sf::Vector2f(ICON_SIZE+75,  ICON_SIZE*6+35)),
        new TrackableItem("Blue Jewel",       "blue_jewel.gif",     ITEM_BLUE_JEWEL,    sf::Vector2f(ICON_SIZE+100, ICON_SIZE*6)),
        new TrackableItem("Gola's Nail",      "gola_nail.gif",      ITEM_GOLA_NAIL,     sf::Vector2f(ICON_SIZE*3+50,ICON_SIZE*6)),
        new TrackableItem("Gola's Fang",      "gola_fang.gif",      ITEM_GOLA_FANG,     sf::Vector2f(ICON_SIZE*4+25,ICON_SIZE*6+35)),
        new TrackableItem("Gola's Horn",      "gola_horn.gif",      ITEM_GOLA_HORN,     sf::Vector2f(ICON_SIZE*5,   ICON_SIZE*6)),
    };
}

UserInterface::~UserInterface()
{
    for(TrackableItem* ptr : _trackable_items)
        delete ptr;
}