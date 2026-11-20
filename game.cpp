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
