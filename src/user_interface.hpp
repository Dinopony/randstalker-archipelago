#pragma once

#include <vector>
#include <string>
#include "logger.hpp"

class UserInterface {
private:
    char _host[512] = "localhost:25565"; //"archipelago.gg:12345";
    char _slot_name[256] = "";
    char _password[256] = "";
    char _console_input[512] = "";

    uint32_t _window_width = 1000;
    uint32_t _window_height = 600;
    uint32_t _last_message_count = 0;

public:
    void open();

private:
    void draw_archipelago_connection_window();
    void draw_emulator_connection_window();
    void draw_console_window();
    void draw_console_input();

    void process_console_input();
};