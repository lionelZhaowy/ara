# Commands and frozen inputs

Use official Ara `34bd3bc152421b4601a7bf3d6e8e91ffb545c99e` and its unchanged Bender.lock.
Set `PUBLIC_ROOT` to this directory, `ARA_ROOT` to one isolated source checkout, and `LLVM_ROOT` / `SPIKE_ROOT` to the installed tools.
The recorded tools were LLVM 20.1.0, Spike revision prefix `5cf439b2`, and VCS W-2024.09-SP1 Full64.

## Software

Copy [main.c](cases/mask_segments/main.c) to four separate official app directories.
Use these definitions. Separate app names prevent reuse of objects with different compiler flags.

| App | Definitions |
| --- | --- |
| mask_nf5_load | `-DARA_TEST_FAMILY=1 -DARA_TEST_NF5_E64_VL3=1` |
| mask_nf5_store | `-DARA_TEST_FAMILY=2 -DARA_TEST_NF5_E64_VL3=1` |
| mask_nf2_load | `-DARA_TEST_FAMILY=1 -DARA_TEST_NF2_E8_VL129=1` |
| mask_nf2_store | `-DARA_TEST_FAMILY=2 -DARA_TEST_NF2_E8_VL129=1` |

Example for one app:

```bash
make -C "$ARA_ROOT/apps" config=2_lanes "LLVM_INSTALL_DIR=$LLVM_ROOT" \
  "LLVM_FLAGS=--target=riscv64-unknown-elf --sysroot=$LLVM_ROOT/riscv64-unknown-elf -isystem $LLVM_ROOT/riscv64-unknown-elf/include -march=rv64gcv_zfh_zvfh -mabi=lp64d -mno-relax -fuse-ld=lld --rtlib=compiler-rt -L$LLVM_ROOT/riscv64-unknown-elf/lib" \
  "LLVM_V_FLAGS=-fno-vectorize -fno-slp-vectorize -mno-implicit-float" \
  RISCV_STRIP=true ENV_DEFINES='-DARA_TEST_FAMILY=1 -DARA_TEST_NF5_E64_VL3=1' bin/mask_nf5_load
```

Build each RTL ELF once. Keep the same ELF across compared source variants.
The current public C was compiled again for this update. The source, RTL ELF, and separate Spike ELF hashes are recorded in cases/results.json. The symbol and completion-path checks were repeated on each new RTL ELF. The SHA/PC table applies only to those exact bytes.

## RTL variants and build

O is unchanged official main. M applies [masku_payload.patch](cases/masku_payload.patch).
L applies [lane_mask_words.patch](cases/lane_mask_words.patch). ML applies both.
A applies David Harris's complete [fixed proposal](https://github.com/davidharrishmc/ara-repros/blob/868f8a21f51f82e8955665fa325526e6a002c182/masked-vstart-mask-entries/fix.patch).
Use separate source and build directories. Do not add the index-scale or slide changes.

The official hardware Makefile has Questa and Verilator entries. The VCS recipe uses the real official top and DPI loader:

```bash
cd "$ARA_ROOT"
mkdir -p "$BUILD_DIR"
bender --local --git-submodules false script vcs \
  -t rtl -t cv64a6_imafdcv_sv39 \
  -t tech_cells_generic_include_tc_sram -t tech_cells_generic_include_tc_clk \
  -t exclude_first_pass_decoder -t ara_test -t cva6_test \
  -D NR_LANES=2 -D VLEN=2048 -D ARIANE_ACCELERATOR_PORT=1 \
  --vlogan-args="-full64 -assert svaext +v2k -timescale=1ns/1ps" > "$BUILD_DIR/compile.sh"
cd "$BUILD_DIR"
bash compile.sh
vlogan -full64 -sverilog -timescale=1ns/1ps -assert svaext \
  "$PUBLIC_ROOT/common/ara_observer.sv" "$PUBLIC_ROOT/common/ara_mask_observer.sv"
vcs -full64 -sverilog -timescale=1ns/1ps -assert svaext \
  -top ara_tb -top ara_observer -top ara_mask_observer \
  "$ARA_ROOT/hardware/tb/dpi/elfloader.cc" -CFLAGS "-I$SPIKE_ROOT/include" -o simv
```

## Run and check

Use a new `RUN_DIR` for every invocation. `APP`, `FENCE_PC`, `SIGNATURE_BASE`, `SIGNATURE_WORDS`, and `PHASE_BASE` describe the actual ELF.
Use `readelf -Ws` for full symbols and `llvm-objdump -d` for the executed completion path.
The selected `fence rw,rw` must follow all signature/footer stores and precede the final DONE print.

