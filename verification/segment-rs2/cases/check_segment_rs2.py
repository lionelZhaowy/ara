#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
# Prepared with AI assistance.
"""Independent byte oracle and ELF encoding checks for the fixed rs2 probe.

No simulator, build, input mutation, or runtime framework. The runtime owner
must separately validate ELF/simv binding, actual stores, completion and traps.
"""
import argparse
from collections import Counter
import hashlib
import json
from pathlib import Path
import re
import struct

KINDS = ((2, 0x07, 8), (2, 0x07, 9),
         (2, 0x27, 8), (2, 0x27, 9),
         (1, 0x07, 8), (1, 0x07, 12),
         (1, 0x27, 8), (1, 0x27, 12),
         (3, 0x07, 8), (3, 0x07, 12),
         (3, 0x27, 8), (3, 0x27, 12))


def oracle(legacy=False):
    """Model accesses as (element, field) byte addresses; no RTL predicates."""
    values = [256]
    cases = []
    for ident, (mode, opcode, reg) in enumerate(KINDS):
        store = opcode == 0x27
        field_values = [0xa1, 0xb2] if store else [0xd1, 0xe2]
        registers = [[value, value] for value in field_values]
        memory = list(range(0x40, 0x50))
        # The ISA has two fields and one active element. The legacy failure
        # model suppresses field 1 only in the six independently chosen probes.
        active_fields = [0] if legacy and ident in (0, 2, 4, 6, 8, 10) else [0, 1]
        addresses = {(0, f): (4 * 0 if mode == 2 else [3, 7][0]) + f
                     for f in range(2)}
        for field in active_fields:
            address = addresses[0, field]
            if store:
                memory[address] = registers[field][0]
            else:
                registers[field][0] = memory[address]
        observations = [2, 1, 2, 1, 0, 0] + memory
        observations += [registers[f][i] for f in range(2) for i in range(2)]
        observations += [3, 7, 3, 7]
        observations += [0x5a3c91e700000000 + (ident << 16) + (member << 8) + side
                         for member in range(4) for side in range(2)]
        assert len(observations) == 38
        cases.append(observations)
        values += observations
    values.append(12)
    assert len(values) == 458
    return values, cases


def check_values(actual, variant):
    want, _ = oracle(legacy=variant == 'baseline')
    if len(actual) != len(want):
        raise ValueError('Expected exactly 458 actual uint64 comparisons')
    wrong = [dict(index=i, got=got, want=expected)
             for i, (got, expected) in enumerate(zip(actual, want)) if got != expected]
    if wrong:
        raise ValueError('Unexpected observations: ' + json.dumps(wrong[:12]))
    architectural, _ = oracle()
    errors = [dict(index=i, case=(i-1)//38, field=(i-1)%38,
                   got=got, want=expected)
              for i, (got, expected) in enumerate(zip(actual, architectural)) if got != expected]
    assert len(errors) == (6 if variant == 'baseline' else 0)
    return dict(status='PASS', variant=variant, cases=12, checks=458, vsets=36,
                architectural_errors=errors, controls_are_not_ordinary_pass=variant == 'baseline')


def signature_values(path):
    words = []
    for line in path.read_text().splitlines():
        line = line.strip()
        if not line:
            continue
        if not re.fullmatch(r'[0-9a-fA-F]{8}', line):
            raise ValueError('Expected 32-bit signature-granularity=4 words, one per line')
        words.append(int(line, 16))
    if len(words) != 916:
        raise ValueError('Expected exactly 916 signature words')
    return [words[i] | (words[i+1] << 32) for i in range(0, len(words), 2)]


def executable_ranges(data):
    """Select code sections and exclude RISC-V $d mapping-symbol ranges.

    Spike test.ld can put .text and .sdata in one RWX PT_LOAD segment.
    PT_LOAD permissions therefore cannot distinguish instructions from data.
    """
    if data[:6] != b'\x7fELF\x02\x01' or len(data) < 64:
        raise ValueError('Expected little-endian ELF64')
    if struct.unpack_from('<H', data, 18)[0] != 243:
        raise ValueError('Expected RISC-V ELF')
    phoff, shoff = struct.unpack_from('<QQ', data, 32)
    phsize, phnum, shsize, shnum, shstrndx = struct.unpack_from('<HHHHH', data, 54)
    if phsize != 56 or shsize != 64 or not shnum or shstrndx >= shnum:
        raise ValueError('Expected ordinary ELF64 program and section headers')
    if phoff+phsize*phnum > len(data) or shoff+shsize*shnum > len(data):
        raise ValueError('Truncated ELF headers')
    sections = [struct.unpack_from('<IIQQQQIIQQ', data, shoff+n*shsize)
                for n in range(shnum)]
    segments = [struct.unpack_from('<IIQQQQQQ', data, phoff+n*phsize)
                for n in range(phnum)]
    def string(table, offset):
        if offset >= len(table):
            raise ValueError('ELF string offset is outside its table')
        return table[offset:table.index(b'\0', offset)].decode('ascii')
    names = sections[shstrndx]
    strings = data[names[4]:names[4]+names[5]]
    mapping = {}
    for symbol_section in sections:
        if symbol_section[1] not in (2, 11):
            continue
        _, _, _, _, offset, size, link, _, _, entsize = symbol_section
        if entsize != 24 or size % 24 or link >= shnum or offset+size > len(data):
            raise ValueError('Invalid ELF64 symbol table')
        strsec = sections[link]
        strtab = data[strsec[4]:strsec[4]+strsec[5]]
        for cursor in range(offset, offset+size, 24):
            nameoff, info, other, shndx, value, symsize = struct.unpack_from('<IBBHQQ', data, cursor)
            name = string(strtab, nameoff)
            kind = ('d' if name == '$d' or name.startswith('$d.') else
                    'x' if name == '$x' or name.startswith(('$x.', '$xrv')) else None)
            if kind is not None and 0 < shndx < shnum:
                old = mapping.setdefault(shndx, {}).setdefault(value, kind)
                if old != kind:
                    raise ValueError('Conflicting ELF instruction/data mapping symbols')
    result = []
    for n, section in enumerate(sections):
        nameoff, kind, flags, va, offset, size, link, info, align, entsize = section
        if not flags & 4 or not size:  # SHF_EXECINSTR, rather than PF_X.
            continue
        name = string(strings, nameoff)
        if kind != 1 or offset+size > len(data):
            raise ValueError('Executable section is not complete SHT_PROGBITS: '+name)
        if not any(p[0] == 1 and p[1] & 1 and p[2] <= offset and
                   offset+size <= p[2]+p[5] and p[3]+offset-p[2] == va
                   for p in segments):
            raise ValueError('Code section is not backed by an executable PT_LOAD: '+name)
        points = mapping.get(n, {})
        if any(addr < va or addr > va+size for addr in points):
            raise ValueError('Mapping symbol is outside its code section')
        cursor, state = va, 'x'
        for end, new_state in sorted(points.items())+[(va+size, 'd')]:
            if end > cursor and state == 'x':
                result.append(dict(section=name, va=cursor,
                                   offset=offset+cursor-va, size=end-cursor))
            cursor, state = end, new_state
    if not result:
        raise ValueError('No executable instruction ranges')
    return result


