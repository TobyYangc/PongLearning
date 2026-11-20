# Pong v3｜Tkinter＋C++20單一視窗

Python/Tkinter管理選單、設定、戰績、輸入與繪圖；C++20 DLL管理物理、CPU、計分和階段。開打、暫停、結算、返回主選單都在同一個視窗。

## 組員請依這個順序閱讀

1. 先看[組員入口手冊](https://hackmd.io/7eoGOVjXTPapJ91ctSQgmg)，完成環境、下載及建置檢查。
2. 再看[引導主文](https://hackmd.io/tGkm7Yn-TnSZjNuPpCFwDg)，從第0章依序做到第11章；每步看到預期結果再繼續。
3. 第1～4章搭配[C++附錄](https://hackmd.io/43y2pN6dSZKgqRBGcmbcHg)，依序讀game.h→game.cpp→bridge.h→bridge.cpp→build.ps1。
4. 第4～9章搭配[Python附錄](https://hackmd.io/46KzKWO9TR2zgItJh1M20g)，依序讀core.py→play_step3.py→storage.py→launcher.py→sound.py。
5. 最後完成第10章驗證，再依第11章封裝。附錄供查完整檔案，主文負責逐步引導。

想先試玩可下載GitHub新版安裝檔；想跟著學習，請從組員入口與主文開始。

## 下載與立即試玩

[下載目前Tkinter版本](https://github.com/TobyYangc/PongLearning/releases/tag/v3.0.0-tk.1)。安裝檔可直接安裝；可攜ZIP需完整解壓縮Pong資料夾再執行Pong.exe。

## 原始碼建置

Windows x64，需64位元Python/Tkinter與MSYS2 UCRT64的g++。raylib5.5相依套件已附在external；全部使用C++20。

```powershell
$env:Path = "C:\msys64\ucrt64\bin;" + $env:Path
.\build.ps1
python core_check.py
python verify.py
python launcher.py --data-dir .\lesson-data
```

build.ps1產生pong_core.dll並執行核心與碰撞測試。Python啟動介面，DLL不建立視窗。正常資料位置為%LOCALAPPDATA%/Pong，練習請使用獨立--data-dir。

## 本機教材與檔案

- [組員入口與環境排解](docs/TEAM-HANDBOOK.md)
- [第0～11章引導主文](docs/GUIDE.md)
- [C++完整程式附錄](docs/APPENDIX_CPP.md)
- [Python完整程式附錄](docs/APPENDIX_PYTHON.md)
- [第三方套件與授權](THIRD-PARTY-NOTICES.md)

程式閱讀順序：game.h/cpp→bridge.h/cpp→core.py→play_step3.py→storage.py→launcher.py→sound.py→verify.py→package.ps1。

## 驗證與協作

已重新建置，核心、CircleRec高速碰撞、DLL與單一Tk視窗整合測試通過。組員仍須依主文第10～11章在自己的電腦驗收音訊、DPI、實際操作和安裝/解除安裝。

儲存庫與HackMD公開閱讀。程式修改請fork並提交Pull Request；問題請開Issue，附版本、步驟、預期與實際結果。舊版純C++介面保留在legacy/raylib-ui，舊手冊在docs/archive；目前入口是根目錄launcher.py。
