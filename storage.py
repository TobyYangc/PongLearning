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
