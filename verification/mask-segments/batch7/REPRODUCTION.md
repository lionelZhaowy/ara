# Mask payload alignment: reproduction status

Updated on 2026-11-07. The twenty-input comparison, eight official regressions, and seven combination-J calls completed on current official main.
David Harris's complete proposal and the local two-file combination pass all four inputs.
The original and one-file variants retain the specific failures below.
The previous four separate Spike results and frozen-input generic module checks remain valid.
The RTL ELFs remain identical to their retained 2026-11-01 bytes.

Base: Ara `34bd3bc152421b4601a7bf3d6e8e91ffb545c99e`, official `config=2_lanes`, VLEN=2048, ELEN=64. The code diffs preserve the original source copyright and SHL-0.51 headers. The prior public method is David Harris's proposal linked in #529 at `868f8a21f51f82e8955665fa325526e6a002c182`. The MaskM start subset originally adopted downstream came from #468/#462. These are distinct historical statements.

## Five variants

| Variant | Patch order from original main |
| --- | --- |
| Original | None |
| MASKU only | `masku_payload.patch` |
| Lane only | `lane_mask_words.patch` |
| Combined | `lane_mask_words.patch`, then `masku_payload.patch` |
| David | The complete author patch at `868f8a21f51f82e8955665fa325526e6a002c182`, including both files |

The two changes touch different files. Each passed a forward/reverse static application check. Their combination passed a forward/reverse static application check. Do not add the index EEW change, rejected scale change, slide changes, or private integration declaration move to this comparison.

The downstream MASKU hunk did not apply directly. Its context used a different slide condition. The draft copies only the final accepted masked-memory block. It retains official main's `VSLIDEUP && !use_scalar_op` condition. The added block matches the final downstream functional block. The preparation record retains the failed context check separately from the corrected draft.

## Difference from the existing public proposal

| Part | David Harris's public patch | Local draft |
| --- | --- | --- |
| Data payload origin | Round the internal start down to the first intersecting payload | Same intended origin for the fixed two-lane configuration |
| Lane start/length | Derive skipped mask words from the aligned element origin | Use the bit-packed start word and subtract its index from the word count |
| MASKU issue/processing | Reduce both counts | Leave these counts unchanged |
| MASKU read/pointer | Update in the issue initialization | Override after the existing initializations, only for masked memory and nonzero internal start |
| MASKU commit | Reduce the initial count | Same intended remaining interval, placed in the final masked-memory block |
| Private declaration issue | Not part of this comparison | Excluded because the target lacks the downstream integration use |

The code is not byte-identical. A successful local run does not establish that the test exercised the existing author's full patch. Do not claim the root cause or common method as unique local work.

## Deterministic public input

Place `mask_segments/main.c` in a separate app directory for each definition set:

| App | Definitions |
| --- | --- |
| `mask_nf5_load` | `-DV01_FAMILY=1 -DV01_NF5_E64_VL3=1` |
| `mask_nf5_store` | `-DV01_FAMILY=2 -DV01_NF5_E64_VL3=1` |
| `mask_nf2_load` | `-DV01_FAMILY=1 -DV01_NF2_E8_VL129=1` |
| `mask_nf2_store` | `-DV01_FAMILY=2 -DV01_NF2_E8_VL129=1` |

NF5 uses e64/m1, VL3, and five fields. Its complete mask bytes are A6. NF2 uses e8/m1, VL129, and two fields. Its mask bytes 0–15 are A6, bytes 16–31 are 59, and the remainder are C9. Thus bit128 is one while bit0 is zero.

Each phase loads all 256 bytes of v0 before its target. It initializes every byte of v1/v2/v3/v4 to 0x17/0x3B/0x6D/0xE1 respectively. These values differ from the mask data. The C clobber list includes all four registers. The unit-stride path does not overwrite them before the masked target.

The software CSR vstart remains zero. Segment expansion creates the internal starts. The input does not establish architectural nonzero-vstart or fault-restart coverage. The deterministic initialization changes the input. Old results involving unknown VRF values, including 142 errors or 72 differing words, cannot define its expected failures.

## Three phases in one ELF

Each input executes these phases in the same simulation if all target instructions finish:

1. Execute the original masked target. Export its data, memory, CSR values, and guards.
2. Initialize a fresh software fixture. Invert all 256 mask bytes with scalar C. Execute the second masked target.
3. Initialize another fresh fixture. Execute the same layout without a mask.

Phase two uses 59 throughout NF5. It uses 59/A6/36 in the three NF2 byte regions. Each phase initializes all bytes of v1–v4 and all data registers. Phase IDs are 7/15/23 for NF5 and 1/9/17 for NF2. Fresh data and guards depend on those IDs.

The firmware exports one complete record per phase. Signature format version 2 contains three records. A complete NF5 run has 13887 checks and 1502 u32 signature words. A complete NF2 run has 9279 checks and 926 words. These counts require all phases to complete. A hang before phase two does not establish later-phase coverage.

A completed numerical error does not suppress later phases. An environment error prevents target execution and prints an environment-failure marker. A stalled instruction cannot reach later phases. The runtime result and actual event trace must distinguish these cases.