def inspect_elf(path):
    data = path.read_bytes()
    ranges = executable_ranges(data)
    rows = []
    for area in ranges:
        va, offset, size = area['va'], area['offset'], area['size']
        if size % 2:
            raise ValueError('Instruction range is not halfword aligned')
        cursor = 0
        while cursor < size:
            half = struct.unpack_from('<H', data, offset+cursor)[0]
            if half & 3 != 3:
                cursor += 2
                continue
            if (half >> 2) & 7 == 7:
                raise ValueError('Unexpected instruction longer than 32 bits in code range')
            if cursor + 4 > size:
                raise ValueError('Truncated 32-bit instruction')
            raw = struct.unpack_from('<I', data, offset+cursor)[0]
            opcode, nf = raw & 0x7f, (raw >> 29) & 7
            if opcode in (0x07, 0x27) and nf == 1:
                row = dict(pc=hex(va+cursor), raw=hex(raw), opcode=opcode,
                           mop=(raw >> 26) & 3, vm=(raw >> 25) & 1,
                           mew=(raw >> 28) & 1, rs2=(raw >> 20) & 31,
                           rs1=(raw >> 15) & 31, width=(raw >> 12) & 7,
                           vd_vs3=(raw >> 7) & 31)
                if (row['vm'], row['mew'], row['width'], row['vd_vs3']) != (1, 0, 0, 16):
                    raise ValueError('Unexpected segment probe encoding: '+str(row))
                rows.append(row)
            cursor += 4
    got = Counter((row['mop'], row['opcode'], row['rs2']) for row in rows)
    if got != Counter(KINDS):
        raise ValueError('Expected exactly one of each of the 12 fixed encodings: '+str(got))
    return dict(elf=str(path), elf_sha256=hashlib.sha256(data).hexdigest(),
                executable_code_ranges=ranges, target_encodings=rows)


def selftest():
    correct, _ = oracle()
    legacy, _ = oracle(legacy=True)
    assert check_values(correct, 'candidate')['status'] == 'PASS'
    assert check_values(legacy, 'baseline')['status'] == 'PASS'
    wrong = [i for i, (a, b) in enumerate(zip(legacy, correct)) if a != b]
    assert [(i-1)//38 for i in wrong] == [0, 2, 4, 6, 8, 10]
    # Reject a lost tail, memory scribble, altered index, guard, CSR and binding count.
    rejected = 0
    for index in [0, 1, 5, 1+6, 1+23, 1+26, 1+30, 457]:
        bad = correct.copy(); bad[index] ^= 1
        try:
            check_values(bad, 'candidate')
        except ValueError:
            rejected += 1
        else:
            raise AssertionError('Mutation accepted')
    return dict(status='PASS', positive_models=2, rejected_mutations=rejected,
                baseline_errors=[dict(index=i, case=(i-1)//38, field=(i-1)%38,
                                      got=legacy[i], want=correct[i]) for i in wrong],
                scope='Independent byte arithmetic only; no C/RTL build or execution')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--elf', type=Path)
    inputs = parser.add_mutually_exclusive_group()
    inputs.add_argument('--signature', type=Path)
    inputs.add_argument('--values-json', type=Path, help='List of exactly 458 uint64 actual observations')
    parser.add_argument('--variant', choices=('baseline', 'candidate'), default='candidate')
    parser.add_argument('--selftest', action='store_true')
    args = parser.parse_args()
    result = {}
    if args.selftest:
        result['selftest'] = selftest()
    if args.elf:
        result['encoding'] = inspect_elf(args.elf)
    if args.signature or args.values_json:
        actual = signature_values(args.signature) if args.signature else json.loads(args.values_json.read_text())
        result['observations'] = check_values(actual, args.variant)
    if not result:
        parser.error('Select --selftest, --elf, --signature or --values-json')
    print(json.dumps(result, indent=2))


if __name__ == '__main__':
    main()
