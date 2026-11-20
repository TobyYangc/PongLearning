#pragma once

#include "game.h"

#include "json.hpp"

#include <filesystem>
using Json = nlohmann::json;
namespace fs = std::filesystem;

Config loadConfig(const fs::path& path);

void writeResult(const fs::path& path, const Json& matches);

Json resultFor(const Game& game);

Config parseConfig(const Json& value);

Json configFor(const Config& cfg);

Json readJson(const fs::path& path, const Json& fallback);

void writeJson(const fs::path& path, const Json& value);
