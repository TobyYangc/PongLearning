# Pong完整程式附錄B｜Python介面與打包

[主文](https://hackmd.io/tGkm7Yn-TnSZjNuPpCFwDg)｜[C++附錄](https://hackmd.io/43y2pN6dSZKgqRBGcmbcHg)｜[Python附錄](https://hackmd.io/46KzKWO9TR2zgItJh1M20g)

每個標題是一個完整檔案。同一檔案若有多個程式區塊，依序接起來。

## `core.py`

```python
"""Python 透過 ctypes 呼叫 C ABI；只有 C++ 可以操作 Game 內部。"""
import ctypes as C
from enum import IntEnum
from pathlib import Path
import secrets

#STEP是固定物理步長，單位秒；與game.h的Step對應。
STEP = 1 / 120

#Phase順序要與C++的enum class Phase完全一致，快照中的整數才會對到相同階段。
class Phase(IntEnum):
    INTRO, SERVE, RALLY, POINT, PAUSED, LEAVE, RESUME, OVER = range(8)

#Options的欄位順序與大小，必須和bridge.h的PongOptions一致。
class Options(C.Structure):
    _fields_ = [(name, C.c_int32) for name in
                ("mode", "difficulty", "side", "preset", "serve", "target")] + [("seed", C.c_uint32)]

#Snapshot是DLL寫回的資料布局，前7個int32，後8個float，共60位元組。
class Snapshot(C.Structure):
    _fields_ = [(name, C.c_int32) for name in
                ("phase", "saved", "left_score", "right_score", "last", "last_bat", "round_id")]
    _fields_ += [(name, C.c_float) for name in
                 ("left_y", "right_y", "ball_x", "ball_y", "vx", "vy", "timer", "hit_timer")]

class Core:
    #settings是設定字典；seed省略時產生32位元種子。載入同資料夾DLL，先檢查ABI版本，再宣告每個函式的參數與回傳型別，最後建立Game。
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

    #建立Python端Snapshot，交給DLL填入目前遊戲狀態；回傳的是資料副本，可供畫面讀取。
    def snapshot(self):
        result = Snapshot()
        if not self.handle or not self.dll.pong_snapshot(self.handle, C.byref(result)):
            raise RuntimeError("Unable to read C++ state")
        return result

    #左右輸入為-1向上、0停止、1向下；每次固定推進STEP秒，回傳Wall/Paddle/Score事件位元。
    def tick(self, left, right):
        if not self.handle:
            raise RuntimeError("Game already closed")
        events = self.dll.pong_tick(self.handle, STEP, left, right)
        if events < 0:
            raise RuntimeError("Invalid physics update")
        return events

    #action是Python動作名稱，先換成整數命令，再由DLL操作核心階段。
    def command(self, action):
        codes = {"pause": 0, "resume": 1, "restart": 2, "leave": 3, "cancel": 4}
        if not self.handle or not self.dll.pong_command(self.handle, codes[action]):
            raise RuntimeError("Invalid game command")

    #handle是DLL內Game的地址；交回pong_destroy釋放後設成None，避免重複釋放或再次使用。
    def close(self):
        #getattr在成員尚未存在時回None，讓初始化失敗後也能安全檢查。
        if getattr(self, "handle", None):
            self.dll.pong_destroy(self.handle)
            self.handle = None

    #配合with Core(...) as game使用，將目前Core物件交給as後面的名稱。
    def __enter__(self):
        return self

    #離開with區塊時呼叫close，即使區塊中發生例外，也會釋放DLL內的Game。
    def __exit__(self, *args):
        self.close()
```

## `storage.py`

```python
"""設定、戰績、原子存檔與單一資料目錄鎖，由 Python 管理。"""
from datetime import datetime, timezone
import json
import msvcrt
import os
from pathlib import Path
import uuid

#DEFAULT是缺設定時使用的預設值；ALLOWED限制每個欄位可接受的型別與選項。
DEFAULT = dict(difficulty="normal", winning_score=7, input_preset="preset2",
               player_side="left", serve_to="loser")
ALLOWED = dict(difficulty=("easy", "normal", "hard"), winning_score=(5, 7, 11),
               input_preset=("preset1", "preset2"), player_side=("left", "right"),
               serve_to=("loser", "scorer", "random"))

#path是檔案路徑；檔案不存在才回default。JSON損壞和權限錯誤繼續交給呼叫端處理。
def read_json(path, default):
    try:
        return json.loads(path.read_text(encoding="utf-8-sig"))
    except FileNotFoundError:
        return default

#先寫同目錄唯一暫存檔、flush與fsync，再os.replace提交；finally清理未使用暫存檔。
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

#逐項驗證完整單人戰績：分數為整數、有人達目標、勝負一致、難度/側別合法、日期帶時區。
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

class Store:
    #建立資料目錄並鎖住pong.lock的第1個位元組；讀設定，嘗試恢復上次未提交完成局。
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

    #mode是pvcpu或pvp；依欄位允許值挑出該模式設定，缺值用DEFAULT，音效偏好共用。
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

    #先複製現有設定並更新目前模式，原子寫入成功後才更新記憶體內的config。
    def save_settings(self, settings):
        updated = self.config.copy()
        updated[settings["mode"]] = settings.copy()
        updated["sound_enabled"] = settings["sound_enabled"]
        atomic(self.directory / "config.json", updated)
        self.config = updated

    #取得原始戰績陣列；非陣列視為格式錯誤，避免把壞資料當空清單覆蓋。
    def raw_history(self):
        rows = read_json(self.directory / "history.json", [])
        if not isinstance(rows, list):
            raise ValueError("history.json must contain an array")
        return rows

    #過濾可顯示的有效紀錄，按帶時區的時間由新到舊排序；原檔中的其他資料仍保留。
    def history(self):
        rows = [r for r in self.raw_history() if valid_record(r)]
        return sorted(rows, key=lambda r: datetime.fromisoformat(r["timestamp"].replace("Z", "+00:00")),
                      reverse=True)
```

```python
    #若有pending-match.json，檢查場次識別並去重追加；history成功後才刪除pending。
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

    #只接受已完成pvcpu。p為玩家索引，scores依左/右排列，存檔則固定玩家/CPU比分。
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

    #先恢復待提交完成局，再將history原子寫成空陣列；由GUI確認後才能呼叫。
    def clear(self):
        self.recover()
        atomic(self.directory / "history.json", [])

    #先將檔案位置移回0，解鎖先前鎖住的1個位元組，再關閉檔案。
    def close(self):
        if not self.lock.closed:
            #msvcrt鎖定從目前檔案位置開始，故加鎖/解鎖都先移回第0位元組。
            self.lock.seek(0)
            msvcrt.locking(self.lock.fileno(), msvcrt.LK_UNLCK, 1)
            self.lock.close()
```

## `sound.py`

```python
"""三種短音先合成 WAV，播放使用 Windows 非同步 API，不阻塞 Tkinter。"""
import math
import struct
import tempfile
import wave
from pathlib import Path
import winsound

class Audio:
    #在系統暫存目錄合成3段48kHz、16-bit、單聲道WAV；索引0撞牆、1擊拍、2得分。
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

    #events是事件位元集合；優先Score，再Paddle，再Wall。SND_ASYNC讓播放不阻塞Tk事件迴圈。
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

    #先停止目前音效，才清理TemporaryDirectory中的WAV，避免播放中的檔案仍被使用。
    def close(self):
        try:
            winsound.PlaySound(None, 0)
        except RuntimeError:
            pass
        self.temp.cleanup()
```

## `launcher.py`

```python
"""唯一 Tk 主視窗：Python 處理輸入與繪圖，C++ DLL 推進遊戲規則。"""
import argparse
import ctypes
import math
import os
from pathlib import Path
import sys
import time
import tkinter as tk
from tkinter import messagebox
import uuid

from core import Core, Phase, STEP
from sound import Audio
from storage import Store

#W/H為邏輯場地尺寸；實際視窗縮放由scale處理。
W, H = 1280, 720
#呈現目標間隔約8.33ms；物理STEP同為1/120秒，但呈現與物理分開排程。
DRAW_INTERVAL = 1 / 120
WHITE, BLACK, GRAY, CYAN = "#ffffff", "#000000", "#aaaaaa", "#00e5ed"

class App:
    #root是唯一Tk主視窗；directory是存檔目錄。初始化頁面、核心、輸入、繪圖快取與排程，再註冊事件。
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

    #event.x/y是Canvas實際座標；先減黑邊偏移，再除縮放倍率，得到1280×720邏輯座標。
    def logical(self, event):
        return ((event.x - self.ox) / self.scale, (event.y - self.oy) / self.scale)

    #只記住滑鼠邏輯座標，下一次draw依它計算按鈕懸停框。
    def on_motion(self, event):
        self.pointer = self.logical(event)

    #依目前buttons做一次命中判斷；觸發動作立即return，避免切頁後同一次點擊再觸發別的頁面。
    def on_click(self, event):
        point = self.logical(event)
        # 本次點擊只對當前頁面做一次命中；回呼即使切頁，也立即結束此事件。
        for rect, action, hotkey, enabled in self.buttons:
            x, y, w, h = rect
            if enabled and x <= point[0] <= x + w and y <= point[1] <= y + h:
                self.activate(action)
                return "break"

    #鬆開鍵時，同時清除keys和blocked；下一次按下才會重新產生有效輸入。
    def on_release(self, event):
        key = event.keysym.lower()
        self.keys.discard(key)
        self.blocked.discard(key)

    #用after_idle等本輪焦點事件處理完，再判斷是否真的離開這個主視窗。
    def on_focus_out(self, event):
        #先等焦點在同一視窗內轉移完成，避免誤將內部焦點變更當成失焦。
        self.root.after_idle(self.check_focus)

    #確定失焦才暫停核心、清空按鍵、封鎖舊輸入並清除物理累積時間。
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

    #keys保存持續按住狀態；重複KeyPress不重複觸發按鈕。移動鍵留給advance，選單鍵在此處理。
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

```python
    #戰績頁滾輪只改scroll索引，範圍限制在0到最後可顯示3筆的起點。
    def on_wheel(self, event):
        if self.page == "records":
            self.scroll = min(max(0, len(self.rows) - 3),
                              max(0, self.scroll + (-1 if event.delta > 0 else 1)))

    #依page與phase決定Esc用途：返回、取消離開、暫停或結算返回；避免一律關閉視窗。
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

    #視窗×：未完賽先LEAVE確認；已完賽但dirty時要求先重試保存；其餘可正常關閉。
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

    #切換Python頁面、清理焦點/累積量/插值、封鎖仍按住的鍵，立即重建新頁面命中區。
    def change_page(self, page):
        self.page = page
        self.focus = 0
        self.message = ""
        self.acc = 0
        self.previous_state = None
        self.blocked.update(self.keys)
        self.draw()  # 立即重建命中區，下一個事件只會看到新頁面

    #從DLL取得最新快照；state是Python端資料副本，繪圖只讀這份資料。
    def refresh_state(self):
        self.state = self.core.snapshot()

    #先標記dirty；存檔成功才清除，失敗保留本局和RETRY SAVE入口，並寫log。
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

    #動作的統一入口；將可預期錯誤轉成畫面訊息，處理後重畫當前頁面。
    def activate(self, action):
        try:
            self.perform(action)
        except (OSError, ValueError, RuntimeError) as error:
            self.message = "ACTION FAILED - CHECK DATA FILES / DLL"
            self.log(error)
        if not self.closed:
            self.draw()

    #依動作建立/關閉Core、改設定或切頁。tuple代表設定欄位和值，字串代表按鈕動作。
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

    #向下按住值減向上按住值，回傳-1/0/1；blocked中的舊按鍵不採用。
    def direction(self, up, down):
        return int(down in self.keys and down not in self.blocked) - int(up in self.keys and up not in self.blocked)
```

```python
    #elapsed是實際幀秒數。累積到acc，最多8次固定STEP更新；合併音效事件並在完賽時保存戰績。
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

    #每次Tk回呼先量時間、advance再draw，最後按next_frame目標時刻排程；繪圖耗時包含在週期內。
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

    #kind為rectangle/text/oval，coordinates為實際座標；重用對應Canvas項目，只送出變更的屬性。
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

    #本次未用到的舊項目隱藏；只有建立/替換項目時，才依繪圖順序重排圖層。
    def finish_paint(self):
        for entry in self.items[self.item_cursor:]:
            if not entry["hidden"]:
                self.canvas.itemconfigure(entry["id"], state="hidden")
                entry["hidden"] = True
        if self.layout_changed:
            for entry in self.items[:self.item_cursor]:
                self.canvas.tag_raise(entry["id"])

    #x/y/w/h都是邏輯尺寸；乘scale並加ox/oy後，才交給Canvas畫矩形。
    def rect(self, x, y, w, h, fill=BLACK, outline="", width=2):
        s = self.scale
        self.paint("rectangle", (self.ox+x*s, self.oy+y*s,
                                 self.ox+(x+w)*s, self.oy+(y+h)*s),
                   fill=fill, outline=outline, width=max(1, width*s))
```

```python
    #label是文字；x/y是邏輯位置，負字級讓Tk以像素計算，隨scale等比縮放。
    def text(self, label, x, y, size=28, fill=WHITE, anchor="n"):
        self.paint("text", (self.ox+x*self.scale, self.oy+y*self.scale),
                   text=label, fill=fill, anchor=anchor,
                   font=("Consolas", -max(1, round(size*self.scale)), "bold"))

    #以前後物理快照和acc/STEP做顯示插值；碰撞/切階段清除前快照，且不修改DLL資料。
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

    #畫按鈕並記錄命中區、action、快捷鍵和enabled。focus索引只計算可操作項目。
    def button(self, rect, label, action, hotkey="", selected=False, enabled=True, size=28):
        x, y, w, h = rect
        hover = x <= self.pointer[0] <= x+w and y <= self.pointer[1] <= y+h
        index = sum(b[3] for b in self.buttons)
        keyboard_focus = enabled and self.focus == index and self.page != "settings"
        outline = CYAN if enabled and (hover or keyboard_focus) else WHITE if enabled else "#555555"
        self.rect(x, y, w, h, WHITE if selected else BLACK, outline)
        self.text(label, x+w/2, y+(h-size)/2-2, size, BLACK if selected else WHITE if enabled else "#555555")
        self.buttons.append((rect, action, hotkey, enabled))

    #label是設定列標題，field是字典欄位，choices每項保存顯示文字、實際值與快捷鍵。
    def row(self, label, y, field, choices):
        self.text(label, 59, y+10, 25, anchor="nw")
        #一列可用寬928px，選項間隔15px，剩餘寬度平均分配。
        width = (928 - 15*(len(choices)-1)) / len(choices)
        for i, (caption, value, key) in enumerate(choices):
            self.button((293+i*(width+15), y, width, 50), caption,
                        (field, value), key, self.draft[field] == value)

    #依單/雙人顯示可選設定；每列由row處理，Enter固定START，Esc放棄草稿返回。
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

    #摘要由全部有效紀錄計算；列表只顯示scroll起的3筆。清除確認另建按鈕，避免點到底層。
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

```python
    #依phase畫球場、暫停/離開/結算；位置來自顯示插值，比分/勝負仍使用核心快照。
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

    #依目前Canvas尺寸計算scale與黑邊，再依page繪圖；最後隱藏剩餘快取項目。
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

```python
    #將例外種類和原因附加到資料目錄game.log；log本身寫入失敗時不再拋出第二個例外。
    def log(self, error):
        try:
            with (self.store.directory / "game.log").open("a",encoding="utf-8") as out:
                out.write(f"{type(error).__name__}: {error}\n")
        except OSError:
            pass

    #避免重複清理；取消after、釋放DLL核心、停止音效、解鎖存檔、歸還計時精度，再關閉Tk。
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

#讀命令列並在建立Tk前設定DPI，建立唯一root和App，最後由mainloop持續處理事件。
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

if __name__ == "__main__":
    sys.exit(main())
```

## `verify.py`

```python
"""架構改版回歸：測真實 DLL、頁面事件、完賽存檔和視窗身分。"""
from pathlib import Path
from tempfile import TemporaryDirectory
from types import SimpleNamespace
import ctypes
import json
import tkinter as tk

from core import Core, Phase, STEP, Options, Snapshot
from launcher import App
from storage import Store, atomic

def check_core():
    settings = dict(mode="pvcpu", difficulty="normal", player_side="right",
                    input_preset="preset2", serve_to="loser", winning_score=5, sound_enabled=False)
    assert ctypes.sizeof(Options) == 28 and ctypes.sizeof(Snapshot) == 60
    with Core(settings,11) as core:
        for _ in range(700):
            core.tick(0,0)
        assert core.snapshot().phase == Phase.RALLY
        before = core.snapshot()
        core.command("pause")
        for _ in range(20):
            core.tick(0,0)
        paused = core.snapshot()
        assert paused.ball_x == before.ball_x and paused.vx == before.vx
        core.command("resume")
        assert core.snapshot().phase == Phase.RESUME
        for _ in range(360):
            core.tick(0,0)
        after = core.snapshot()
        assert after.phase == Phase.RALLY and after.ball_x == before.ball_x
        core.command("restart")
        assert core.snapshot().round_id == 2
        assert core.dll.pong_tick(core.handle,float("nan"),0,0) == -1
    print("PASS DLL ABI, real tick, pause/resume, restart, invalid input")

def click(app,x,y):
    app.on_click(SimpleNamespace(x=app.ox+x*app.scale,y=app.oy+y*app.scale))

def check_app():
    with TemporaryDirectory(prefix="Pong-中文-") as directory:
        root = tk.Tk()
        app = App(root,Path(directory))
        try:
            root.update_idletasks()
            app.draw()
            wid = root.winfo_id()
            click(app,640,300)
            assert app.page == "settings" and app.draft["mode"] == "pvcpu"
            app.draft["winning_score"] = 5
            app.draft["sound_enabled"] = False
            app.draw()
            click(app,640,600)
            assert app.page == "match"
            app.draw()
            item_ids = app.canvas.find_all()
            for _ in range(30):
                app.draw()
            assert app.canvas.find_all() == item_ids

            original_state = app.state
            previous = Snapshot.from_buffer_copy(bytes(original_state))
            current = Snapshot.from_buffer_copy(bytes(original_state))
            previous.phase = current.phase = Phase.RALLY
            previous.ball_x, current.ball_x = 100, 110
            app.previous_state, app.state, app.acc = previous, current, STEP / 2
            assert abs(app.render_positions()[2] - 105) < .001
            assert app.core.snapshot().ball_x == original_state.ball_x
            app.previous_state = None
            app.state, app.acc = original_state, 0
            app.activate("pause")
            app.draw()
            click(app,640,475)
            assert app.state.phase == Phase.LEAVE
            # 此座標正好與主選單雙人入口重疊，返回後不得再次處理同次點擊。
            click(app,640,390)
            assert app.page == "menu" and app.core is None
            app.draw()
            assert app.page == "menu" and root.winfo_id() == wid

            app.activate("pvcpu")
            app.draft.update(player_side="right",winning_score=5,sound_enabled=False)
            app.activate("start")
            for _ in range(20000):
                app.advance(8*STEP)
                if app.state.phase == Phase.OVER:
                    break
            assert app.state.phase == Phase.OVER and not app.dirty
            assert len(app.store.history()) == 1
            app.save_result()
            assert len(app.store.history()) == 1
            app.activate("restart")
            app.advance(.04)
            app.activate("pause")
            app.activate("main")
            app.activate("confirm_leave")
            assert len(app.store.history()) == 1
            app.activate("records")
            app.activate("clear")
            app.perform("back")
            assert len(app.store.history()) == 1

            # 待提交紀錄已入帳但 pending 未刪除時，恢復不能重複計分。
            row = app.store.history()[0]
            atomic(app.store.directory / "pending-match.json",row)
            app.store.recover()
            assert len(app.store.history()) == 1
            try:
                second = Store(directory)
            except RuntimeError:
                pass
            else:
                second.close()
                raise AssertionError("Expected exclusive data lock")
            for size in ("640x360","960x720","1280x720"):
                root.geometry(size)
                root.update_idletasks()
                app.draw()
                assert root.winfo_id() == wid
                assert abs(app.ox + 640*app.scale - app.canvas.winfo_width()/2) < .01
            app.activate("clear")
            app.activate("confirm_clear")
            assert app.store.history() == []
            print("PASS single Tk window, mouse return, completed match, retry dedup, aborted match, clear cancel/confirm, recovery, lock, resize, retained canvas items, display interpolation")
        finally:
            app.shutdown()
```

```python
if __name__ == "__main__":
    check_core()
    check_app()
```

## `step0.py`

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

## `core_check.py`

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

## `play_step3.py`

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

## `package.ps1`

```powershell
param([string]$Python = 'python')
$ErrorActionPreference = 'Stop'
Push-Location $PSScriptRoot
try {
    & ./build.ps1
    & $Python -m PyInstaller --noconfirm --clean --windowed --onedir --name Pong --add-binary 'pong_core.dll;.' --manifest app.manifest launcher.py
    if ($LASTEXITCODE) { throw 'Python packaging failed' }
    $licenseDir = Join-Path $PSScriptRoot 'dist/Pong/_internal/licenses'
    New-Item -ItemType Directory -Path $licenseDir -Force | Out-Null
    Copy-Item -LiteralPath (Join-Path $PSScriptRoot 'external/raylib-5.5_win64_mingw-w64/LICENSE') -Destination (Join-Path $licenseDir 'raylib-LICENSE') -Force
} finally { Pop-Location }
```

## `app.manifest`

```xml
<?xml version="1.0" encoding="UTF-8" standalone="yes"?>
<assembly xmlns="urn:schemas-microsoft-com:asm.v1" manifestVersion="1.0">
  <assemblyIdentity version="3.0.0.0" processorArchitecture="amd64" name="Pong.Tkinter" type="win32"/>
  <application xmlns="urn:schemas-microsoft-com:asm.v3">
    <windowsSettings>
      <dpiAware xmlns="http://schemas.microsoft.com/SMI/2005/WindowsSettings">true/pm</dpiAware>
      <dpiAwareness xmlns="http://schemas.microsoft.com/SMI/2016/WindowsSettings">PerMonitorV2</dpiAwareness>
    </windowsSettings>
  </application>
</assembly>
```

## `Pong.iss`

```ini
[Setup]
AppId=PongV3
AppName=Pong
AppVersion=3.0.0
DefaultDirName={localappdata}\Programs\Pong
DefaultGroupName=Pong
DisableProgramGroupPage=yes
PrivilegesRequired=lowest
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
OutputDir=release
OutputBaseFilename=Pong-Tk-Setup-3.0.0-win-x64
Compression=lzma2
SolidCompression=yes

[Files]
Source: "dist\Pong\*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs createallsubdirs

[Tasks]
Name: desktopicon; Description: "Create a desktop shortcut"; Flags: unchecked

[Icons]
Name: "{autoprograms}\Pong"; Filename: "{app}\Pong.exe"
Name: "{autodesktop}\Pong"; Filename: "{app}\Pong.exe"; Tasks: desktopicon

[Run]
Filename: "{app}\Pong.exe"; Description: "Launch Pong"; Flags: nowait postinstall skipifsilent
```

## `settings_check.py`

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

