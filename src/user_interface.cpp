#include <SFML/Window.hpp>
#include <SFML/Graphics.hpp>
#include <imgui-SFML.h>

#include "user_interface.hpp"
#include "client.hpp"

// TODO: ImGui UI
//      - Deport logging to console + ImGui console
//      - Enable typing commands & chat
//      - Enable rich hint handling
//      - Handle session mutex on actions that matter
//      - Set input ROM path for generation
//      - Set Retroarch path to launch automatically (checkbox?)
//      - Follow ROM generation status on UI

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
    ImGui::SetNextWindowPos(ImVec2(10,10));
    ImGui::SetNextWindowSize(ImVec2(340,200));
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
    ImGui::SetNextWindowPos(ImVec2(10,220));
    ImGui::SetNextWindowSize(ImVec2(340,100));
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
    ImGui::SetNextWindowPos(ImVec2(360,10));
    ImGui::SetNextWindowSize(ImVec2(910,700));

    ImGui::Begin("Console", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_AlwaysVerticalScrollbar);
    {
        for(const std::string& msg : _message_log)
            ImGui::Text("%s", msg.c_str());
    }
    ImGui::End();
}

void UserInterface::open()
{
    sf::VideoMode video_settings(1280, 720, 32);
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
                window.close();
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



