"""Converts a raw mic recording of the OcarinaMod 3DS plugin to a WAV file you can play and analyse on the PC.

Raw format written by the plugin: PCM 16-bit signed little endian, mono, 8182 Hz, no header.

Usage (stdlib only, works with `python -I`):
    python -I raw_to_wav.py rec1.raw                 -> rec1.wav next to the input, prints statistics
    python -I raw_to_wav.py rec1.raw out.wav         -> explicit output name
    python -I raw_to_wav.py rec1.raw --gain 4        -> multiply the samples by 4 (clipped) to hear quiet takes
    python -I raw_to_wav.py rec1.raw --normalize     -> scale so the loudest sample reaches ~90% of full scale
Statistics printed: duration, peak, RMS, clipped samples, and a one-line loudness map (one character per second).
"""
import array
import os
import sys
import wave

RATE = 8182          # Hz (the console's "8180" setting is really 8182.1245 Hz)


def load(path):
    with open(path, "rb") as f:
        data = f.read()
    if len(data) % 2:
        data = data[:-1]
    samples = array.array("h")
    samples.frombytes(data)
    if sys.byteorder == "big":
        samples.byteswap()
    return samples


def stats(samples):
    n = len(samples)
    if n == 0:
        return {"n": 0, "peak": 0, "rms": 0.0, "clip": 0, "mean": 0.0}
    peak = max(max(samples), -min(samples))
    total = sum(samples)
    sq = sum(s * s for s in samples)
    clip = sum(1 for s in samples if s >= 32000 or s <= -32000)
    return {"n": n, "peak": peak, "rms": (sq / n) ** 0.5, "clip": clip, "mean": total / n}


def loudness_map(samples):
    """One character per second: peak level 0-9 relative to full scale (log-ish steps), '.' = silence."""
    chars = []
    for start in range(0, len(samples), RATE):
        chunk = samples[start:start + RATE]
        pk = max(max(chunk), -min(chunk)) if len(chunk) else 0
        if pk < 100:
            chars.append(".")
        else:
            level = min(9, int(pk / 3600))
            chars.append(str(level))
    return "".join(chars)


def apply_gain(samples, gain):
    out = array.array("h", samples)
    for i, s in enumerate(out):
        v = int(round(s * gain))
        out[i] = 32767 if v > 32767 else (-32768 if v < -32768 else v)
    return out


def write_wav(path, samples):
    data = array.array("h", samples)
    if sys.byteorder == "big":
        data.byteswap()
    with wave.open(path, "wb") as w:
        w.setnchannels(1)
        w.setsampwidth(2)
        w.setframerate(RATE)
        w.writeframes(data.tobytes())


def main(argv):
    args = [a for a in argv[1:] if not a.startswith("--")]
    gain = None
    normalize = "--normalize" in argv
    if "--gain" in argv:
        i = argv.index("--gain")
        try:
            gain = float(argv[i + 1])
        except (IndexError, ValueError):
            print("--gain needs a number")
            return 2
        args = [a for a in args if a != argv[i + 1]]
    if not args or len(args) > 2:
        print(__doc__)
        return 2
    src = args[0]
    dst = args[1] if len(args) > 1 else os.path.splitext(src)[0] + ".wav"
    samples = load(src)
    st = stats(samples)
    print("file     :", src)
    print("samples  : %d  (%.2f s at %d Hz)" % (st["n"], st["n"] / RATE, RATE))
    print("peak     : %d  (%.1f%% of full scale)" % (st["peak"], st["peak"] * 100.0 / 32768))
    print("rms      : %.1f   mean (DC offset): %.1f   clipped: %d" % (st["rms"], st["mean"], st["clip"]))
    print("per second peak (.=silent, 0-9=level):", loudness_map(samples))
    if normalize and st["peak"] > 0:
        gain = 29500.0 / st["peak"]
    if gain is not None:
        samples = apply_gain(samples, gain)
        print("gain     : x%.2f" % gain)
    write_wav(dst, samples)
    print("wrote    :", dst)
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
