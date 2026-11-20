#include "game.h"
#include <cassert>
#include <iostream>
int main() {
    Config settings;
    settings.side = "right";
    Game game(settings, 11);
    assert(game.human() == 1);
    assert(game.ball.x == W/2 && game.ball.y == H/2);
    assert(game.now_phase == Phase::Intro);
    std::cout << "Stage 1 passed\n";
}
