#pragma once
#include <random>
#include <string>

constexpr int W = 1280, H = 720;//遊戲視窗運算尺寸w=寬,h=高，縮放不影響計算

constexpr float Half = 56, Radius = 10, Step = 1.0f / 120;//Half指拍子長度的一半,Radius=球半徑,step=每次物理更新的固定秒數

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
跳過3是gpt教的天才方法，腦子沒點問題或刻意優化程式根本不會這樣寫
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
    //名為：rng(random number generator縮寫)的種子初始化亂數產生器
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
