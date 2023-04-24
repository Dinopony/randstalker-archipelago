#pragma once

#include <vector>
#include <string>

class UserInterface {
private:
    char _host[512] = "localhost:25565"; //"archipelago.gg:12345";
    char _slot_name[256] = "";
    char _password[256] = "";

    std::vector<std::string> _message_log;

public:
    UserInterface() = default;

    void open();

private:
    void draw_archipelago_connection_window();
    void draw_emulator_connection_window();
    void draw_console_window();
};