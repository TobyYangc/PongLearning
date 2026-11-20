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

if __name__ == "__main__":
    check_core()
    check_app()
