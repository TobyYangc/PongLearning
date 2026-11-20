#include "menu.h"

#include <algorithm>
#include <cctype>
#include <initializer_list>
#include <stdexcept>

#include <utility>

Font uiFont{};

void textAt(const char* text, float x, float y, float size, Color color) {
    DrawTextEx(uiFont, text, {x,y}, size, 1, color);
}

void centered(const char* text, float x, float y, float size, Color color) {
    textAt(text,x-MeasureTextEx(uiFont,text,size,1).x/2,y,size,color);
}

void drawButton(Rectangle r, const char* label, bool selected, bool hovered, float size) {
    DrawRectangleRec(r, selected ? WHITE : BLACK);
    DrawRectangleLinesEx(r, hovered ? 3 : 2, hovered ? SKYBLUE : WHITE);
    centered(label,r.x+r.width/2,r.y+(r.height-size)/2,size,selected ? BLACK : WHITE);
}

namespace {

bool validRecord(const Json& row) {
    if (!row.is_object()) return false;

    const auto timestamp = row.find("timestamp");

    const auto player = row.find("player_score");

    const auto cpu = row.find("cpu_score");
    if (timestamp == row.end() || !timestamp->is_string() ||
        timestamp->get_ref<const std::string&>().size() < 10 ||
        player == row.end() || !player->is_number_integer() ||
        cpu == row.end() || !cpu->is_number_integer()) return false;

    const auto p = player->get<long long>(), c = cpu->get<long long>();

    const auto target = std::max(p,c);
    return row.value("completed",Json(false)) == true && p >= 0 && c >= 0 && p != c &&
           (target == 5 || target == 7 || target == 11) &&
           row.value("winning_score",Json(target)) == target &&
           row.value("winner",Json()) == (p > c ? "player" : "cpu") &&
           (row.value("difficulty",Json()) == "easy" ||
            row.value("difficulty",Json()) == "normal" || row.value("difficulty",Json()) == "hard");
}
}

Menu::Menu(fs::path directory) : directory(std::move(directory)) {
    modes[0].preset = "preset2";
    try {

        const auto settings = readJson(this->directory/"config.json",Json::object());
        if (settings.is_object()) {

            const auto solo = settings.value("pvcpu",settings);
            modes[0] = parseConfig(solo);
            if (!solo.is_object() || (solo.value("input_preset",Json()) != "preset1" &&
                                      solo.value("input_preset",Json()) != "preset2"))
                modes[0].preset = "preset2";
            modes[1] = parseConfig(settings.value("pvp",settings));
            sound = parseConfig(settings).sound;
        }
    } catch (const std::exception&) {
        error = "INVALID SETTINGS - USING DEFAULTS";
    }
    modes[0].mode = "pvcpu";
    modes[1].mode = "pvp";

    try {

        const auto history = readJson(this->directory/"history.json",Json::array());
        if (!history.is_array()) throw std::runtime_error("Invalid history");

        for (const auto& row : history)
            if (validRecord(row)) records.push_back(row);
        std::stable_sort(records.begin(),records.end(),[](const Json& a, const Json& b) {
            return a["timestamp"] > b["timestamp"];
        });
    } catch (const std::exception&) {
        error = "UNABLE TO READ RECORDS";
    }
}

void Menu::saveSettings() {

    const auto& next = draft;

    Json settings{{"sound_enabled",draft.sound},
                  {"pvcpu",configFor(draft.mode == "pvcpu" ? next : modes[0])},
                  {"pvp",configFor(draft.mode == "pvp" ? next : modes[1])}};
    writeJson(directory/"config.json",settings);
    modes[draft.mode == "pvp"] = next;
    sound = draft.sound;
}

void Menu::addResult(const Game& game) {

    const auto row = resultFor(game);

    for (const auto& saved : records)
        if (saved.value("match_id",Json()) == row["match_id"] &&
            saved.value("session_id",Json()) == row["session_id"]) return;

    auto stored = readJson(directory/"history.json",Json::array());
    if (!stored.is_array()) throw std::runtime_error("Invalid history");
    stored.insert(stored.begin(),row);
    writeJson(directory/"history.json",stored);
    records.insert(records.begin(),row);
}

