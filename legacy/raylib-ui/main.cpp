#include "raylib.h" //引入raylib的視窗、繪圖、輸入與音訊等宣告
#include "audio.h"  //引入Audio型別，供主程式管理與播放音效
#include "menu.h"   //引入Page、Menu、共用字型與介面繪製函式宣告

#define WIN32_LEAN_AND_MEAN //減少windows.h引入的部分非必要宣告

#ifndef NOMINMAX //若尚未定義NOMINMAX
#define NOMINMAX //停用Windows的min/max巨集，避免干擾std::min/std::max
#endif

#define NOGDI  //略過部分Windows GDI宣告，避免與raylib名稱衝突
#define NOUSER //略過部分Windows視窗與使用者介面宣告
#include <windows.h> //引入本程式需要的Windows API型別與函式宣告
#undef NOUSER //取消此巨集定義；不會補回剛才略過的宣告

/*
 * 手動宣告Windows的SystemParametersInfoW函式，後面用來查詢螢幕工作區域。
 * 前面的NOUSER會略過其標頭宣告，因此在這裡補上。
 * extern "C"：採 C 語言連結方式。
 * dllimport：函式實作由 Windows DLL 提供。
 * BOOL：回傳成功或失敗；WINAPI：指定 Windows API 呼叫慣例。
 * 四個參數依序為操作代碼、額外數值、資料指標、操作旗標。
 */
extern "C" __declspec(dllimport) BOOL WINAPI SystemParametersInfoW(UINT, UINT, PVOID, UINT);

#include <algorithm> //提供min、max、clamp、排序及計數等功能
#include <cmath> //提供ceil、sin、cos等數學函式
#include <cstdlib> //提供getenv等環境變數讀取功能
#include <limits> //查詢型別的數值範圍，例如unsigned的最大值
#include <fstream> //提供檔案讀寫串流，例如讀字型、寫入log
#include <chrono> //提供時間功能，本程式用時間計數產生對局識別
#include <memory> //提供unique_ptr，自動管理資源的生命週期
#include <vector> //提供可變長度陣列，保存字型資料、按鈕文字等

