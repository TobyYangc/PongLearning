# Pong組員入口｜下載、建置與閱讀順序

目前主分支為Python/Tkinter單一主視窗＋C++20 DLL。球場、選單、設定、暫停、結算與戰績都在同一個Tk視窗。raylib5.5只在DLL內提供CheckCollisionCircleRec碰撞函式。

[組員入口](https://hackmd.io/7eoGOVjXTPapJ91ctSQgmg)｜[引導主文](https://hackmd.io/tGkm7Yn-TnSZjNuPpCFwDg)｜[C++附錄](https://hackmd.io/43y2pN6dSZKgqRBGcmbcHg)｜[Python附錄](https://hackmd.io/46KzKWO9TR2zgItJh1M20g)｜[GitHub原始碼](https://github.com/TobyYangc/PongLearning)

## 組員請依這個順序閱讀

1. 先看[組員入口手冊](https://hackmd.io/7eoGOVjXTPapJ91ctSQgmg)，完成環境、下載及建置檢查。
2. 再看[引導主文](https://hackmd.io/tGkm7Yn-TnSZjNuPpCFwDg)，從第0章依序做到第11章；每步看到預期結果再繼續。
3. 第1～4章搭配[C++附錄](https://hackmd.io/43y2pN6dSZKgqRBGcmbcHg)，依序讀game.h→game.cpp→bridge.h→bridge.cpp→build.ps1。
4. 第4～9章搭配[Python附錄](https://hackmd.io/46KzKWO9TR2zgItJh1M20g)，依序讀core.py→play_step3.py→storage.py→launcher.py→sound.py。
5. 最後完成第10章驗證，再依第11章封裝。附錄供查完整檔案，主文負責逐步引導。

想先試玩可下載GitHub新版安裝檔；想跟著學習，請從組員入口與主文開始。

## 先試玩

[新版下載頁](https://github.com/TobyYangc/PongLearning/releases/tag/v3.0.0-tk.1)

- Pong-Tk-Setup-3.0.0-win-x64.exe：Windows x64安裝檔。
- Pong-Tk-win-x64.zip：可攜版，解壓縮後執行Pong/Pong.exe；Pong資料夾需完整保留，不能只取出exe。
- SHA256SUMS.txt：下載檔案校驗碼。

## 跟著建置

使用Windows x64，安裝包含Tkinter的64位元Python與MSYS2 UCRT64的g++工具鏈。若使用既有MSYS2，先在UCRT64終端確認工具鏈已安裝；工具安裝可參照[MSYS2官網](https://www.msys2.org/)。

先從[GitHub](https://github.com/TobyYangc/PongLearning)下載ZIP並解壓縮，或使用Git：
```powershell
git clone https://github.com/TobyYangc/PongLearning.git
Set-Location PongLearning
```

在解壓縮後或clone後的專案根目錄開PowerShell：
```powershell
$env:Path = "C:\msys64\ucrt64\bin;" + $env:Path
python --version
python -c "import tkinter, struct; print('Tk', tkinter.TkVersion, 'bits', struct.calcsize('P')*8)"
g++ --version
.\build.ps1
python core_check.py
python verify.py
python launcher.py --data-dir .\lesson-data
```

預期依序看到Core checks passed、Collision regressions passed、Python與DLL互通成功及單視窗整合測試通過，最後開啟遊戲。若從空資料夾逐步學習，依主文第0章建立練習資料夾，不必先貼整份最終檔案。

## 檔案與章節

|主文章節|閱讀檔案|要理解的事|
|---|---|---|
|0|step0.py|唯一視窗與Canvas|
|1～3|game.h、game.cpp、核心測試|資料、階段、CPU、物理與圓矩形碰撞|
|4|bridge.h、bridge.cpp、core.py|DLL、C ABI與ctypes|
|5|play_step3.py|最小可玩球場、按鍵與固定更新|
|6|storage.py|設定、戰績、原子保存與去重|
|7～9|launcher.py、sound.py|切頁、輸入、繪圖、插值、排程、音效|
|10|verify.py、test_game.cpp、test_collision.cpp|回歸與人工驗收|
|11|package.ps1、app.manifest、Pong.iss|封裝、DPI與安裝檔|

## 操作與驗收

主選單：1單人、2雙人、R戰績、Esc離開。雙人左W/S、右方向鍵；單人依設定選鍵位與側別。對局Esc暫停，恢復先倒數；離開確認取消應保留原局。

每位組員請測：鍵盤與滑鼠切頁、完賽後再玩與回主選單、球拍中央/拍緣、高速碰撞、最小視窗、最大化還原、音效，以及自己的DPI。全程主視窗不重建。完成單人局只入帳一次，中止局不入帳。測試用--data-dir獨立目錄。

## 協作方式

GitHub和本套HackMD教材公開閱讀，HackMD由擁有者維護；程式修改請fork後開Pull Request，說明改動、測試和重現步驟。既有自寫註解保留，新增註解沿用短用途行尾、複雜判斷多行說明的風格。

legacy/raylib-ui保存舊版純C++介面供歷史對照；docs/archive是舊版手冊，不適用目前主分支。現在請執行根目錄launcher.py。

## 回報格式

請在[GitHub Issues](https://github.com/TobyYangc/PongLearning/issues)填寫：使用版本、Windows/Python位元數、操作步驟、預期結果、實際結果和畫面；game.log只附相關且不含個人資料的錯誤內容。
