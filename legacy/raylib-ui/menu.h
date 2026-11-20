#pragma once

#include "files.h"

#include "raylib.h"

enum class Page { Menu, Settings, Records, ClearRecords, Match, Exit };

extern Font uiFont;

void textAt(const char* text, float x, float y, float size, Color color = WHITE);

void centered(const char* text, float x, float y, float size, Color color = WHITE);

void drawButton(Rectangle bounds, const char* label, bool selected, bool hovered, float size = 28);

struct Menu {

    explicit Menu(fs::path directory);

    void saveSettings();

    void addResult(const Game& game);

    void draw(Vector2 cursor, bool click, bool input);

    Page page = Page::Menu;

    Config draft;

    bool sound = true;
private:

    fs::path directory;

    Config modes[2];

    Json records = Json::array();

    int focus = 0;

    int scroll = 0;

    std::string error;
};
