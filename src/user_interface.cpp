#include <SFML/Window.hpp>
#include <SFML/Graphics.hpp>
#include <imgui-SFML.h>
#include <fstream>

#include "user_interface.hpp"
#include "client.hpp"

constexpr uint32_t MIN_WINDOW_WIDTH = 800;
constexpr uint32_t MIN_WINDOW_HEIGHT = 600;

constexpr uint32_t MARGIN = 8;
constexpr uint32_t LEFT_PANEL_WIDTH = 340;

constexpr uint32_t ROM_WINDOW_X = MARGIN;
constexpr uint32_t ROM_WINDOW_Y = MARGIN;
constexpr uint32_t ROM_WINDOW_W = LEFT_PANEL_WIDTH;
constexpr uint32_t ROM_WINDOW_H = 200;

constexpr uint32_t AP_WINDOW_X = MARGIN;
constexpr uint32_t AP_WINDOW_Y = ROM_WINDOW_Y + ROM_WINDOW_H + MARGIN * 4;
constexpr uint32_t AP_WINDOW_W = LEFT_PANEL_WIDTH;
constexpr uint32_t AP_WINDOW_H = 180;

constexpr uint32_t EMU_WINDOW_X = MARGIN;
constexpr uint32_t EMU_WINDOW_Y = AP_WINDOW_Y + AP_WINDOW_H + MARGIN;
constexpr uint32_t EMU_WINDOW_W = LEFT_PANEL_WIDTH;
constexpr uint32_t EMU_WINDOW_H = 60;

constexpr uint32_t HINT_WINDOW_X = MARGIN;
constexpr uint32_t HINT_WINDOW_W = LEFT_PANEL_WIDTH;
constexpr uint32_t HINT_WINDOW_H = 60;

constexpr uint32_t CONSOLE_INPUT_HEIGHT = 35;

#include <commdlg.h>
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

void UserInterface::draw_archipelago_connection_window()
{
    ImGui::SetNextWindowPos(ImVec2(AP_WINDOW_X, AP_WINDOW_Y));
    ImGui::SetNextWindowSize(ImVec2(AP_WINDOW_W, AP_WINDOW_H));

    ImGui::Begin("AP Connection", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove);
    {
        ImGui::PushItemWidth(-1);
        ImGui::Text("Host");
        ImGui::InputText("##Host", _host, IM_ARRAYSIZE(_host));
        ImGui::Text("Slot name");
        ImGui::InputText("##Slot name", _slot_name, IM_ARRAYSIZE(_slot_name));
        ImGui::Text("Password");
        ImGui::InputText("##Password", _password, IM_ARRAYSIZE(_password));
        ImGui::PopItemWidth();

        ImGui::Separator(); // ------------------------------------------------------------

        ImGui::Text("Connection to server:");
        ImGui::SameLine();
        if(archipelago && archipelago->is_connected())
        {
            ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(0,255,0,255));
            ImGui::Text("Connected");
            ImGui::PopStyleColor();

            ImGui::Separator(); // ------------------------------------------------------------

            if(ImGui::Button("Disconnect"))
                disconnect_ap();

            if(game_state.has_preset_json())
            {
                ImGui::SameLine();
                if(ImGui::Button("Rebuild ROM"))
                    build_rom();
            }
        }
        else if(archipelago)
        {
            ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(255,200,0,255));
            ImGui::Text("Connecting...");
            ImGui::PopStyleColor();

            ImGui::Separator(); // ------------------------------------------------------------

            if(ImGui::Button("Stop"))
                disconnect_ap();
        }
        else
        {
            ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(255,0,0,255));
            ImGui::Text("Disconnected");
            ImGui::PopStyleColor();

            ImGui::Separator(); // ------------------------------------------------------------

            if(ImGui::Button("Connect"))
                connect_ap(_host, _slot_name, _password);
        }
    }
    ImGui::End();
}

void UserInterface::draw_rom_generation_window()
{
    ImGui::SetNextWindowPos(ImVec2(ROM_WINDOW_X, ROM_WINDOW_Y));
    ImGui::SetNextWindowSize(ImVec2(ROM_WINDOW_W, ROM_WINDOW_H));
    ImGui::Begin("ROM Generation", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove);
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
//        ImGui::PushItemWidth(-35);
        ImGui::PushItemWidth(-1);
        ImGui::InputText("##OutputPath", _output_rom_path, IM_ARRAYSIZE(_output_rom_path));
        ImGui::PopItemWidth();
//       ImGui::SameLine();
//       if(ImGui::Button("...  "))
//       {
//        std::string picked_path = ask_for_file();
//        if(!picked_path.empty())
//            sprintf(_output_rom_path, "%s", picked_path.c_str());
//       }

        ImGui::Separator();

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
    }
    ImGui::End();
}

