#include <iostream>
#include <cmath>

#include "archipelago_interface.hpp"
#include "retroarch_interface.hpp"
#include "game_state.hpp"
#include <landstalker_lib/constants/item_codes.hpp>

#include <chrono>
#include <thread>

#if !defined WIN32 && !defined _WIN32
#include <poll.h>
#endif

// TODO: Handle the following on ASM side:
//      - Set 0x21.b to 0x93 when Gola is beaten to mark endgame

// TODO: ImGui UI
//      - Deport logging to console + ImGui console
//      - Enable typing commands & chat
//      - Enable rich hint handling
//      - Handle session mutex on actions that matter
//      - Set input ROM path for generation
//      - Set Retroarch path to launch automatically (checkbox?)
//      - Follow ROM generation status on UI

// TODO: Handle hints
//      - Oracle Stone (currently empty)
//      - Lithograph   (currently empty)
//      - Fortune Teller? (seems to work)
//      - Foxies?

// TODO: Handle deathlink

// TODO: Handle collection from server (make check flags match with game state at all times)
//      - The problem is that if we reload, local items won't be reobtainable anymore
//      - We need to enforce this only for checks containing non-local items

std::mutex session_mutex;

GameState game_state;
ArchipelagoInterface* archipelago = nullptr;
RetroarchInterface* emulator = nullptr;

constexpr uint16_t ADDR_RECEIVED_ITEM = 0x20;
constexpr uint16_t ADDR_IS_IN_GAME = 0x1200;
constexpr uint16_t ADDR_SEED = 0x22;
constexpr uint16_t ADDR_IS_GOLA_BEATEN = 0x21;
constexpr uint16_t ADDR_CURRENT_RECEIVED_ITEM_INDEX = 0x107E;


// =============================================================================================
//      GLOBAL FUNCTIONS (Callable from UI)
// =============================================================================================

void connect_ap(std::string host, const std::string& slot_name, const std::string& password)
{
    if(host.empty())
    {
        std::cerr << "Cannot connect with an empty URI" << std::endl;
        return;
    }

    if(archipelago)
    {
        std::cerr << "Cannot connect when there is an active connection going on. Please use /disconnect first." << std::endl;
        return;
    }

    if(!host.empty() && host.find("ws://") != 0 && host.find("wss://") != 0)
        host = "ws://" + host;

    std::cout << "Attempting to connect to Archipelago server at '" << host << "'..." << std::endl;

    archipelago = new ArchipelagoInterface(host, slot_name, password, &game_state);
}

void disconnect_ap()
{
    delete archipelago;
    archipelago = nullptr;
}

void connect_emu()
{
    try
    {
        if(!emulator)
            emulator = new RetroarchInterface();
    }
    catch(EmulatorException& ex)
    {
        std::cerr << "[ERROR] Connection to emulator failed: " << ex.message() << std::endl;
    }
}

void disconnect_emu()
{
    delete emulator;
    emulator = nullptr;
}

