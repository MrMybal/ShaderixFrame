"""Generate the original demonstration soundtrack on demand."""
import array
import math
import wave


def write_demo_audio(path):
    samples = array.array('h')
    for i in range(12 * 48000):
        t = i / 48000
        fade = min(1, t / 1.5, (12 - t) / 2)
        left = sum(math.sin(t * 2 * math.pi * f) for f in (110, 164.8138, 220, 261.6256))
        right = sum(math.sin(t * 2 * math.pi * f * 1.002) for f in (110, 164.8138, 220, 261.6256))
        pulse = 0.6 + 0.4 * math.sin(t * math.pi / 3) ** 2
        samples.extend((int(left * 700 * fade * pulse), int(right * 700 * fade * pulse)))
    with wave.open(str(path), 'wb') as file:
        file.setnchannels(2)
        file.setsampwidth(2)
        file.setframerate(48000)
        file.writeframes(samples.tobytes())
