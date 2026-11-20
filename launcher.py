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
