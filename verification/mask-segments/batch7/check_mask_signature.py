#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
# AI-assisted public test checker. No third-party reproducer code is copied.
"""Check every word of the three-phase masked-segment signature.

The oracle scatters architectural elements into byte arrays. It does not model
Ara queues, lane shuffling, internal starts, or the candidate RTL expression.
Use a separate event observer for those implementation properties.
"""
import argparse
import hashlib
import json
from pathlib import Path
import re
import struct


def configuration(nf):
    return (8, 3, 7) if nf == 5 else (1, 129, 1)


def mask_bytes(nf, phase):
    original = bytes([0xa6] * 256) if nf == 5 else bytes(
        [0xa6] * 16 + [0x59] * 16 + [0xc9] * 224)
    return original if phase == 0 else bytes(x ^ 255 for x in original) if phase == 1 else bytes([255] * 256)


def initial_images(nf, identity):
    old = bytes((0x91 ^ identity * 37 ^ field * 29 ^ offset * 11 ^ (offset >> 3)) & 255
                for field in range(nf) for offset in range(256))
    memory = bytes((0x53 ^ identity * 19 ^ offset * 23 ^ (offset >> 2)) & 255
                   for offset in range(512))
    return old, memory


def images(nf, family, phase, override_mask=None):
    width, vl, selected = configuration(nf)
    identity = selected + 8 * phase
    old, memory = initial_images(nf, identity)
    out, after = bytearray(old), bytearray(memory)
    mask = mask_bytes(nf, phase) if override_mask is None else override_mask
    # Interpret one mathematical integer with architectural bit numbering.
    active = (1 << vl) - 1
    if phase != 2:
        active &= int.from_bytes(mask, "little")
    while active:
        bit = active & -active
        element = bit.bit_length() - 1
        active ^= bit
        for field in range(nf):
            register_start = field * 256 + element * width
            memory_start = (element * nf + field) * width
            if family == 1:
                out[register_start:register_start + width] = memory[memory_start:memory_start + width]
            else:
                after[memory_start:memory_start + width] = old[register_start:register_start + width]
    return bytes(out), bytes(after)


