#include <SFML/Window.hpp>
#include <SFML/Graphics.hpp>
#include <imgui-SFML.h>

#include "user_interface.hpp"
#include "client.hpp"

// TODO: ImGui UI
//      - Enable typing commands & chat
//      - Enable rich hint handling
//      - Set input ROM path for generation
//      - Set Retroarch path to launch automatically (checkbox?)
//      - Follow ROM generation status on UI

constexpr uint32_t MIN_WINDOW_WIDTH = 800;
constexpr uint32_t MIN_WINDOW_HEIGHT = 600;

constexpr uint32_t MARGIN = 6;
constexpr uint32_t LEFT_PANEL_WIDTH = 340;
constexpr uint32_t AP_WINDOW_HEIGHT = 200;
constexpr uint32_t EMU_WINDOW_HEIGHT = 100;

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
    ImGui::SetNextWindowPos(ImVec2(MARGIN, MARGIN));
    ImGui::SetNextWindowSize(ImVec2(LEFT_PANEL_WIDTH, AP_WINDOW_HEIGHT));

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

        if(archipelago && archipelago->is_connected())
        {
            ImGui::Text("Connection status: Connected");
            ImGui::Separator(); // ------------------------------------------------------------

            if(ImGui::Button("Disconnect"))
                disconnect_ap();
        }
        else if(archipelago)
        {
            ImGui::Text("Connection status: Attempting to connect...");
            ImGui::Separator(); // ------------------------------------------------------------

            if(ImGui::Button("Stop"))
                disconnect_ap();
        }
        else
        {
            ImGui::Text("Connection status: Disconnected");
            ImGui::Separator(); // ------------------------------------------------------------

            if(ImGui::Button("Connect"))
                connect_ap(_host, _slot_name, _password);
        }
    }
    ImGui::End();
}

void UserInterface::draw_emulator_connection_window()
{
    ImGui::SetNextWindowPos(ImVec2(MARGIN, AP_WINDOW_HEIGHT + 2*MARGIN));
    ImGui::SetNextWindowSize(ImVec2(LEFT_PANEL_WIDTH, EMU_WINDOW_HEIGHT));
    ImGui::Begin("Emulator Connection", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove);
    {
        if(emulator)
        {
            ImGui::Text("Connected to Retroarch");
            ImGui::Separator(); // ------------------------------------------------------------
            if(ImGui::Button("Disconnect"))
                disconnect_emu();
        }
        else
        {
            ImGui::Text("Not connected to Retroarch");
            ImGui::Separator(); // ------------------------------------------------------------
            if(ImGui::Button("Connect"))
                connect_emu();
        }
    }
    ImGui::End();
}

void UserInterface::draw_console_window()
{
    ImGui::SetNextWindowPos(ImVec2(LEFT_PANEL_WIDTH + 2*MARGIN, MARGIN));
    ImGui::SetNextWindowSize(ImVec2((float)_window_width-(LEFT_PANEL_WIDTH+3*MARGIN), (float)_window_height-(2*MARGIN)));

    ImGui::Begin("Console", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_AlwaysVerticalScrollbar);
    {
        for(const Logger::Message& msg : Logger::get().messages())
        {
            std::string prefix;
            switch(msg.level) {
                case Logger::LOG_DEBUG:
                    ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(120,120,120,255));
                    prefix = "DEBUG";
                    break;
                case Logger::LOG_INFO:
                    ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(230,230,255,255));
                    prefix = "INFO";
                    break;
                case Logger::LOG_WARNING:
                    ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(255,180,0,255));
                    prefix = "WARNING";
                    break;
                case Logger::LOG_ERROR:
                    ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(255,60,60,255));
                    prefix = "ERROR";
                    break;
            }

            ImGui::TextWrapped("[%s] %s", prefix.c_str(), msg.text.c_str());
            ImGui::PopStyleColor();
        }
    }
    ImGui::End();
}

void UserInterface::open()
{
    sf::VideoMode video_settings(_window_width, _window_height, 32);
    sf::ContextSettings context_settings;
    context_settings.depthBits = 24;
    context_settings.stencilBits = 8;

    sf::RenderWindow window(video_settings, "Landstalker Archipelago Client", sf::Style::Default, context_settings);
    window.setVerticalSyncEnabled(true);
    ImGui::SFML::Init(window);

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
        draw_emulator_connection_window();
        draw_console_window();

        ImGui::SFML::Render(window);
        window.display();
    }

    ImGui::SFML::Shutdown();
}



