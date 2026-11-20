# Pong 組員操作、編譯與程式閱讀手冊

本手冊讓組員從取得專案開始，在自己的 Windows 電腦建置、遊玩、閱讀程式並記錄報告。專案使用 C++20、raylib 5.5 和 nlohmann/json 3.11.3。

[TOC]

## 1. 取得專案與權限

專案預計使用 GitHub 私人儲存庫。組員先用自己的 GitHub 帳號接受 Collaborator 邀請，才能存取專案。邀請需要組員的 GitHub 帳號，由專案擁有者在 Settings → Collaborators 加入。

只想先閱讀或編譯，在儲存庫按 Code → Download ZIP，解壓縮到自己的開發資料夾。需要持續修改，使用 GitHub Desktop 登入，再按 File → Clone repository 選此專案。Clone 會保留 Git 版本紀錄，ZIP 是當時的一份快照。

下載後檢查同一層有 main.cpp、game.cpp、build.ps1 和 external。不要在 ZIP 壓縮預覽視窗中直接執行建置。

## 2. 安裝編譯環境

從 https://www.msys2.org/ 安裝 MSYS2，建議沿用 C:\msys64。開啟開始功能表的「MSYS2 UCRT64」，在這個終端機執行：

```bash
pacman -Syu
```

若更新要求關閉終端機，關閉並重新開啟 UCRT64，再執行一次。然後安裝工具鏈：

```bash
pacman -S --needed mingw-w64-ucrt-x86_64-toolchain
```

上述 pacman 指令在 MSYS2 UCRT64 執行。後面的編譯指令在 PowerShell 執行。VS Code 是編輯器，編譯工作由 g++ 和 windres 完成。

## 3. 第一次編譯

用 VS Code 開啟整個專案資料夾，選 Terminal → New Terminal，確認使用 PowerShell。也可在檔案總管的專案資料夾開啟 PowerShell。

如果 MSYS2 在預設位置，執行：

```powershell
$env:Path = "C:\msys64\ucrt64\bin;" + $env:Path
g++ --version
windres --version
Get-ChildItem
```

前兩個工具要能輸出版本；檔案列表要看得到 build.ps1。這個 PATH 修改只作用於目前 PowerShell。若安裝位置不同，請替換上面的完整路徑。

```powershell
.\build.ps1 -Deps .\external
```

成功後資料夾會產生 Pong.exe。腳本先用 windres 將 app.rc 引用的 manifest 編成 app.res，再使用 C++20 編譯五個 .cpp，連結 raylib 與 Windows 函式庫。

```powershell
.\Pong.exe
```

只要主選單出現，就完成第一次編譯。修改程式前先關閉遊戲，重新建置後再開啟。

## 4. 試玩與存檔

主選單按 1 進單人設定、2 進雙人設定、R 看戰績。單人依設定使用 W/S 或上下方向鍵；雙人左側 W/S、右側上下方向鍵。Enter 開始，Esc 暫停。

暫停的 RESUME 接原局，RESTART 清分重開。MAIN MENU 會先確認是否離開。完成局可 PLAY AGAIN 或回主選單。全程使用同一個視窗。

正常存檔在 `%LOCALAPPDATA%\Pong`。練習可改成專案內的獨立目錄：

```powershell
.\Pong.exe --data-dir (Join-Path $PWD 'lesson-data')
```

config.json 保存設定，history.json 保存完成的單人戰績，game.log 記錄錯誤。雙人及中止的對局不列入單人戰績。同一資料目錄一次只允許一個程序寫入。

## 5. 跑核心測試

在同一個 PowerShell 執行：

```powershell
g++ -std=c++20 -Wall -Wextra -static test_game.cpp game.cpp -o test_game.exe
if ($LASTEXITCODE -eq 0) { .\test_game.exe }
```

預期顯示 Core checks passed。這條命令只編譯測試入口和遊戲核心，不加入 main.cpp，避免兩個入口。測試通過後，仍需實際驗證滑鼠、視窗與音訊。

## 6. 程式從哪裡讀

先讀 game.h，認識 Config 設定、Body 座標、Phase 階段、Game 成員與函式宣告。接著讀 game.cpp，從建構子、resetPoint、launch、pause/resume，到 tick 的移拍、碰撞和計分。

再讀 main.cpp，了解輸入如何轉成 -1/0/1、如何每次呼叫 tick(Step, left, right)，以及 game.ball、game.bats、game.score 如何被畫出來。之後依序讀 menu、files、audio 的 .h 和 .cpp。

每次挑一條具體流程追蹤，例如「按 W → 左方輸入 -1 → 左拍速度 -650 → y 減少 → 畫面球拍向上」。遇到一個新變數，記下型別、單位、誰修改、誰使用。

遊戲邏輯場地為 1280×720，座標正右、正下；球拍與球使用中心座標。Step=1/120 秒是固定物理步長，60 FPS 通常每幀執行兩次更新。

launch 使用 570 px/s 初速，角度正值向下、負值向上，左右另由 toward 決定；請以儲存庫目前的 angle 分布數值為準。碰拍後加速 5%，最高 800 px/s。Phase 是目前流程，Event 位元集合記錄本次更新撞牆、碰拍或得分。

## 7. 組員如何修改與交回

GitHub Desktop 先按 Fetch origin，有更新就 Pull。建立自己的分支，例如 feature/menu-layout，再修改程式。先編譯和測試，寫清楚 Commit summary，Commit 後 Push，最後建立 Pull Request 說明修改原因、操作方式與測試結果。

