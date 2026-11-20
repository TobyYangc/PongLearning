#include "game.h"

#include <cassert>

#include <cmath>
#include <iostream>

int main() {

    Config cfg;
    cfg.target = 5;

    Game game(cfg,42);
    game.resetPoint();
    game.tick(Step,1,0);
    assert(game.bats[0].y > 360 && game.ball.x == 640);

    float remaining = game.timer;
    game.pause();
    game.tick(Step,1,1);
    assert(game.timer == remaining);
    game.resume();
    assert(game.now_phase == Phase::Serve && game.timer == remaining);
    game.last = 0;
    game.launch();
    assert(game.vx > 0 && std::abs(std::hypot(game.vx,game.vy)-570)<.01f);

    const auto x = game.ball.x;

    const auto vx = game.vx;
    game.pause();
    game.resume();

    for (int i=0; i<360; ++i) game.tick(Step,1,1);
    assert(game.now_phase == Phase::Rally && game.ball.x == x && game.vx == vx);
    game.tick(Step,0,0);
    assert(game.ball.x > x);

    for (int side=0; side<2; ++side) {
        game.ball = {side ? 1210.0f : 70.0f, 360};
        game.vx = side ? 570 : -570;
        game.vy = 0;
        game.bats[side].y = 360;
        assert(game.tick(Step,0,0) & Paddle);
        assert((game.vx > 0) == (side == 0));
        assert(std::abs(std::hypot(game.vx,game.vy)-598.5f)<.01f);
        assert(!(game.tick(Step,0,0) & Paddle));
    }
    game.ball = {640,11};
    game.vx = 500;
    game.vy = -300;

    const float speed = std::hypot(game.vx,game.vy);
    assert(game.tick(Step,0,0) & Wall);
    assert(game.vy > 0 && std::abs(speed-std::hypot(game.vx,game.vy))<.01f);
    game.score[0] = 4;
    game.ball = {W+9,200};
    game.vx = 570;
    assert(game.tick(Step,0,0) & Score);
    assert(game.now_phase == Phase::Over && game.score[0] == 5);
    assert(game.tick(Step,0,0) == None && game.score[0] == 5);
    game.restart();
    assert(game.roundId == 2 && game.score[0] == 0 && game.now_phase == Phase::Serve);

    cfg.mode = "pvcpu";
    cfg.side = "left";

    Game cpu(cfg,42);
    cpu.resetPoint();
    cpu.now_phase = Phase::Rally;
    cpu.ball = {250,450};
    cpu.vx = 500;
    cpu.tick(Step,0,0);
    assert(cpu.bats[1].y == 360);

    for (int i=0; i<25; ++i) cpu.tick(Step,0,0);
    assert(cpu.ball.x < W/2 && cpu.bats[1].y > 360);

    float y = cpu.bats[1].y;
    cpu.vx = -500;
    cpu.tick(Step,0,0);
    assert(cpu.bats[1].y == y);
    std::cout << "Core checks passed\n";
}
