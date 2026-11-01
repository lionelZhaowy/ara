# Segment rs2 decoding: official Ara comparison

Status: the functional comparison and two real-dispatcher Design Compiler checks completed in isolated official Ara checkouts. The fixed generic structural scope was accepted with the warnings below. The linked fork commit contains the material for the existing issue discussion.

## Source and configuration

Base: Ara `34bd3bc152421b4601a7bf3d6e8e91ffb545c99e`. Variant O uses the original RTL. Variant D adds only the two dispatcher conditions in [segment_decode.patch](segment_decode.patch). No index, MASKU, lane, or other functional repair is in this comparison.

The fix comes from [David Harris's public patch](https://github.com/davidharrishmc/ara-repros/blob/868f8a21f51f82e8955665fa325526e6a002c182/segment-rs2-x8-v8/fix.patch), linked in [Ara #535](https://github.com/pulp-platform/ara/issues/535). The whole-register `rs2=8` exception applies to unit-stride VLE/VSE. Strided segment instructions with x8 and indexed segment instructions with v8 still require segment expansion. Credit for the fix remains with David Harris.

The C input derives from the separate downstream test. It does not copy the test source from the author's repository. This portable input and the independent checker carry Apache-2.0 identifiers. The RTL retains the Ara source license and authorship. AI tools assisted with the adaptation, test preparation, tool execution, result review, and these notes.

The run used the official `2_lanes` configuration: two lanes, VLEN=2048, ELEN=64, write-through D-cache, and zero PMP entries. Dependencies remain at the official lockfile revisions, including CVA6 `99eac9a649001bdf5b8f9da52e0ca73d5c48db1c` and tech_cells_generic `7968dd6e6180df2c644636bc6d2908a49f2190cf`. Tools were LLVM 20.1.0, VCS W-2024.09-SP1, and the Spike installation identified by revision prefix `5cf439b2`.

## Input and coverage

[segment_rs2/main.c](segment_rs2/main.c) has twelve cases. Every target uses e8/m1, VL=1, NFIELDS=2, `tu,mu`, no mask, and architectural `vstart=0`:

- Strided loads and stores pair x8 with x9, with a four-byte stride.
- Unordered-indexed loads and stores pair v8 with v12, with index bytes `[3,7]`.
- Ordered-indexed loads and stores use the same index pair and bytes.

Data registers are v16/v17. Each case checks the two fields, their tails, sixteen memory bytes, index readback, eight guards, returned VL, and VL/vtype/vstart. There are 38 comparisons per case and two platform/completion comparisons: 458 actual uint64 values, exported as 916 32-bit words. Three vset instructions per case give 36 in total.

The platform adaptation uses the official app headers, ordinary arrays, the official `SPIKE` macro, and the normal return from `main`. It has no private address alias or startup macro. The official runtime enables vector state; the C input does not write machine-mode `mstatus` from user-mode `main`.

## Actual results

The two RTL builds used one identical probe ELF, SHA256 `aeccd2080e628c835be08575169395ebf9ae7552fe5fd742c04dab0956186c8e`.

| RTL and input | Cases / checks / vsets | Actual result |
| --- | --- | --- |
| O, segment probe | 12 / 458 / 36 | Six exact data errors; native target 6 |
| D, same probe ELF | 12 / 458 / 36 | Zero errors; native target 0, PASS |
| D, official `rv64uv-ara-vl1r` | Official regression | PASS |
| D, official `rv64uv-ara-vs1r` | Official regression | PASS |

These four final RTL calls are one reproduced failure and three PASS results. Both regressions reached normal success and finish paths. They check that the legal whole-register load/store exceptions remain valid.

Both probes exported all 916 words without unknown values. The independent byte-address oracle found six architectural differences in O and none in D. The six differences occur in cases 0, 2, 4, 6, 8, and 10, which use x8 or v8. All six alternative-register cases match the architecture. For example, case 0 leaves field 1 at `0xe2` instead of loading byte `0x41`; D returns `0x41`.

Actual vector retirement records confirm each of the twelve target instructions once and all 36 vset instructions. The final signature fence at ELF PC `0x8000108a` retired once in each probe. At that event, the LSU committed-store queue and write-through buffer were empty, the combined empty indication was set, and accelerator consistency was enabled. Each run then had one native exit, zero late writes, and zero observer consistency errors. This uses the actual write-through/coherence configuration and queue signals; a fence alone is not the coherence check.

VCS reported the existing `axi_inval_filter` unmatched `unique case` warning at time zero in all four runs. O also reported the intended target-6 failure warning. The accepted runs reached `$finish`; no timeout or unexpected runtime assertion was observed. The final LLVM link reported a non-allocatable `.comment` memory-region warning.

The checker rejected six separate one-bit changes to actual recorded values in each RTL probe. The changes covered vlenb, memory, a vector tail, index readback, a guard, and the completed-case count. These are offline checker controls, not additional RTL runs.

A separate official Spike runtime build used ELF SHA256 `36d8639994971a3dd50a912dc28901cfa929148f4ed9a4244f0f9804169783e0`. It completed with 12 cases, 458 checks, 36 vsets, and zero errors. All 916 signature words match the independent architectural oracle. This ELF differs from the RTL runtime ELF. Both ELFs contain exactly the twelve intended target instruction encodings.

[results.json](results.json) binds the final input, patch, checker, ELFs, simulator executables, exported data, and results by SHA256.

## Dispatcher implementation checks

Design Compiler W-2024.09-SP1 analyzed, elaborated, and linked the real dispatcher in both O and D, then completed `compile -no_map`. The wrapper exposes every DUT port and uses the official request/response types and configuration function. The fixed configuration has two lanes, VLEN=2048, Half/Single/Double FP support, and enabled fixed-point, segment, and FP extensions. Each variant used its own DC process and work directory.

| Variant | DUT SHA256 | Flip-flops before / after | Latches before / after | Post unexplained cells / black-box flags |
| --- | --- | --- | --- | --- |
| O | `ba69694d00d67c62fdde33d1a73b424dc7ff603f3b2cf2341cd98422b58aa0bb` | 738 / 738 | 0 / 0 | 0 / 0 |
| D | `b850bb2b7a4c6deae6a5c0b69c50bbcb45dcadc94555ad561db6b8e916ddc76a` | 738 / 738 | 0 / 0 | 0 / 0 |

O matches the fixed official source byte for byte. Applying only the two published decode-condition edits produces D's exact SHA256. All 738 sequential-cell records remain identical before and after compilation in each variant. Neither run reports a combinational loop or a tri-state cell reference. No DC error or unexplained cell remains in the reviewed reports.

Warnings remain. Before compilation, DC sets a raw black-box flag on 1970 cells: 1960 synthetic operators and ten internal constant cells. Their actual attributes and reference reports explain these cells; they are not 1970 unresolved user IP blocks. After `compile -no_map`, no cell has that flag. The reports also retain signed/unsigned and unsized-constant warnings, constant feature branches, unused fields/bits, feedthrough and shared-constant output warnings, and generic-netlist writer notices. DC ignores an unsupported package `$error`; this run does not check that assertion's semantics.

This is an accepted generic structural check of the real dispatcher. It is not an equivalence proof, technology mapping, timing closure, or physical signoff. The 5 ns clock is for reporting. The independent report/source review did not reopen a DDC or inspect the written generic netlist; it used the retained structure, timing, sequential-cell, reference, and actual cell-attribute reports. The original warnings and report hashes remain recorded in `results.json`.

## Public code files used

| Files | Role |
| --- | --- |
| [segment_rs2/main.c](segment_rs2/main.c) | Public formatting revision of the twelve-case input used for the RTL and separate Spike runtime builds |
| [segment_decode.patch](segment_decode.patch) | David Harris's two functional decode conditions, applied to the fixed official base |
| [check_segment_rs2.py](check_segment_rs2.py) | Independent byte-address data model and actual ELF encoding checks |
| [ara_observer.sv](../common/ara_observer.sv), [review_ara_observer.py](../common/review_ara_observer.py) | Passive real-DRAM export and ELF/fence/completion checks |
| [ara_mask_observer.sv](../common/ara_mask_observer.sv) | Optional extra top compiled in the VCS library; mask-event logging was disabled in these runs |
| [spike_sections.patch](../common/spike_sections.patch) | LLVM suffixed-section placement for the official Spike runtime |
| [ara_dc_types.sv](../common/dc/ara_dc_types.sv), [ara_dispatcher_dc.sv](../common/dc/ara_dispatcher_dc.sv) | Official types/configuration and all-port dispatcher wrapper |
| [check_dispatcher.tcl](../common/dc/check_dispatcher.tcl), [run_module.tcl](../common/dc/run_module.tcl), [capture_structure.tcl](../common/dc/capture_structure.tcl) | Direct DC entry and retained structural/property reports |

The hardware support files retain the original Solderpad notices and authors. The new C/Python and Tcl support use Apache-2.0 identifiers. The unchanged dependency sources come from the pinned official checkout. The two DC source lists are identical except for O's versus D's `hardware/src/ara_dispatcher.sv`; they contain the real packages, common-cell counters/popcount, segment sequencer, public types, DUT, and wrapper.

The actual runs compiled C source SHA256 `5a88275ab0f947cba711f011f3f8c99caebdfadfb284055968c6bb3afd06cdba`. The public source removes a stale header comment and uses LLVM 20.1.0 clang-format. Its SHA256 is `4e6efc36cbec5297981151ed55187aa413f3053eba18e3a7c37bf02c8542593a`. A comparison after standard C line splicing and adjacent-string concatenation found the same complete token stream and literal contents. Original sources and ELFs remain retained. These comment and format changes caused no software build or RTL run.

## Retained setup failures

The first draft attempted a machine-mode `mstatus` write from user-mode `main`. Both initial RTL calls exited with native target 2 before the probe completed. The write was removed and the software was rebuilt. Those two failures remain separate from the four final calls above.

The first Spike attempt selected `zvfh`, which that installed Spike did not support. The next attempt exposed an existing linker-script gap: LLVM function/data sections were left as orphan sections after `_end`, and the Spike TLS putchar buffer overlapped the signature. The [section-wildcard patch](../common/spike_sections.patch) collects `.text.*`, `.data.*`, `.bss.*`, `.tdata.*`, and `.tbss.*`. The final Spike run uses `--isa=rv64gcv` and the corrected linker layout. The intermediate linker-fix ELF is not the final ELF quoted above. These setup failures are not PASS results.

## Commands used

Use isolated checkouts at the fixed base and locked dependencies. Place the C input at `apps/segment_rs2/main.c`. Apply `segment_decode.patch` only to D. The common [VCS recipe and dependencies](../COMMANDS.md) describe the real `ara_tb` top and passive [observer](../common/ara_observer.sv). The official hardware Makefile has Questa and Verilator entries; the VCS build uses that source set and DPI support.

The actual software command follows, with installation paths replaced by `$LLVM_ROOT`. `RISCV_STRIP=true` retains ELF symbols:

```bash
make -C apps config=2_lanes LLVM_INSTALL_DIR="$LLVM_ROOT" \
  LLVM_FLAGS="--target=riscv64-unknown-elf --sysroot=$LLVM_ROOT/riscv64-unknown-elf -isystem $LLVM_ROOT/riscv64-unknown-elf/include -march=rv64gcv_zfh_zvfh -mabi=lp64d -mno-relax -fuse-ld=lld --rtlib=compiler-rt -L$LLVM_ROOT/riscv64-unknown-elf/lib" \
  LLVM_V_FLAGS="-fno-vectorize -fno-slp-vectorize -mno-implicit-float" \
  RISCV_STRIP=true bin/segment_rs2 bin/rv64uv-ara-vl1r bin/rv64uv-ara-vs1r
```

For these exact RTL ELF bytes, the selected arguments were:

```bash
probe_args=(
  -ucli -do "$LIMIT_TCL" "+PRELOAD=$ARA_ROOT/apps/bin/segment_rs2"
  +V01_OBSERVER +V01_VECTOR_COMMIT +V01_FENCE_PC=8000108a +V01_REGIONS=3
  +V01_R0_BASE=80001f00 +V01_R0_WORDS=916 +V01_R0_NAME=v01_segment_rs2_observed
  +V01_R1_BASE=800031d0 +V01_R1_WORDS=4 +V01_R1_NAME=v01_segment_rs2_counts
  +V01_R2_BASE=800031e0 +V01_R2_WORDS=8 +V01_R2_NAME=v01_segment_rs2_first_error
)
"$BUILD_O/simv" "${probe_args[@]}"
"$BUILD_D/simv" "${probe_args[@]}"
"$BUILD_D/simv" -ucli -do "$LIMIT_TCL" "+PRELOAD=$ARA_ROOT/apps/bin/rv64uv-ara-vl1r"
"$BUILD_D/simv" -ucli -do "$LIMIT_TCL" "+PRELOAD=$ARA_ROOT/apps/bin/rv64uv-ara-vs1r"
```

Each call used a separate output directory. `$LIMIT_TCL` contains `run 20 ms` followed by `quit`. These addresses and the fence PC belong to the quoted ELF; obtain them again from ELF symbols and instruction decoding after any rebuild.

For Spike, apply the section-wildcard patch, then use the same software command with only target `bin/segment_rs2.spike`. The official Spike target also enables vector state in its startup code. Run:

```bash
"$SPIKE_ROOT/bin/spike" --isa=rv64gcv --varch=vlen:2048,elen:64 \
  --priv=msu --pmpregions=0 +signature=signature.hex +signature-granularity=4 \
  "$ARA_ROOT/apps/bin/segment_rs2.spike"
```

From the public material directory, the offline checks are:

```bash
python3 common/review_ara_observer.py --elf "$ARA_ROOT/apps/bin/segment_rs2" \
  --log "$RUN_O/v01_observer.log" --expect-target 6
python3 common/review_ara_observer.py --elf "$ARA_ROOT/apps/bin/segment_rs2" \
  --log "$RUN_D/v01_observer.log" --expect-target 0
python3 batch6/check_segment_rs2.py --elf "$ARA_ROOT/apps/bin/segment_rs2" \
  --signature "$RUN_O/v01_region_0.hex" --variant baseline
python3 batch6/check_segment_rs2.py --elf "$ARA_ROOT/apps/bin/segment_rs2" \
  --signature "$RUN_D/v01_region_0.hex" --variant candidate
python3 batch6/check_segment_rs2.py --elf "$ARA_ROOT/apps/bin/segment_rs2.spike" \
  --signature "$RUN_SPIKE/signature.hex" --variant candidate
```

The checker's `baseline` mode confirms the exact known failure values. Its `PASS` means that the observed failure matches that control model; the original RTL still fails the architectural oracle with six errors.

The actual DC entry was `dc_shell -f <public>/common/dc/check_dispatcher.tcl`. With the dependency roots set as described in the [DC instructions](../common/dc/README.md), the two calls are:

```bash
V01_DC_SOURCE="$ARA_ROOT/hardware/src/ara_dispatcher.sv" \
  V01_DC_OUTPUT="$DC_O" dc_shell -f "$PUBLIC_ROOT/common/dc/check_dispatcher.tcl"
V01_DC_SOURCE="$ARA_D_ROOT/hardware/src/ara_dispatcher.sv" \
  V01_DC_OUTPUT="$DC_D" dc_shell -f "$PUBLIC_ROOT/common/dc/check_dispatcher.tcl"
```

The shared package/dependency root is the original fixed checkout in both calls; only `V01_DC_SOURCE` selects the candidate DUT. `$DC_O` and `$DC_D` are separate absolute output directories. The entry reads the selected sources and creates reports; it does not fetch or change dependencies.

## Limits

This functional comparison covers the stated NF2/VL1 forms. It does not establish complete NF4 behavior. The complete downstream NF4 program also required a separate masked-memory change, tracked with #529. The earlier downstream runs remain separate and are not counted as official Ara runs here. The two dispatcher DC runs cover the fixed generic structural scope above. Publication remains pending.