void Menu::draw(Vector2 cursor, bool click, bool input) {

    const auto pressed = [input](int key) { return input && key != KEY_NULL && IsKeyPressed(key); };

    const auto button = [&](Rectangle r, const char* label, int key, bool selected = false, float size = 28) {

        const bool hover = input && CheckCollisionPointRec(cursor,r);
        drawButton(r,label,selected,hover,size);
        return pressed(key) || (hover && click);
    };

    const auto back = [&] {
        page = Page::Menu;
        focus = 0;
        error.clear();
    };
    if (!error.empty()) centered(error.c_str(),640,690,18);

    if (page == Page::Menu) {
        centered("PONG",640,68,120);
        if (pressed(KEY_DOWN) || (pressed(KEY_TAB) && !IsKeyDown(KEY_LEFT_SHIFT))) focus = (focus+1)%4;
        if (pressed(KEY_UP) || (pressed(KEY_TAB) && IsKeyDown(KEY_LEFT_SHIFT))) focus = (focus+3)%4;

        const char* labels[]{"[1]  1 PLAYER","[2]  2 PLAYERS","[R]  RECORDS","[ESC]  EXIT"};

        const int keys[]{KEY_ONE,KEY_TWO,KEY_R,KEY_ESCAPE};

        for (int i=0; i<4; ++i) {

            const Rectangle r = i < 2 ? Rectangle{380,270.0f+84*i,520,66} :
                                        Rectangle{470,475.0f+62*(i-2),340,48};

            const bool hover = input && CheckCollisionPointRec(cursor,r);
            drawButton(r,labels[i],false,hover || focus == i,i < 2 ? 39 : 28);
            if (!pressed(keys[i]) && !(hover && click) && !(focus == i && pressed(KEY_ENTER))) continue;
            if (i < 2) {
                draft = modes[i];
                draft.sound = sound;
                page = Page::Settings;
            } else if (i == 2) {
                scroll = 0;
                page = Page::Records;
            } else page = Page::Exit;
            break;
        }
        return;
    }

    if (page == Page::Settings) {

        const bool solo = draft.mode == "pvcpu";
        centered(solo ? "1 PLAYER" : "2 PLAYERS",640,33,52);

        struct Choice { const char* label; int key; };

        const auto row = [&](const char* title, float y, int selected, std::initializer_list<Choice> choices) {
            textAt(title,59,y+11,25);

            const float width = (928-15*(choices.size()-1))/choices.size();

            int i = 0, result = selected;

            for (const auto& choice : choices) {
                if (button({293+i*(width+15),y,width,50},choice.label,choice.key,i == selected))
                    result = i;
                ++i;
            }
            return result;
        };
        if (solo) {

            const char* difficulties[]{"easy","normal","hard"};
            draft.difficulty = difficulties[row("DIFFICULTY",119,
                draft.difficulty == "easy" ? 0 : draft.difficulty == "hard" ? 2 : 1,
                {{"[E] EASY",KEY_E},{"[N] NORMAL",KEY_N},{"[H] HARD",KEY_H}})];
        } else {
            DrawRectangleLinesEx({59,116,569,118},2,WHITE);
            DrawRectangleLinesEx({653,116,568,118},2,WHITE);
            centered("PLAYER 1 (LEFT)",343,136,28);
            centered("PLAYER 2 (RIGHT)",937,136,28);
            centered("W / S",343,183,30);
            centered("UP / DOWN",937,183,30);
        }

        const int targets[]{5,7,11};
        draft.target = targets[row("PLAY TO",solo ? 190 : 274,
            draft.target == 5 ? 0 : draft.target == 11 ? 2 : 1,
            {{"[1] 5",KEY_ONE},{"[2] 7",KEY_TWO},{"[3] 11",KEY_THREE}})];
        if (solo) {
            draft.preset = row("CONTROLS",260,draft.preset == "preset1" ? 1 : 0,
                {{"[A] ARROW KEYS",KEY_A},{"[W] W / S",KEY_W}}) == 0 ? "preset2" : "preset1";
            draft.side = row("YOUR SIDE",330,draft.side == "right",
                {{"[L] LEFT",KEY_L},{"[R] RIGHT",KEY_R}}) == 0 ? "left" : "right";
        }
        draft.sound = row("SOUND",solo ? 400 : 354,!draft.sound,
            {{"[O] ON",KEY_O},{"[F] OFF",KEY_F}}) == 0;

        const char* serve[]{"loser","scorer","random"};
        draft.serve = serve[row("SERVE TO",solo ? 462 : 422,
            draft.serve == "scorer" ? 1 : draft.serve == "random" ? 2 : 0,
            {{"[U] LOSER",KEY_U},{"[G] SCORER",KEY_G},{"[X] RANDOM",KEY_X}})];
        if (button({455,573,370,58},"[ENTER] START",KEY_ENTER,false,31)) {
            try {
                saveSettings();
                error.clear();
                page = Page::Match;
            } catch (const std::exception&) {
                error = "UNABLE TO SAVE SETTINGS - TRY AGAIN";
            }
        } else if (pressed(KEY_ESCAPE) || (input && click && CheckCollisionPointRec(cursor,{540,646,200,42}))) {
            back();
        }
        centered("[ESC] BACK",640,654,23);
        return;
    }

    centered("RECORDS",640,35,51);

    const int count = static_cast<int>(records.size());

    const int wins = static_cast<int>(std::count_if(records.begin(),records.end(),[](const Json& row) {
        return row["winner"] == "player";
    }));

    const char* stats[]{"GAMES","WINS","LOSSES","WIN RATE"};

    const std::string values[]{std::to_string(count),std::to_string(wins),std::to_string(count-wins),
        count ? TextFormat("%.1f%%",100.0*wins/count) : "-"};

    for (int i=0; i<4; ++i) {

        const float x = 75.0f+288*i;
        DrawRectangleLinesEx({x,122,267,118},2,GRAY);
        centered(stats[i],x+133,140,24);
        centered(values[i].c_str(),x+133,178,36);
    }

    const int cols[]{220,530,810,1060};

    const char* headers[]{"DATE","DIFFICULTY","FINAL SCORE","RESULT"};

    for (int i=0; i<4; ++i) centered(headers[i],cols[i],283,24);

    const int last = std::max(0,count-3);
    if (input && page == Page::Records) {

        const float wheel = GetMouseWheelMove();
        scroll = std::clamp(scroll+(pressed(KEY_DOWN) || wheel < 0)-(pressed(KEY_UP) || wheel > 0),0,last);
    }

    for (int i=0; i<=3; ++i) DrawLine(75,326+i*62,1206,326+i*62,DARKGRAY);

    for (int i=0; i<3 && i+scroll<count; ++i) {

        const auto& record = records.at(i+scroll);

        auto difficulty = record["difficulty"].get<std::string>();

        for (char& c : difficulty) c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));

        const float y = 345.0f+i*62;
        centered(record["timestamp"].get<std::string>().substr(0,10).c_str(),cols[0],y,24);
        centered(difficulty.c_str(),cols[1],y,24);
        centered(TextFormat("%d : %d",record["player_score"].get<int>(),record["cpu_score"].get<int>()),cols[2],y,24);
        centered(record["winner"] == "player" ? "WIN" : "LOSS",cols[3],y,24);
    }
    if (!count) centered("NO RECORDS YET",640,408,26);
    if (last) {

        constexpr float track = 186;

        const float thumb = std::max(24.0f,track*3/count);
        DrawRectangleRec({1217,326,10,track},DARKGRAY);
        DrawRectangleRec({1217,326+(track-thumb)*scroll/last,10,thumb},LIGHTGRAY);
    }
    centered("ALL MATCHES",640,537,19,LIGHTGRAY);

    if (page == Page::Records) {

        const Rectangle clear{325,602,340,65};
        if (!count) {
            DrawRectangleLinesEx(clear,2,DARKGRAY);
            centered("[C] CLEAR RECORDS",495,620,28,GRAY);
        }
        if (count && button(clear,"[C] CLEAR RECORDS",KEY_C)) {
            page = Page::ClearRecords;
            focus = 0;
        } else if (button({695,602,260,65},"[ESC] BACK",KEY_ESCAPE)) back();
        return;
    }
    DrawRectangle(0,0,W,H,Fade(BLACK,.68f));
    DrawRectangleRec({410,226,460,268},Color{21,21,21,255});
    DrawRectangleLinesEx({410,226,460,268},2,GRAY);
    centered("CLEAR ALL RECORDS?",640,253,39);
    if (pressed(KEY_TAB) || pressed(KEY_UP) || pressed(KEY_DOWN)) focus = 1-focus;
    if (button({474,326,332,54},"CANCEL",KEY_ESCAPE,focus == 0) || (focus == 0 && pressed(KEY_ENTER))) {
        page = Page::Records;
    } else if (button({474,400,332,54},"CLEAR",KEY_NULL,focus == 1) || (focus == 1 && pressed(KEY_ENTER))) {
        try {
            writeJson(directory/"history.json",Json::array());
            records = Json::array();
            error.clear();
            scroll = 0;
            page = Page::Records;
        } catch (const std::exception&) {
            error = "UNABLE TO CLEAR RECORDS - TRY AGAIN";
            page = Page::Records;
        }
    }
}
