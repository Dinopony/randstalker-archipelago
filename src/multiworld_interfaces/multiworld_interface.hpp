#pragma once

#include <vector>
#include <string>

class MultiworldInterface
{
public:
    MultiworldInterface() = default;
    virtual ~MultiworldInterface() = default;

    virtual void poll() = 0;
    virtual void send_checked_locations_to_server(const std::vector<int64_t>& checked_locations) = 0;
    virtual void say(const std::string& msg) = 0;

    [[nodiscard]] virtual bool is_connected() const = 0;
    [[nodiscard]] virtual bool is_offline_session() const = 0;
    [[nodiscard]] virtual bool connection_failed() const = 0;
    [[nodiscard]] virtual std::string player_name() const = 0;

    virtual void notify_game_completed() = 0;
    virtual void notify_death() = 0;
};