同時修改同一行可能衝突，先和該檔案負責人對照兩邊的內容再合併。合併完成後每位組員更新自己的 main。不要用上傳整份舊資料夾的方式蓋掉其他人的最新修改。

若只改註解，也要在提交中說明。若將 phase 改名成 now_phase，需同步改宣告、全部使用處與測試。第三方 external 套件保持上游內容與授權資訊。

## 8. 報告如何引用程式

記錄儲存庫版本（commit 或 release tag）、編譯環境、日期和測試結果。碰撞章節引用 Game::tick 的接觸判斷，CPU 章節引用觀察間隔與追球，介面章節引用 Menu::draw 與 main 的切頁流程。

每段引用交代輸入資料、判斷方式、修改的狀態、畫面或測試證據。完整程式作附件，報告正文放能支持說明的片段。測試紀錄包含重現步驟、預期、實際和截圖，四人試玩尚未做完的結果不要寫成已通過。

## 9. 常見問題

|現象|先檢查與處理|
|---|---|
|看不到私人儲存庫或 404|登入被邀請的帳號，接受邀請，確認網址|
|找不到 g++ 或 windres|安裝 UCRT64 工具鏈，在本次 PowerShell 加入 C:\msys64\ucrt64\bin 到 PATH|
|無法載入 build.ps1，系統禁止執行腳本|對下載 ZIP 或專案資料夾確認來源後，先嘗試 Unblock-File .\build.ps1；若仍受執行原則限制，改用下方的手動編譯指令。受學校管理政策限制時請管理員協助|
|找不到 raylib.h|確認 external/raylib-5.5_win64_mingw-w64/include/raylib.h 存在，建置使用 -Deps .\external|
|找不到 json.hpp|確認 external/json.hpp 已下載；不要只抓 .cpp|
|std::numbers 不存在|編譯須用 -std=c++20，並確認 g++ 支援 C++20|
|cannot find -lraylib|檢查 external 下的 lib/libraylib.a 與 -L 路徑，使用 x64 UCRT64 工具鏈|
|undefined reference|確認所需 .cpp 已加入建置；不要只編譯 main.cpp|
|multiple definition of main|不要把 test_game.cpp 和 main.cpp 編成同一個程式|
|cannot open output file Pong.exe|先關閉目前執行中的遊戲，再重新編譯|
|點兩下遊戲沒反應|確認沒有另一個程序使用相同資料目錄，或以 --data-dir 指定新的測試目錄|
|最大化後裁切或按鈕偏移|確認使用本版 app.manifest，及 raylib 只設定 FLAG_WINDOW_RESIZABLE；用相同版本原始碼重建|
|返回主選單後跳到雙人模式|確認切頁路徑有執行 present()/EndDrawing()，避免同一次滑鼠點擊被重複處理|
|讀不到或存不了戰績|確認資料目錄可寫，查看 game.log，保留原始 JSON，勿先刪除正式紀錄|

腳本無法執行時，可在 PowerShell 專案目錄直接編譯：

```powershell
$deps = (Resolve-Path .\external).Path
$ray = Join-Path $deps 'raylib-5.5_win64_mingw-w64'
windres app.rc -O coff -o app.res
if ($LASTEXITCODE -ne 0) { throw 'Resource build failed' }
$buildArgs = @(
    '-std=c++20', '-Wall', '-Wextra', '-static', '-municode', '-mwindows'
    '-I', (Join-Path $ray 'include'), '-I', $deps
    'main.cpp', 'menu.cpp', 'game.cpp', 'files.cpp', 'audio.cpp', 'app.res'
    '-L', (Join-Path $ray 'lib'), '-lraylib'
    '-lopengl32', '-lgdi32', '-lwinmm', '-luser32'
    '-o', 'Pong.exe'
)
& g++ @buildArgs
if ($LASTEXITCODE -ne 0) { throw 'Build failed' }
.\Pong.exe
```

## 10. 安裝檔和版本發佈

先建置 Pong.exe，另外安裝 Inno Setup，開啟 Pong.iss 編譯，產物在 release/Pong-Setup-3.0.0-win-x64.exe。安裝檔和 Pong.exe 放 GitHub Releases；自動產生的 .exe、.res、遊戲存檔及備份資料夾由 .gitignore 排除。

發行前用新解壓縮的專案資料夾編譯，確認相依檔案完整。乾淨 Windows 的安裝、解除安裝和組員實機試玩另行記錄。

## 11. HackMD 共筆方式

組員可以在此手冊補充遭遇的錯誤、重現步驟、解法與結果。分享權限由筆記右上角「分享」控制，邀請組員並選可編輯。新帳號先完成 email 驗證，以免影響後續編輯與邀請。

每次重要更動同時更新 GitHub 的 docs/TEAM-HANDBOOK.md，避免手冊和程式版本不同步。

## 官方參考

- MSYS2 安裝與工具鏈 https://www.msys2.org/
- MSYS2 UCRT64 環境 https://www.msys2.org/docs/environments/
- GitHub Clone https://docs.github.com/en/repositories/creating-and-managing-repositories/cloning-a-repository
- GitHub 協作者 https://docs.github.com/en/repositories/managing-your-repositorys-settings-and-features/repository-access-and-collaboration/inviting-collaborators-to-a-personal-repository
- HackMD 筆記權限 https://hackmd.io/@docs/note-permission-en
