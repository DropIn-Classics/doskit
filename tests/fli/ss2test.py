#!/usr/bin/env python3
"""flifiles.py's SS2 (FLC chunk 7) decoder on hand-made packets: a line
skip, a last-pixel word, literal and repeated words, a second line; and
that a packet past the line's end is refused; the pad byte.

    python3 tests/fli/ss2test.py
"""
import os, struct, sys

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)),
                                '..', '..', 'tools'))
from flifiles import check_padding, decode_ss2

W, H = 8, 4
data = (struct.pack('<H', 2) +
        struct.pack('<HH', 0xFFFF, 0x8009) +     # skip 1 line; last pixel 9
        struct.pack('<H', 2) +                   # line 1: two packets
        bytes((1, 1)) + b'\x01\x02' +            # x=1: literal word 01 02
        bytes((1, 0xFE)) + b'\x05\x06' +         # x=4: word 05 06 twice
        struct.pack('<H', 1) +                   # line 2: one packet
        bytes((0, 1)) + b'\x07\x08')
pixels = bytearray(W * H)
decode_ss2(data, pixels, W, H)
want = bytes(8) + bytes((0, 1, 2, 0, 5, 6, 5, 9)) + \
    bytes((7, 8, 0, 0, 0, 0, 0, 0)) + bytes(8)
assert bytes(pixels) == want, pixels.hex()

# a pad byte to an even length may be anything; an odd length or more
# bytes is refused
check_padding(b'abcZ', 3, 'pad')
for data, pos in ((b'abZ', 2), (b'abZZ', 2)):
    try:
        check_padding(data, pos, 'pad')
    except ValueError:
        pass
    else:
        raise AssertionError(f'padding {data[pos:]} was accepted')

bad = struct.pack('<HH', 1, 1) + bytes((6, 2)) + b'\0' * 4
try:
    decode_ss2(bad, bytearray(W * H), W, H)
except ValueError:
    pass
else:
    raise AssertionError('a packet past the line was accepted')
print('ss2 ok')
