#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
"""Compare complete index signatures with an independent byte-address model.

Prepared with AI assistance. This tool does not run a simulator.
"""
import argparse
import json
from pathlib import Path
import re


def u32(value):
    return value & 0xffffffff


def guard(ident, member, side):
    return 0x13579bdf ^ (ident << 16) ^ (member << 8) ^ (0xffffffff if side else 0)


def old(ident, field, element):
    return u32(0x91400000 ^ ident * 0x10001 ^ field * 0x10001001 ^ element * 0x1010103)


def memory(ident, element):
    return u32(0x52340000 ^ ident * 0x10101 ^ element * 0x1020407)


def expected(nf, first, count, vl):
    header, stride = (8, 148) if nf == 1 else (12, 52 + nf * 64)
    checks = 3 + count * (728 if nf == 1 else 568 + nf * 128)
    result = [0] * (header + 16 * stride)
    result[:header] = ([0x56314f49, 1, 16, 32, vl, 256, checks, 0] if nf == 1 else
                      [0x56314f53, 1, 16, nf, 32, vl, 256, checks, 0, count, stride, len(result)])
    for ident in range(first, first + count):
        store, masked = (ident % 4) // 2, ident % 2
        base = header + ident * stride
        out = [[old(ident, f, i) for i in range(64)] for f in range(nf)]
        mem = [memory(ident, i) for i in range(64 if nf == 1 else 32)]
        for element in range(vl):
            if masked and not element % 2:
                continue
            address = 8 if store else [8, 0, 12][element]
            for field in range(nf):
                word = address // 4 + field
                if store:
                    mem[word] = out[field][element]
                else:
                    out[field][element] = mem[word]
        meta = [ident, store, 8 << (ident // 4), masked, 2,
                vl, 0, vl, 0, 16, 0, 0, 0, 0, 64,
                sum(not masked or e % 2 for e in range(vl))]
        payload = meta + [x for field in out for x in field] + mem
        payload += [guard(ident, m, side) for m in (1, 2) for side in (0, 1)]
        assert len(payload) == stride
        result[base:base + stride] = payload
    return result


def pair_regions():
    def value(buffer, element):
        return u32(0x53790000 ^ buffer * 0x110203 ^ element * 0x1020407)
    offsets = [0, 12, 4, 16, 8] + [(i % 64) * 4 for i in range(5, 256)]
    buffers, observed = [], []
    for b in range(6):
        data = [value(b, i) if b < 4 else
                value(b - 4, offsets[i] // 4) if i < 5 else value(b - 2, i)
                for i in range(64)]
        observed += data
        buffers += [0x13579bdf ^ b] + data + [0xeca86420 ^ b]
    packed = [sum(offsets[i+j] << (8*j) for j in range(4)) for i in range(0, 256, 4)]
    return [observed, [5, 0, 5, 0, 16, 0, 0, 0, 0, 0, 1, 0], buffers, packed]


def read_words(path):
    lines = path.read_text().splitlines()
    if not all(re.fullmatch('[0-9a-fA-F]{8}', line) for line in lines):
        raise ValueError('Unknown or malformed signature word')
    return [int(line, 16) for line in lines]


def compare(actual, gold):
    diffs = [dict(index=i, got=hex(a), want=hex(b))
             for i, (a, b) in enumerate(zip(actual, gold)) if a != b]
    return dict(assessment='PASS' if len(actual) == len(gold) and not diffs else 'FAIL',
                actual_words=len(actual), expected_words=len(gold),
                different_words=len(diffs), first_differences=diffs[:20])


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--nf', type=int, choices=(1, 2))
    p.add_argument('--first', type=int, default=12)
    p.add_argument('--count', type=int, default=1)
    p.add_argument('--vl', type=int, default=3)
    p.add_argument('--pair', action='store_true')
    p.add_argument('--signature', type=Path)
    p.add_argument('--regions', type=Path, nargs=4)
    p.add_argument('--write-gold', type=Path)
    p.add_argument('--diagnostic', action='store_true')
    args = p.parse_args()
    if args.pair:
        a, b, c, d = pair_regions()
        gold = a + b + [0]*4 + c + [0]*4 + d
    else:
        if args.nf is None or args.vl not in (2, 3) or args.first + args.count > 16:
            p.error('Select a supported fixed NF/VL/interval')
        if args.nf == 2 and args.vl != 3:
            p.error('The NF2 C input fixes VL to three')
        if args.nf == 1 and args.vl == 2 and any((i % 4) // 2 for i in range(args.first, args.first + args.count)):
            p.error('The selected VL2 scope includes loads only')
        gold = expected(args.nf, args.first, args.count, args.vl)
    if args.write_gold:
        args.write_gold.write_text(''.join('%08x\n' % x for x in gold))
    if args.regions:
        if not args.pair:
            p.error('--regions requires --pair')
        checks = [compare(read_words(path), want)
                  for path, want in zip(args.regions, pair_regions())]
        result = dict(assessment='PASS' if all(x['assessment'] == 'PASS' for x in checks) else 'FAIL', regions=checks)
    elif args.signature:
        result = compare(read_words(args.signature), gold)
    else:
        result = dict(assessment='GOLD_ONLY', words=len(gold), rtl_executed=False)
    print(json.dumps(result, indent=2))
    if result['assessment'] == 'FAIL' and not args.diagnostic:
        raise SystemExit(1)


if __name__ == '__main__':
    main()