void poll_emulator()
{
    if(!emulator)
        return;

    // If no save file is currently loaded, no need to do anything
    if(emulator->read_game_word(ADDR_IS_IN_GAME) == 0x00)
        return;

    if(emulator->read_game_long(ADDR_SEED) != game_state.expected_seed())
    {
        if(game_state.expected_seed() != 0xFFFFFFFF)
        {
            // If seed is 0xFFFFFFFF, it means we did not connect to Archipelago server yet.
            // We only output an error and delete the current emulator interface if we were expecting a specific ROM
            // during gameplay and another one was provided.
            throw EmulatorException("Invalid seed. Please ensure the right ROM was loaded.");
        }
    }

    // Test all location flags to see if player checked new locations since last poll
    const std::vector<Location>& locations = game_state.locations();
    for(const Location& location : locations)
    {
        if(location.was_checked(*emulator))
        {
            if(game_state.set_location_checked_by_player(location.id()))
            {
                std::cout << "Location '" << location.name() << "' is checked!" << std::endl;
            }
        }
    }

    // If there are received items that are not yet processed, send the next pending one to the player
    uint16_t current_item_index_in_game = emulator->read_game_word(ADDR_CURRENT_RECEIVED_ITEM_INDEX);
    if(game_state.current_item_index() > current_item_index_in_game)
    {
        if(emulator->read_game_byte(ADDR_RECEIVED_ITEM) == 0xFF)
        {
            emulator->write_game_byte(ADDR_RECEIVED_ITEM, game_state.item_with_index(current_item_index_in_game));
            emulator->write_game_word(ADDR_CURRENT_RECEIVED_ITEM_INDEX, current_item_index_in_game + 1);
        }
    }

    // Check goal completion
    if(archipelago->is_connected() && emulator->read_game_byte(ADDR_IS_GOLA_BEATEN) == 0x93)
    {
        archipelago->notify_game_completed();
        emulator->write_game_byte(ADDR_IS_GOLA_BEATEN, 0x00);
    }
}


// =============================================================================================
//      UI HANDLING
// =============================================================================================

#include <SFML/Window.hpp>
#include <SFML/Graphics.hpp>
#include <imgui-SFML.h>

namespace uiglobals {
char host[512] = "localhost:25565"; //"archipelago.gg:12345";
char slot_name[256];
char password[256];
std::vector<std::string> message_log;
}

static void draw_archipelago_connection_window()
{
    ImGui::SetNextWindowPos(ImVec2(10,10));
    ImGui::SetNextWindowSize(ImVec2(340,200));
    ImGui::Begin("AP Connection", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove);
    {
        ImGui::PushItemWidth(-1);
        ImGui::Text("Host");
        ImGui::InputText("##Host", uiglobals::host, IM_ARRAYSIZE(uiglobals::host));
        ImGui::Text("Slot name");
        ImGui::InputText("##Slot name", uiglobals::slot_name, IM_ARRAYSIZE(uiglobals::slot_name));
        ImGui::Text("Password");
        ImGui::InputText("##Password", uiglobals::password, IM_ARRAYSIZE(uiglobals::password));
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
                connect_ap(uiglobals::host, uiglobals::slot_name, uiglobals::password);
        }
    }
    ImGui::End();
}

static void draw_emulator_connection_window()
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

static void draw_console_window()
{
    ImGui::SetNextWindowPos(ImVec2(360,10));
    ImGui::SetNextWindowSize(ImVec2(910,700));
    ImGui::Begin("Console", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove);
    {
        for(const std::string& msg : uiglobals::message_log)
            ImGui::Text("%s", msg.c_str());
    }
    ImGui::End();
}

void open_ui()
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

int main(int argc, char** argv)
{
    // Network + game handling thread
    bool keep_working = true;
    std::thread process_thread([&keep_working]() {
        while(keep_working)
        {
            session_mutex.lock();
            {
                try
                {
                    poll_emulator();
                }
                catch(EmulatorException& ex)
                {
                    std::cerr << "[ERROR] " << ex.message() << std::endl;
                    delete emulator;
                    emulator = nullptr;
                }

                if(archipelago)
                    archipelago->poll();
            }
            session_mutex.unlock();
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    });

    // UI thread
    open_ui();

    // When UI is closed, tell the other thread to stop working
    keep_working = false;
    process_thread.join();
    return EXIT_SUCCESS;
}

/*
void on_command(const std::string& command)
{
    if (command == "/help") {
        printf("Available commands:\n"
               "  /connect [addr[:port]] - connect to AP server\n"
               "  /disconnect - disconnect from AP server\n");
               // TODO: "  /force-send - send missing items to game, ignoring locks\n"
               // TODO: "  /force-resend - resend all items to game\n"
               // TODO: "  /sync - resync items/locations with AP server\n");
    } else if (archipelago) {
        archipelago->say(command);
    }
}
*/