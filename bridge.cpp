#define PONG_EXPORTS
#include "bridge.h"
#include "game.h"
#include <algorithm>
#include <cmath>
#include <new>

//static_assert在編譯期檢查ABI大小，布局改動時要同步Python與版本號。
static_assert(sizeof(PongOptions) == 28);
static_assert(sizeof(PongSnapshot) == 60);

//ABI=Application Binary Interface；Python以版本號確認可使用同一份二進位協定。
std::int32_t pong_abi_version() noexcept { return 1; }

//o指向Python準備的參數；先驗證數值範圍，再配置Game，例外轉成nullptr。
void* pong_create(const PongOptions* o) noexcept {
    if (!o || o->mode < 0 || o->mode > 1 ||
        o->difficulty < 0 || o->difficulty > 2 ||
        o->side < 0 || o->side > 1 || o->preset < 0 || o->preset > 1 ||
        o->serve < 0 || o->serve > 2 ||
        (o->target != 5 && o->target != 7 && o->target != 11)) return nullptr;
    try {
        Config c;
        c.mode = o->mode ? "pvcpu" : "pvp";
        const char* difficulty[]{"easy","normal","hard"};
        const char* serve[]{"loser","scorer","random"};
        c.difficulty = difficulty[o->difficulty];
        c.side = o->side ? "right" : "left";
        c.preset = o->preset ? "preset2" : "preset1";
        c.serve = serve[o->serve];
        c.target = o->target;
        return new Game(c,o->seed);
    } catch (...) { return nullptr; }
}

//由建立Game的DLL執行delete；delete nullptr也安全。
void pong_destroy(void* handle) noexcept { delete static_cast<Game*>(handle); }

//dt須是有效正數且不超過一個STEP容許值；左右輸入限制-1/0/1，錯誤回-1。
std::int32_t pong_tick(void* handle, float dt, std::int32_t left, std::int32_t right) noexcept {
    if (!handle || !std::isfinite(dt) || dt <= 0 || dt > Step*1.01f ||
        left < -1 || left > 1 || right < -1 || right > 1) return -1;
    try { return static_cast<Game*>(handle)->tick(dt,left,right); }
    catch (...) { return -1; }
}

//command 0暫停、1恢復、2重開、3離開確認、4取消；不允許的階段操作回0。
std::int32_t pong_command(void* handle, std::int32_t command) noexcept {
    if (!handle) return 0;
    //將不透明地址還原成Game*，解參考後取別名g，操作原本那個物件。
    auto& g = *static_cast<Game*>(handle);
    // 0 暫停、1 恢復、2 重開、3 離開確認、4 取消。
    switch (command) {
    case 0: g.pause(); break;
    case 1:
        if (g.now_phase != Phase::Paused) return 0;
        g.resume(); break;
    case 2: g.restart(); break;
    case 3:
        if (g.now_phase == Phase::Over) return 0;
        g.pause(); g.now_phase = Phase::Leave; break;
    case 4:
        if (g.now_phase != Phase::Leave) return 0;
        g.now_phase = Phase::Paused; break;
    default: return 0;
    }
    return 1;
}

//out是Python端可寫結構地址；將Game目前資料複製過去，不暴露string或亂數引擎。
std::int32_t pong_snapshot(void* handle, PongSnapshot* out) noexcept {
    if (!handle || !out) return 0;
    const auto& g = *static_cast<Game*>(handle);
    *out = {static_cast<std::int32_t>(g.now_phase),static_cast<std::int32_t>(g.bf_pause_phase),
            g.score[0],g.score[1],g.last,g.lastBat,g.roundId,
            g.bats[0].y,g.bats[1].y,g.ball.x,g.ball.y,
            g.vx,g.vy,g.timer,g.hitTimer};
    return 1;
}
