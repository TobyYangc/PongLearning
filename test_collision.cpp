#include "game.h"
#include <cassert>
#include <cmath>
#include <iostream>

Game rally() {
    Game g(Config{}, 11);
    g.now_phase = Phase::Rally;
    g.vx = -500;
    g.vy = -600;
    return g;
}

//此測試直接設定核心位置重現漏判，包含左右側、圓角、移拍、高速、重疊和真正漏球。
int main() {
    for (int side = 0; side < 2; ++side) {
        auto g = rally();
        // 首次跨過 x=67/1213 時尚未碰拍，之後才進入圓角接觸範圍。
        g.ball = {side ? 1210.0f : 70.0f, 430.5f};
        g.vx = side ? 500 : -500;
        int events = None;
        for (int n = 0; n < 5; ++n) events |= g.tick(Step, 0, 0);
        assert(events & Paddle);
        assert((g.vx > 0) == (side == 0));
        assert(g.score[0] == 0 && g.score[1] == 0);

        auto moving = rally();
        moving.ball = {side ? 1214.0f : 66.0f, 429};
        moving.vx = side ? 570 : -570;
        moving.vy = 0;
        assert(moving.tick(Step, side ? 0 : 1, side ? 1 : 0) & Paddle);

        auto overlap = rally();
        overlap.ball = {side ? 1215.0f : 65.0f, 360};
        overlap.vx = side ? 800 : -800;
        overlap.vy = 0;
        assert(overlap.tick(Step, 0, 0) & Paddle);
        assert(!(overlap.tick(Step, 0, 0) & Paddle));

        auto fast = rally();
        fast.ball = {side ? 1170.0f : 110.0f, 360};
        fast.vx = side ? 3000 : -3000;
        fast.vy = 0;
        assert(fast.tick(.05f, 0, 0) & Paddle);
        assert((fast.vx > 0) == (side == 0));

        auto miss = rally();
        miss.ball = {side ? 1214.0f : 66.0f, 425};
        miss.vx = side ? 1 : -1;
        miss.vy = 0;
        // 擴張方框內，但圓距離矩形角落大於半徑，應該不算命中。
        assert(!(miss.tick(Step, 0, 0) & Paddle));

        auto away = rally();
        away.ball = {side ? 1215.0f : 65.0f, 360};
        away.vx = side ? -570 : 570;
        away.vy = 0;
        assert(!(away.tick(Step, 0, 0) & Paddle));
    }

    // 最高正式球速、兩種垂直方向、左右側、不同拍高及合法移拍方向。
    for (int side = 0; side < 2; ++side) {
        for (float y : {80.0f, 360.0f, 640.0f}) {
            for (int vertical : {-1, 0, 1}) {
                for (int movement : {-1, 0, 1}) {
                    auto g = rally();
                    g.bats[side].y = y;
                    g.ball = {side ? 1208.0f : 72.0f, y};
                    g.vx = side ? 640.0f : -640.0f;
                    g.vy = vertical * 480.0f;
                    assert(g.tick(Step, side ? 0 : movement, side ? movement : 0) & Paddle);
                    assert((g.vx > 0) == (side == 0));
                    assert(std::hypot(g.vx, g.vy) <= 800.01f);
                }
            }
        }
    }
    std::cout << "Collision regressions passed\n";
}