void UserInterface::draw_emulator_connection_window()
{
    ImGui::SetNextWindowPos(ImVec2(EMU_WINDOW_X, EMU_WINDOW_Y));
    ImGui::SetNextWindowSize(ImVec2(EMU_WINDOW_W, EMU_WINDOW_H));
    ImGui::Begin("Emulator Connection", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove);
    {
        ImGui::Text("Connection to Retroarch:");
        ImGui::SameLine();
        if(emulator)
        {
            ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(0,255,0,255));
            ImGui::Text("Connected");
            ImGui::PopStyleColor();

            ImGui::Separator(); // ------------------------------------------------------------
            if(ImGui::Button("Disconnect"))
                disconnect_emu();
        }
        else
        {
            ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(255,0,0,255));
            ImGui::Text("Disconnected");
            ImGui::PopStyleColor();

            ImGui::Separator(); // ------------------------------------------------------------
            if(ImGui::Button("Connect"))
                connect_emu();
        }
    }
    ImGui::End();
}

void UserInterface::draw_hint_window()
{
    ImGui::SetNextWindowPos(ImVec2(HINT_WINDOW_X, _window_height - HINT_WINDOW_H - MARGIN));
    ImGui::SetNextWindowSize(ImVec2(HINT_WINDOW_W, HINT_WINDOW_H));
    ImGui::Begin("Hints", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove);
    {
        ImGui::Text("Item to hint");

        const char* items[] = {
                "Safety Pass", "Key", "Idol Stone", "Axe Magic", "Lantern", "Armlet", "Garlic",
                "Einstein Whistle", "Sun Stone", "Buyer Card", "Casino Ticket",
                "Gola's Eye", "Red Jewel", "Purple Jewel", "Green Jewel", "Blue Jewel", "Yellow Jewel",
                "Gola's Nail", "Gola's Horn", "Gola's Fang",
                "-------------",
                "Magic Sword", "Sword of Ice", "Thunder Sword", "Sword of Gaia",
                "Progressive Armor", "Steel Breast", "Chrome Breast", "Shell Breast", "Hyper Breast",
                "Fireproof", "Iron Boots", "Healing Boots", "Snow Spikes",
                "Mars Stone", "Moon Stone", "Saturn Stone", "Venus Stone",
                "-------------",
                "Oracle Stone", "Lithograph",
                "Life Stock", "EkeEke", "Detox Grass", "Statue of Gaia", "Golden Statue", "Mind Repair",
                "Blue Ribbon", "Anti Paralyze", "Statue of Jypta", "Detox Book", "Pawn Ticket", "Death Statue",
                "Dahl", "Restoration", "Logs", "Bell", "Short Cake"
        };
        static const char* current_item = items[0];

        if (ImGui::BeginCombo("##combo", current_item)) // The second parameter is the label previewed before opening the combo.
        {
            for (auto& item : items)
            {
                bool is_selected = (current_item == item); // You can store your selection however you want, outside or inside your objects
                if (ImGui::Selectable(item, is_selected))
                    current_item = item;
                if (is_selected)
                    ImGui::SetItemDefaultFocus();   // You may set the initial focus when opening the combo (scrolling + for keyboard navigation support)
            }
            ImGui::EndCombo();
        }
        ImGui::SameLine();

        if(ImGui::Button("Ask hint"))
        {
            process_console_input("!hint " + std::string(current_item));
        }
    }
    ImGui::End();
}

void UserInterface::draw_console_window()
{
    ImGui::SetNextWindowPos(ImVec2(LEFT_PANEL_WIDTH + 2*MARGIN, MARGIN));
    ImGui::SetNextWindowSize(ImVec2((float)_window_width-(LEFT_PANEL_WIDTH+3*MARGIN), (float)_window_height-(CONSOLE_INPUT_HEIGHT+2*MARGIN)));

    ImGui::Begin("Console", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_AlwaysVerticalScrollbar);
    {
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
        }

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

    ImGui::Begin("ConsoleInputWindow", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove);
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
    this->load_personal_settings();
    this->load_client_settings();

    sf::VideoMode video_settings(_window_width, _window_height, 32);
    sf::ContextSettings context_settings;
    context_settings.depthBits = 24;
    context_settings.stencilBits = 8;

    sf::RenderWindow window(video_settings, "Landstalker Archipelago Client", sf::Style::Default, context_settings);
    window.setFramerateLimit(30);
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
            }
        }

        window.clear(sf::Color::Black);
        ImGui::SFML::Update(window, delta_clock.restart());

        draw_archipelago_connection_window();
        draw_rom_generation_window();
        draw_emulator_connection_window();
        draw_hint_window();
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
