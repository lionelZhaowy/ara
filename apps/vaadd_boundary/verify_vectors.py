# SPDX-License-Identifier: Apache-2.0
import pathlib
rows = 0
for line in pathlib.Path(__file__).with_name("vectors.txt").read_text().splitlines():
    signed, width, mode, a, b, gold = line.split()
    width, mode = int(width), int(mode)
    assert signed == "1" and width in [8, 16, 32, 64] and mode in range(4)
    a, b, expected = int(a, 16), int(b, 16), 0
    mask = (1 << width) - 1
    for offset in range(0, 64, width):
        x, y = (a >> offset) & mask, (b >> offset) & mask
        if x >> (width - 1): x -= 1 << width
        if y >> (width - 1): y -= 1 << width
        total = x + y
        floor, odd = total // 2, total & 1
        increment = [odd, odd & (floor & 1), 0, odd & (~floor & 1)][mode]
        expected |= ((floor + increment) & mask) << offset
    assert expected == int(gold, 16), rows
    rows += 1
assert rows == 256
print("VAADD vectors: 256 packed rows match unbounded signed arithmetic")
