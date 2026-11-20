# Pong完整程式附錄A｜C++核心與DLL

[主文](https://hackmd.io/tGkm7Yn-TnSZjNuPpCFwDg)｜[C++附錄](https://hackmd.io/43y2pN6dSZKgqRBGcmbcHg)｜[Python附錄](https://hackmd.io/46KzKWO9TR2zgItJh1M20g)

每個標題是一個完整檔案。同一檔案若有多個程式區塊，依序接起來。

## `game.h`

```cpp
#pragma once
#include <random>
#include <string>

constexpr int W = 1280, H = 720;//遊戲視窗運算尺寸w=寬,h=高，縮放不影響計算

constexpr float Half = 56, Radius = 10, Step = 1.0f / 120;//Half指拍子長度的一半,Radius=球半徑,Step=每次物理更新的固定秒數

//config=configuration配置縮寫；遊戲配置設定struct
struct Config {

    std::string mode = "pvp", difficulty = "normal", side = "left";
    //mode：pvp/pvcpu；difficulty：easy/normal/hard；side：玩家在 left/right
    std::string preset = "preset1", serve = "loser", session;
    //preset：preset1=W/S、preset2=上下方向鍵；serve(發球)：朝向loser/scorer(得分者)/random；session：執行階段識別
    int target = 7;//預設單局結束分數：7分
    bool sound = true;//聲音開啟
};

enum class Phase { Intro, Serve, Rally, Point, Paused, Leave, Resume, Over };
/*
enum class 強型別列舉：作用域安全性較高，不會汙染命名空間；無隱式整數轉換
Intro開場、Serve發球倒數、Rally對打中、Point得分後停留 Paused暫停、Leave離開確認、Resume恢復倒數、Over結算
用 |= 累積事件，用&判斷某事件是否發生(後面code有)
*/
enum Event { None = 0, Wall = 1, Paddle = 2, Score = 4 };
/*記錄「這次更新發生了哪些事」：None=無,Wall=球撞牆,Paddle=球碰到球拍,Score=有人得分
--------------------------------------------------------------
事件各占一個位元，使用 1、2、4，才能組合保存多個事件。
名稱      十進位   二進位
None      0       000
Wall      1       001
Paddle    2       010
Score     4       100

使用 | 可以把事件合併：
    Wall | Paddle → 001 | 010 = 011，也就是十進位的 3。
因此 Score 使用 4；若設成 3，就會和「撞牆＋碰拍」混淆。

使用 & 可以檢查其中一種事件：
    events & Wall：結果非 0，表示這次更新包含撞牆。

為什麼不用 ||？
    || 只回答「是否至少有一項成立」，結果只有 true 或 false。
    合併後就無法知道到底是撞牆、碰拍，還是兩者都有。

這樣設計的好處是，一個變數就能保存多種事件，方便後續播放音效。
使用時要留意：
    1. 新增獨立事件時，繼續使用 8、16、32……等尚未占用的位元。
    2. 檢查是否包含某事件時用 &；使用 == 會要求整個組合完全相同。

*/
struct Body { float x, y; };//座標位置
/*座標系
----------→X
|
|
↓
Y
*/
struct Game {

    Config cfg;//此局遊戲設定檔
    std::mt19937 rng;
    //rng 是 random number generator 縮寫，為亂數引擎物件；建構子以 seed 初始化
    Phase now_phase = Phase::Intro, bf_pause_phase = Phase::Intro;
    //now_phase是目前階段；bf_pause_phase是暫停前階段，用來接回原流程
    Body bats[2]{{48, H/2}, {W-48, H/2}}, ball{W/2, H/2};
    //bats[0]/bats[1]分別是左右拍心；ball是球心，起始都在場地中線高度
    float vx = 0, vy = 0, timer = 2.25f;
    // vx/vy：球速像素/秒，正右/正下；timer：目前階段剩餘秒數
    //Intro階段前0.25秒，球場、球拍和比分逐漸亮起。後2秒，顯示玩家側別和操作按鍵。
    float cpuClock = 0, cpuTarget = H/2, hitTimer = 0;
    //cpuClock：距上次觀察秒數；cpuTarget：記住的目標 y；hitTimer：命中效果剩餘秒數。
    bool cpuObserved = false;//是否已有有效的來球觀察，離去球時清除
    int score[2]{0, 0}, last = -1, lastBat = -1, roundId = 1;
    //score[0/1]：左右比分；last：最後得分方(-1:沒人得分、0左方得分、1右方得分)，-1 代表尚無；lastBat：最近碰拍；roundId：局編號
    explicit Game(Config c, unsigned seed);
    //建構子參數c是本局設定，seed=亂數種子；explicit要求明確建立 Game。。
    int human() const;//回傳單人玩家側索引，0左1右；const表此查詢不改物件
    void resetPoint();//準備新球並進三秒發球倒數，保留本局比分
    void launch();//依發球規則給球初速並切進對打
    void restart();//清分、增加局編號並準備新球
    void pause();//保存目前階段後進暫停，保留位置、速度及計時
    void resume();//依bf_pause_phase恢復；原為Rally則先倒數再接原球
    int tick(float dt, int left, int right);
    //dt是秒數，left/right是-1向上0無1向下；回傳本步事件的位元集合
};
```

## `game.cpp`

```cpp
#include "game.h"
#include "raylib.h"
#include <algorithm>
#include <numbers>
#include <cmath>
#include <utility>

constexpr float Rad = std::numbers::pi_v<float> / 180.0f;

// 建構子：c接收一份設定副本，移入cfg；seed用來初始化亂數引擎 rng
Game::Game(Config c, unsigned seed)
     : cfg(std::move(c)), rng(seed) {}

// human()：依cfg.side回傳玩家的球拍索引，左0右1
int Game::human() const {
     return cfg.side == "right" ? 1 : 0;
}


void Game::resetPoint() {
    ball = {W/2, H/2}; //球回到場地中央
    bats[0].y = bats[1].y = H/2; // 左右拍回中央
    vx = vy = 0; //球靜止，等待發球倒數
    cpuTarget = H/2; //清除上一球的追球目標資料，設回中央
    cpuClock = hitTimer = 0;//CPU觀察計時歸零，並結束上一球的擊拍亮框
    cpuObserved = false; //清除上一球的觀察結果，等待重新觀察來球
    now_phase = Phase::Serve; //切換到發球倒數階段
    timer = 3;  //倒數三秒，歸零後發球
}

/*
 * 發球方向和角度
 * 首次隨機；已有上一球時依serve朝得分方、失分方或隨機。
 * 以570px/s的總速率用sin/cos拆成vx、vy向量，最後切換為Rally。
 */

void Game::launch() {
    std::uniform_int_distribution<int> coin(0, 1);
    std::uniform_real_distribution<float> angle(12, 25);
    int toward = coin(rng);//發球朝向，0左1右
    if (last >= 0 && cfg.serve != "random") 
        toward = cfg.serve == "scorer" ? last : 1-last;
        /*
        last   last   1-last    toward
        0      0      1         1右方
        1      1      0         0左方
        */
    const float a = angle(rng) * Rad * (coin(rng) ? 1 : -1);
    /*
    coin抽出值   條件結果   結果
    1            真        +1朝下 
    0            假        -1朝上
    */
    vx = (toward == 1 ? 1 : -1) * 570 * std::cos(a);
    vy = 570 * std::sin(a);
    now_phase = Phase::Rally;
}
/*
    (留自己看)
    三元運算子?:
    條件 ? 成立時使用的值 : 不成立時使用的值
*/

/*
 * 重開比賽
 * 增加roundId、重置比分和最後得分方，再準備新球。
 */
void Game::restart() {
    ++roundId;
    score[0] = score[1] = 0;
    last = -1; //-1:沒人得分
    resetPoint();
}

/*
 * 暫停只保存原階段並切狀態。
 * 球的位置、速度、timer保留，之後tick會直接返回而不推進。
 */
void Game::pause() {
    if (now_phase == Phase::Paused || now_phase == Phase::Leave || now_phase == Phase::Over) 
    return;
    //排除 已經暫停、正在顯示離開確認、對局已經結束 狀況
    bf_pause_phase = now_phase;
    now_phase = Phase::Paused;
}


/*
 * 恢復規則
 * 從Rally(對戰中)暫停時先用resume倒數三秒，再沿原球繼續；
 * 其他階段直接回bf_pause_phase，保留原本剩餘計時。
 */
void Game::resume() {
    if (bf_pause_phase == Phase::Rally) {
        timer = 3;//倒數三秒
        now_phase = Phase::Resume;
    } else {
        now_phase = bf_pause_phase;
    }
}

int Game::tick(float dt, int left, int right) {

    if (now_phase == Phase::Intro || now_phase == Phase::Point || now_phase == Phase::Resume) {
    //若phase為	開場漸亮與操作提示結束、得分後短暫停留結束、暫停恢復前的三秒倒數結束
        timer = std::max(0.0f, timer - dt);//扣掉這次更新經過的時間，最低為0
        if (timer <= 0.00001f) {  
            if (now_phase == Phase::Resume) now_phase = Phase::Rally;// 恢復原球，保留位置與速度
            else resetPoint();
        }
        return None;
    }
    if (now_phase != Phase::Serve && now_phase != Phase::Rally) return None;
    hitTimer = std::max(0.0f, hitTimer - dt);
    float maxSpeed[2]{650, 650};// 左右球拍的速度上限，單位px/s；索引0是左拍，1是右拍
    float move[2]{left * 650.0f, right * 650.0f};// 這次更新中，左右球拍想使用的速度；負值向上，正值向下，0不動

    if (cfg.mode == "pvcpu") {
    // 單人模式，由CPU接手其中一支球拍
        const int ai = 1 - human();//玩家索引0/1，另一側就是CPU
        const bool incoming = now_phase == Phase::Rally && (ai == 1 ? vx > 0 : vx < 0);
        // 正在對打，而且球朝CPU飛：右側看 vx>0，左側看 vx<0
        const float interval = cfg.difficulty == "easy" ? .28f : cfg.difficulty == "hard" ? .09f : .16f;
        // CPU 觀察間隔，單位秒：Easy 0.28、Hard 0.09、Normal 0.16

        maxSpeed[ai] = cfg.difficulty == "easy" ? 300 : cfg.difficulty == "hard" ? 520 : 430;
        // CPU最高移速，單位px/s：Easy 300、Hard 520、Normal 430

        move[ai] = 0;// 每步先預設停住，需追球時才設定移動速度

        if (now_phase == Phase::Rally) {
            if (!incoming) cpuObserved = false;// 球已飛離，清除舊觀察的有效狀態
            cpuClock += dt;// 累積距離上次觀察經過的秒數

            if (cpuClock >= interval) {
                cpuClock -= interval; // 到觀察時間，保留超出的零碎秒數
                if (incoming) {
                    cpuTarget = ball.y;  //記住球心y，作為追球目標
                    cpuObserved = true; // 已取得可用的來球觀察結果
                }
            }

            const float diff = cpuTarget - bats[ai].y; //正值往下追，負值往上追
            if (incoming && cpuObserved && std::abs(diff) > .05f * (2*Half))
            // 有來球、有觀察結果，且距離超過拍高的5%（5.6px）才移動
                move[ai] = std::clamp(diff / dt, -maxSpeed[ai], maxSpeed[ai]);
                // 算出本步到達目標所需速度，再限制於 CPU 的正負速度上限
        }
    }

    float startY[2]{}; // 保存左右拍心在本步開始時的y

    for (int i = 0; i < 2; ++i) {

        const float old = bats[i].y;// 記下這支球拍移動前的位置
        bats[i].y = std::clamp(old + move[i]*dt, Half, H-Half);
        startY[i] = old;// 留給後面的碰撞計算使用
    }
    if (now_phase == Phase::Serve) {
        timer = std::max(0.0f, timer - dt);// 發球倒數，最低為0
        if (timer <= .00001f) launch();// 倒數結束，設定球速並切換至Rally
        return None;
    }

    int events = None;
```

```cpp
    // CheckCollisionCircleRec 判斷當下是否相交，所以整個 Step 再細分。
    // 球與拍的相對位移每小步不超過約 Radius/4，避免跨過拍子而漏判。
    //ballSpeed取正式上限800與目前總速率的較大值，碰拍加速後仍有足夠細分。
    const float ballSpeed = std::max(800.0f, std::hypot(vx, vy));
    //取兩拍較大的移速量值，和球速相加估計最大相對速度。
    const float paddleSpeed = std::max(std::abs(move[0]), std::abs(move[1]));
    //parts=向上取整(最大相對速度×dt÷2.5px)，至少1段；ceil避免小數被直接截掉。
    const int parts = std::max(1, static_cast<int>(
        std::ceil((ballSpeed + paddleSpeed) * dt / (Radius * .25f))));
    //slice為每個碰撞小步的秒數，parts段合計仍是原本dt。
    const float slice = dt / parts;

    for (int part = 1; part <= parts; ++part) {
        //elapsed是從本次tick起點到目前小步終點的秒數，用來推算同一時刻的拍心。
        const float elapsed = part * slice;
        ball.x += vx * slice;
        ball.y += vy * slice;

        if (ball.y <= Radius && vy < 0) {
            ball.y = 2 * Radius - ball.y;
            vy = -vy;
            events |= Wall;
        } else if (ball.y >= H - Radius && vy > 0) {
            ball.y = 2 * (H - Radius) - ball.y;
            vy = -vy;
            events |= Wall;
        }

        // 只檢查正在迎向的球拍，反彈離開後不會再次加速同一個接觸。
        const int i = vx < 0 ? 0 : 1;
        const float sign = i == 0 ? 1.0f : -1.0f;
        const float batY = std::clamp(startY[i] + move[i] * elapsed, Half, H - Half);
        //Rectangle採左上角x/y加寬高；球拍中心座標先減半寬9和半高56。
        const Rectangle paddle{bats[i].x - 9, batY - Half, 18, 2 * Half};

        if (vx != 0 &&
            //CheckCollisionCircleRec接收球心、半徑和矩形，會檢查矩形邊與四個圓角接觸範圍。
            CheckCollisionCircleRec({ball.x, ball.y}, Radius, paddle)) {
            const bool againstEdge = (batY <= Half && move[i] < 0) ||
                                     (batY >= H - Half && move[i] > 0);
            //拍子被上下邊界擋住時用0計帶角；其他情況使用帶方向的移動速度。
            const float actual = againstEdge ? 0 : move[i];
            const float offset = ball.y - batY;
            //offset/56換成命中比例；偏下28px得+30°，偏上28px得-30°，中央0°。
            const float base = 60 * std::clamp(offset / Half, -1.0f, 1.0f);
            const float angle = std::clamp(
                base + 8 * std::clamp(actual / maxSpeed[i], -1.0f, 1.0f),
                -60.0f, 60.0f) * Rad;
            //hypot算總速率sqrt(vx²+vy²)，命中增加5%，min限制在800px/s。
            const float speed = std::min(800.0f, 1.05f * std::hypot(vx, vy));
            vx = sign * speed * std::cos(angle);
            vy = speed * std::sin(angle);

            // 圓推出拍子的場內側，避免留在拍內而連續碰撞。
            //左拍推到右方、右拍推到左方，球心距拍面大於半徑，避免下一小步重複命中。
            ball.x = bats[i].x + sign * (9 + Radius + .01f);
            ball.y = std::clamp(ball.y, Radius, H - Radius);
            lastBat = i;
            hitTimer = .09f;
            events |= Paddle;
        }

        // 每個小步都判斷出界，一次得分後立即結束本次物理更新。
        if (ball.x + Radius < 0 || ball.x - Radius > W) {
            last = ball.x < 0 ? 1 : 0;
            ++score[last];
            now_phase = score[last] >= cfg.target ? Phase::Over : Phase::Point;
            timer = now_phase == Phase::Over ? .18f : .35f;
            events |= Score;
            break;
        }
    }
    return events;// 回傳本步發生的事件組合
}
```

## `test_game.cpp`

```cpp
#include "game.h"

#include <cassert>

#include <cmath>
#include <iostream>

int main() {

    Config cfg;
    cfg.target = 5;

    Game game(cfg,42);
    game.resetPoint();
    game.tick(Step,1,0);
    assert(game.bats[0].y > 360 && game.ball.x == 640);

    float remaining = game.timer;
    game.pause();
    game.tick(Step,1,1);
    assert(game.timer == remaining);
    game.resume();
    assert(game.now_phase == Phase::Serve && game.timer == remaining);
    game.last = 0;
    game.launch();
    assert(game.vx > 0 && std::abs(std::hypot(game.vx,game.vy)-570)<.01f);

    const auto x = game.ball.x;

    const auto vx = game.vx;
    game.pause();
    game.resume();

    for (int i=0; i<360; ++i) game.tick(Step,1,1);
    assert(game.now_phase == Phase::Rally && game.ball.x == x && game.vx == vx);
    game.tick(Step,0,0);
    assert(game.ball.x > x);

    for (int side=0; side<2; ++side) {
        game.ball = {side ? 1210.0f : 70.0f, 360};
        game.vx = side ? 570 : -570;
        game.vy = 0;
        game.bats[side].y = 360;
        assert(game.tick(Step,0,0) & Paddle);
        assert((game.vx > 0) == (side == 0));
        assert(std::abs(std::hypot(game.vx,game.vy)-598.5f)<.01f);
        assert(!(game.tick(Step,0,0) & Paddle));
    }
    game.ball = {640,11};
    game.vx = 500;
    game.vy = -300;

    const float speed = std::hypot(game.vx,game.vy);
    assert(game.tick(Step,0,0) & Wall);
    assert(game.vy > 0 && std::abs(speed-std::hypot(game.vx,game.vy))<.01f);
    game.score[0] = 4;
    game.ball = {W+9,200};
    game.vx = 570;
    assert(game.tick(Step,0,0) & Score);
    assert(game.now_phase == Phase::Over && game.score[0] == 5);
    assert(game.tick(Step,0,0) == None && game.score[0] == 5);
    game.restart();
    assert(game.roundId == 2 && game.score[0] == 0 && game.now_phase == Phase::Serve);

    cfg.mode = "pvcpu";
    cfg.side = "left";

    Game cpu(cfg,42);
    cpu.resetPoint();
    cpu.now_phase = Phase::Rally;
    cpu.ball = {250,450};
    cpu.vx = 500;
    cpu.tick(Step,0,0);
    assert(cpu.bats[1].y == 360);

    for (int i=0; i<25; ++i) cpu.tick(Step,0,0);
    assert(cpu.ball.x < W/2 && cpu.bats[1].y > 360);

    float y = cpu.bats[1].y;
    cpu.vx = -500;
    cpu.tick(Step,0,0);
    assert(cpu.bats[1].y == y);
    std::cout << "Core checks passed\n";
}
```

## `test_collision.cpp`

```cpp
#include "game.h"
#include <cassert>
#include <cmath>
#include <iostream>

Game rally() {
    Game g(Config{}, 11);
    g.now_phase = Phase::Rally;
    g.vx = -500;
    g.vy = -600;
    return g;
}

//此測試直接設定核心位置重現漏判，包含左右側、圓角、移拍、高速、重疊和真正漏球。
int main() {
    for (int side = 0; side < 2; ++side) {
        auto g = rally();
        // 首次跨過 x=67/1213 時尚未碰拍，之後才進入圓角接觸範圍。
        g.ball = {side ? 1210.0f : 70.0f, 430.5f};
        g.vx = side ? 500 : -500;
        int events = None;
        for (int n = 0; n < 5; ++n) events |= g.tick(Step, 0, 0);
        assert(events & Paddle);
        assert((g.vx > 0) == (side == 0));
        assert(g.score[0] == 0 && g.score[1] == 0);

        auto moving = rally();
        moving.ball = {side ? 1214.0f : 66.0f, 429};
        moving.vx = side ? 570 : -570;
        moving.vy = 0;
        assert(moving.tick(Step, side ? 0 : 1, side ? 1 : 0) & Paddle);

        auto overlap = rally();
        overlap.ball = {side ? 1215.0f : 65.0f, 360};
        overlap.vx = side ? 800 : -800;
        overlap.vy = 0;
        assert(overlap.tick(Step, 0, 0) & Paddle);
        assert(!(overlap.tick(Step, 0, 0) & Paddle));

        auto fast = rally();
        fast.ball = {side ? 1170.0f : 110.0f, 360};
        fast.vx = side ? 3000 : -3000;
        fast.vy = 0;
        assert(fast.tick(.05f, 0, 0) & Paddle);
        assert((fast.vx > 0) == (side == 0));

        auto miss = rally();
        miss.ball = {side ? 1214.0f : 66.0f, 425};
        miss.vx = side ? 1 : -1;
        miss.vy = 0;
        // 擴張方框內，但圓距離矩形角落大於半徑，應該不算命中。
        assert(!(miss.tick(Step, 0, 0) & Paddle));

        auto away = rally();
        away.ball = {side ? 1215.0f : 65.0f, 360};
        away.vx = side ? -570 : 570;
        away.vy = 0;
        assert(!(away.tick(Step, 0, 0) & Paddle));
    }

    // 最高正式球速、兩種垂直方向、左右側、不同拍高及合法移拍方向。
    for (int side = 0; side < 2; ++side) {
        for (float y : {80.0f, 360.0f, 640.0f}) {
            for (int vertical : {-1, 0, 1}) {
                for (int movement : {-1, 0, 1}) {
                    auto g = rally();
                    g.bats[side].y = y;
                    g.ball = {side ? 1208.0f : 72.0f, y};
                    g.vx = side ? 640.0f : -640.0f;
                    g.vy = vertical * 480.0f;
                    assert(g.tick(Step, side ? 0 : movement, side ? movement : 0) & Paddle);
                    assert((g.vx > 0) == (side == 0));
                    assert(std::hypot(g.vx, g.vy) <= 800.01f);
                }
            }
        }
    }
    std::cout << "Collision regressions passed\n";
}
```

## `bridge.h`

```cpp
#pragma once
#include <cstdint>

// C ABI 邊界只傳固定大小數值；C++ 的 string、Game 和亂數引擎留在 DLL 內。
//PongOptions是跨語言的建立參數；6個int32加1個uint32，共28bytes，順序須與core.py一致。
struct PongOptions {
    std::int32_t mode, difficulty, side, preset, serve, target;
    std::uint32_t seed;
};
//PongSnapshot是畫面快照；7個int32加8個float，共60bytes。位置單位px、速度px/s、計時秒。
struct PongSnapshot {
    std::int32_t phase, saved, leftScore, rightScore, last, lastBat, roundId;
    float leftY, rightY, ballX, ballY, vx, vy, timer, hitTimer;
};
#ifdef PONG_EXPORTS
//extern "C"採C連結名稱；dllexport讓Python可從DLL取得這些函式。
#define PONG_API extern "C" __declspec(dllexport)
#else
#define PONG_API extern "C" __declspec(dllimport)
#endif
PONG_API std::int32_t pong_abi_version() noexcept;
//回傳不透明handle；Python只傳回DLL操作，不直接解讀Game記憶體。
PONG_API void* pong_create(const PongOptions* options) noexcept;
PONG_API void pong_destroy(void* handle) noexcept;
PONG_API std::int32_t pong_tick(void* handle, float dt, std::int32_t left, std::int32_t right) noexcept;
PONG_API std::int32_t pong_command(void* handle, std::int32_t command) noexcept;
PONG_API std::int32_t pong_snapshot(void* handle, PongSnapshot* output) noexcept;
```

## `bridge.cpp`

```cpp
#define PONG_EXPORTS
#include "bridge.h"
#include "game.h"
#include <algorithm>
#include <cmath>
#include <new>

//static_assert在編譯期檢查ABI大小，布局改動時要同步Python與版本號。
static_assert(sizeof(PongOptions) == 28);
static_assert(sizeof(PongSnapshot) == 60);

//ABI=Application Binary Interface；Python以版本號確認可使用同一份二進位協定。
std::int32_t pong_abi_version() noexcept { return 1; }

//o指向Python準備的參數；先驗證數值範圍，再配置Game，例外轉成nullptr。
void* pong_create(const PongOptions* o) noexcept {
    if (!o || o->mode < 0 || o->mode > 1 ||
        o->difficulty < 0 || o->difficulty > 2 ||
        o->side < 0 || o->side > 1 || o->preset < 0 || o->preset > 1 ||
        o->serve < 0 || o->serve > 2 ||
        (o->target != 5 && o->target != 7 && o->target != 11)) return nullptr;
    try {
        Config c;
        c.mode = o->mode ? "pvcpu" : "pvp";
        const char* difficulty[]{"easy","normal","hard"};
        const char* serve[]{"loser","scorer","random"};
        c.difficulty = difficulty[o->difficulty];
        c.side = o->side ? "right" : "left";
        c.preset = o->preset ? "preset2" : "preset1";
        c.serve = serve[o->serve];
        c.target = o->target;
        return new Game(c,o->seed);
    } catch (...) { return nullptr; }
}

//由建立Game的DLL執行delete；delete nullptr也安全。
void pong_destroy(void* handle) noexcept { delete static_cast<Game*>(handle); }

//dt須是有效正數且不超過一個STEP容許值；左右輸入限制-1/0/1，錯誤回-1。
std::int32_t pong_tick(void* handle, float dt, std::int32_t left, std::int32_t right) noexcept {
    if (!handle || !std::isfinite(dt) || dt <= 0 || dt > Step*1.01f ||
        left < -1 || left > 1 || right < -1 || right > 1) return -1;
    try { return static_cast<Game*>(handle)->tick(dt,left,right); }
    catch (...) { return -1; }
}

//command 0暫停、1恢復、2重開、3離開確認、4取消；不允許的階段操作回0。
std::int32_t pong_command(void* handle, std::int32_t command) noexcept {
    if (!handle) return 0;
    //將不透明地址還原成Game*，解參考後取別名g，操作原本那個物件。
    auto& g = *static_cast<Game*>(handle);
    // 0 暫停、1 恢復、2 重開、3 離開確認、4 取消。
    switch (command) {
    case 0: g.pause(); break;
    case 1:
        if (g.now_phase != Phase::Paused) return 0;
        g.resume(); break;
    case 2: g.restart(); break;
    case 3:
        if (g.now_phase == Phase::Over) return 0;
        g.pause(); g.now_phase = Phase::Leave; break;
    case 4:
        if (g.now_phase != Phase::Leave) return 0;
        g.now_phase = Phase::Paused; break;
    default: return 0;
    }
    return 1;
}

//out是Python端可寫結構地址；將Game目前資料複製過去，不暴露string或亂數引擎。
std::int32_t pong_snapshot(void* handle, PongSnapshot* out) noexcept {
    if (!handle || !out) return 0;
    const auto& g = *static_cast<Game*>(handle);
    *out = {static_cast<std::int32_t>(g.now_phase),static_cast<std::int32_t>(g.bf_pause_phase),
            g.score[0],g.score[1],g.last,g.lastBat,g.roundId,
            g.bats[0].y,g.bats[1].y,g.ball.x,g.ball.y,
            g.vx,g.vy,g.timer,g.hitTimer};
    return 1;
}
```

## `build.ps1`

```powershell
param([string]$Deps = (Join-Path $PSScriptRoot 'external'))
$ErrorActionPreference = 'Stop'
Push-Location $PSScriptRoot
try {
    $ray = Join-Path $Deps 'raylib-5.5_win64_mingw-w64'
    $flags = @('-std=c++20', '-Wall', '-Wextra', '-O2', '-static',
               '-I', (Join-Path $ray 'include'))
    $libraries = @('-L', (Join-Path $ray 'lib'), '-lraylib',
                   '-lopengl32', '-lgdi32', '-lwinmm')
    # 靜態使用 raylib 的碰撞函式；視窗與繪圖仍由 Tkinter 負責。
    & g++ @flags -shared bridge.cpp game.cpp @libraries -o pong_core.dll
    if ($LASTEXITCODE) { throw 'DLL build failed. Close the running game before rebuilding.' }
    foreach ($test in @('test_game', 'test_collision')) {
        & g++ @flags "$test.cpp" game.cpp @libraries -o "$test.exe"
        if ($LASTEXITCODE) { throw "$test compilation failed" }
        & "./$test.exe"
        if ($LASTEXITCODE) { throw "$test failed" }
    }
} finally { Pop-Location }
```

## `smoke.cpp`

```cpp
#include "game.h"
#include <cassert>
#include <iostream>
int main() {
    Config settings;
    settings.side = "right";
    Game game(settings, 11);
    assert(game.human() == 1);
    assert(game.ball.x == W/2 && game.ball.y == H/2);
    assert(game.now_phase == Phase::Intro);
    std::cout << "Stage 1 passed\n";
}
```

