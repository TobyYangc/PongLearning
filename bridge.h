#pragma once
#include <cstdint>

// C ABI 邊界只傳固定大小數值；C++ 的 string、Game 和亂數引擎留在 DLL 內。
//PongOptions是跨語言的建立參數；6個int32加1個uint32，共28bytes，順序須與core.py一致。
struct PongOptions {
    std::int32_t mode, difficulty, side, preset, serve, target;
    std::uint32_t seed;
};
//PongSnapshot是畫面快照；7個int32加8個float，共60bytes。位置單位px、速度px/s、計時秒。
struct PongSnapshot {
    std::int32_t phase, saved, leftScore, rightScore, last, lastBat, roundId;
    float leftY, rightY, ballX, ballY, vx, vy, timer, hitTimer;
};
#ifdef PONG_EXPORTS
//extern "C"採C連結名稱；dllexport讓Python可從DLL取得這些函式。
#define PONG_API extern "C" __declspec(dllexport)
#else
#define PONG_API extern "C" __declspec(dllimport)
#endif
PONG_API std::int32_t pong_abi_version() noexcept;
//回傳不透明handle；Python只傳回DLL操作，不直接解讀Game記憶體。
PONG_API void* pong_create(const PongOptions* options) noexcept;
PONG_API void pong_destroy(void* handle) noexcept;
PONG_API std::int32_t pong_tick(void* handle, float dt, std::int32_t left, std::int32_t right) noexcept;
PONG_API std::int32_t pong_command(void* handle, std::int32_t command) noexcept;
PONG_API std::int32_t pong_snapshot(void* handle, PongSnapshot* output) noexcept;
