# Pong完整專題引導教學｜Tkinter＋C++20 DLL

這本教材從空資料夾開始，逐步完成與電腦中交付版相同的Pong。Python/Tkinter管理唯一主視窗、選單、設定、戰績、輸入及球場繪圖；C++20管理球、球拍、CPU、比分和階段。raylib5.5的CheckCollisionCircleRec在DLL內負責圓矩形碰撞。

一次互動的路徑是：按W→Python記住按鍵→方向-1→DLL的tick→拍心y減少→Python讀快照→Canvas更新球拍。後面的章節分別把這條路徑中的每一層完成。

以電腦中現有檔案為來源，既有註解逐項保留。新增註解補上名稱、單位、方向和索引，複雜運算另列例子。正文、完整程式與本機檔案來自同一份來源。

[組員入口](https://hackmd.io/7eoGOVjXTPapJ91ctSQgmg)｜[引導主文](https://hackmd.io/tGkm7Yn-TnSZjNuPpCFwDg)｜[C++附錄](https://hackmd.io/43y2pN6dSZKgqRBGcmbcHg)｜[Python附錄](https://hackmd.io/46KzKWO9TR2zgItJh1M20g)｜[GitHub原始碼](https://github.com/TobyYangc/PongLearning)

## 組員請依這個順序閱讀

1. 先看[組員入口手冊](https://hackmd.io/7eoGOVjXTPapJ91ctSQgmg)，完成環境、下載及建置檢查。
2. 再看[引導主文](https://hackmd.io/tGkm7Yn-TnSZjNuPpCFwDg)，從第0章依序做到第11章；每步看到預期結果再繼續。
3. 第1～4章搭配[C++附錄](https://hackmd.io/43y2pN6dSZKgqRBGcmbcHg)，依序讀game.h→game.cpp→bridge.h→bridge.cpp→build.ps1。
4. 第4～9章搭配[Python附錄](https://hackmd.io/46KzKWO9TR2zgItJh1M20g)，依序讀core.py→play_step3.py→storage.py→launcher.py→sound.py。
5. 最後完成第10章驗證，再依第11章封裝。附錄供查完整檔案，主文負責逐步引導。

想先試玩可下載GitHub新版安裝檔；想跟著學習，請從組員入口與主文開始。

### 使用方式

第7～9章的方法節錄需要放進class App，保留4格方法縮排；函式內部8格。main與模組層級函式位於class外。完整附錄已排好全部縮排，可用來逐段核對，避免把方法貼到其他方法內。

### 學習路徑

依章節建立列出的檔案；完整檔案見對應附錄。先讀說明再保存，執行本章命令並確認結果。第0～6章有獨立可跑成果；第7～9章把正式GUI各方法接起來，第9章末執行完整遊戲。期間可持續執行第5章最小球場比較功能。

同一檔案若分成多個程式區塊，依序接在一起，段落標題不用貼進去。每次重建DLL前先關閉遊戲。用--data-dir指定練習目錄，避免混進日常戰績。

| 章節 | 完成成果 |
|---|---|
| 0 | Tk視窗與工具檢查 |
| 1～3 | C++核心、階段、CPU與圓矩形碰撞 |
| 4 | DLL與Python的ctypes呼叫 |
| 5 | 最小可玩雙人球場 |
| 6 | 設定/戰績保存與復原 |
| 7 | 主選單、設定、輸入與切頁 |
| 8 | 更新、插值、排程與快取 |
| 9 | 正式繪圖、音效、完整遊戲 |
| 10 | 回歸與逐頁驗收 |
| 11 | 封裝和Windows安裝檔 |


## 0｜工具和第一個Tk視窗

### 0.1新建練習資料夾

在PowerShell執行，後面命令都在此資料夾。
```powershell
New-Item -ItemType Directory PongLearning_Tkinter
Set-Location PongLearning_Tkinter
python --version
python -c "import tkinter, struct; print('Tk', tkinter.TkVersion, 'bits', struct.calcsize('P')*8)"
g++ --version
```

Python需包含Tkinter，並和DLL同為x64。找不到g++，先加入已安裝的UCRT64路徑，例如$env:Path = "C:\msys64\ucrt64\bin;" + $env:Path。之後全部使用-std=c++20。編輯器負責保存，python和g++才實際執行或建置。

### 0.2新建step0.py

下面是此步完整程式。
```python
"""階段 0：只建立一個 Tk 視窗。"""
import tkinter as tk

root = tk.Tk()
root.title("Pong - Step 0")
root.geometry("1280x720")
canvas = tk.Canvas(root, bg="black", highlightthickness=0)
canvas.pack(fill="both", expand=True)
canvas.create_text(640, 360, text="PONG", fill="white",
                   font=("Consolas", 64, "bold"))
root.mainloop()
```

```powershell
python step0.py
```

root=tk.Tk()建立視窗，Canvas是裡面的畫布，pack填滿可用範圍，create_text畫文字。mainloop持續處理鍵盤、滑鼠和重繪，關閉後才回終端機。

驗證：看到黑底白字PONG；把文字y改成200重跑，文字應上移。沒有Tk模組先查Python安裝，沒有視窗先查是否真的執行.py及是否保留mainloop。此關座標固定，第5章才加入等比縮放。


## 1｜Game型別、宣告和資料來源

本章建立game.h、game.cpp、test_game.cpp，使用附錄完整檔案。先準備external/raylib-5.5_win64_mingw-w64，內含include/raylib.h、lib/libraylib.a和LICENSE，可從原專案複製。第三方內容保持上游原樣。

### 1.1先完成最小核心，再逐步加入規則

先依附錄A建立game.h的完整宣告。宣告函式不等於立刻需要定義全部；只有真正使用到的函式需要在連結時找到實作。這一步不要先貼整份game.cpp。

新建game.cpp，只放下面內容。之後第2章在檔案尾端逐一新增函式，第3章最後新增tick，完成後與附錄A一致。

```cpp
#include "game.h"
#include "raylib.h"
#include <algorithm>
#include <numbers>
#include <cmath>
#include <utility>

constexpr float Rad = std::numbers::pi_v<float> / 180.0f;

Game::Game(Config c, unsigned seed)
     : cfg(std::move(c)), rng(seed) {}

int Game::human() const {
     return cfg.side == "right" ? 1 : 0;
}
```

新建smoke.cpp作為這一步的測試入口。它不進行對打，只驗證物件是否帶著預期初值。

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

在PowerShell設定編譯選項並執行，這幾個變數後面繼續使用。同一份設定傳進Game後成為cfg；把right改left，對應assert也改成0再跑一次。

```powershell
$ray = Join-Path $PWD 'external\raylib-5.5_win64_mingw-w64'
$flags = @('-std=c++20', '-Wall', '-Wextra', '-static',
           '-I', (Join-Path $ray 'include'))
$libs = @('-L', (Join-Path $ray 'lib'), '-lraylib',
          '-lopengl32', '-lgdi32', '-lwinmm')
g++ @flags smoke.cpp game.cpp @libs -o smoke.exe
if ($LASTEXITCODE -eq 0) { .\smoke.exe }
```

預期Stage 1 passed。-I找標頭、-L找函式庫、-lraylib連結實作；-static靜態連結執行時與本次所用函式庫。這個測試不建立視窗。找不到raylib.h查include，cannot find -lraylib查lib，undefined reference查是否少加game.cpp或少定義目前有呼叫的函式。完整test_game要等第3章把tick完成後再跑。

### 1.2 game.h逐項讀

#pragma once避免同一編譯單元重複引入。random提供亂數引擎/分布，string提供字串。W/H是1280×720邏輯尺寸，Half=56是拍高112的一半，Radius=10是球半徑，Step=1/120秒是固定物理步長，和畫面幀率分開。

Config是設定型別。mode是pvp/pvcpu，difficulty是easy/normal/hard，side是玩家left/right。preset選W/S或方向鍵，serve選loser/scorer/random，session保存識別，target是勝利分數，sound是音效開關。Config{}採宣告時初值。

Phase是強型別列舉，使用Phase::Rally等名稱。Event用位元：Wall=1(001)、Paddle=2(010)、Score=4(100)，Wall|Paddle=3(011)，所以Score不能用3。events|=Wall加入事件，events&Wall檢查包含，||只有真假。

Body保存x/y。bats[2]是2個Body，索引0左、1右；ball是另一個Body。拍心x為48/1232，起始y為360，球心640/360。畫矩形才換算左上角。

| Game成員 | 單位/用途 | 主要讀寫處 |
|---|---|---|
| cfg | Config設定 | 建構子保存，核心讀取 |
| rng | 亂數引擎 | 建構子用seed初始化，launch抽取 |
| now_phase/bf_pause_phase | 目前/暫停前階段 | tick、pause、resume |
| bats/ball | px座標 | tick更新，GUI畫快照 |
| vx/vy | px/s，正右正下 | launch/碰撞設定，tick積分 |
| timer | 秒 | 各階段等待時間 |
| cpuClock | 秒 | CPU距上次觀察累積時間 |
| cpuTarget | y座標 | CPU記住的球高 |
| cpuObserved | bool | 是否有可用來球觀察 |
| hitTimer/lastBat | 秒/拍索引 | 碰拍亮框 |
| score[2] | 左右比分 | 出界加分 |
| last | -1尚無、0左、1右 | 上一球得分方，決定發球/勝負 |
| roundId | 局編號 | restart增加，存檔去重 |

### 1.3 .h/.cpp和建構子

Game是型別，Game game(settings,11)建立物件。game.h宣告可保存資料與可呼叫函式，game.cpp定義步驟；建置時一起編譯連結。呼叫前仍需宣告，#include讓使用端取得它。

`Game::Game(Config c,unsigned seed):cfg(std::move(c)),rng(seed){}`中，c/seed是參數名稱，cfg/rng是成員。settings依位置傳給c，11傳給seed；cfg利用c初始化，rng利用seed初始化。std::move本身允許移動初始化，真正接手資料發生在初始化過程。

human() const讀cfg.side，右側回1、其他回0，對應bats索引。const表示查詢不改一般成員。void可用return;提早結束，tick回傳int所以return None帶回0。先在測試找出Config cfg、Game game(cfg,42)，逐一對照來源。


## 2｜發球、暫停、CPU與計分

### 2.1依函式順序讀game.cpp

`Rad=std::numbers::pi_v<float>/180`，將度換弧度。建構子保存設定與種子。resetPoint把球/拍回中央、球速0、CPU資料清除、Serve倒數3秒，但保留比分。restart先增加roundId、清比分、last=-1，再resetPoint。

launch建立coin(0,1)和angle(12,25)。coin/angle是抽取規則，coin(rng)或angle(rng)才抽一次。toward先隨機0左1右，有上一球且serve不是random時，scorer用last、loser用1-last。a=角度×Rad×隨機正負，正下負上。vx=左右符號×570×cos(a)，vy=570×sin(a)。570是每次發球初速，碰拍才加速。

| toward | a | 方向 |
|---|---|---|
| 0左 | 負 | 左上 |
| 0左 | 正 | 左下 |
| 1右 | 負 | 右上 |
| 1右 | 正 | 右下 |

例如20°，速度分量量值約535.6與195.0，hypot算回570。水平線是0°，朝左時以水平向左的線量偏角。

pause先排除Paused/Leave/Over，避免重複呼叫覆蓋原階段；先bf_pause_phase=now_phase，再設Paused。resume原為Rally先Resume倒數3秒，球位置/速度保留；其他階段接原timer，例如Serve剩1.5秒就續1.5秒。

### 2.2 tick前半的早退

Intro/Point/Resume只扣timer=max(0,timer-dt)。接近0時Resume回Rally，其餘resetPoint，隨即return None，這一步不移物件。之後只允許Serve/Rally繼續。hitTimer扣dt。maxSpeed初始左右650px/s，move由-1/0/1乘650得到帶方向速度。

startY保存移拍前y，用clamp(old+move*dt,56,664)更新，保證拍子留場內。Serve在移拍後才扣timer，所以玩家可預先移拍；倒數歸零launch後本步返回，下一步才開始移球。

### 2.3 CPU整段追蹤

ai=1-human()得到另一側。incoming要求Rally且朝CPU：右側vx>0、左側vx<0。Easy/Normal/Hard觀察間隔0.28/0.16/0.09秒，速度上限300/430/520。每步先move[ai]=0；只在Rally觀察。

球飛離就cpuObserved=false。cpuClock+=dt，到interval時減interval保留零碎時間，有來球才cpuTarget=ball.y並設true。diff=目標y-拍心y，正下負上，距離超過5.6px才移動，diff/dt再限制正負速度上限。

例：拍心360、目標400、dt=1/120，需要4800px/s才一步到達；Normal限制430，所以每步只走3.58px。觀察間隔使CPU追上一眼看到的位置。

### 2.4出界與勝負

整顆球左出界是ball.x+Radius<0，右出界是ball.x-Radius>W。左出界右方得分，反之左方。last保存得分方，score[last]加1，達target進Over，否則Point。Point停0.35秒後resetPoint；Over的0.18秒由GUI供結算淡入使用。

用test_game的出界案例追一次：球心W+9仍未整顆出界，下一tick才加分。Over後再tick不可再加分。events|=Score加入事件，最終return events交回GUI。


### 2.5逐一補進檔案，確認參數與成員的關係

下面每段都新增在game.cpp現有函式之後，不放進main。每完成一段可重跑smoke，確認檔案仍可編譯。這裡沿用現有註解原文；正文另外補上閱讀順序。

#### 新增resetPoint

先讀賦值左邊：ball、bats、vx/vy、CPU相關資料、now_phase、timer，全是目前Game物件的成員。右邊常數代表新球初值。bats[0].y=bats[1].y=H/2先令右拍360，再把同樣的360交給左拍。score沒有被寫入，所以得分保留。cpuClock清0代表重新計算觀察等待時間；cpuTarget回360並不表示CPU會立即追中央，還需cpuObserved和incoming成立。

```cpp
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
```

#### 新增launch

先建立抽取規則，再用rng抽出toward。if只控制下一個toward賦值，後面的a/vx/vy不受它控制。三元運算是「產生一個值」，不是把兩側都執行。scorer時選last，其他非random規則選1-last。coin(rng)?1:-1用0/1的真假選正負；左右符號只乘vx，上下符號透過a影響sin。每次發球重新使用570，上一球加速不延續。

```cpp
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
```

#### 新增restart

++roundId先將本局識別加1，用於戰績去重；左右分數歸零、last恢復尚無得分，再呼叫resetPoint。這與resume不同：restart會丟掉這局比分與球位置，resume保留它們。

```cpp
void Game::restart() {
    ++roundId;
    score[0] = score[1] = 0;
    last = -1; //-1:沒人得分
    resetPoint();
}
```

#### 新增pause

第一個if是守門條件：已Paused/Leave/Over就return;。void的return不帶值，只結束這個函式，不是退出整個程式。若允許暫停，先保存原階段再換Paused；先後顛倒就只會保存Paused。位置和timer沒有被改，tick遇到暫停會早退。

```cpp
void Game::pause() {
    if (now_phase == Phase::Paused || now_phase == Phase::Leave || now_phase == Phase::Over) 
    return;
    //排除 已經暫停、正在顯示離開確認、對局已經結束 狀況
    bf_pause_phase = now_phase;
    now_phase = Phase::Paused;
}
```

#### 新增resume

bf_pause_phase只表示暫停前的階段。原為Rally時設3秒並進Resume；tick將來負責倒數結束後回Rally。若原為Serve、Intro、Point等，直接回該階段，沿用原timer。這個函式不建立新Game，也不呼叫restart。

```cpp
void Game::resume() {
    if (bf_pause_phase == Phase::Rally) {
        timer = 3;//倒數三秒
        now_phase = Phase::Resume;
    } else {
        now_phase = bf_pause_phase;
    }
}
```

#### 這一步的可執行檢查

暫時在smoke.cpp的輸出前加入下列幾行，確認新球、發球、暫停與恢復。這個測試不需要tick實作。測完後可保留smoke作為學習檢查。

```cpp
game.resetPoint();
    assert(game.now_phase == Phase::Serve && game.timer == 3);
    game.launch();
    assert(game.now_phase == Phase::Rally);
    const float beforeX = game.ball.x;
    game.pause();
    assert(game.now_phase == Phase::Paused);
    assert(game.bf_pause_phase == Phase::Rally);
    game.resume();
    assert(game.now_phase == Phase::Resume && game.timer == 3);
    assert(game.ball.x == beforeX);
```

再次執行第1章的smoke編譯命令，仍應顯示Stage 1 passed。這證明狀態切換已接起來，但時間還不會自己前進，下一章補上tick。


## 3｜CircleRec碰撞與高速穿透回歸

本章建立test_collision.cpp。舊版只在球心跨過拍面接觸線時檢查一次高度，會漏掉「跨線時略過拍緣，之後斜向才碰到角落」或移拍稍後接到球的情況。

### 3.1圓和矩形的參數

CheckCollisionCircleRec接收Vector2球心、半徑與Rectangle球拍。矩形用左上角/寬/高，因此以拍心x-9、當時拍心y-Half、寬18、高112建立。它會檢查矩形邊和角落圓距離，角落方框內但超出半徑的空白點不算命中。

### 3.2為何還要細分

只查單一終點仍可能跨過拍子。ballSpeed取800與目前總速率的較大值，paddleSpeed取兩拍移速絕對值較大者。`parts=ceil((ballSpeed+paddleSpeed)*dt/(Radius*0.25))`，至少1，slice=dt/parts。

球速800、拍速650、dt=1/120時，相對位移上界12.08px，除2.5後向上取整為5段。每段移球、反射牆面，以startY+move*elapsed求同一時刻拍心，再做圓矩形判定。整次更新仍是原本dt。

### 3.3命中後角度和分離

i依vx選迎向的拍，sign左拍+1/右拍-1代表朝場內。offset=球心y-拍心y，`base=60*clamp(offset/Half,-1,1)`，中央0°、下偏28px為+30°。再加`8*clamp(actual/maxSpeed,-1,1)`，總角度限制±60°。拍子抵邊仍朝界外輸入時actual=0。

總速率乘1.05、上限800，再用sin/cos拆分。球心推出到拍心x+sign*(9+Radius+0.01)，避免留在拍內連續加速。剩餘小步沿新方向繼續，每小步也檢查出界，得分後break。

### 3.4執行回歸
```powershell
g++ @flags test_collision.cpp game.cpp @libs -o test_collision.exe
if ($LASTEXITCODE -eq 0) { .\test_collision.exe }
```

這段命令在第3.5節將tick完成後執行。沿用第1章的flags/libs，預期Collision regressions passed。案例包含左右拍緣斜入、移動球拍、初始重疊、離拍不重複命中、長dt高速、角落外真正漏球與54組正式球速/拍高/方向。

先畫出ball={70,430.5}、vx=-500、vy=-600的路徑，理解第一次到x=67還偏低、之後才碰到拍角。這例在舊版會失敗。

[raylib5.5碰撞原始碼](https://github.com/raysan5/raylib/blob/5.5/src/rshapes.c)


### 3.5現在新增完整tick，逐段接起來

下面所有程式區塊依序接在game.cpp尾端，合起來是同一個tick函式，不能替每段另加大括號。上面的smoke仍可編譯。每段先辨認「目前階段」「本次秒數」「起點/小步終點」再追蹤賦值。

#### tick步驟1

tick的三個參數來自呼叫端：dt是本步秒數，left/right是左右輸入-1/0/1。函式內使用的是目前這個Game的資料。

```cpp
int Game::tick(float dt, int left, int right) {
```

#### tick步驟2

先處理只需倒數的階段。max避免timer變成負數，0.00001f是接近零的容許量，不是畫面幀時間。return None結束本次更新。

```cpp
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
```

#### tick步驟3

CPU只在pvcpu模式接管。ai=1-human()交換0與1。incoming同時檢查階段和球速方向。巢狀三元依easy→hard→其餘normal選取觀察間隔與速度，並先讓CPU停住。

```cpp
if (cfg.mode == "pvcpu") {
    // 單人模式，由CPU接手其中一支球拍
        const int ai = 1 - human();//玩家索引0/1，另一側就是CPU
        const bool incoming = now_phase == Phase::Rally && (ai == 1 ? vx > 0 : vx < 0);
        // 正在對打，而且球朝CPU飛：右側看 vx>0，左側看 vx<0
        const float interval = cfg.difficulty == "easy" ? .28f : cfg.difficulty == "hard" ? .09f : .16f;
        // CPU 觀察間隔，單位秒：Easy 0.28、Hard 0.09、Normal 0.16
```

#### tick步驟4

maxSpeed[ai]依CPU難度設定速度上限，單位px/s：Easy=300、Normal=430、Hard=520。只覆寫CPU那一側，另一側仍是玩家650。巢狀三元先問easy，否則再問hard，兩者皆否就採normal。

```cpp
maxSpeed[ai] = cfg.difficulty == "easy" ? 300 : cfg.difficulty == "hard" ? 520 : 430;
        // CPU最高移速，單位px/s：Easy 300、Hard 520、Normal 430
```

#### tick步驟5

move[ai]=0先讓CPU這一步停止；之後只有來球、已觀察且離目標夠遠時才給追球速度。這會覆蓋該側由真人按鍵帶入的值。

```cpp
move[ai] = 0;// 每步先預設停住，需追球時才設定移動速度
```

#### tick步驟6

有對打時才累積觀察秒數；球離開CPU就使舊觀察失效。達interval減掉一段間隔而非直接歸零，可保留多出來的時間。

```cpp
if (now_phase == Phase::Rally) {
            if (!incoming) cpuObserved = false;// 球已飛離，清除舊觀察的有效狀態
            cpuClock += dt;// 累積距離上次觀察經過的秒數
```

#### tick步驟7

cpuClock達interval才觀察一次。先扣掉一段interval保留餘數，有來球時才記住ball.y並設cpuObserved=true；沒有來球則不更新目標。

```cpp
if (cpuClock >= interval) {
                cpuClock -= interval; // 到觀察時間，保留超出的零碎秒數
                if (incoming) {
                    cpuTarget = ball.y;  //記住球心y，作為追球目標
                    cpuObserved = true; // 已取得可用的來球觀察結果
                }
            }
```

#### tick步驟8

diff是目標減目前拍心，正向下、負向上。距離超過拍高5%才追，避免在目標附近抖動；diff/dt是本步到達目標所需速度，clamp限制不超過難度上限。

```cpp
const float diff = cpuTarget - bats[ai].y; //正值往下追，負值往上追
            if (incoming && cpuObserved && std::abs(diff) > .05f * (2*Half))
            // 有來球、有觀察結果，且距離超過拍高的5%（5.6px）才移動
                move[ai] = std::clamp(diff / dt, -maxSpeed[ai], maxSpeed[ai]);
                // 算出本步到達目標所需速度，再限制於 CPU 的正負速度上限
        }
    }
```

#### tick步驟9

startY保留兩拍在整次tick起點的位置，後面計算小步碰撞時還會用它。

```cpp
float startY[2]{}; // 保存左右拍心在本步開始時的y
```

#### tick步驟10

for索引0、1分別更新左拍與右拍。拍心限制Half到H-Half，代表上緣與下緣剛好不能超出場地。

```cpp
for (int i = 0; i < 2; ++i) {
```

#### tick步驟11

old保留移拍前y；old+move[i]*dt得到整步終點，clamp讓拍心留在56～664。startY保存起點。接著Serve扣倒數，歸零呼叫launch，再return None，這一步不移球。

```cpp
const float old = bats[i].y;// 記下這支球拍移動前的位置
        bats[i].y = std::clamp(old + move[i]*dt, Half, H-Half);
        startY[i] = old;// 留給後面的碰撞計算使用
    }
    if (now_phase == Phase::Serve) {
        timer = std::max(0.0f, timer - dt);// 發球倒數，最低為0
        if (timer <= .00001f) launch();// 倒數結束，設定球速並切換至Rally
        return None;
    }
```

#### tick步驟12

events從None=0開始。後面每次事件用|=合併，不會蓋掉同一次更新其他事件。

```cpp
int events = None;
```

#### tick步驟13

估算小步數：使用球速上界加拍速量值，除以允許的相對位移2.5px並ceil向上取整，至少1步。slice是每小步秒數。

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
```

#### tick步驟14

part從1到parts，每次計算elapsed，將球按目前vx/vy推進slice秒。碰撞後改變的速度會用於後續小步。

```cpp
for (int part = 1; part <= parts; ++part) {
        //elapsed是從本次tick起點到目前小步終點的秒數，用來推算同一時刻的拍心。
        const float elapsed = part * slice;
        ball.x += vx * slice;
        ball.y += vy * slice;
```

#### tick步驟15

上牆球心邊界是Radius=10，下牆是H-Radius=710。超出多少就鏡射回多少，vy反號。條件同時看速度，避免已離牆仍重複反射。

```cpp
if (ball.y <= Radius && vy < 0) {
            ball.y = 2 * Radius - ball.y;
            vy = -vy;
            events |= Wall;
        } else if (ball.y >= H - Radius && vy > 0) {
            ball.y = 2 * (H - Radius) - ball.y;
            vy = -vy;
            events |= Wall;
        }
```

#### tick步驟16

依vx挑正在迎向的球拍，左拍反彈向右所以sign=+1，右拍反彈向左所以sign=-1。batY由起點和elapsed推算；Rectangle需要左上角，所以減半寬9與半高56。

```cpp
// 只檢查正在迎向的球拍，反彈離開後不會再次加速同一個接觸。
        const int i = vx < 0 ? 0 : 1;
        const float sign = i == 0 ? 1.0f : -1.0f;
        const float batY = std::clamp(startY[i] + move[i] * elapsed, Half, H - Half);
        //Rectangle採左上角x/y加寬高；球拍中心座標先減半寬9和半高56。
        const Rectangle paddle{bats[i].x - 9, batY - Half, 18, 2 * Half};
```

#### tick步驟17

CheckCollisionCircleRec相交才處理命中。againstEdge排除抵邊仍往界外的輸入；offset決定基礎角度，再加最多±8°移拍帶角並限制±60°。hypot求目前總速率，乘1.05且最多800，再由sin/cos拆分新速度。

```cpp
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
```

#### tick步驟18

將球推出拍面，拍半寬9加球半徑10再加0.01px，使球不殘留在拍內。lastBat記哪支拍命中，hitTimer點亮0.09秒，events合併Paddle。

```cpp
// 圓推出拍子的場內側，避免留在拍內而連續碰撞。
            //左拍推到右方、右拍推到左方，球心距拍面大於半徑，避免下一小步重複命中。
            ball.x = bats[i].x + sign * (9 + Radius + .01f);
            ball.y = std::clamp(ball.y, Radius, H - Radius);
            lastBat = i;
            hitTimer = .09f;
            events |= Paddle;
        }
```

#### tick步驟19

每小步檢查整顆球出界。last選另一側得分，比分達target進Over，未達則Point；合併Score後break離開小步迴圈，避免一次tick重複計分。最後將事件集合回傳。

```cpp
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

### 3.6接上完整核心回歸

現在建立附錄A的test_game.cpp和test_collision.cpp；這時所有Game函式都已定義，可執行完整測試。

```powershell
g++ @flags test_game.cpp game.cpp @libs -o test_game.exe
if ($LASTEXITCODE -eq 0) { .\test_game.exe }
g++ @flags test_collision.cpp game.cpp @libs -o test_collision.exe
if ($LASTEXITCODE -eq 0) { .\test_collision.exe }
```

預期依序顯示Core checks passed和Collision regressions passed。第一組檢查基本階段、發球、CPU、分數；第二組檢查曾漏掉的碰撞。assert失敗時看錯誤行，先對照該案例的起始座標、速度與預期事件，再回tick找哪一層判斷應該成立。


## 4｜DLL與Python的二進位介面

建立bridge.h、bridge.cpp、build.ps1、core.py和core_check.py。Game含string和亂數引擎，留在DLL；Python只取得handle並透過函式操作。

### 4.1 Options和Snapshot

Options為6個int32加1個uint32，共28bytes。mode0雙人/1單人、difficulty0/1/2=easy/normal/hard、side0左1右、preset0W/S1方向鍵、serve0失分方1得分方2隨機、target5/7/11、seed為32位元種子。

Snapshot為7個int32加8個float，共60bytes。欄位順序和大小與core.py的_fields_一一對應。static_assert在編譯時檢查，Python也以sizeof檢查。

### 4.2 bridge.cpp每個函式

| 函式 | 作用/回傳 |
|---|---|
| pong_abi_version | 回1，驗證二進位協定 |
| pong_create | 驗證Options，new Game，失敗nullptr |
| pong_destroy | 將handle轉回Game*再delete |
| pong_tick | 驗證dt和輸入，呼叫核心，回事件或-1 |
| pong_command | 0暫停/1恢復/2重開/3離開確認/4取消，成功1失敗0 |
| pong_snapshot | 將目前欄位複製到out，成功1失敗0 |

extern "C"提供C連結名稱，dllexport讓符號導出。noexcept和catch避免C++例外穿過語言邊界。auto& g=*static_cast<Game*>(handle)取得原物件別名。由DLL配置也由DLL釋放，不能對同一handle呼叫兩次delete。

### 4.3建置檢查點
```powershell
.\build.ps1
```

產生pong_core.dll並顯示兩組核心測試通過。DLL靜態使用raylib碰撞，視窗仍只由Tk建立，交付時不需額外raylib.dll。

### 4.4 core.py的型別與資源管理

Phase(IntEnum)順序對應C++。CDLL從core.py同目錄載入DLL，argtypes指定參數型別、restype指定回傳型別。handle要用c_void_p，以免64位元地址被截成預設c_int。byref傳結構地址，snapshot取得的是副本。

依下列順序讀每個方法，對照它收到什麼、交給哪個DLL函式、如何回報錯誤。
### core.py的__init__

settings是設定字典；seed省略時產生32位元種子。載入同資料夾DLL，先檢查ABI版本，再宣告每個函式的參數與回傳型別，最後建立Game。

```python
    def __init__(self, settings, seed=None):
        self.dll = C.CDLL(str(Path(__file__).resolve().with_name("pong_core.dll")))
        self.dll.pong_abi_version.argtypes = []
        self.dll.pong_abi_version.restype = C.c_int32
        #ABI版本不相符就停止，避免用錯欄位布局讀寫記憶體。
        if self.dll.pong_abi_version() != 1:
            raise RuntimeError("DLL ABI version mismatch")
        self.dll.pong_create.argtypes = [C.POINTER(Options)]
        #handle是64位元程序中的地址，必須用c_void_p接收，不能使用預設的c_int。
        self.dll.pong_create.restype = C.c_void_p
        self.dll.pong_destroy.argtypes = [C.c_void_p]
        self.dll.pong_destroy.restype = None
        self.dll.pong_tick.argtypes = [C.c_void_p, C.c_float, C.c_int32, C.c_int32]
        self.dll.pong_tick.restype = C.c_int32
        self.dll.pong_command.argtypes = [C.c_void_p, C.c_int32]
        self.dll.pong_command.restype = C.c_int32
        self.dll.pong_snapshot.argtypes = [C.c_void_p, C.POINTER(Snapshot)]
        self.dll.pong_snapshot.restype = C.c_int32
        self.settings = settings.copy()
        #將文字設定換成ABI數值：mode 0雙人/1單人、side 0左/1右、preset 0W/S/1方向鍵。
        options = Options(int(settings["mode"] == "pvcpu"),
                          ("easy", "normal", "hard").index(settings["difficulty"]),
                          int(settings["player_side"] == "right"),
                          int(settings["input_preset"] == "preset2"),
                          ("loser", "scorer", "random").index(settings["serve_to"]),
                          settings["winning_score"],
                          secrets.randbits(32) if seed is None else seed)
        #byref傳結構地址；DLL讀設定並建立Game，回傳由DLL持有的物件地址。
        self.handle = self.dll.pong_create(C.byref(options))
        if not self.handle:
            raise RuntimeError("Unable to create C++ game")
```

### core.py的snapshot

建立Python端Snapshot，交給DLL填入目前遊戲狀態；回傳的是資料副本，可供畫面讀取。

```python
    def snapshot(self):
        result = Snapshot()
        if not self.handle or not self.dll.pong_snapshot(self.handle, C.byref(result)):
            raise RuntimeError("Unable to read C++ state")
        return result
```

### core.py的tick

左右輸入為-1向上、0停止、1向下；每次固定推進STEP秒，回傳Wall/Paddle/Score事件位元。

```python
    def tick(self, left, right):
        if not self.handle:
            raise RuntimeError("Game already closed")
        events = self.dll.pong_tick(self.handle, STEP, left, right)
        if events < 0:
            raise RuntimeError("Invalid physics update")
        return events
```

### core.py的command

action是Python動作名稱，先換成整數命令，再由DLL操作核心階段。

```python
    def command(self, action):
        codes = {"pause": 0, "resume": 1, "restart": 2, "leave": 3, "cancel": 4}
        if not self.handle or not self.dll.pong_command(self.handle, codes[action]):
            raise RuntimeError("Invalid game command")
```

### core.py的close

handle是DLL內Game的地址；交回pong_destroy釋放後設成None，避免重複釋放或再次使用。

```python
    def close(self):
        #getattr在成員尚未存在時回None，讓初始化失敗後也能安全檢查。
        if getattr(self, "handle", None):
            self.dll.pong_destroy(self.handle)
            self.handle = None
```

### core.py的__enter__

配合with Core(...) as game使用，將目前Core物件交給as後面的名稱。

```python
    def __enter__(self):
        return self
```

### core.py的__exit__

離開with區塊時呼叫close，即使區塊中發生例外，也會釋放DLL內的Game。

```python
    def __exit__(self, *args):
        self.close()
```


### 4.5獨立測Python呼叫

建立core_check.py：
```python
"""階段 2：確認 Python 能載入 DLL，推進狀態並讀取快照。"""
from core import Core, Phase

settings = dict(mode="pvp", difficulty="normal", player_side="left",
                input_preset="preset1", serve_to="loser", winning_score=7)
with Core(settings, seed=11) as game:
    first = game.snapshot()
    print("初始階段", Phase(first.phase).name)
    for _ in range(700):
        game.tick(0, 0)
    state = game.snapshot()
    print("更新後", Phase(state.phase).name, state.ball_x, state.ball_y)
    assert state.phase == Phase.RALLY
print("Python -> DLL -> Game -> Python OK")
```
```powershell
python core_check.py
```
預期INTRO→RALLY，最後Python -> DLL -> Game -> Python OK。找不到DLL查同層檔案；WinError193查Python/DLL位元數；ABI錯誤查版本和_fields_，不要刪型別檢查來繞過。


## 5｜最小可玩雙人球場

建立play_step3.py，沿用DLL與core.py。完整檔案如下。
```python
"""階段 3：最小可玩 Tk Canvas，沿用正式 Core 封裝。"""
import time
import tkinter as tk
from core import Core, Phase, STEP

settings = dict(mode="pvp", difficulty="normal", player_side="left",
                input_preset="preset1", serve_to="loser", winning_score=7)
game = Core(settings, seed=11)
root = tk.Tk()
root.title("Pong - Step 3")
root.geometry("960x540")
root.minsize(640, 360)
canvas = tk.Canvas(root, bg="black", highlightthickness=0)
canvas.pack(fill="both", expand=True)
held = set()
acc = 0.0
last = time.perf_counter()
job = None

#將按鍵加入held，Esc暫停/恢復、R重開；同一個持續按鍵不重複觸發。
def press(event):
    key = event.keysym.lower()
    if key in held:
        return
    held.add(key)
    state = game.snapshot()
    if key == "escape":
        if state.phase == Phase.PAUSED:
            game.command("resume")
        else:
            game.command("pause")
    elif key == "r":
        game.command("restart")

#移除鬆開的鍵，下一幀方向計算便不再採用它。
def release(event):
    held.discard(event.keysym.lower())

#失焦時清除輸入並暫停核心，切回後須由玩家恢復。
def lose_focus(event):
    held.clear()
    game.command("pause")

#最小示範迴圈：讀輸入、固定STEP更新、讀快照、等比畫出場地，再排程下一幀。
def frame():
    global acc, last, job
    now = time.perf_counter()
    acc = min(acc + min(now-last,8*STEP),8*STEP)
    last = now
    left = int("s" in held)-int("w" in held)
    right = int("down" in held)-int("up" in held)
    for _ in range(8):
        if acc + 1e-7 < STEP:
            break
        acc = max(0,acc-STEP)
        game.tick(left,right)
    state = game.snapshot()
    canvas.delete("all")
    width, height = max(1,canvas.winfo_width()), max(1,canvas.winfo_height())
    scale = min(width/1280,height/720)
    ox, oy = (width-1280*scale)/2,(height-720*scale)/2
    def rectangle(x,y,w,h):
        canvas.create_rectangle(ox+x*scale,oy+y*scale,ox+(x+w)*scale,oy+(y+h)*scale,
                                fill="white",outline="")
    rectangle(39,state.left_y-56,18,112)
    rectangle(1223,state.right_y-56,18,112)
    canvas.create_oval(ox+(state.ball_x-10)*scale,oy+(state.ball_y-10)*scale,
                       ox+(state.ball_x+10)*scale,oy+(state.ball_y+10)*scale,
                       fill="white",outline="")
    canvas.create_text(width/2,oy+45*scale,text=f"{state.left_score} : {state.right_score}",
                       fill="white",font=("Consolas",-max(1,round(44*scale)),"bold"))
    canvas.create_text(width/2,oy+130*scale,
                       text=f"{Phase(state.phase).name}  W/S | UP/DOWN | ESC PAUSE/RESUME | R RESTART",
                       fill="white",font=("Consolas",-max(1,round(20*scale))))
    job = root.after(16,frame)

#取消排程並釋放DLL中的Game，最後關閉這個Tk視窗。
def close():
    if job:
        root.after_cancel(job)
    game.close()
    root.destroy()

root.bind("<KeyPress>",press)
root.bind("<KeyRelease>",release)
root.bind("<FocusOut>",lose_focus)
root.protocol("WM_DELETE_WINDOW",close)
frame()
root.mainloop()
```
```powershell
python play_step3.py
```

KeyPress把小寫鍵名加入held，KeyRelease移除。int("s" in held)-int("w" in held)得到-1/0/1，右拍用down-up。重複KeyPress不重複切暫停。FocusOut清輸入並pause。

Tk有mainloop，不能用長while或sleep佔住回呼。after請Tk之後再執行frame。perf_counter量真正經過時間，acc累積、每次扣STEP，最多8步。這個練習版先用after(16)，第8章正式版改成目標時刻排程。

scale取寬/1280和高/720的較小值，ox/oy置中；畫點先乘scale再加偏移。960×720得到960×540畫布，上下各90px黑邊。球拍中心轉左上角，球以中心±Radius畫oval。

驗證W/S、方向鍵、Esc暫停/恢復、R重開；再拖成640×360和寬窗。到此已能玩完整雙人局。保存這份檔案，後續組合正式GUI時可持續比較。


## 6｜設定、戰績與原子提交

新增storage.py。DEFAULT提供缺值，ALLOWED限制欄位。settings依mode讀設定，sound_enabled共用。設定頁draft只在START保存成功後成為上次設定。

### 6.1先做獨立保存測試

建立settings_check.py：
```python
from storage import Store
store = Store("lesson-settings")
try:
    settings = store.settings("pvcpu")
    settings["winning_score"] = 5
    store.save_settings(settings)
    assert store.settings("pvcpu")["winning_score"] == 5
    print("Settings OK")
finally:
    store.close()
```
```powershell
python settings_check.py
Get-Content .\lesson-settings\config.json
```

### 6.2原子寫入和鎖

atomic先寫同目錄UUID暫存檔，dump保存完整UTF-8，flush送出Python緩衝，fsync同步檔案，再os.replace提交，finally清理。它保障一般讀取看見完整前版或後版，多檔案仍由pending復原。

Store以a+b開pong.lock，確保有1byte，seek(0)後msvcrt鎖住第一個位元組。另一程序使用相同資料目錄會失敗。close先seek回0解鎖相同範圍，再關檔案。

### 6.3完賽去重與復原

valid_record檢查completed、整數分數、目標、勝負、難度、側別和帶時區日期。type(x)is int排除bool當比分。raw_history保留原始陣列，history只過濾排序供顯示。

record以玩家側p從左右比分取玩家/CPU分數，session加round_id組成match_id。先恢復舊pending，再建立新紀錄；寫pending成功後才recover到history，history成功後才刪pending。中斷於刪pending前，下次以識別去重，不再加一局。

| 失敗處 | 保留資料 | 處理 |
|---|---|---|
| 寫pending | 核心完賽狀態 | RETRY SAVE |
| 寫history | pending | 重試或下次啟動復原 |
| history成功、刪pending失敗 | history與pending | 去重後清理 |
| JSON損壞 | 原檔 | 查log和修復，避免覆蓋成空資料 |

下面按實際使用順序讀完整方法。clear由GUI確認後才可呼叫。
### storage.py的read_json

path是檔案路徑；檔案不存在才回default。JSON損壞和權限錯誤繼續交給呼叫端處理。

```python
def read_json(path, default):
    try:
        return json.loads(path.read_text(encoding="utf-8-sig"))
    except FileNotFoundError:
        return default
```

### storage.py的atomic

先寫同目錄唯一暫存檔、flush與fsync，再os.replace提交；finally清理未使用暫存檔。

```python
def atomic(path, value):
    #每次提交使用唯一檔名；放同目錄，才能以同檔案系統的replace完成提交。
    temp = path.with_name(path.name + "." + uuid.uuid4().hex + ".tmp")
    try:
        with temp.open("w", encoding="utf-8") as stream:
            json.dump(value, stream, indent=2, ensure_ascii=False)
            #先把Python緩衝資料交給作業系統。
            stream.flush()
            #要求作業系統同步檔案內容，再進行替換。
            os.fsync(stream.fileno())
        #原子替換正式檔，讀者看到的是提交前或提交後的完整JSON。
        os.replace(temp, path)
    finally:
        temp.unlink(missing_ok=True)
```

### storage.py的valid_record

逐項驗證完整單人戰績：分數為整數、有人達目標、勝負一致、難度/側別合法、日期帶時區。

```python
def valid_record(row):
    if not isinstance(row, dict) or row.get("completed") is not True:
        return False
    p, c = row.get("player_score"), row.get("cpu_score")
    #type(x)is int會排除bool；Python的bool雖是int子類，也不能當比分。
    if type(p) is not int or type(c) is not int or min(p, c) < 0 or p == c:
        return False
    target = row.get("winning_score", max(p, c))
    if type(target) is not int or target not in (5, 7, 11) or max(p, c) != target:
        return False
    if row.get("winner") != ("player" if p > c else "cpu"):
        return False
    if row.get("difficulty") not in ALLOWED["difficulty"] or row.get("player_side") not in ALLOWED["player_side"]:
        return False
    try:
        stamp = datetime.fromisoformat(row["timestamp"].replace("Z", "+00:00"))
        return stamp.utcoffset() is not None
    except (KeyError, TypeError, ValueError, AttributeError):
        return False
```

### storage.py的__init__

建立資料目錄並鎖住pong.lock的第1個位元組；讀設定，嘗試恢復上次未提交完成局。

```python
    def __init__(self, directory):
        self.directory = Path(directory)
        self.directory.mkdir(parents=True, exist_ok=True)
        self.lock = (self.directory / "pong.lock").open("a+b")
        self.lock.seek(0, 2)
        if self.lock.tell() == 0:
            self.lock.write(b"0")
            self.lock.flush()
        #msvcrt鎖定從目前檔案位置開始，故加鎖/解鎖都先移回第0位元組。
        self.lock.seek(0)
        try:
            msvcrt.locking(self.lock.fileno(), msvcrt.LK_NBLCK, 1)
        except OSError:
            self.lock.close()
            raise RuntimeError("Another Pong is using this data directory")
        self.notice = ""
        try:
            self.config = read_json(self.directory / "config.json", {})
            if not isinstance(self.config, dict):
                raise ValueError("Invalid settings")
        except (OSError, ValueError):
            self.config = {}
            self.notice = "INVALID SETTINGS - USING DEFAULTS"
        try:
            self.recover()
        except (OSError, ValueError):
            self.notice = "RECORD RECOVERY FAILED - CHECK DATA FILES"
```

### storage.py的settings

mode是pvcpu或pvp；依欄位允許值挑出該模式設定，缺值用DEFAULT，音效偏好共用。

```python
    def settings(self, mode):
        saved = self.config.get(mode, self.config)
        if not isinstance(saved, dict):
            saved = {}
        result = DEFAULT.copy()
        for key, options in ALLOWED.items():
            value = saved.get(key)
            if type(value) is type(result[key]) and value in options:
                result[key] = value
        #模式由呼叫端決定；共用音效只有明確False才關閉。
        result.update(mode=mode, sound_enabled=self.config.get("sound_enabled") is not False)
        return result
```

### storage.py的save_settings

先複製現有設定並更新目前模式，原子寫入成功後才更新記憶體內的config。

```python
    def save_settings(self, settings):
        updated = self.config.copy()
        updated[settings["mode"]] = settings.copy()
        updated["sound_enabled"] = settings["sound_enabled"]
        atomic(self.directory / "config.json", updated)
        self.config = updated
```

### storage.py的raw_history

取得原始戰績陣列；非陣列視為格式錯誤，避免把壞資料當空清單覆蓋。

```python
    def raw_history(self):
        rows = read_json(self.directory / "history.json", [])
        if not isinstance(rows, list):
            raise ValueError("history.json must contain an array")
        return rows
```

### storage.py的history

過濾可顯示的有效紀錄，按帶時區的時間由新到舊排序；原檔中的其他資料仍保留。

```python
    def history(self):
        rows = [r for r in self.raw_history() if valid_record(r)]
        return sorted(rows, key=lambda r: datetime.fromisoformat(r["timestamp"].replace("Z", "+00:00")),
                      reverse=True)
```

### storage.py的recover

若有pending-match.json，檢查場次識別並去重追加；history成功後才刪除pending。

```python
    def recover(self):
        path = self.directory / "pending-match.json"
        row = read_json(path, None)
        if row is None and not path.exists():
            return
        if not valid_record(row) or not isinstance(row.get("match_id"), str) or not row["match_id"]:
            raise ValueError("Invalid pending match")
        rows = self.raw_history()
        if not any(isinstance(r, dict) and r.get("match_id") == row["match_id"] and
                   r.get("session_id") == row.get("session_id") for r in rows):
            atomic(self.directory / "history.json", rows + [row])
        #history已成功提交，才刪除pending；中途失敗時下次仍可重試。
        path.unlink()
```

### storage.py的record

只接受已完成pvcpu。p為玩家索引，scores依左/右排列，存檔則固定玩家/CPU比分。

```python
    def record(self, settings, snapshot, session):
        if settings["mode"] != "pvcpu":
            return
        self.recover()
        p = int(settings["player_side"] == "right")
        #scores的索引0左/1右，存檔時以p取得玩家分數、1-p取得CPU分數。
        scores = (snapshot.left_score, snapshot.right_score)
        row = dict(completed=True, session_id=session,
                   match_id=f"{session}-{snapshot.round_id}",
                   timestamp=datetime.now(timezone.utc).isoformat(),
                   difficulty=settings["difficulty"], player_side=settings["player_side"],
                   player_score=scores[p], cpu_score=scores[1-p],
                   winner="player" if snapshot.last == p else "cpu",
                   winning_score=settings["winning_score"])
        if not valid_record(row):
            raise ValueError("Invalid completed match")
        rows = self.raw_history()
        if any(isinstance(r, dict) and r.get("match_id") == row["match_id"] and
               r.get("session_id") == session for r in rows):
            return
        atomic(self.directory / "pending-match.json", row)
        self.recover()
```

### storage.py的clear

先恢復待提交完成局，再將history原子寫成空陣列；由GUI確認後才能呼叫。

```python
    def clear(self):
        self.recover()
        atomic(self.directory / "history.json", [])
```

### storage.py的close

先將檔案位置移回0，解鎖先前鎖住的1個位元組，再關閉檔案。

```python
    def close(self):
        if not self.lock.closed:
            #msvcrt鎖定從目前檔案位置開始，故加鎖/解鎖都先移回第0位元組。
            self.lock.seek(0)
            msvcrt.locking(self.lock.fileno(), msvcrt.LK_UNLCK, 1)
            self.lock.close()
```


## 7｜正式GUI的入口、頁面與輸入

第7～9章組合launcher.py全部方法。先建立附錄完整launcher.py和sound.py，再逐段讀、標記並對照。每段提供的是同一個App類別的方法，位置已在完整檔中，不要在頂層重貼一份。

### 7.1初始化資料與唯一視窗

main讀--data-dir，預設LOCALAPPDATA/Pong，在Tk之前設定DPI。Tk只建立一次。App初始化core=None、page="menu"、draft設定草稿、focus焦點、scroll列表起點、keys按住鍵、blocked待鬆開鍵、buttons命中區、items繪圖快取、dirty未提交場次、acc物理秒數。

page管Python畫面menu/settings/records/clear/match，state.phase管對局Intro/Serve/Rally/Paused等。change_page只換頁與清理輸入，沒有建立另一個視窗。

### 7.2一個按鈕的路徑

draw→button記錄矩形和動作→on_click換算座標→activate接錯誤→perform執行→change_page重畫。命中後立即return，避免同一次滑鼠點擊穿透到新頁面。

| action | 動作 |
|---|---|
| tuple欄位/值 | 修改draft |
| pvcpu/pvp | 讀該模式設定，進設定 |
| start | 建立暫存Core，存設定成功才交給App，建立session，切match |
| records/clear/confirm_clear | 讀列表、顯示確認、確認後清空 |
| back/exit | 返回或走關閉流程 |
| main | 完賽返回，未完賽先確認 |
| confirm_leave | 關閉Core回選單，或依×請求關程式 |
| retry | 重試保存完成局 |
| pause/resume/restart/leave/cancel | 交給Core.command |

### 7.3鍵盤與焦點

on_key排除重複KeyPress，移動鍵由advance讀keys集合。Tab/上下循環焦點，event.state&1檢查Shift。設定頁Enter固定START；其他頁觸發焦點項目。on_release清keys與blocked。

on_focus_out用after_idle等焦點轉移完成，再檢查是否真的離開本視窗；真的失焦才pause，清輸入與acc。Esc依頁面返回/暫停/取消，×在未完賽先LEAVE確認，dirty時先要求保存。

以下逐一讀所有事件與動作方法。
### launcher.py的main

讀命令列並在建立Tk前設定DPI，建立唯一root和App，最後由mainloop持續處理事件。

```python
def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--data-dir", type=Path)
    args = parser.parse_args()
    # 建立 Tk 之前設定 DPI，所有尺寸和滑鼠都使用同一組 Tk 客戶區座標。
    try:
        ctypes.windll.user32.SetProcessDpiAwarenessContext(ctypes.c_void_p(-4))
    except (AttributeError, OSError):
        pass
    root = tk.Tk()
    try:
        App(root,args.data_dir or Path(os.environ.get("LOCALAPPDATA",Path.home())) / "Pong")
    except Exception as error:
        messagebox.showerror("Pong",str(error),parent=root)
        root.destroy()
        return 1
    root.mainloop()
    return 0
```

### launcher.py的__init__

root是唯一Tk主視窗；directory是存檔目錄。初始化頁面、核心、輸入、繪圖快取與排程，再註冊事件。

```python
    def __init__(self, root, directory):
        self.root = root
        self.store = Store(directory)
        self.audio = Audio()
        self.core = None
        self.state = None
        self.previous_state = None
        self.page = "menu"
        self.draft = {}
        self.focus = 0
        self.scroll = 0
        self.rows = []
        #keys保存目前按住的鍵；集合不重複，避免鍵盤自動重複造成多次選單動作。
        self.keys = set()
        #blocked保存暫停/切頁時仍按住的鍵，先鬆開才重新採用。
        self.blocked = set()
        self.pointer = (-100, -100)
        #每項為(rect, action, hotkey, enabled)，只對當前畫面做命中。
        self.buttons = []
        self.message = self.store.notice
        self.dirty = False
        self.session = ""
        self.close_requested = False
        #acc是尚未消耗的物理秒數，每執行一次tick扣掉STEP。
        self.acc = 0.0
        self.last_frame = time.perf_counter()
        self.next_frame = self.last_frame
        self.timer_resolution = False
        # Windows 的高解析度計時只在本程式存活期間啟用，關閉時成對歸還。
        try:
            self.timer_resolution = ctypes.windll.winmm.timeBeginPeriod(1) == 0
        except (AttributeError, OSError):
            pass
        #items是Canvas繪圖快取，保存項目ID、種類、座標、選項與隱藏狀態。
        self.items = []
        self.item_cursor = 0
        self.layout_changed = False
        self.over_age = 0.0
        self.after_id = None
        self.closed = False
        self.scale, self.ox, self.oy = 1, 0, 0

        root.title("Pong")
        scale = min(1, (root.winfo_screenwidth() - 40) / W, (root.winfo_screenheight() - 100) / H)
        root.geometry(f"{int(W * scale)}x{int(H * scale)}")
        root.minsize(640, 360)
        self.canvas = tk.Canvas(root, bg=BLACK, highlightthickness=0)
        self.canvas.pack(fill="both", expand=True)
        root.bind("<KeyPress>", self.on_key)
        root.bind("<KeyRelease>", self.on_release)
        root.bind("<FocusOut>", self.on_focus_out)
        self.canvas.bind("<Button-1>", self.on_click)
        self.canvas.bind("<Motion>", self.on_motion)
        self.canvas.bind("<MouseWheel>", self.on_wheel)
        root.protocol("WM_DELETE_WINDOW", self.request_close)
        self.canvas.focus_set()
        self.frame()
```

### launcher.py的logical

event.x/y是Canvas實際座標；先減黑邊偏移，再除縮放倍率，得到1280×720邏輯座標。

```python
    def logical(self, event):
        return ((event.x - self.ox) / self.scale, (event.y - self.oy) / self.scale)
```

### launcher.py的on_motion

只記住滑鼠邏輯座標，下一次draw依它計算按鈕懸停框。

```python
    def on_motion(self, event):
        self.pointer = self.logical(event)
```

### launcher.py的on_click

依目前buttons做一次命中判斷；觸發動作立即return，避免切頁後同一次點擊再觸發別的頁面。

```python
    def on_click(self, event):
        point = self.logical(event)
        # 本次點擊只對當前頁面做一次命中；回呼即使切頁，也立即結束此事件。
        for rect, action, hotkey, enabled in self.buttons:
            x, y, w, h = rect
            if enabled and x <= point[0] <= x + w and y <= point[1] <= y + h:
                self.activate(action)
                return "break"
```

### launcher.py的on_release

鬆開鍵時，同時清除keys和blocked；下一次按下才會重新產生有效輸入。

```python
    def on_release(self, event):
        key = event.keysym.lower()
        self.keys.discard(key)
        self.blocked.discard(key)
```

### launcher.py的on_focus_out

用after_idle等本輪焦點事件處理完，再判斷是否真的離開這個主視窗。

```python
    def on_focus_out(self, event):
        #先等焦點在同一視窗內轉移完成，避免誤將內部焦點變更當成失焦。
        self.root.after_idle(self.check_focus)
```

### launcher.py的check_focus

確定失焦才暫停核心、清空按鍵、封鎖舊輸入並清除物理累積時間。

```python
    def check_focus(self):
        if self.closed:
            return
        focused = self.root.focus_get()
        if focused is None or focused.winfo_toplevel() != self.root:
            self.blocked.update(self.keys)
            self.keys.clear()
            if self.core:
                self.core.command("pause")
                self.refresh_state()
                self.acc = 0
                self.previous_state = None
                self.focus = 0
```

### launcher.py的on_key

keys保存持續按住狀態；重複KeyPress不重複觸發按鈕。移動鍵留給advance，選單鍵在此處理。

```python
    def on_key(self, event):
        key = event.keysym.lower()
        repeated = key in self.keys
        self.keys.add(key)
        if repeated:
            return "break"
        if key == "escape":
            self.escape()
            return "break"
        if self.page == "match" and self.state.phase not in (Phase.PAUSED, Phase.LEAVE, Phase.OVER):
            return
        enabled = [b for b in self.buttons if b[3]]
        if self.page == "records" and key in ("up", "down"):
            self.scroll = min(max(0, len(self.rows) - 3), max(0, self.scroll + (1 if key == "down" else -1)))
        elif key in ("tab", "up", "down") and enabled:
            #event.state的第0位元表示Shift；Shift+Tab或Up反向移焦。
            backwards = key == "up" or (key == "tab" and bool(event.state & 1))
            #餘數讓焦點首尾循環；Python對正除數的%結果不會是負索引。
            self.focus = (self.focus + (-1 if backwards else 1)) % len(enabled)
        elif key in ("return", "kp_enter") and enabled:
            # 設定頁 Enter 永遠啟動；其他頁面觸發焦點選項。
            #設定頁Enter固定開賽；其他頁面Enter觸發焦點所在項目。
            action = "start" if self.page == "settings" else enabled[self.focus % len(enabled)][1]
            self.activate(action)
        else:
            for rect, action, hotkey, active in self.buttons:
                if active and key == hotkey:
                    self.activate(action)
                    break
        return "break"
```

### launcher.py的on_wheel

戰績頁滾輪只改scroll索引，範圍限制在0到最後可顯示3筆的起點。

```python
    def on_wheel(self, event):
        if self.page == "records":
            self.scroll = min(max(0, len(self.rows) - 3),
                              max(0, self.scroll + (-1 if event.delta > 0 else 1)))
```

### launcher.py的escape

依page與phase決定Esc用途：返回、取消離開、暫停或結算返回；避免一律關閉視窗。

```python
    def escape(self):
        if self.page == "menu":
            self.shutdown()
        elif self.page in ("settings", "records"):
            self.change_page("menu")
        elif self.page == "clear":
            self.change_page("records")
        elif self.state.phase == Phase.LEAVE:
            self.activate("cancel")
        elif self.state.phase == Phase.OVER:
            if not self.dirty:
                self.activate("main")
        elif self.state.phase != Phase.PAUSED:
            self.activate("pause")
```

### launcher.py的request_close

視窗×：未完賽先LEAVE確認；已完賽但dirty時要求先重試保存；其餘可正常關閉。

```python
    def request_close(self):
        if not self.core:
            self.shutdown()
        elif self.state.phase == Phase.OVER:
            if self.dirty:
                self.message = "SAVE THE RESULT BEFORE EXITING"
            else:
                self.shutdown()
        else:
            self.close_requested = True
            self.activate("leave")
```

### launcher.py的change_page

切換Python頁面、清理焦點/累積量/插值、封鎖仍按住的鍵，立即重建新頁面命中區。

```python
    def change_page(self, page):
        self.page = page
        self.focus = 0
        self.message = ""
        self.acc = 0
        self.previous_state = None
        self.blocked.update(self.keys)
        self.draw()  # 立即重建命中區，下一個事件只會看到新頁面
```

### launcher.py的refresh_state

從DLL取得最新快照；state是Python端資料副本，繪圖只讀這份資料。

```python
    def refresh_state(self):
        self.state = self.core.snapshot()
```

### launcher.py的save_result

先標記dirty；存檔成功才清除，失敗保留本局和RETRY SAVE入口，並寫log。

```python
    def save_result(self):
        self.dirty = True
        try:
            self.store.record(self.draft, self.state, self.session)
        except (OSError, ValueError) as error:
            self.message = "UNABLE TO SAVE RESULT - RETRY SAVE"
            self.log(error)
        else:
            self.dirty = False
            self.message = ""
```

### launcher.py的activate

動作的統一入口；將可預期錯誤轉成畫面訊息，處理後重畫當前頁面。

```python
    def activate(self, action):
        try:
            self.perform(action)
        except (OSError, ValueError, RuntimeError) as error:
            self.message = "ACTION FAILED - CHECK DATA FILES / DLL"
            self.log(error)
        if not self.closed:
            self.draw()
```

### launcher.py的perform

依動作建立/關閉Core、改設定或切頁。tuple代表設定欄位和值，字串代表按鈕動作。

```python
    def perform(self, action):
        if isinstance(action, tuple):
            key, value = action
            self.draft[key] = value
            return
        if action in ("pvcpu", "pvp"):
            self.draft = self.store.settings(action)
            self.change_page("settings")
        elif action == "start":
            # DLL 建立成功才保存設定並切頁，失敗時清理暫存的核心。
            core = Core(self.draft)
            try:
                self.store.save_settings(self.draft)
            except Exception:
                core.close()
                raise
            #設定保存成功後才將新核心交給App持有；先前失敗會關閉暫存核心。
            self.core = core
            self.session = uuid.uuid4().hex
            self.dirty = False
            self.close_requested = False
            self.over_age = 0
            self.refresh_state()
            self.change_page("match")
        elif action == "records":
            self.rows = self.store.history()
            self.scroll = 0
            self.change_page("records")
        elif action == "clear":
            self.change_page("clear")
        elif action == "confirm_clear":
            self.store.clear()
            self.rows = []
            self.scroll = 0
            self.change_page("records")
        elif action == "back":
            self.change_page("records" if self.page == "clear" else "menu")
        elif action == "exit":
            self.request_close()
        elif action == "main":
            if self.dirty:
                return
            if self.state.phase != Phase.OVER:
                self.close_requested = False
                self.perform("leave")
                return
            self.core.close()
            self.core = None
            self.change_page("menu")
        elif action == "confirm_leave":
            if self.close_requested:
                self.shutdown()
                return
            self.core.close()
            self.core = None
            self.change_page("menu")
        elif action == "retry":
            self.save_result()
        else:
            self.blocked.update(self.keys)
            self.acc = 0
            self.focus = 0
            self.core.command(action)
            self.refresh_state()
            self.previous_state = None
            if action == "cancel":
                self.close_requested = False
```


## 8｜固定更新、插值、排程與圖形快取

### 8.1方向與核心更新

direction回傳有效向下鍵減向上鍵。advance選玩家鍵位並送到對應左右側，CPU側傳0交核心。elapsed累積進acc，一次消耗STEP最多8步。每步保存前快照、tick、讀新快照，事件以|合併。碰撞或階段切換清插值；完賽停止物理，單人提交一次戰績，再播本幀音效。

### 8.2顯示插值

alpha=acc/STEP，前球心100、目前110、alpha0.5時畫105。只改呈現，不寫回DLL。最多增加1個Step約8.33ms呈現延遲，讓非整數步邊界的畫面更連續。非Rally或碰撞當步直接顯示目前位置。

### 8.3排程

frame先量實際經過秒數，advance、draw後再依next_frame目標時刻計算等待，避免繪圖耗時累加在固定16ms之後。DRAW_INTERVAL=1/120，逾時跳過過期呈現時刻。Windows在程式存活期間要求timeBeginPeriod(1)，關閉時timeEndPeriod成對歸還。

### 8.4快取

paint依畫圖順序保存Canvas item ID、kind、coordinates和options。種類相同只更新變動部分，未使用項目隱藏；建立新項目時才重排層次。rect/text負責邏輯轉實際座標。這讓正式版不需每幀delete("all")。

同一台電腦、1280×720、相同240幀移動軌跡比較，舊版平均幀距32.38ms、繪圖4.65ms；新版平均8.31ms、p95為9.09ms、繪圖0.23ms。這是回呼耗時，實際顯示仍受螢幕刷新與合成影響。
### launcher.py的direction

向下按住值減向上按住值，回傳-1/0/1；blocked中的舊按鍵不採用。

```python
    def direction(self, up, down):
        return int(down in self.keys and down not in self.blocked) - int(up in self.keys and up not in self.blocked)
```

### launcher.py的advance

elapsed是實際幀秒數。累積到acc，最多8次固定STEP更新；合併音效事件並在完賽時保存戰績。

```python
    def advance(self, elapsed):
        if not self.core:
            return
        phase = self.state.phase
        if phase == Phase.OVER:
            self.over_age = min(.18, self.over_age + elapsed)
            return
        if phase in (Phase.PAUSED, Phase.LEAVE):
            self.acc = 0
            return
        left, right = self.direction("w", "s"), self.direction("up", "down")
        if self.draft["mode"] == "pvcpu":
            move = left if self.draft["input_preset"] == "preset1" else right
            left = move if self.draft["player_side"] == "left" else 0
            right = move if self.draft["player_side"] == "right" else 0
        #最多保留8個物理步長，避免長幀後無限制補算造成事件迴圈卡住。
        self.acc = min(self.acc + elapsed, 8 * STEP)
        events = 0
        for _ in range(8):
            #1e-7秒是浮點比較容許值，累積時間不足一個STEP就停止更新。
            if self.acc + 1e-7 < STEP:
                break
            self.acc = max(0, self.acc - STEP)
            #更新前的快照保留給畫面插值；新快照由refresh_state另行取得。
            self.previous_state = self.state
            step_events = self.core.tick(left, right)
            #用|合併本幀各子步的事件，音效只在整幀更新後觸發一次。
            events |= step_events
            self.refresh_state()
            # 碰撞會改變運動方向，該小步直接顯示新位置，避免插值穿入拍面或牆。
            #碰撞或狀態切換會打斷軌跡，清除插值避免球被畫進拍面。
            if step_events or self.previous_state.phase != self.state.phase:
                self.previous_state = None
            if self.state.phase == Phase.OVER:
                self.acc = 0
                self.focus = 0
                self.over_age = 0
                if self.draft["mode"] == "pvcpu":
                    self.save_result()
                break
        self.audio.play(events, self.draft["sound_enabled"])
```

### launcher.py的frame

每次Tk回呼先量時間、advance再draw，最後按next_frame目標時刻排程；繪圖耗時包含在週期內。

```python
    def frame(self):
        if self.closed:
            return
        now = time.perf_counter()
        elapsed = min(now - self.last_frame, 8 * STEP)
        self.last_frame = now
        try:
            self.advance(elapsed)
            self.draw()
        except Exception as error:
            self.log(error)
            if self.core:
                self.core.command("pause")
                self.refresh_state()
            self.message = "UPDATE FAILED - SEE GAME.LOG"
        if not self.closed:
            # 以目標時刻排程，繪圖花費包含在幀週期內，逾時則跳過過期的呈現時刻。
            #累加目標呈現時刻，不把每次繪圖耗時額外加在等待時間後。
            self.next_frame += DRAW_INTERVAL
            now = time.perf_counter()
            if self.next_frame <= now:
                self.next_frame += (math.floor((now-self.next_frame)/DRAW_INTERVAL)+1)*DRAW_INTERVAL
            #Tk的after使用整數毫秒；向上取整並至少等待1ms，避免忙迴圈。
            delay_ms = max(1, math.ceil((self.next_frame-now)*1000))
            self.after_id = self.root.after(delay_ms, self.frame)
```

### launcher.py的paint

kind為rectangle/text/oval，coordinates為實際座標；重用對應Canvas項目，只送出變更的屬性。

```python
    def paint(self, kind, coordinates, **options):
        # 每個繪圖位置重用一個 Canvas item；只更新真的改變的座標、文字或顏色。
        index = self.item_cursor
        self.item_cursor += 1
        coordinates = tuple(coordinates)
        entry = self.items[index] if index < len(self.items) else None
        if entry is None or entry["kind"] != kind:
            if entry is not None:
                self.canvas.delete(entry["id"])
            item_id = getattr(self.canvas, "create_" + kind)(*coordinates, **options)
            entry = dict(id=item_id, kind=kind, coordinates=coordinates, options=options, hidden=False)
            if index < len(self.items):
                self.items[index] = entry
            else:
                self.items.append(entry)
            self.layout_changed = True
            return
        if entry["coordinates"] != coordinates:
            self.canvas.coords(entry["id"], *coordinates)
            entry["coordinates"] = coordinates
        if entry["options"] != options:
            self.canvas.itemconfigure(entry["id"], **options)
            entry["options"] = options
        if entry["hidden"]:
            self.canvas.itemconfigure(entry["id"], state="normal")
            entry["hidden"] = False
```

### launcher.py的finish_paint

本次未用到的舊項目隱藏；只有建立/替換項目時，才依繪圖順序重排圖層。

```python
    def finish_paint(self):
        for entry in self.items[self.item_cursor:]:
            if not entry["hidden"]:
                self.canvas.itemconfigure(entry["id"], state="hidden")
                entry["hidden"] = True
        if self.layout_changed:
            for entry in self.items[:self.item_cursor]:
                self.canvas.tag_raise(entry["id"])
```

### launcher.py的rect

x/y/w/h都是邏輯尺寸；乘scale並加ox/oy後，才交給Canvas畫矩形。

```python
    def rect(self, x, y, w, h, fill=BLACK, outline="", width=2):
        s = self.scale
        self.paint("rectangle", (self.ox+x*s, self.oy+y*s,
                                 self.ox+(x+w)*s, self.oy+(y+h)*s),
                   fill=fill, outline=outline, width=max(1, width*s))
```

### launcher.py的text

label是文字；x/y是邏輯位置，負字級讓Tk以像素計算，隨scale等比縮放。

```python
    def text(self, label, x, y, size=28, fill=WHITE, anchor="n"):
        self.paint("text", (self.ox+x*self.scale, self.oy+y*self.scale),
                   text=label, fill=fill, anchor=anchor,
                   font=("Consolas", -max(1, round(size*self.scale)), "bold"))
```

### launcher.py的render_positions

以前後物理快照和acc/STEP做顯示插值；碰撞/切階段清除前快照，且不修改DLL資料。

```python
    def render_positions(self):
        current = self.state
        previous = self.previous_state
        if previous is None or current.phase != Phase.RALLY or previous.phase != Phase.RALLY:
            return current.left_y, current.right_y, current.ball_x, current.ball_y
        #alpha是剩餘時間占一個物理步的比例，0顯示前位置，1接近目前位置。
        alpha = min(1, max(0, self.acc / STEP))
        #線性插值=前值+(目前值-前值)×比例；只影響畫面，不改DLL。
        return tuple(getattr(previous, name) + (getattr(current, name)-getattr(previous, name))*alpha
                     for name in ("left_y", "right_y", "ball_x", "ball_y"))
```


## 9｜正式繪圖、聲音與完整遊戲

### 9.1各頁怎麼畫

draw計算scale/ox/oy後依page分派。button同時畫外觀和保存命中區，disabled不列入可操作焦點。row把928px扣除15px間距再平均分配，白底黑字顯示draft選取。

draw_settings單人有難度/鍵位/側別，雙人固定左W/S右方向鍵；兩者都有目標分/音效/發球。START保存，BACK放棄草稿。

draw_records用全部rows算摘要，只畫scroll起3筆。超過3筆畫捲軸，空列表停用清除。clear確認畫在上層且重建buttons，只保留CANCEL/CLEAR，避免命中底層。

draw_match依phase繪球場。暫停底場用saved，intro在最初0.25秒淡入、後2秒提示。Serve/Resume收比分畫3/2/1，ceil向上取整，pulse改單個數字亮度。Point突出得分方，hit_timer畫最近命中拍亮框。Paused/Leave顯示暗場面板，Over純黑結算，dirty只給RETRY SAVE。

### 9.2所有繪圖方法

### launcher.py的button

畫按鈕並記錄命中區、action、快捷鍵和enabled。focus索引只計算可操作項目。

```python
    def button(self, rect, label, action, hotkey="", selected=False, enabled=True, size=28):
        x, y, w, h = rect
        hover = x <= self.pointer[0] <= x+w and y <= self.pointer[1] <= y+h
        index = sum(b[3] for b in self.buttons)
        keyboard_focus = enabled and self.focus == index and self.page != "settings"
        outline = CYAN if enabled and (hover or keyboard_focus) else WHITE if enabled else "#555555"
        self.rect(x, y, w, h, WHITE if selected else BLACK, outline)
        self.text(label, x+w/2, y+(h-size)/2-2, size, BLACK if selected else WHITE if enabled else "#555555")
        self.buttons.append((rect, action, hotkey, enabled))
```

### launcher.py的row

label是設定列標題，field是字典欄位，choices每項保存顯示文字、實際值與快捷鍵。

```python
    def row(self, label, y, field, choices):
        self.text(label, 59, y+10, 25, anchor="nw")
        #一列可用寬928px，選項間隔15px，剩餘寬度平均分配。
        width = (928 - 15*(len(choices)-1)) / len(choices)
        for i, (caption, value, key) in enumerate(choices):
            self.button((293+i*(width+15), y, width, 50), caption,
                        (field, value), key, self.draft[field] == value)
```

### launcher.py的draw_settings

依單/雙人顯示可選設定；每列由row處理，Enter固定START，Esc放棄草稿返回。

```python
    def draw_settings(self):
        solo = self.draft["mode"] == "pvcpu"
        self.text("1 PLAYER" if solo else "2 PLAYERS", 640, 33, 52)
        if solo:
            self.row("DIFFICULTY",119,"difficulty",
                     [("[E] EASY","easy","e"),("[N] NORMAL","normal","n"),("[H] HARD","hard","h")])
        else:
            for x, title, keys in ((59,"PLAYER 1 (LEFT)","W / S"),(653,"PLAYER 2 (RIGHT)","UP / DOWN")):
                self.rect(x,116,568,118,outline=WHITE)
                self.text(title,x+284,136)
                self.text(keys,x+284,183,30)
        self.row("PLAY TO",190 if solo else 274,"winning_score",
                 [("[1] 5",5,"1"),("[2] 7",7,"2"),("[3] 11",11,"3")])
        if solo:
            self.row("CONTROLS",260,"input_preset",
                     [("[A] ARROW KEYS","preset2","a"),("[W] W / S","preset1","w")])
            self.row("YOUR SIDE",330,"player_side",
                     [("[L] LEFT","left","l"),("[R] RIGHT","right","r")])
        self.row("SOUND",400 if solo else 354,"sound_enabled",[("[O] ON",True,"o"),("[F] OFF",False,"f")])
        self.row("SERVE TO",462 if solo else 422,"serve_to",
                 [("[U] LOSER","loser","u"),("[G] SCORER","scorer","g"),("[X] RANDOM","random","x")])
        self.button((455,573,370,58),"[ENTER] START","start","return",size=31)
        self.button((540,646,200,42),"[ESC] BACK","back","escape",size=23)
```

### launcher.py的draw_records

摘要由全部有效紀錄計算；列表只顯示scroll起的3筆。清除確認另建按鈕，避免點到底層。

```python
    def draw_records(self):
        self.text("RECORDS",640,35,51)
        count = len(self.rows)
        wins = sum(r["winner"] == "player" for r in self.rows)
        for i, (label, value) in enumerate(zip(("GAMES","WINS","LOSSES","WIN RATE"),
                                              (count,wins,count-wins,f"{100*wins/count:.1f}%" if count else "-"))):
            x = 75+288*i
            self.rect(x,122,267,118,outline="#888888")
            self.text(label,x+133,140,24)
            self.text(str(value),x+133,178,36)
        columns = (220,530,810,1060)
        for x, label in zip(columns,("DATE","DIFFICULTY","FINAL SCORE","RESULT")):
            self.text(label,x,283,24)
        for i in range(4):
            self.rect(75,326+i*62,1131,1,fill="#555555")
        for i, row in enumerate(self.rows[self.scroll:self.scroll+3]):
            values = (row["timestamp"][:10],row["difficulty"].upper(),
                      f'{row["player_score"]} : {row["cpu_score"]}',
                      "WIN" if row["winner"] == "player" else "LOSS")
            for x, value in zip(columns,values):
                self.text(value,x,345+i*62,24)
        if not count:
            self.text("NO RECORDS YET",640,408,26)
        if count > 3:
            h = max(24,186*3/count)
            self.rect(1217,326,10,186,fill="#333333")
            self.rect(1217,326+(186-h)*self.scroll/(count-3),10,h,fill=GRAY)
        self.text("ALL MATCHES",640,537,19,GRAY)
        self.button((325,602,340,65),"[C] CLEAR RECORDS","clear","c",enabled=bool(count))
        self.button((695,602,260,65),"[ESC] BACK","back","escape")
        if self.page == "clear":
            #每項為(rect, action, hotkey, enabled)，只對當前畫面做命中。
            self.buttons = []
            self.rect(410,226,460,268,fill="#151515",outline=GRAY)
            self.text("CLEAR ALL RECORDS?",640,253,36)
            self.button((474,326,332,54),"CANCEL","back",selected=self.focus == 0)
            self.button((474,400,332,54),"CLEAR","confirm_clear",selected=self.focus == 1)
```

### launcher.py的draw_match

依phase畫球場、暫停/離開/結算；位置來自顯示插值，比分/勝負仍使用核心快照。

```python
    def draw_match(self):
        s = self.state
        left_y, right_y, ball_x, ball_y = self.render_positions()
        phase = Phase(s.phase)
        overlay = phase in (Phase.PAUSED,Phase.LEAVE,Phase.OVER)
        scene = Phase(s.saved) if phase in (Phase.PAUSED,Phase.LEAVE) else phase
        solo = self.draft["mode"] == "pvcpu"
        p = int(self.draft["player_side"] == "right")
        intro = min(1,max(0,(2.25-s.timer)/.25)) if scene == Phase.INTRO else 1
        # Canvas 用灰階模擬黑底淡入；暫停時讓場地元素變暗。
        brightness = intro * (.66 if phase in (Phase.PAUSED,Phase.LEAVE) else 1)
        white = "#" + f"{round(255*brightness):02x}"*3
        gray = "#" + f"{round(80*brightness):02x}"*3
        if scene != Phase.OVER:
            for y in range(135,658,36):
                self.rect(639,y,3,18,fill=gray)
            for i, y in enumerate((left_y,right_y)):
                self.rect((48 if i == 0 else 1232)-9,y-56,18,112,fill=white)
                if s.hit_timer > 0 and s.last_bat == i:
                    self.rect((48 if i == 0 else 1232)-9,y-56,18,112,fill=white,outline=GRAY,width=3)
            if scene not in (Phase.SERVE,Phase.RESUME) or overlay:
                names = ["PLAYER 1","PLAYER 2"]
                if solo:
                    names = ["YOU (LEFT)" if p == 0 else "COMPUTER",
                             "YOU (RIGHT)" if p == 1 else "COMPUTER"]
                for i, (name, score) in enumerate(zip(names,(s.left_score,s.right_score))):
                    self.text(name,881 if i else 399,34,36,white)
                    size = 49 if scene == Phase.POINT and s.timer > .13 and s.last == i else 44
                    self.text(str(score),686 if i else 594,53-size/2,size,white)
                self.text(":",640,31,44,white)
                self.text(f'FIRST TO {self.draft["winning_score"]}',640,93,22,white)
            if not overlay:
                self.text("ESC PAUSE",640,674,22,white)
            k = self.scale
            self.paint("oval", (self.ox+(ball_x-10)*k,self.oy+(ball_y-10)*k,
                                self.ox+(ball_x+10)*k,self.oy+(ball_y+10)*k),
                       fill=white,outline="")
            if scene == Phase.INTRO and s.timer <= 2:
                if solo:
                    self.text("YOU ARE ON THE RIGHT" if p else "YOU ARE ON THE LEFT",640,245,30,white)
                    self.text("W / S TO MOVE" if self.draft["input_preset"] == "preset1" else "UP / DOWN TO MOVE",
                              1040 if p else 240,375,24,white)
                else:
                    for x, name, keys in ((220,"PLAYER 1","W / S"),(1060,"PLAYER 2","UP / DOWN")):
                        self.text(name,x,314,28,white)
                        self.text(keys,x,365,32,white)
            if not overlay and scene in (Phase.SERVE,Phase.RESUME):
                #ceil將剩餘秒數向上取整，2.1顯示3；限制顯示1～3。
                number = max(1,min(3,math.ceil(s.timer)))
                #每個數字的亮度在約0.55～1變化，讓倒數閃動但不閃整個場地。
                pulse = .55+.45*(s.timer-(number-1))
                color = "#" + f"{max(0,min(255,round(pulse*255))):02x}"*3
                self.rect(582,199,116,117)
                self.text(str(number),640,200,102,color)
        if phase in (Phase.PAUSED,Phase.LEAVE):
            self.rect(410,172,460,307 if phase == Phase.LEAVE else 383,fill="#151515",outline=GRAY,width=3)
            self.text("LEAVE MATCH?" if phase == Phase.LEAVE else "PAUSED",640,202,48)
            choices = [("CANCEL","cancel"),("LEAVE","confirm_leave")] if phase == Phase.LEAVE else [
                ("RESUME","resume"),("RESTART","restart"),("MAIN MENU","main")]
            for i, (label, action) in enumerate(choices):
                self.button((474,289+79*i,332,57),label,action,selected=i == self.focus)
        elif phase == Phase.OVER:
            winner = ("YOU WIN" if s.last == p else "COMPUTER WINS") if solo else f"PLAYER {s.last+1} WINS"
            fade = "#" + f"{round(255*min(1,self.over_age/.18)):02x}"*3
            self.text(winner,640,119,61,fade)
            self.text("FINAL SCORE",640,250,34)
            self.text(f"{s.left_score} : {s.right_score}",640,304,60)
            choices = [("RETRY SAVE","retry")] if self.dirty else [("PLAY AGAIN","restart"),("MAIN MENU","main")]
            for i, (label, action) in enumerate(choices):
                self.button((385,426+104*i,510,75),label,action,selected=i == self.focus)
```

### launcher.py的draw

依目前Canvas尺寸計算scale與黑邊，再依page繪圖；最後隱藏剩餘快取項目。

```python
    def draw(self):
        width, height = max(1,self.canvas.winfo_width()), max(1,self.canvas.winfo_height())
        #取寬高縮放比的較小值維持16:9；下限避免初始化時除以0。
        self.scale = max(.001,min(width/W,height/H))
        #ox/oy是置中畫布的黑邊偏移；畫圖加上它，滑鼠反向換算時減掉。
        self.ox, self.oy = (width-W*self.scale)/2, (height-H*self.scale)/2
        self.item_cursor = 0
        self.layout_changed = False
        #每項為(rect, action, hotkey, enabled)，只對當前畫面做命中。
        self.buttons = []
        if self.page == "menu":
            self.text("PONG",640,68,120)
            for i, (label, action, key) in enumerate((("[1]  1 PLAYER","pvcpu","1"),
                                                      ("[2]  2 PLAYERS","pvp","2"),
                                                      ("[R]  RECORDS","records","r"),("[ESC]  EXIT","exit","escape"))):
                rect = (380,270+84*i,520,66) if i < 2 else (470,475+62*(i-2),340,48)
                self.button(rect,label,action,key,size=39 if i < 2 else 28)
        elif self.page == "settings":
            self.draw_settings()
        elif self.page in ("records","clear"):
            self.draw_records()
        else:
            self.draw_match()
        if self.message:
            self.text(self.message,640,690,16)
        self.finish_paint()
```


### 9.3音效

Audio生成48kHz、16-bit、單聲道WAV。相位每取樣增加2π×頻率/48000，sin得到波形，以小端序有號16-bit保存。頭尾192取樣等於4ms淡入淡出。得分音480→720Hz，撞牆/擊拍為固定短音。

play按Score→Paddle→Wall挑一種，以SND_ASYNC播放，不阻塞Tk。關音或音訊失敗不影響遊戲。close先停止播放再清暫存檔。

### sound.py的__init__

在系統暫存目錄合成3段48kHz、16-bit、單聲道WAV；索引0撞牆、1擊拍、2得分。

```python
    def __init__(self):
        self.temp = tempfile.TemporaryDirectory(prefix="pong-audio-")
        self.paths = []
        try:
            for i, (start, end, count) in enumerate(((300, 300, 1680), (680, 680, 2160), (480, 720, 7680))):
                path = Path(self.temp.name) / f"{i}.wav"
                phase = 0
                pcm = bytearray()
                for n in range(count):
                    hz = start + (end - start) * n / count
                    #一個取樣前進2π×頻率/取樣率弧度，累積相位才能連續產生波形。
                    phase += 2 * math.pi * hz / 48000
                    #頭尾各192個取樣淡入淡出，192/48000=0.004秒，減少突然起落的爆音。
                    envelope = min(1, n / 192, (count - 1 - n) / 192)
                    #將波形轉成小端序16-bit有號整數，寫入WAV音訊資料。
                    pcm += struct.pack("<h", int(7000 * envelope * math.sin(phase)))
                with wave.open(str(path), "wb") as wav:
                    wav.setparams((1, 2, 48000, 0, "NONE", "not compressed"))
                    wav.writeframes(pcm)
                self.paths.append(path)
        except OSError:
            self.paths = []
```

### sound.py的play

events是事件位元集合；優先Score，再Paddle，再Wall。SND_ASYNC讓播放不阻塞Tk事件迴圈。

```python
    def play(self, events, enabled):
        if not events or not enabled or not self.paths:
            return
        #三元判斷按得分→擊拍→撞牆挑一種；&檢查events是否包含該事件位元。
        index = 2 if events & 4 else 1 if events & 2 else 0
        try:
            winsound.PlaySound(str(self.paths[index]), winsound.SND_FILENAME |
                               winsound.SND_ASYNC | winsound.SND_NODEFAULT)
        except (OSError, RuntimeError):
            pass
```

### sound.py的close

先停止目前音效，才清理TemporaryDirectory中的WAV，避免播放中的檔案仍被使用。

```python
    def close(self):
        try:
            winsound.PlaySound(None, 0)
        except RuntimeError:
            pass
        self.temp.cleanup()
```


### 9.4錯誤與生命週期

log把例外寫入game.log，寫log失敗時避免再丟第二個例外。shutdown取消after、關Core、停Audio、解鎖Store、歸還計時要求、destroy唯一視窗。closed防止重複清理。

### launcher.py的log

將例外種類和原因附加到資料目錄game.log；log本身寫入失敗時不再拋出第二個例外。

```python
    def log(self, error):
        try:
            with (self.store.directory / "game.log").open("a",encoding="utf-8") as out:
                out.write(f"{type(error).__name__}: {error}\n")
        except OSError:
            pass
```

### launcher.py的shutdown

避免重複清理；取消after、釋放DLL核心、停止音效、解鎖存檔、歸還計時精度，再關閉Tk。

```python
    def shutdown(self):
        if self.closed:
            return
        self.closed = True
        if self.after_id:
            self.root.after_cancel(self.after_id)
        if self.core:
            self.core.close()
        self.audio.close()
        self.store.close()
        #成功要求高解析度計時後，關閉時才呼叫timeEndPeriod成對歸還。
        if self.timer_resolution:
            ctypes.windll.winmm.timeEndPeriod(1)
            self.timer_resolution = False
        self.root.destroy()
```


### 9.5啟動正式版

```powershell
python launcher.py --data-dir .\lesson-data
```

驗證順序：單人設定→5分→START→Esc→MAIN MENU→CANCEL，確認原局保留。再選LEAVE停在主選單。完賽→PLAY AGAIN→中止第二局，戰績只加完成局。鍵盤和滑鼠各做一次，全程同一視窗。再測SOUND OFF、左右側、兩套鍵位、最小尺寸、最大化和還原。


## 10｜測試、驗收與常見問題

建立verify.py，執行：
```powershell
.\build.ps1
python core_check.py
python verify.py
```

check_core實際載入DLL，檢查ABI、對打、暫停/恢復、重開、無效dt。check_app使用暫存資料夾和真實Tk，測視窗ID、滑鼠退出重疊、完賽去重、中止局、清除取消/確認、pending恢復、鎖衝突、3種視窗比例、Canvas ID重用、插值不改核心。

| 檢查 | 操作與預期 |
|---|---|
| 主選單/設定 | 快捷鍵、Tab、滑鼠一致，反白正確，BACK放棄草稿 |
| 開場/倒數 | 玩家側別清楚，3/2/1期間球依階段靜止 |
| 對打 | 中央/拍緣/快速移拍反彈，速率有上限 |
| 暫停/失焦 | 原位置保留，切回不自動恢復或延續舊鍵 |
| 結算 | 再玩清分，完成局入帳一次，保存失敗可重試 |
| 戰績 | 玩家:CPU順序、摘要、捲動、清除確認 |
| 視窗 | 640×360、960×720、最大化還原保持16:9與命中 |

| 問題 | 先查 |
|---|---|
| DLL找不到/WinError193 | 同層路徑、是否build、Python/DLL位元數 |
| 找不到CheckCollisionCircleRec | -I/-L/-lraylib和套件路徑 |
| DLL不能覆蓋 | 是否仍有遊戲載入 |
| GUI無回應 | 回呼內是否用了長while/sleep |
| 返回自動進其他模式 | 點擊後是否return、命中區是否重建 |
| 球移動跳動 | 正式排程/快取/插值版本，記錄實際幀距 |
| 戰績讀寫失敗 | data權限、game.log、JSON與pending |
| 重複/遺失場次 | session/match_id、提交和去重順序 |

自動測試不能代替真實音訊硬體、其他DPI和CPU難度/掌控感試玩。每項記版本、步驟、預期、實際、證據，未執行項目留待驗證。


## 11｜封裝、安裝與最終成果

建立package.ps1、app.manifest和Pong.iss。第10章通過後，用相同來源打包。
```powershell
python -m pip install pyinstaller
.\package.ps1
.\dist\Pong\Pong.exe --data-dir .\packaged-test-data
```

package先build，再以PyInstaller --windowed --onedir封裝。--add-binary包含pong_core.dll，--manifest嵌入DPI。dist/Pong要整個交付，_internal有Python/Tk執行時與DLL，licenses保留raylib授權。

app.manifest是Windows的PerMonitorV2宣告，xmlns網址不加角括號。本版PyInstaller直接嵌入，不需app.rc或windres。launcher仍在Tk建立前設定DPI，畫圖和滑鼠共用Canvas座標。

用Inno Setup編譯Pong.iss，或ISCC.exe Pong.iss。Files遞迴收錄dist/Pong，Icons建立開始功能表與選用桌面捷徑，Run可在安裝後啟動。輸出release/Pong-Tk-Setup-3.0.0-win-x64.exe。

在乾淨Windows測一般使用者安裝、捷徑、遊玩、保存、再次啟動和解除安裝。存檔在LOCALAPPDATA/Pong，和安裝位置分開。

最終核對：能重建DLL、跑Python入口、跑封裝入口、產生安裝檔；所有入口用相同核心；全程一個Tk視窗；完賽只入帳一次；中止不入帳；高速碰拍、滑鼠返回和縮放通過。

本次既有註解保留，新增部分才採你指定的新格式。原本註解可能有自己的空格或措辭，未統一改寫。對應原始碼已同步至GitHub主分支；組員請以本頁閱讀順序和根目錄的Python入口為準。
