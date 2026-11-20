# Pong v3 組員協作專案

Windows x64、C++20、raylib 5.5。此份原始碼取自目前的 PongLearning，附 raylib 和 nlohmann/json 編譯相依套件。

## 先閱讀

- [組員教學手冊](docs/TEAM-HANDBOOK.md)，包含下載、環境安裝、編譯、閱讀程式、協作及錯誤排解。
- [第三方套件說明](THIRD-PARTY-NOTICES.md)。

## 快速編譯

先安裝 MSYS2 的 UCRT64 C++ 工具鏈。在專案資料夾開 PowerShell，若使用預設安裝位置，執行：

```powershell
$env:Path = "C:\msys64\ucrt64\bin;" + $env:Path
g++ --version
windres --version
.\build.ps1 -Deps .\external
if ($LASTEXITCODE -eq 0) { .\Pong.exe }
```

核心測試：

```powershell
g++ -std=c++20 -Wall -Wextra -static test_game.cpp game.cpp -o test_game.exe
if ($LASTEXITCODE -eq 0) { .\test_game.exe }
```

一般存檔位置是 `%LOCALAPPDATA%\Pong`。測試時可使用獨立目錄：

```powershell
.\Pong.exe --data-dir (Join-Path $PWD 'lesson-data')
```

主選單按 1 單人、2 雙人、R 戰績；對局按 Esc 暫停。設定頁選擇鍵位和玩家側別。安裝版與可執行檔放在 GitHub Releases。

## 檔案地圖

|檔案|責任|
|---|---|
|game.h / game.cpp|資料、物理、CPU、狀態、計分與事件|
|main.cpp|視窗、輸入、固定步長更新、繪圖與生命週期|
|menu.h / menu.cpp|主選單、設定、戰績及按鈕|
|files.h / files.cpp|JSON 設定、結果與原子寫檔|
|audio.h / audio.cpp|音效生成及播放|
|test_game.cpp|不開視窗的核心測試|
|build.ps1|Windows C++20 建置|
|app.rc / app.manifest|Windows 資源與 DPI 宣告|
|Pong.iss|Inno Setup 安裝檔設定|