int wmain(int argc, wchar_t** argv) { //Windows寬字元入口，接收命令列參數

    //從程式路徑取得父資料夾，作為存檔位置的備援
    const fs::path base = fs::absolute(fs::path(argv[0])).parent_path();

    //讀取使用者本機資料目錄；找不到時為空指標
    const wchar_t* appData = _wgetenv(L"LOCALAPPDATA");

    //優先使用--data-dir指定的路徑，其次LocalAppData/Pong，最後使用base
    const fs::path data = argc == 3 && std::wstring(argv[1]) == L"--data-dir" ?
        fs::path(argv[2]) : (appData ? fs::path(appData)/L"Pong" : base);

    std::error_code error; //接收檔案系統操作的錯誤
    fs::create_directories(data,error); //確保存檔資料夾存在
    if (error) return 1; //建立失敗，結束程式

    const auto lockPath = data/L"pong.lock";

    auto closeLock = [](void* handle) { CloseHandle(handle); };

    const auto handle = CreateFileW(lockPath.c_str(), GENERIC_WRITE, 0, nullptr,
                                    OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (handle == INVALID_HANDLE_VALUE) return 1;
    const std::unique_ptr<void, decltype(closeLock)> lock(handle, closeLock);

    Menu menu(data);

    Config config;
    std::ofstream log(data/L"game.log", std::ios::app);

    unsigned seed = std::random_device{}();
    if (const char* raw = std::getenv("PONG_TEST_SEED")) {
        try {

            std::size_t count = 0;

            const unsigned long value = std::stoul(raw, &count);
            if (*raw >= '0' && *raw <= '9' && raw[count] == '\0' &&
                value <= std::numeric_limits<unsigned>::max())
                seed = static_cast<unsigned>(value);
        } catch (const std::exception&) {
            log << "Ignoring invalid PONG_TEST_SEED\n";
        }
    }

    Game game(config, seed);

    bool closeAfterLeave = false;

    bool dirty = false;

    auto commit = [&]() {
        if (!dirty) return;
        try {
            menu.addResult(game);
            dirty = false;
        } catch (const std::exception& e) {
            log << e.what() << '\n';
        }
    };

    RECT work{};

    float startScale = 1;
    if (SystemParametersInfoW(0x0030,0,&work,0)) {
        startScale = std::clamp(std::min((work.right-work.left-32)/1280.0f,
                                         (work.bottom-work.top-64)/720.0f), .5f, 1.0f);
    }

    SetConfigFlags(FLAG_WINDOW_RESIZABLE);

    InitWindow(static_cast<int>(W*startScale), static_cast<int>(H*startScale), "Pong");
    SetWindowMinSize(640,360);

    SetExitKey(KEY_NULL);
    SetTargetFPS(60);

    const auto fontPath = fs::path(_wgetenv(L"WINDIR") ? _wgetenv(L"WINDIR") : L"C:/Windows")
                          /L"Fonts"/L"consolab.ttf";

    std::ifstream fontFile(fontPath,std::ios::binary);
    const std::vector<unsigned char> fontBytes((std::istreambuf_iterator<char>(fontFile)),{});
    uiFont = fontBytes.empty() ? GetFontDefault() :
        LoadFontFromMemory(".ttf",fontBytes.data(),static_cast<int>(fontBytes.size()),96,nullptr,0);
    SetTextureFilter(uiFont.texture,TEXTURE_FILTER_BILINEAR);

    auto target = LoadRenderTexture(W,H);
    SetTextureFilter(target.texture,TEXTURE_FILTER_BILINEAR);

    bool running = true, blocked[4]{};

    const int moveKeys[]{KEY_W, KEY_S, KEY_UP, KEY_DOWN};

    float acc = 0;

    int focus = 0;
    {

        std::unique_ptr<Audio> audio;

        unsigned matchNumber = 0;
        while (running) {

            const float frame = std::min(GetFrameTime(), 8*Step);

            const float scale = std::min(GetScreenWidth()/1280.0f, GetScreenHeight()/720.0f);

            const Rectangle dest{(GetScreenWidth()-W*scale)/2, (GetScreenHeight()-H*scale)/2,
                                 W*scale,H*scale};

            const auto mouse = GetMousePosition();

            const Vector2 cursor{(mouse.x-dest.x)/scale,(mouse.y-dest.y)/scale};

            const bool click = CheckCollisionPointRec(mouse,dest) && IsMouseButtonPressed(MOUSE_BUTTON_LEFT);

            const bool close = WindowShouldClose();

            const bool esc = IsKeyPressed(KEY_ESCAPE);

            const bool focused = IsWindowFocused();

            const auto present = [&] {
                EndTextureMode();
                BeginDrawing();
                ClearBackground(BLACK);
                DrawTexturePro(target.texture,{0,0,W,-H},dest,{0,0},0,WHITE);
                EndDrawing();
            };

            if (menu.page != Page::Match) {
                if (close) break;
                BeginTextureMode(target);
                ClearBackground(BLACK);
                menu.draw(cursor,click,focused);
                present();
                if (menu.page == Page::Exit) break;
                if (menu.page == Page::Match) {
                    config = menu.draft;

                    const auto stamp = std::chrono::system_clock::now().time_since_epoch().count();
                    config.session = std::to_string(stamp) + "-" + std::to_string(GetCurrentProcessId()) +
                                     "-" + std::to_string(++matchNumber);
                    game = Game(config,seed+matchNumber-1);
                    audio = std::make_unique<Audio>(config.sound);
                    acc = 0;
                    focus = 0;
                    closeAfterLeave = false;

                    for (int i=0; i<4; ++i) blocked[i] = IsKeyDown(moveKeys[i]);
                }
                continue;
            }

            const auto prev = game.now_phase;
            if (!focused) game.pause();
            if (close) {
                if (game.now_phase == Phase::Over && !dirty) break;
                if (game.now_phase != Phase::Over) {
                    game.pause();
                    game.now_phase = Phase::Leave;
                    closeAfterLeave = true;
                    focus = 0;
                }
            } else if (focused && esc) {
                if (game.now_phase == Phase::Leave) {
                    game.now_phase = Phase::Paused;
                    focus = 0;
                } else if (game.now_phase != Phase::Paused) {
                    game.pause();
                }
            }
            if (prev != game.now_phase) { acc = 0; focus = 0; }
            if (!focused || game.now_phase == Phase::Paused || game.now_phase == Phase::Leave) {

                for (int i=0; i<4; ++i) blocked[i] = IsKeyDown(moveKeys[i]);
            } else {

                for (int i=0; i<4; ++i) if (!IsKeyDown(moveKeys[i])) blocked[i] = false;
            }

            const bool paused = game.now_phase == Phase::Paused;

            const bool leave = game.now_phase == Phase::Leave;

            const bool over = game.now_phase == Phase::Over;

            const bool overlay = paused || leave || over;

            std::vector<const char*> labels;

            const char* back = "MAIN MENU";
            if (paused) labels = {"RESUME","RESTART",back};
            if (leave) labels = {"CANCEL","LEAVE"};
            if (over) labels = dirty ? std::vector<const char*>{"RETRY SAVE"} :
                                       std::vector<const char*>{"PLAY AGAIN",back};

            const auto button = [over](int i) {
                return over ? Rectangle{385,426.0f+104*i,510,75} :
                              Rectangle{474,289.0f+79*i,332,57};
            };

            int action = -1;
            if (overlay && focused && prev == game.now_phase && !close && !esc) {

                const int n = static_cast<int>(labels.size());

                const bool tab = IsKeyPressed(KEY_TAB);

                const bool shift = IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT);
                if ((tab && shift) || IsKeyPressed(KEY_UP)) focus = (focus+n-1)%n;
                else if (tab || IsKeyPressed(KEY_DOWN)) focus = (focus+1)%n;
                if (IsKeyPressed(KEY_ENTER)) action = focus;

                for (int i=0; click && i<n; ++i)
                    if (CheckCollisionPointRec(cursor,button(i))) action = i;
            }
            if (action >= 0) {
                acc = 0;
                focus = 0;
                if (paused && action == 0) game.resume();
                else if (paused && action == 1) game.restart();
                else if (paused) { game.now_phase = Phase::Leave; closeAfterLeave = false; }
                else if (leave && action == 0) game.now_phase = Phase::Paused;
                else if (leave && closeAfterLeave) running = false;
                else if (leave) menu.page = Page::Menu;
                else if (dirty) commit();
                else if (over && action == 0) game.restart();
                else menu.page = Page::Menu;
            }

            if (!running || menu.page != Page::Match) {
                audio.reset();
                if (!running) break;
                BeginTextureMode(target);
                ClearBackground(BLACK);
                menu.draw(cursor, false, false);
                present();
                continue;
            }

            if (!overlay && focused && !close && !esc) {

                const auto held = [&](int i) { return !blocked[i] && IsKeyDown(moveKeys[i]); };

                int left = held(1)-held(0), right = held(3)-held(2);
                if (config.mode == "pvcpu") {

                    const int input = config.preset == "preset1" ? left : right;
                    left = game.human() == 0 ? input : 0;
                    right = game.human() == 1 ? input : 0;
                }

                int events = None;
                acc = std::min(acc+frame, 8*Step);

                for (int n=0; n<8 && acc+1e-7f>=Step; ++n) {
                    acc = std::max(0.0f,acc-Step);
                    events |= game.tick(Step,left,right);
                    if (game.now_phase == Phase::Over) {
                        if (config.mode == "pvcpu" && !config.session.empty()) {
                            dirty = true;
                            commit();
                        }
                        acc = 0;
                        focus = 0;
                        break;
                    }
                }
                audio->play(events);
            }
            if (game.now_phase == Phase::Over) game.timer = std::max(0.0f,game.timer-frame);

            const auto scene = (paused || leave) ? game.bf_pause_phase : game.now_phase;

            const float intro = scene == Phase::Intro ? std::clamp((2.25f-game.timer)/.25f,0.0f,1.0f) : 1;

            const bool versus = config.mode == "pvcpu";

            const int p = game.human();

            const char* names[2]{"PLAYER 1","PLAYER 2"};
            if (versus) {
                names[0] = p == 0 ? "YOU (LEFT)" : "COMPUTER";
                names[1] = p == 1 ? "YOU (RIGHT)" : "COMPUTER";
            }
            BeginTextureMode(target);
            ClearBackground(BLACK);
            if (scene != Phase::Over) {

                for (int y=135; y<658; y+=36) DrawRectangle(W/2-1,y,3,18,Fade(DARKGRAY,intro));

                for (int i=0; i<2; ++i) {

                    const Rectangle paddle{game.bats[i].x-9,game.bats[i].y-Half,18,2*Half};
                    DrawRectangleRec(paddle,Fade(WHITE,intro));
                    if (game.hitTimer > 0 && game.lastBat == i) DrawRectangleLinesEx(paddle,3,LIGHTGRAY);
                }
                if ((scene != Phase::Serve && scene != Phase::Resume) || overlay) {

                    for (int i=0; i<2; ++i) {
                        centered(names[i],i ? 881 : 399,34,36,Fade(WHITE,intro));

                        const Color scoreColor = scene == Phase::Point && game.timer > .13f &&
                            game.last == i ? WHITE : LIGHTGRAY;

                        const float size = scene == Phase::Point && game.timer > .13f &&
                            game.last == i ? 49 : 44;
                        centered(TextFormat("%d",game.score[i]),i ? 686 : 594,53-size/2,size,Fade(scoreColor,intro));
                    }
                    centered(":",640,31,44,Fade(WHITE,intro));
                    centered(TextFormat("FIRST TO %d",config.target),640,93,22,Fade(WHITE,intro));
                }
                if (!overlay) centered("ESC PAUSE",640,674,22,Fade(WHITE,intro));
                DrawCircleV({game.ball.x,game.ball.y},Radius,Fade(RAYWHITE,intro));
                if (scene == Phase::Intro && game.timer <= 2) {
                    if (versus) {
                        centered(p ? "YOU ARE ON THE RIGHT" : "YOU ARE ON THE LEFT",640,245,30);
                        centered(config.preset=="preset1" ? "W / S TO MOVE" : "UP / DOWN TO MOVE",
                                 p ? 1040 : 240,375,24);
                    } else {
                        centered("PLAYER 1",220,314,28);
                        centered("PLAYER 2",1060,314,28);
                        centered("W / S",220,365,32);
                        centered("UP / DOWN",1060,365,32);
                    }
                }
                if (!overlay && (scene == Phase::Serve || scene == Phase::Resume)) {

                    const float remaining = game.timer;

                    const int number = std::clamp(static_cast<int>(std::ceil(remaining)),1,3);

                    const float pulse = .55f+.45f*(remaining-(number-1));
                    DrawRectangle(582,199,116,117,BLACK);
                    centered(TextFormat("%d",number),640,200,102,Fade(WHITE,pulse));
                }
            }

            if (paused || leave) {
                DrawRectangle(0,0,W,H,Fade(BLACK,.34f));

                const Rectangle panel{410,172,460,leave ? 307.0f : 383.0f};
                DrawRectangleRec(panel,Color{21,21,21,255});
                DrawRectangleLinesEx(panel,3,LIGHTGRAY);
                centered(leave ? "LEAVE MATCH?" : "PAUSED",640,202,48);
            }
            if (over) {

                const char* winner = versus ? (game.last == p ? "YOU WIN" : "COMPUTER WINS") :
                                              (game.last == 0 ? "PLAYER 1 WINS" : "PLAYER 2 WINS");

                const float opacity = 1-game.timer/.18f;
                centered(winner,640,119,61,Fade(WHITE,opacity));
                centered("FINAL SCORE",640,250,34);
                centered(TextFormat("%d : %d",game.score[0],game.score[1]),640,304,60);
                if (dirty) centered("UNABLE TO SAVE RESULT",640,391,24);
            }

            for (int i=0; i<static_cast<int>(labels.size()); ++i) {

                const auto r = button(i);
                drawButton(r,labels[i],i == focus,CheckCollisionPointRec(cursor,r),over ? 39 : 28);
            }
            present();

        }

    }

    if (uiFont.texture.id != GetFontDefault().texture.id) UnloadFont(uiFont);

    UnloadRenderTexture(target);
    CloseWindow();

    return 0;

}