The setup and export operations are unmasked. The input does not reset the DUT or clear mask queues between phases. It computes the inverse with scalar C. Do not insert vmnot.m, vmxor.mm, or another MASKU operation between the targets. The next masked operation must be phase two.

## Independent expectations and actual signatures

For each field and element, derive its byte address from the architectural segment layout. Read its activity from the explicit mask array. Under tu,mu, inactive and tail elements retain their initialized bytes. The independent Python oracle scatters active architectural elements into complete byte arrays. It does not copy the RTL expression or model the lane implementation.

The C checks the complete NF5 output of 1280 bytes or NF2 output of 512 bytes. It also checks all 512 memory bytes, full mask readback, input images, CSR values, and all guards. The Python checker compares every exported signature word, including all phase records, successful counters, CSR values, guards, and footer. It does not replace the C checks of inputs that the signature does not export.

After a real run, extract the bytes of `v01_memory_gap_signature` using its actual ELF symbol and size. Use raw little-endian bytes or hexadecimal u32 words without addresses. For Spike, request signature granularity 4. Do not treat an absent signature or a partial record as zero-filled success.

```bash
python3 check_mask_signature.py --nf 5 --family 1 --signature actual.sig --negative-controls
python3 check_mask_signature.py --nf 2 --family 2 --signature actual.bin --binary --negative-controls
```

Use family 1 for loads and 2 for stores. Run the controls on each actual passing input. They change exported data, guards, CSR state, phase identity, and the check count. They also inject stale first-phase masks and repeated NF2 mask words while preserving successful software counters. Every changed signature must fail. The checker reports the input SHA and mismatch locations.

`python3 check_mask_signature.py --self-test` checks artificial oracle fixtures and parser rejection only. It is an offline check, not C, Spike, or RTL execution. The first ML/NF5-load attempt supplied an actual complete signature. Its thirteen value-mutation controls and six event-mutation controls rejected every changed input. Its export check failed, as explained below. These subchecks do not accept that RTL attempt.

## Required event checks

Independently enumerate data payloads that intersect each internal active interval. Map architectural mask bits through the fixed Ara shuffle tables. Check accepted MaskM requests, physical mask words, produced mask packets, cut fills, and actual VLDU/VSTU consumption by owner.

Distinguish first-target tail consumption from setup/export events. After its last legitimate consumption, setup/export must not consume leftover entries. Outstanding production or cut transfers must preserve entry conservation. Record the state before the second masked target accepts its first entry. Reject an entry from the first target that the second target consumes. Complete passing variants must leave no stale mask or cut entry and must finish all owners.

Use the fixed official target's scalar/vector memory attributes and coherence method. A fence alone does not prove coherence. Keep the HDL limit, actual VCS stop command, wall limit, completion marker, software status, and assertion checks in the run record. Do not add an unsupported timeout plusarg.

## Actual official comparison

| Input | Original O | MASKU only M | Lane only L | Local combination ML | David full A |
| --- | --- | --- | --- | --- | --- |
| NF5/e64/VL3 load | Wrong mask/owner, HDL cutoff | Full data passes, 20 wrong lane requests | Wrong mask/owner, HDL cutoff | PASS | PASS |
| NF5/e64/VL3 store | Wrong mask/owner, HDL cutoff | Full data passes, 20 wrong lane requests | Wrong mask/owner, HDL cutoff | PASS | PASS |
| NF2/e8/VL129 load | Wrong mask/owner, HDL cutoff | Operand-command FIFO assertion | Wrong mask/owner, HDL cutoff | PASS | PASS |
| NF2/e8/VL129 store | Wrong mask/owner, HDL cutoff | Operand-command FIFO assertion | Wrong mask/owner, HDL cutoff | PASS | PASS |

Each ML/A NF5 call checks all 1502 signature words and 13887 software comparisons.
Each ML/A NF2 call checks all 926 words and 9279 comparisons.
Each completes all three phases, returns native target zero, and ends normally.
The actual retired final fence has empty store queues, empty WT buffer, accelerator consistency, and zero late writes.
Independent event checks match accepted requests, physical VRF words, mask payloads, per-lane cut ownership, actual consumption, and complete drain.
Thirteen actual-signature mutations and six actual-event mutations fail for each passing input.

The eight O/L calls reach the 20 ms HDL limit with known incorrect mask packets or stale owners.
They are functional failures with a retained HDL cutoff. We do not classify them as time-only failures.
Their incomplete signatures do not establish numerical error counts or later-phase coverage.
The two M NF2 calls stop at a real `fifo_v3` full-write assertion.
The M NF5 calls finish with correct data, but each has twenty incorrect MaskM request origins across both lanes and masked phases.
They do not satisfy the full request/event criteria.

Eight official regressions pass: O and ML each run `vcpop`, `vfirst`, `vslideup`, and `vslidedown`.
Combination J adds the accepted decode and index-EEW changes to ML.
Its seven calls pass: `idx64_vl3`, `idx_nf2`, `segment_rs2`, and all four masked-memory inputs.
The three index/decode checks use their exact earlier ELFs and complete independent signatures.
The four mask checks use the same signatures and event criteria as ML/A.
These seven checks establish only the stated composition, rather than the complete upstream PRs.

