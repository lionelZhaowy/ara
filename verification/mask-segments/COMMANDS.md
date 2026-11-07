# Commands and frozen inputs

Use official Ara `34bd3bc152421b4601a7bf3d6e8e91ffb545c99e` and its unchanged Bender.lock.
Set `PUBLIC_ROOT` to this directory, `ARA_ROOT` to one isolated source checkout, and `LLVM_ROOT` / `SPIKE_ROOT` to the installed tools.
The recorded tools were LLVM 20.1.0, Spike revision prefix `5cf439b2`, and VCS W-2024.09-SP1 Full64.

## Software

Copy [main.c](batch7/mask_segments/main.c) to four separate official app directories.
Use these definitions. Separate app names prevent reuse of objects with different compiler flags.

| App | Definitions |
| --- | --- |
| mask_nf5_load | `-DV01_FAMILY=1 -DV01_NF5_E64_VL3=1` |
| mask_nf5_store | `-DV01_FAMILY=2 -DV01_NF5_E64_VL3=1` |
| mask_nf2_load | `-DV01_FAMILY=1 -DV01_NF2_E8_VL129=1` |
| mask_nf2_store | `-DV01_FAMILY=2 -DV01_NF2_E8_VL129=1` |

Example for one app:

```bash
make -C "$ARA_ROOT/apps" config=2_lanes "LLVM_INSTALL_DIR=$LLVM_ROOT" \
  "LLVM_FLAGS=--target=riscv64-unknown-elf --sysroot=$LLVM_ROOT/riscv64-unknown-elf -isystem $LLVM_ROOT/riscv64-unknown-elf/include -march=rv64gcv_zfh_zvfh -mabi=lp64d -mno-relax -fuse-ld=lld --rtlib=compiler-rt -L$LLVM_ROOT/riscv64-unknown-elf/lib" \
  "LLVM_V_FLAGS=-fno-vectorize -fno-slp-vectorize -mno-implicit-float" \
  RISCV_STRIP=true ENV_DEFINES='-DV01_FAMILY=1 -DV01_NF5_E64_VL3=1' bin/mask_nf5_load
```

Build each RTL ELF once. Keep the same ELF across compared source variants.
The published C differs from the retained compiled C in formatting and comments.
Their complete canonical tokens, literal contents, and logical preprocessor directives match.
A new build still requires its own symbol and final-fence binding.
The recorded SHA/PC table in the reproduction report applies only to its exact retained ELF bytes.

## RTL variants and build

O is unchanged official main. M applies [masku_payload.patch](batch7/masku_payload.patch).
L applies [lane_mask_words.patch](batch7/lane_mask_words.patch). ML applies both.
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
The eight original incomplete O/L calls are retained. Only those diagnostics required new invocations.
Already completed A/ML calls retain their original `run 20 ms; quit` input.

```bash
mkdir "$RUN_DIR"
cd "$RUN_DIR"
timeout 600 "$BUILD_DIR/simv" -no_save -ucli -do "$LIMIT_TCL" \
  "+PRELOAD=$ARA_ROOT/apps/bin/$APP" +V01_OBSERVER \
  "+V01_FENCE_PC=$FENCE_PC" +V01_REGIONS=2 \
  +V01_MASK_START_OBSERVE +V01_MASK_SNAPSHOT_NS=20000000 \
  "+V01_R0_BASE=$SIGNATURE_BASE" "+V01_R0_WORDS=$SIGNATURE_WORDS" \
  +V01_R0_NAME=v01_memory_gap_signature \
  "+V01_R1_BASE=$PHASE_BASE" +V01_R1_WORDS=1 +V01_R1_NAME=v01_memory_gap_phase \
  > run.log 2>&1
python3 "$PUBLIC_ROOT/common/review_ara_observer.py" \
  --elf "$ARA_ROOT/apps/bin/$APP" --log v01_observer.log --expect-target 0
python3 "$PUBLIC_ROOT/batch7/check_mask_signature.py" \
  --nf "$NF" --family "$FAMILY" --signature v01_region_0.hex --negative-controls
python3 "$PUBLIC_ROOT/batch7/check_mask_events.py" \
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
The public [B5/B6 commands](https://github.com/lionelZhaowy/ara/blob/c70bbe98e38434736378407b3f30da331e7ec4cf/verification/index-eew/COMMANDS.md) record the previously accepted index/decode inputs and module checks.
The combination J checks use those exact ELFs with decode, index EEW, and the two local mask patches.
