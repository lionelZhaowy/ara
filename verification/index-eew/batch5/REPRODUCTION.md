# Index operand width: official Ara comparison

Status: three RTL variants, fifteen SoC calls, and five separate Spike runs completed. The O/I real-lane Design Compiler checks were accepted for the fixed generic structural scope below. S is a simulation control and had no planned or executed DC run. Publication is pending.

## Fixed source and configuration

Base: Ara `34bd3bc152421b4601a7bf3d6e8e91ffb545c99e`, official `2_lanes`, VLEN=2048, ELEN=64, write-through D-cache, and zero PMP entries. Keep the unchanged Bender.lock dependencies, including CVA6 `99eac9a649001bdf5b8f9da52e0ca73d5c48db1c` and tech_cells_generic `7968dd6e6180df2c644636bc6d2908a49f2190cf`. Tools were LLVM 20.1.0, VCS W-2024.09-SP1, and the Spike installation identified by revision prefix `5cf439b2`.

The source variants are:

- O: original main.
- S: main plus [rejected_scale_only_control.patch](rejected_scale_only_control.patch). Only the load/store index requests change to `scale_vl=0`. This is a diagnostic control, not the complete [#468](https://github.com/pulp-platform/ara/pull/468) head. Its MaskM and slide changes are absent.
- I: main plus [index_eew.patch](index_eew.patch). Two assignments set the index request's local `vtype.vsew` to `pe_req.eew_vs2`. The original scale flag and start handling remain.

Apply the two alternatives to separate source copies. No dispatcher, MASKU, or other repair is in this comparison. The index-EEW alternative was prepared locally. The input sources and independent checker carry Apache-2.0 identifiers; the RTL retains the Ara source license and authorship. AI tools assisted with the local change, adaptation, tool execution, result review, and these notes.

## Inputs and actual results

All inputs use data e32/m1, `tu,mu`, no mask, and architectural `vstart=0`. Each input used the same RTL ELF across O/S/I.

| App / source | Compile definitions | O | S | I |
| --- | --- | --- | --- | --- |
| `idx64_vl2` / [index_nf1/main.c](index_nf1/main.c) | `V01_FIRST=12 V01_COUNT=1 V01_TEST_VL=2` | PASS | PASS | PASS |
| `idx64_vl3` / same | `V01_FIRST=12 V01_COUNT=1 V01_TEST_VL=3` | No completion by 20 ms HDL limit | PASS | PASS |
| `idx64_store` / same | `V01_FIRST=14 V01_COUNT=1 V01_TEST_VL=3` | No completion by 20 ms HDL limit | PASS | PASS |
| `idx_nf2` / [index_nf2/main.c](index_nf2/main.c) | Defaults: NF2, case12, count1, VL3 | PASS | One data error; target 96 | PASS |
| `idx8_pair` / [index8_pair/main.c](index8_pair/main.c) | None; two consecutive ordered index8 loads, VL5 | PASS | PASS | PASS |

Thus the fifteen RTL calls are twelve ordinary PASS results, one numeric counterexample, and two HDL-cutoff controls. I passes all five inputs; S passes four and regresses NF2; O passes three. The original NF2 PASS and NF1 VL2 PASS remain valid boundary results. A different input was not added to force an original failure.

Each NF1 run that completed reports one case, 731 checks, and zero errors. NF2 reports one case and 827 checks: zero errors on O/I and one on S. The consecutive-load input checks 656 values plus its VLEN condition, then reports `errors=0 completed=1` in each variant.

The complete S diagnostic is:

```text
V01_ORDERED_SEG_FIRST_FAIL id=12 field=5 i=66 got=00000000:5732182f want=00000000:56301c10
V01_ORDERED_SEG_DONE nf=2 cases=1 checks=827 errors=1
Core Test *** FAILED *** (tohost = 96)
```

`field=5` is the active-destination comparison category. It is not segment field 5. The flattened destination index 66 is field 1, element 2. There is one incorrect data word, at full-signature index 2254. Three additional signature words correctly report the error count and failed-case state; they are not three additional data errors.

The two original VL3 runs reached `20000000000 ps` (20 ms) and the UCLI `quit` command without a software summary, selected final fence, native exit, or normal testbench `$finish`. Wall times were 253.527 and 252.335 seconds, and `wall_timeout` was false. The exported record state remained started. These results establish no completion within the fixed HDL interval, not a permanent deadlock or a 600-second wall timeout.

## Complete data checks

The independent [check_index_signature.py](check_index_signature.py) uses byte addresses and ordered last-writer semantics. It checks every word in each completed export, including status, full destination tails, memory images, and exported guards:

- NF1: 2376 words per signature, including unused case records.
- NF2: 2892 words, with field-major destination images.
- Consecutive-load RTL: four full symbols, with 384 observed words, 12 status words, 396 buffer/guard words, and 64 packed index words: 856 semantic words.

The C consecutive-load signature also has two four-word alignment gaps, for 864 words in the full Spike signature. The four RTL regions do not export those eight padding words. All 856 RTL semantic words and all 864 Spike words were checked; padding was not inferred as RTL observation.

Every ordinary PASS signature matches the independent model. The S NF2 signature has exactly the one data difference and three status differences stated above. The two incomplete original exports are retained as incomplete observations, not checked as completed architectural results.

Each of the thirteen completed RTL runs has one selected final fence and one native exit. At the fence, the LSU committed-store queue, write-through buffer, and combined empty indication were set, with accelerator consistency enabled. There were no late result writes or observer consistency errors. The two cutoff runs have zero final fences and zero exits. Host return code zero alone is not a PASS condition.

Six separate one-bit changes to actual recorded values were rejected for each of the twelve ordinary PASS runs. The controls cover returned state/counts, active data, tails, memory, guards, and index data as applicable. These are offline checker controls, not additional RTL calls.

Five separate official Spike runtime ELFs match every full signature. The three NF1 signatures contain 2376 words each; NF2 contains 2892 and the consecutive-load signature 864. The model ELFs differ from the RTL runtime ELFs. Both sets of actual ELF hashes are listed separately in [results.json](results.json), with complete candidate data and retained failure observations.

VCS reports the existing unmatched `unique case` warning in `axi_inval_filter` at time zero. The S numeric control also reports the expected target-96 warning. The completed runs finish normally; no unexpected runtime assertion was observed. The known warning and intended failure are retained.

## Lane implementation checks

Design Compiler W-2024.09-SP1 analyzed, elaborated, and linked the real `lane_sequencer` for O and I, then completed `compile -no_map`. The all-port wrapper uses the fixed official types, NrLanes=2, VLEN=2048, and ELEN=64. Each variant used a separate process and work directory. Both were accepted for this generic structural scope.

| Variant | DUT SHA256 | Sequential cells before / after | Latches before / after | Post unexplained cells / black-box flags |
| --- | --- | --- | --- | --- |
| O | `eea6a3c2db1e5e8f681ffbd92e8e2cab0a62b61609db5c755ad5d83f955d0b76` | 1050 / 1050 | 0 / 0 | 0 / 0 |
| I | `5975757d7f28f4583438f948219bc253920dedd3c4f5f15f107d725e8987f82b` | 1050 / 1050 | 0 / 0 | 0 / 0 |

The O source matches fixed official main byte for byte. Applying `index_eew.patch` produces I's exact source SHA. The 1050 sequential-cell records are identical before/after and between O/I. This count comes from `all_registers -cells`; it is a cell count, not an independently calculated number of storage bits. No combinational-loop, multiple-driver, or new frontend width-mismatch diagnostic was reported.

Warnings remain. Each pre-compile table has 255 raw black-box flags: 249 synthetic operators and six internal constant cells, all explained by their actual attributes/reference reports. Post-compile flags and unexplained cells are zero. Missing design/library lookups for native GEN/SEQGEN cells are not unresolved IP counts. O/I have the same typing warnings, unused fields/arithmetic ports, constant controls, feedthrough and generic writer notices. DC ignores an unsupported package `$error`; its assertion semantics were not checked. The 5 ns clock supports reporting; high-fanout, missing input-delay, and unconstrained-endpoint warnings remain.

S had no separate DC run, and none was in the plan. The shared lane campaign also reviewed L, A, and IL for the separate masked-memory comparison; those are not S and are not part of this O/I result. The check does not establish functional equivalence, mapped area, timing closure, or physical signoff. The local review used preserved reports/cell attributes and did not reopen a DDC or read back the written generic netlist. Writer readback warnings remain recorded.

The public [types](../common/dc/ara_dc_types.sv), [lane wrapper](../common/dc/ara_lane_sequencer_dc.sv), [entry](../common/dc/check_lane_sequencer.tcl), [tool script](../common/dc/run_module.tcl), and [report capture](../common/dc/capture_structure.tcl) define the reproducible scope. Their hardware notices and original authors remain intact. `results.json` binds these helpers, the DUT sources, and the actual report hashes.

## Actual commands

Copy the selected sources to the five named app directories in an isolated base checkout. Use distinct names for different compile definitions, because the official Makefile does not track changed compiler flags as object dependencies. The common [VCS/LLVM recipe](../COMMANDS.md) supplies the locked source list, real `ara_tb` top, DPI support, and passive [observer](../common/ara_observer.sv). The official hardware Makefile has Questa and Verilator entries, not a VCS target.

The app builds used these exact flag values, with the installation path replaced by `$LLVM_ROOT`:

```bash
llvm_args=(
  config=2_lanes "LLVM_INSTALL_DIR=$LLVM_ROOT"
  "LLVM_FLAGS=--target=riscv64-unknown-elf --sysroot=$LLVM_ROOT/riscv64-unknown-elf -isystem $LLVM_ROOT/riscv64-unknown-elf/include -march=rv64gcv_zfh_zvfh -mabi=lp64d -mno-relax -fuse-ld=lld --rtlib=compiler-rt -L$LLVM_ROOT/riscv64-unknown-elf/lib"
  "LLVM_V_FLAGS=-fno-vectorize -fno-slp-vectorize -mno-implicit-float"
  RISCV_STRIP=true
)
make -C apps "${llvm_args[@]}" ENV_DEFINES='-DV01_FIRST=12 -DV01_COUNT=1 -DV01_TEST_VL=2' bin/idx64_vl2
make -C apps "${llvm_args[@]}" ENV_DEFINES='-DV01_FIRST=12 -DV01_COUNT=1 -DV01_TEST_VL=3' bin/idx64_vl3
make -C apps "${llvm_args[@]}" ENV_DEFINES='-DV01_FIRST=14 -DV01_COUNT=1 -DV01_TEST_VL=3' bin/idx64_store
make -C apps "${llvm_args[@]}" ENV_DEFINES= bin/idx_nf2
make -C apps "${llvm_args[@]}" ENV_DEFINES= bin/idx8_pair
```

`RISCV_STRIP=true` retains the symbols required by the observer. The final ELF bytes select these addresses; re-read symbols and the last signature fence after any rebuild:

| App | Final fence PC | Region 0 symbol / base / u32 words |
| --- | --- | --- |
| `idx64_vl2` | `800013d6` | `v01_ordered_signature` / `80002240` / 2376 |
| `idx64_vl3` | `80001546` | `v01_ordered_signature` / `800023c0` / 2376 |
| `idx64_store` | `800014fc` | `v01_ordered_signature` / `80002380` / 2376 |
| `idx_nf2` | `8000194a` | `v01_ordered_segment_signature` / `800027c0` / 2892 |
| `idx8_pair` | `8000053a` | Four regions listed below |

Example for the NF2 input; use each of `$BUILD_O/simv`, `$BUILD_S/simv`, and `$BUILD_I/simv` with these same arguments:

```bash
"$BUILD_I/simv" -ucli -do "$LIMIT_TCL" "+PRELOAD=$ARA_ROOT/apps/bin/idx_nf2" \
  +V01_OBSERVER +V01_VECTOR_COMMIT +V01_FENCE_PC=8000194a +V01_REGIONS=1 \
  +V01_R0_BASE=800027c0 +V01_R0_WORDS=2892 +V01_R0_NAME=v01_ordered_segment_signature
```

For each NF1 input, use its app/fence/base from the table, one region of 2376 words, and `+V01_R0_NAME=v01_ordered_signature`. For the consecutive-load input, the recorded region arguments are:

```text
+V01_FENCE_PC=8000053a +V01_REGIONS=4
+V01_R0_BASE=80001380 +V01_R0_WORDS=384 +V01_R0_NAME=index8_pair_observed
+V01_R1_BASE=80001980 +V01_R1_WORDS=12 +V01_R1_NAME=index8_pair_status
+V01_R2_BASE=800019c0 +V01_R2_WORDS=396 +V01_R2_NAME=index8_pair_buffers
+V01_R3_BASE=80002000 +V01_R3_WORDS=64 +V01_R3_NAME=index8_pair_offsets
```

Each call used a separate output directory and `$LIMIT_TCL` containing `run 20 ms` then `quit`. The same ELF for an app was reused across the three variants.

The five model builds use the same flags/definitions with the corresponding `.spike` target. They also use the [section-wildcard linker adaptation](../common/spike_sections.patch) for LLVM suffixed sections. The official target enables vector state in startup. The actual model command form is:

```bash
"$SPIKE_ROOT/bin/spike" --isa=rv64gcv --varch=vlen:2048,elen:64 \
  --priv=msu --pmpregions=0 +signature=signature.hex +signature-granularity=4 \
  "$ARA_ROOT/apps/bin/idx_nf2.spike"
```

From the public material directory, check export integrity with `common/review_ara_observer.py --elf <RTL_ELF> --log <run>/v01_observer.log --expect-target 0`. Use target 96 for the S NF2 counterexample; use `--allow-incomplete` without an expected target for the two cutoff controls. Apply the value checker to each exported signature:

```bash
python3 batch5/check_index_signature.py --nf 1 --first 12 --count 1 --vl 2 --signature "$RUN/v01_region_0.hex"
python3 batch5/check_index_signature.py --nf 1 --first 12 --count 1 --vl 3 --signature "$RUN/v01_region_0.hex"
python3 batch5/check_index_signature.py --nf 1 --first 14 --count 1 --vl 3 --signature "$RUN/v01_region_0.hex"
python3 batch5/check_index_signature.py --nf 2 --first 12 --count 1 --vl 3 --signature "$RUN/v01_region_0.hex"
python3 batch5/check_index_signature.py --pair --regions "$RUN/v01_region_0.hex" "$RUN/v01_region_1.hex" "$RUN/v01_region_2.hex" "$RUN/v01_region_3.hex"
```

Select the one command that matches the input. The S NF2 value check must fail with the exact differences above. For Spike, use its `signature.hex`; the consecutive-load check uses `--pair --signature <signature.hex>` to include padding.

For DC, set the existing dependency roots as in the [DC instructions](../common/dc/README.md), then use the actual direct entry for O and I:

```bash
V01_DC_SOURCE="$ARA_ROOT/hardware/src/lane/lane_sequencer.sv" \
  V01_DC_OUTPUT="$DC_O" dc_shell -f "$PUBLIC_ROOT/common/dc/check_lane_sequencer.tcl"
V01_DC_SOURCE="$ARA_I_ROOT/hardware/src/lane/lane_sequencer.sv" \
  V01_DC_OUTPUT="$DC_I" dc_shell -f "$PUBLIC_ROOT/common/dc/check_lane_sequencer.tcl"
```

The shared package/dependency root is the original fixed checkout in both calls. Only the selected DUT source changes; `$DC_O` and `$DC_I` are separate absolute output directories. These commands do not describe an S DC run.

## Input revision and limits

These results use new portable inputs. The draft user-mode `mstatus` write was removed. The consecutive-load input gained its full signature, and the selected final fence follows all result writes. Old downstream ELF results do not apply to these new ELFs. Source and binary hashes in `results.json` record this actual revision.

No NF2 prefix was added or run. The fixed five-input matrix is the scope of this result. It does not cover all index widths, all VL values, masked memory, every segment size, or the complete #468 patch. The retained NF2 original PASS is not replaced by a different failing setup. The checker rejects NF2 VL2 and NF1 VL2-store combinations that these inputs do not support. O/I generic structural checks are accepted; no S DC result is claimed.

## Public source formatting

The public C copies use clang-format 20.1.0 after the recorded runs. The consecutive-load input also removes its stale no-run header, with every byte after that comment unchanged. Their complete canonical C tokens and literal contents match the original compiled sources, after standard line splicing and adjacent-string concatenation. `results.json` separates current public hashes from compiled-run source hashes. The three public C files passed the official clang-format 20.1.0 check. The formatting edit caused no software build or RTL run.
