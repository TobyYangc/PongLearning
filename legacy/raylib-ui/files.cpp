#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

#include "files.h"

#include <algorithm>
#include <ctime>

#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <stdexcept>

Config loadConfig(const fs::path& path) {
    try { return parseConfig(readJson(path, Json::object())); }

    catch (const std::exception&) { return {}; }
}

Config parseConfig(const Json& j) {

    Config cfg;
    try {
        if (!j.is_object()) return cfg;

        const auto option = [&](const char* key, std::string& value,

                                std::initializer_list<const char*> allowed) {
            if (!j.contains(key) || !j[key].is_string()) return;

            const auto candidate = j[key].get<std::string>();
            if (std::find(allowed.begin(), allowed.end(), candidate) != allowed.end())
                value = candidate;
        };
        option("mode", cfg.mode, {"pvp", "pvcpu"});
        option("difficulty", cfg.difficulty, {"easy", "normal", "hard"});
        option("player_side", cfg.side, {"left", "right"});
        option("input_preset", cfg.preset, {"preset1", "preset2"});
        option("serve_to", cfg.serve, {"loser", "scorer", "random"});
        if (j.contains("winning_score") && j["winning_score"].is_number_integer() &&
            (j["winning_score"] == 5 || j["winning_score"] == 7 || j["winning_score"] == 11))
            cfg.target = j["winning_score"].get<int>();
        if (j.contains("sound_enabled") && j["sound_enabled"].is_boolean())
            cfg.sound = j["sound_enabled"].get<bool>();
        if (j.contains("session_id") && j["session_id"].is_string())
            cfg.session = j["session_id"].get<std::string>();
    } catch (const Json::exception& e) {
        std::cerr << "Config fallback: " << e.what() << '\n';
    }
    return cfg;
}

Json readJson(const fs::path& path, const Json& fallback) {
    if (!fs::exists(path)) return fallback;

    std::ifstream in(path, std::ios::binary);
    return Json::parse(in);
}

Json configFor(const Config& cfg) {
    return {{"mode",cfg.mode}, {"difficulty",cfg.difficulty}, {"player_side",cfg.side},
            {"input_preset",cfg.preset}, {"serve_to",cfg.serve},
            {"winning_score",cfg.target}, {"sound_enabled",cfg.sound}};
}

void writeJson(const fs::path& path, const Json& value) {

    fs::path temp = path;
    temp += L".tmp";

    std::ofstream out(temp, std::ios::binary);
    out << value.dump(2) << '\n';
    out.close();
    if (!out || !MoveFileExW(temp.c_str(), path.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
        throw std::runtime_error("Could not save " + path.filename().string());
}

void writeResult(const fs::path& path, const Json& matches) {

    Json result = matches.back();
    result["matches"] = matches;
    writeJson(path, result);
}

Json resultFor(const Game& game) {

    const std::time_t now = std::time(nullptr);

    const std::tm* utc = std::gmtime(&now);
    if (!utc) throw std::runtime_error("Could not read clock");

    std::ostringstream iso;
    iso << std::put_time(utc, "%Y-%m-%dT%H:%M:%SZ");

    const int p = game.human();
    return Json{{"completed",true}, {"session_id",game.cfg.session},
                {"match_id",game.cfg.session + "-" + std::to_string(game.roundId)},
                {"timestamp",iso.str()}, {"difficulty",game.cfg.difficulty},
                {"player_side",game.cfg.side}, {"winner",game.last == p ? "player" : "cpu"},
                {"player_score",game.score[p]}, {"cpu_score",game.score[1-p]},
                {"winning_score",game.cfg.target}};
}