The finite UCLI file contains:

```tcl
run 20 ms
run 0 ns
quit
```

`run 0 ns` processes events at the same deadline timestamp. It does not advance simulation time.
Without it, UCLI can quit before that timestamp's passive snapshot executes.
This behavior was reproduced in a separate minimal VCS test.
Every new matrix and combination call used this complete UCLI file. The earlier endpoint failures and corrected diagnostic reruns remain in the previous public commit. They are separate from this rebuilt-input comparison.

```bash
mkdir "$RUN_DIR"
cd "$RUN_DIR"
timeout 600 "$BUILD_DIR/simv" -no_save -ucli -do "$LIMIT_TCL" \
  "+PRELOAD=$ARA_ROOT/apps/bin/$APP" +ARA_TEST_OBSERVER \
  "+ARA_TEST_FENCE_PC=$FENCE_PC" +ARA_TEST_REGIONS=2 \
  +ARA_TEST_MASK_START_OBSERVE +ARA_TEST_MASK_SNAPSHOT_NS=20000000 \
  "+ARA_TEST_R0_BASE=$SIGNATURE_BASE" "+ARA_TEST_R0_WORDS=$SIGNATURE_WORDS" \
  +ARA_TEST_R0_NAME=ara_test_memory_gap_signature \
  "+ARA_TEST_R1_BASE=$PHASE_BASE" +ARA_TEST_R1_WORDS=1 +ARA_TEST_R1_NAME=ara_test_memory_gap_phase \
  > run.log 2>&1
python3 "$PUBLIC_ROOT/common/review_ara_observer.py" \
  --elf "$ARA_ROOT/apps/bin/$APP" --log ara_test_observer.log --expect-target 0
python3 "$PUBLIC_ROOT/cases/check_mask_signature.py" \
  --nf "$NF" --family "$FAMILY" --signature ara_test_region_0.hex --negative-controls
python3 "$PUBLIC_ROOT/cases/check_mask_events.py" \
  --nf "$NF" --family "$FAMILY" --log run.log \
  --ara-pkg "$ARA_ROOT/hardware/include/ara_pkg.sv" --variant "$VARIANT" --negative-controls
```

`NF` is 2 or 5. `FAMILY` is 1 for load and 2 for store.
Review the normal native completion and all assertion messages separately.
For incomplete O/L diagnostics, use `--allow-incomplete` for the export checker.
Use `--diagnostic --incomplete --deadline-ns 20000000` for their event checker.
No incomplete signature is accepted as a full gold result.
M-only assertion failures are functional failures, even when simv returns zero.
Its completed NF5 inputs still have incorrect physical mask-word requests.

Independent invocations may run concurrently after inputs are frozen. Writable outputs must remain separate.
No shared object tree is rebuilt during these runs.
Combination J applies the independent index-EEW and segment-decode patches before the two local mask patches. Its seven actual calls use the rebuilt index, segment, and mask ELFs recorded in results.json. The independent index/decode input definitions and complete signatures remain documented in their corresponding public material directories.

## Separate Spike runtime

Apply [spike_sections.patch](common/spike_sections.patch) in a separate software checkout for LLVM suffixed sections. Build the same four sources and definition sets with their `.spike` targets. This runtime prepares its own startup source; do not build RTL software in that checkout at the same time. Each model call has a 60-second wall limit:

```bash
timeout 60 "$SPIKE_ROOT/bin/spike" --isa=rv64gcv --varch=vlen:2048,elen:64 \
  --priv=msu --pmpregions=0 +signature=signature.hex +signature-granularity=4 \
  "$ARA_ROOT/apps/bin/mask_nf5_load.spike"
```

Select each app's actual name, NF, and family for its complete signature check. All four separate model signatures passed the independent checker. These model results do not establish RTL behavior.

## Reused generic implementation checks

The production RTL, packages, and wrapper types are unchanged. The public DC environment-variable labels were renamed without changing the script logic. The earlier accepted ten generic module checks remain recorded as reused evidence. This update contains no new DC invocation.

## Fixed public sources for combination J

Combination J starts from the same official base and locked dependencies.
Its production changes apply these four patches, in order:

