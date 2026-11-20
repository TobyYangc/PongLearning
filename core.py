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