def words(data):
    if len(data) % 4:
        raise ValueError("Byte count is not a multiple of four")
    return list(struct.unpack("<" + "I" * (len(data) // 4), data))


def u64(value):
    return [value & 0xffffffff, (value >> 32) & 0xffffffff]


def expected_signature(nf, family):
    width, vl, selected = configuration(nf)
    case_words = 16 + nf * 64 + 128 + 28
    signature_words = 16 + 3 * case_words + 10
    checks = 3 * (6 + 2 * nf * 256 + 512 + 2 * 512 + 2 * 256 + 14) + 3
    result = [0x56314d47, 2, family, nf, 15, 3, 256, checks, 0, 3,
              case_words, signature_words, vl, 0, 0, 0]
    for phase in range(3):
        identity = selected + 8 * phase
        result += [identity, width * 8, width * 8, int(phase < 2), 2, 0,
                   vl, 0, vl, 0, (selected // 2) * 8, 0, 0, 0, 256 // width, 0]
        out, memory = images(nf, family, phase)
        result += words(out) + words(memory)
        for which in range(7):
            before = 0x123456789abcdef0 ^ (identity << 40) ^ (which * 0x0102030405060708)
            result += u64(before) + u64(before ^ 0xffffffffffffffff)
    result += [0] * 10
    assert len(result) == signature_words
    assert (checks, signature_words) == ((13887, 1502) if nf == 5 else (9279, 926))
    return result


def compare(actual, nf, family):
    expected = expected_signature(nf, family)
    differences = [{"word": i, "got": actual[i], "want": expected[i]}
                   for i in range(min(len(actual), len(expected))) if actual[i] != expected[i]]
    return {"assessment": "PASS" if len(actual) == len(expected) and not differences else "FAIL",
            "actual_words": len(actual), "expected_words": len(expected),
            "checks": expected[7], "different_words": len(differences),
            "first_differences": differences[:12], "length_matches": len(actual) == len(expected)}


def read_signature(path, binary):
    raw = path.read_bytes()
    if binary:
        return words(raw), raw
    tokens = raw.decode("ascii").split()
    if not tokens or any(re.fullmatch(r"(?:0x)?[0-9a-fA-F]{8}", token) is None for token in tokens):
        raise ValueError("Expected one 32-bit hexadecimal word per token, without addresses or unknown bits")
    return [int(token, 16) for token in tokens], raw


def controls(actual, nf, family):
    """Mutate real or fixture payload data while preserving successful counters."""
    if compare(actual, nf, family)["assessment"] != "PASS":
        raise ValueError("Negative controls require a complete passing input signature")
    stride = 16 + nf * 64 + 128 + 28
    cases = {}
    for phase in range(3):
        base = 16 + phase * stride
        # This changes actual exported data, without changing the error counter.
        changed = actual.copy()
        location = base + 16 + (nf * 64 if family == 2 else 0)
        changed[location] ^= 1
        cases["phase%d_data_bit" % phase] = compare(changed, nf, family)["assessment"] == "FAIL"
        changed = actual.copy()
        changed[base + 16 + nf * 64 + 128] ^= 1
        cases["phase%d_guard" % phase] = compare(changed, nf, family)["assessment"] == "FAIL"
        changed = actual.copy()
        changed[base + 12] = 1
        cases["phase%d_vstart" % phase] = compare(changed, nf, family)["assessment"] == "FAIL"
    # A stale first-phase mask applied to the second operation has valid metadata.
    changed = actual.copy()
    out, memory = images(nf, family, 1, mask_bytes(nf, 0))
    start = 16 + stride + 16
    changed[start:start + nf * 64 + 128] = words(out + memory)
    cases["stale_first_mask_in_second_phase"] = changed != actual and compare(changed, nf, family)["assessment"] == "FAIL"
    if nf == 2:
        changed = actual.copy()
        mask = mask_bytes(nf, 0)
        out, memory = images(nf, family, 0, mask[:16] + mask[:16] + mask[32:])
        start = 32
        changed[start:start + nf * 64 + 128] = words(out + memory)
        cases["repeated_first_mask_word"] = changed != actual and compare(changed, nf, family)["assessment"] == "FAIL"
    changed = actual.copy()
    changed[16 + stride] = changed[16]
    cases["duplicate_phase_identity"] = compare(changed, nf, family)["assessment"] == "FAIL"
    cases["truncated_final_record"] = compare(actual[:-20], nf, family)["assessment"] == "FAIL"
    changed = actual.copy()
    changed[7] -= 1
    cases["wrong_check_count"] = compare(changed, nf, family)["assessment"] == "FAIL"
    return {"assessment": "PASS" if all(cases.values()) else "FAIL", "rejected": cases}


def self_test():
    rows = []
    for nf in (5, 2):
        for family in (1, 2):
            expected = expected_signature(nf, family)
            row = {"nf": nf, "family": family, "positive": compare(expected, nf, family),
                   "negative_controls": controls(expected, nf, family)}
            rows.append(row)
    return {"kind": "offline_gold_fixture_checks", "rtl_execution": False,
            "actual_run_data": False,
            "assessment": "PASS" if all(row["negative_controls"]["assessment"] == "PASS" for row in rows) else "FAIL",
            "rows": rows}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--nf", type=int, choices=(2, 5))
    parser.add_argument("--family", type=int, choices=(1, 2))
    parser.add_argument("--signature", type=Path)
    parser.add_argument("--binary", action="store_true", help="Read a little-endian raw binary signature")
    parser.add_argument("--negative-controls", action="store_true", help="Mutate the actual passing signature and require rejection")
    parser.add_argument("--self-test", action="store_true")
    args = parser.parse_args()
    if args.self_test:
        result = self_test()
    else:
        if args.nf is None or args.family is None or args.signature is None:
            parser.error("A real review requires --nf, --family, and --signature")
        actual, raw = read_signature(args.signature, args.binary)
        result = compare(actual, args.nf, args.family)
        result.update(kind="actual_signature_review", signature_sha256=hashlib.sha256(raw).hexdigest(),
                      nf=args.nf, family=args.family)
        if args.negative_controls:
            result["negative_controls"] = controls(actual, args.nf, args.family)
            if result["negative_controls"]["assessment"] != "PASS":
                result["assessment"] = "FAIL"
    result["checker_sha256"] = hashlib.sha256(Path(__file__).read_bytes()).hexdigest()
    print(json.dumps(result, indent=2))
    return 0 if result["assessment"] == "PASS" else 1


if __name__ == "__main__":
    raise SystemExit(main())