1. [Index-EEW patch](https://github.com/lionelZhaowy/ara/blob/e434b86c00759c9bc474e050edc720315b4cb62b/verification/index-eew/cases/index_eew.patch).
2. [Segment-decode patch](https://github.com/lionelZhaowy/ara/blob/d0f57f2eb2f25f21646578c7784007ab4690d665/verification/segment-rs2/cases/segment_decode.patch), credited to David Harris.
3. [Local lane mask-word patch](https://github.com/lionelZhaowy/ara/blob/c48eef7abf6f45b959b6002c68780411b757217a/verification/mask-segments/cases/lane_mask_words.patch).
4. [Local mask-unit payload patch](https://github.com/lionelZhaowy/ara/blob/c48eef7abf6f45b959b6002c68780411b757217a/verification/mask-segments/cases/masku_payload.patch).

These links select fixed source commits. They do not select moving branch names.
The comparison excludes the rejected scale-only control, slide changes, and unrelated repairs.
Set `INDEX_PUBLIC`, `SEGMENT_PUBLIC`, and `MASK_PUBLIC` to local copies of these three fixed material directories.
For a fresh isolated official source copy, apply:

```bash
cd "$ARA_ROOT"
git apply "$INDEX_PUBLIC/cases/index_eew.patch"
git apply "$SEGMENT_PUBLIC/cases/segment_decode.patch"
git apply "$MASK_PUBLIC/cases/lane_mask_words.patch"
git apply "$MASK_PUBLIC/cases/masku_payload.patch"
```

Build J with the [mask comparison's passive observers and official VCS recipe](https://github.com/lionelZhaowy/ara/blob/c48eef7abf6f45b959b6002c68780411b757217a/verification/mask-segments/COMMANDS.md#rtl-variants-and-build).
The seven input sources and definitions are:

| App | Exact source | Compile definitions |
| --- | --- | --- |
| `idx64_vl3` | [index NF1 C](https://github.com/lionelZhaowy/ara/blob/e434b86c00759c9bc474e050edc720315b4cb62b/verification/index-eew/cases/index_nf1/main.c) | `-DARA_TEST_FIRST=12 -DARA_TEST_COUNT=1 -DARA_TEST_TEST_VL=3` |
| `idx_nf2` | [index NF2 C](https://github.com/lionelZhaowy/ara/blob/e434b86c00759c9bc474e050edc720315b4cb62b/verification/index-eew/cases/index_nf2/main.c) | Defaults, no extra definitions |
| `segment_rs2` | [segment C](https://github.com/lionelZhaowy/ara/blob/d0f57f2eb2f25f21646578c7784007ab4690d665/verification/segment-rs2/cases/segment_rs2/main.c) | No extra definitions |
| `mask_nf5_load` | [mask C](https://github.com/lionelZhaowy/ara/blob/c48eef7abf6f45b959b6002c68780411b757217a/verification/mask-segments/cases/mask_segments/main.c) | `-DARA_TEST_FAMILY=1 -DARA_TEST_NF5_E64_VL3=1` |
| `mask_nf5_store` | Same mask C | `-DARA_TEST_FAMILY=2 -DARA_TEST_NF5_E64_VL3=1` |
| `mask_nf2_load` | Same mask C | `-DARA_TEST_FAMILY=1 -DARA_TEST_NF2_E8_VL129=1` |
| `mask_nf2_store` | Same mask C | `-DARA_TEST_FAMILY=2 -DARA_TEST_NF2_E8_VL129=1` |

Copy each source to its distinct `apps/<app>/main.c` path.
Use the recorded LLVM flags and preserve one common rebuilt ELF per app across the relevant variants.
The [index completion table](https://github.com/lionelZhaowy/ara/blob/e434b86c00759c9bc474e050edc720315b4cb62b/verification/index-eew/cases/REPRODUCTION.md#actual-commands), [segment arguments](https://github.com/lionelZhaowy/ara/blob/d0f57f2eb2f25f21646578c7784007ab4690d665/verification/segment-rs2/cases/REPRODUCTION.md#commands-used), and [mask binding table](https://github.com/lionelZhaowy/ara/blob/c48eef7abf6f45b959b6002c68780411b757217a/verification/mask-segments/cases/REPRODUCTION.md#actual-final-fence-bindings) give the exact final fences and export regions.
The fixed commits' `cases/results.json` files contain their actual ELF hashes, complete signatures, and run arguments.
After any rebuild, repeat the symbol, footer-store, and completion-path checks rather than copying these PCs to different ELF bytes.
J's complete-value and event checks use the corresponding public checkers from these same fixed directories.
Use a separate writable output directory for each call and retain the finite HDL and wall limits.
