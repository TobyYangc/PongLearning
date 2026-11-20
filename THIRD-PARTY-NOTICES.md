# 第三方套件

## raylib 5.5

官方來源 https://github.com/raysan5/raylib

附 Windows x64 MinGW 編譯套件，位於 `external/raylib-5.5_win64_mingw-w64`。授權全文保留於該資料夾的 `LICENSE`，標頭亦保留上游聲明。

## JSON for Modern C++ 3.11.3

作者 Niels Lohmann，官方來源 https://github.com/nlohmann/json

單一標頭位於 `external/json.hpp`。標頭保留 SPDX 與 MIT 授權全文。此套件採 MIT 授權。

第三方套件各依其授權使用。本儲存庫未替自有程式碼額外指定開源授權。

## 本版使用範圍

主分支以Python/Tkinter建立介面，raylib5.5靜態連結於C++ DLL提供圓矩形碰撞。JSON由Python標準函式庫處理；json.hpp保留供legacy/raylib-ui使用。可攜版以PyInstaller封裝Python/Tk執行環境並保留內附授權檔。自有程式碼授權狀態維持原儲存庫聲明。