This continuation makes 43 RTL calls: twenty matrix calls, eight deadline-diagnostic reruns, eight regressions, and seven J calls.
There are 23 complete ordinary passes: eight ML/A calls, eight regressions, and seven J calls.
The other matrix outcomes remain distinct diagnostics, protocol failures, or assertion failures.
No new Spike or synthesis call is counted in this continuation.
The [machine-readable results](results.json) retain exact hashes, elapsed times, checks, and failure categories.

### Retained setup failures and deadline correction

The historical first ML/NF5-load attempt selected intermediate fence `0x800008b6`.
It reported 2004 late protected byte writes and remains rejected.
The new accepted invocation uses `0x800002ee`, after all footer stores in that exact ELF.

The first eight O/L calls exposed a separate run-control issue.
`run 20 ms; quit` stopped before the observer's scheduled deadline snapshot.
The checker correctly rejected missing snapshots.
A minimal VCS test reproduced the endpoint behavior.
Adding `run 0 ns` after `run 20 ms` processes the same timestamp without advancing HDL time.
Only those eight diagnostics reran in new output directories.
Each new log has two lane snapshots and one unit snapshot at exactly 20 ms.
Their fields match the actual final summaries.
The DUT, observer, ELF, checker, and time limits remain identical.
The accepted completed ML/A calls retain their original commands and evidence.

### Reused implementation and model evidence

The ten frozen-input Ara module checks remain accepted: five lane variants, three MASKU variants, and two dispatcher variants.
They use real modules at two lanes, VLEN2048, and ELEN64.
The lane checks have 1050 sequential cells, MASKU checks 1640, and dispatcher checks 738.
Each has zero latches and no unexplained post-compile module, loop, or multiple-driver diagnostic.
The retained pre-compile native-operator black-box attributes and other warnings remain documented in the original structural review.
This is generic `compile -no_map` evidence. It does not establish technology mapping, timing closure, or physical signoff.

The four unchanged separate Spike ELFs retain their previously accepted complete gold results.
They use their own runtime and differ from the common RTL ELFs.
Their results do not replace official RTL execution.

## Actual final-fence bindings

The following PCs follow the complete signature footer stores and precede the final DONE print. Each actual executable opcode is `0x0330000f`, or `fence rw,rw`. The local binding record retains the original wrong-PC attempt and disassembly context. Address order alone must not select the final fence.

| RTL ELF | SHA256 | Final fence PC | Signature u32 words |
| --- | --- | --- | ---: |
| `mask_nf5_load` | `985051a75d940a860d5507919cc7f0ff4cddae6a033b9c0893d9c288460f022d` | `0x800002ee` | 1502 |
| `mask_nf5_store` | `991f4e2e87602381aa95a4f29595e7208d2737676d21e48f5a33a799c480aaaf` | `0x800002ec` | 1502 |
| `mask_nf2_load` | `26990a560f763cfeedb21230052b300e98e6004f4e23670933b9a051c2298211` | `0x80001898` | 926 |
| `mask_nf2_store` | `8b2220dd03773e0127fb61cd6e5a289c4623c863b2ab29dfddbed9738955dee7` | `0x80001954` | 926 |

These bindings apply only to the listed ELF bytes. A new software build requires a new symbol, footer, and fence check. Acceptance requires the correct unique final fence, zero late writes, complete gold, actual owner checks, normal exit, and assertion review.

## Public source and compiled-source distinction

The recorded ELF builds used C source SHA256 `759bec16ca09d00113154da24b6ff19525f810ff25aaf3996427911811f78d9b`. The current public C has SHA256 `be98bfc99a0fb316ccd8e8ead1d62fbb94aa78d5dabcc19e0148e04a75a9897d`.

The public copy contains clang-format 20.1.0 changes and a neutral header comment. Standard C line splicing and adjacent-string concatenation produce the same complete 3759 tokens and literal contents. All 127 logical preprocessor directives also match. The official formatting checker accepted the final public file. These format and comment edits caused no new software build or RTL run. The compiled source and original ELF bytes remain retained.

## Commands and acceptance scope

[COMMANDS.md](../COMMANDS.md) provides the official app, Bender/VCS, run, and checking entries.
Use separate sources and writable outputs for each variant.
The official source, dependencies, executable, and ELF remain frozen during each call.
The actual calls used at most eight concurrent independent runs.

The unchanged time-zero `axi_inval_filter` unique-case warning remains in the accepted logs.
All accepted candidates pass native completion, full data/guard checks, final-fence export, actual owner checks, and unexpected-assertion review.
A process exit, an incomplete export, or a software summary alone cannot establish acceptance.

These inputs cover the stated segment-generated internal starts at architectural `vstart=0`.
They do not establish architectural nonzero-vstart, fault restart, other lane/VLEN configurations, or all NF/SEW/LMUL combinations.
The complete three-phase input preserves queues between targets and includes the inverse-mask transition and unmasked control.
