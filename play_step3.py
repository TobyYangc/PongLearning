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
