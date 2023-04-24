#pragma once

#include <vector>
#include <string>
#include "logger.hpp"

class UserInterface {
private:
    char _host[512] = "localhost:25565"; //"archipelago.gg:12345";
    char _slot_name[256] = "";
    char _password[256] = "";

    uint32_t _window_width = 800;
    uint32_t _window_height = 600;

public:
    void open();

private:
    void draw_archipelago_connection_window();
    void draw_emulator_connection_window();
    void draw_console_window();
};