"""三種短音先合成 WAV，播放使用 Windows 非同步 API，不阻塞 Tkinter。"""
import math
import struct
import tempfile
import wave
from pathlib import Path
import winsound

class Audio:
    #在系統暫存目錄合成3段48kHz、16-bit、單聲道WAV；索引0撞牆、1擊拍、2得分。
    def __init__(self):
        self.temp = tempfile.TemporaryDirectory(prefix="pong-audio-")
        self.paths = []
        try:
            for i, (start, end, count) in enumerate(((300, 300, 1680), (680, 680, 2160), (480, 720, 7680))):
                path = Path(self.temp.name) / f"{i}.wav"
                phase = 0
                pcm = bytearray()
                for n in range(count):
                    hz = start + (end - start) * n / count
                    #一個取樣前進2π×頻率/取樣率弧度，累積相位才能連續產生波形。
                    phase += 2 * math.pi * hz / 48000
                    #頭尾各192個取樣淡入淡出，192/48000=0.004秒，減少突然起落的爆音。
                    envelope = min(1, n / 192, (count - 1 - n) / 192)
                    #將波形轉成小端序16-bit有號整數，寫入WAV音訊資料。
                    pcm += struct.pack("<h", int(7000 * envelope * math.sin(phase)))
                with wave.open(str(path), "wb") as wav:
                    wav.setparams((1, 2, 48000, 0, "NONE", "not compressed"))
                    wav.writeframes(pcm)
                self.paths.append(path)
        except OSError:
            self.paths = []

    #events是事件位元集合；優先Score，再Paddle，再Wall。SND_ASYNC讓播放不阻塞Tk事件迴圈。
    def play(self, events, enabled):
        if not events or not enabled or not self.paths:
            return
        #三元判斷按得分→擊拍→撞牆挑一種；&檢查events是否包含該事件位元。
        index = 2 if events & 4 else 1 if events & 2 else 0
        try:
            winsound.PlaySound(str(self.paths[index]), winsound.SND_FILENAME |
                               winsound.SND_ASYNC | winsound.SND_NODEFAULT)
        except (OSError, RuntimeError):
            pass

    #先停止目前音效，才清理TemporaryDirectory中的WAV，避免播放中的檔案仍被使用。
    def close(self):
        try:
            winsound.PlaySound(None, 0)
        except RuntimeError:
            pass
        self.temp.cleanup()
