#!/usr/bin/env python3
"""Makes tests/cdplay/TONE.OGG, the Ogg Vorbis track of the selftest's cue
sheet: one second (44100 samples) at 44.1 kHz stereo, the left channel a
441 Hz sine at half scale, the right 1102.5 Hz at quarter scale.  Written
for the kit; selftest.py checks the tones' pitch and loudness in what the
runner decodes.

    python3 tests/cdplay/make_tone.py

It needs `soundfile` (pip install soundfile numpy; libsndfile's Vorbis
encoder), which the kit does not otherwise use: the file is made once and
kept, selftest.py only reads it.  Made with soundfile 0.13.1, libsndfile
1.2.2.
"""
import math, os

import numpy as np
import soundfile as sf

RATE, N = 44100, 44100
t = np.arange(N) / RATE
left = 0.5 * np.sin(2 * math.pi * 441 * t)
right = 0.25 * np.sin(2 * math.pi * 1102.5 * t)
out = os.path.join(os.path.dirname(os.path.abspath(__file__)), 'TONE.OGG')
sf.write(out, np.stack([left, right], axis=1), RATE, format='OGG', subtype='VORBIS')
print(out, os.path.getsize(out), 'bytes')
