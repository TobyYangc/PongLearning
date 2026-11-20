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
