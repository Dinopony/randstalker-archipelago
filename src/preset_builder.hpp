#pragma once

#include <nlohmann/json.hpp>

using nlohmann::json;

json build_preset_json(const json& slot_data, const json& locations_data, const std::string& player_name, bool winter_theme);
