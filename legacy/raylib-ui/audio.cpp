#include "audio.h"

#include "game.h"

#include <algorithm>

#include <cmath>

#include <vector>

Audio::Audio(bool enabled) {
    if (!enabled) return;
    InitAudioDevice();
    ready = IsAudioDeviceReady();
    if (!ready) return;

    const float start[]{300,680,480}, end[]{300,680,720};

    const int count[]{1680,2160,7680};

    constexpr unsigned rate = 48000;

    for (int i=0; i<3; ++i) {
        std::vector<short> pcm(count[i]);

        double phase = 0;

        for (int n=0; n<count[i]; ++n) {

            const double hz = start[i]+(end[i]-start[i])*n/count[i];
            phase += 6.283185307179586*hz/rate;

            const double env = std::min({1.0, n/192.0, (count[i]-1-n)/192.0});
            pcm[n] = static_cast<short>(7000*env*std::sin(phase));
        }

        Wave wave{static_cast<unsigned>(count[i]),rate,16,1,pcm.data()};
        sounds[i] = LoadSoundFromWave(wave);
    }
}

Audio::~Audio() {
    if (!ready) return;

    for (auto sound : sounds) if (IsSoundValid(sound)) UnloadSound(sound);
    CloseAudioDevice();
}

void Audio::play(int events) const {
    if (!ready || events == None) return;

    const int index = (events & Score) ? 2 : (events & Paddle) ? 1 : 0;
    if (IsSoundValid(sounds[index])) PlaySound(sounds[index]);
}
