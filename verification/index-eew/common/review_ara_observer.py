#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
"""Check a passive Ara memory export against its ELF symbols and fence opcode.

This does not supply architectural gold or run a simulator. Pass the exported
region files to the relevant independent value checker after this check.
Prepared with AI assistance.
"""
import argparse
import hashlib
import json
from pathlib import Path
import re
import struct


def elf_symbols(path):
    data = path.read_bytes()
    if data[:6] != b'\x7fELF\x02\x01' or struct.unpack_from('<H', data, 18)[0] != 243:
        raise ValueError('Expected a little-endian RISC-V ELF64')
    phoff, shoff = struct.unpack_from('<QQ', data, 32)
    phsz, phnum, shsz, shnum = struct.unpack_from('<HHHH', data, 54)
    if phsz != 56 or shsz != 64:
        raise ValueError('Unexpected ELF header sizes')
    sections = [struct.unpack_from('<IIQQQQIIQQ', data, shoff+i*shsz) for i in range(shnum)]
    segments = [struct.unpack_from('<IIQQQQQQ', data, phoff+i*phsz) for i in range(phnum)]
    symbols = {}
    for sec in sections:
        if sec[1] != 2:
            continue
        if sec[9] != 24 or sec[5] % 24:
            raise ValueError('Invalid ELF symbol entry size')
        strings = sections[sec[6]]
        table = data[strings[4]:strings[4]+strings[5]]
        for off in range(sec[4], sec[4]+sec[5], 24):
            name, info, other, index, value, size = struct.unpack_from('<IBBHQQ', data, off)
            if index and name and (info >> 4) in (1, 2):
                key = table[name:table.index(b'\0', name)].decode('ascii')
                if key in symbols and symbols[key] != (value, size):
                    raise ValueError('Conflicting symbol: '+key)
                symbols[key] = (value, size)
    return data, symbols, segments


def records(path):
    rows = []
    for line in path.read_text().splitlines():
        fields = line.split()
        if not fields or not fields[0].startswith('V01_'):
            raise ValueError('Unexpected observer log line: '+line)
        row = dict(item.split('=', 1) for item in fields[1:])
        row['kind'] = fields[0]
        rows.append(row)
    return rows


def review(elf, log, expect_target=None, allow_incomplete=False):
    data, symbols, segments = elf_symbols(elf)
    rows = records(log)
    cfgs = [r for r in rows if r['kind'] == 'V01_CFG']
    finals = [r for r in rows if r['kind'] == 'V01_FINAL']
    if len(cfgs) != 1 or len(finals) != 1:
        raise ValueError('Expected one configuration and one final record')
    cfg, final = cfgs[0], finals[0]
    if any(cfg[k] != v for k, v in dict(lanes='2', vlen='2048', wt='1', pmp='0').items()):
        raise ValueError('Unexpected configuration')
    pc = int(cfg['fence_pc'], 16)
    code = [p for p in segments if p[0] == 1 and p[1] & 1 and p[3] <= pc and pc+4 <= p[3]+p[5]]
    if len(code) != 1:
        raise ValueError('Fence PC is not uniquely backed by executable ELF bytes')
    p = code[0]
    opcode = struct.unpack_from('<I', data, p[2]+pc-p[3])[0]
    if opcode != 0x0330000f:
        raise ValueError('Selected PC is not fence rw,rw')
    exports = []
    region_rows = [r for r in rows if r['kind'] == 'V01_REGION']
    if len(region_rows) != int(cfg['regions']):
        raise ValueError('Region count differs from configuration')
    for i, row in enumerate(region_rows):
        name, base, words = row['name'], int(row['base'], 16), int(row['words'])
        if int(row['index']) != i or row['file'] != f'v01_region_{i}.hex':
            raise ValueError('Unexpected region index or filename')
        if symbols.get(name) != (base, words*4):
            raise ValueError(f'Region does not match the complete ELF symbol: {name}')
        contents = (log.parent/row['file']).read_text().splitlines()
        if len(contents) != words:
            raise ValueError('Truncated region: '+name)
        unknown = sum(not re.fullmatch('[0-9a-fA-F]{8}', w) for w in contents)
        exports.append(dict(symbol=name, base=hex(base), words=words, unknown_words=unknown,
                            file=row['file'], sha256=hashlib.sha256((log.parent/row['file']).read_bytes()).hexdigest()))
    fences = [r for r in rows if r['kind'] == 'V01_FENCE']
    exits = [r for r in rows if r['kind'] == 'V01_EXIT']
    if int(final['fences']) != len(fences) or int(final['exits']) != len(exits):
        raise ValueError('Final event counts differ from the actual event records')
    if int(final['late_writes']) or int(final['consistency_errors']):
        raise ValueError('Late result write or accelerator consistency error')
    if any(r['kind'] in ('V01_LATE_WRITE', 'V01_DUMP_ERROR') for r in rows):
        raise ValueError('Observer reported a late write or dump error')
    for row in fences:
        if int(row['pc'], 16) != pc or any(row[k] != '1' for k in
                ('valid', 'ack', 'lsu_empty', 'wt_empty', 'all_empty', 'acc_cons')):
            raise ValueError('Invalid fence witness')
    complete = len(fences) == 1 and len(exits) == 1
    if not complete and not allow_incomplete:
        raise ValueError('Missing or duplicate final fence/native exit')
    if len(fences) > 1 or len(exits) > 1:
        raise ValueError('Duplicate final fence/native exit')
    if complete:
        if any(r['unknown_words'] for r in exports):
            raise ValueError('Unknown result words in a completed run')
        if float(exits[0]['time_ns']) < float(fences[0]['time_ns']):
            raise ValueError('Native exit preceded the selected fence')
        target = int(exits[0]['target'])
        raw = int(exits[0]['raw'], 16)
        if raw != (target << 1) | 1 or int(final['raw_exit'], 16) != raw:
            raise ValueError('Exit payload is inconsistent')
        if expect_target is not None and target != expect_target:
            raise ValueError('Unexpected native target result')
    else:
        target = None
    return dict(status='EXPORT_CHECKED' if complete else 'INCOMPLETE_DIAGNOSTIC',
                elf_sha256=hashlib.sha256(data).hexdigest(), fence_pc=hex(pc),
                native_target=target, exports=exports,
                scope='Export integrity only. Architectural gold and native simulator logs require separate checks.')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--elf', type=Path, required=True)
    parser.add_argument('--log', type=Path, required=True)
    parser.add_argument('--expect-target', type=int)
    parser.add_argument('--allow-incomplete', action='store_true')
    args = parser.parse_args()
    print(json.dumps(review(args.elf, args.log, args.expect_target, args.allow_incomplete), indent=2))


if __name__ == '__main__':
    main()
