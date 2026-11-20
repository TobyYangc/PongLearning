#pragma once

#include "raylib.h"
struct Audio {

    Sound sounds[3]{};

    bool ready = false;

    explicit Audio(bool enabled);

    ~Audio();

    void play(int events) const;

    Audio(const Audio&) = delete;

    Audio& operator=(const Audio&) = delete;
};
