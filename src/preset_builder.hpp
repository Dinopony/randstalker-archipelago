#pragma once

#include <nlohmann/json.hpp>

using nlohmann::json;

json build_preset_json(const json& slot_data, const std::string& player_name);
