# Ara build, run, and check commands

## Fixed source and tools

Use separate source copies at Ara `34bd3bc152421b4601a7bf3d6e8e91ffb545c99e`. Keep the unchanged `Bender.lock` dependencies and CVA6 HPDcache gitlink. The source variants contain only the patches stated in the input report. Do not add an unrelated repair stack.

| Locked dependency | Commit |
| --- | --- |
| APB | `77ddf073f194d44b9119949d2421be59789e69ae` |
| AXI | `853ede23b2a9837951b74dbdc6d18c3eef5bac7d` |
| common_cells | `c27bce39ebb2e6bae52f60960814a2afca7bd4cb` |
| common_verification | `9c07fa860593b2caabd9b5681740c25fac04b878` |
| CVA6 | `99eac9a649001bdf5b8f9da52e0ca73d5c48db1c` |
| FPnew | `e5aa6a01b5bbe1675c3aa8872e1203413ded83d1` |
| fpu_div_sqrt_mvp | `86e1f558b3c95e91577c41b2fc452c86b04e85ac` |
| tech_cells_generic | `7968dd6e6180df2c644636bc6d2908a49f2190cf` |

The executed configuration was `2_lanes`, VLEN=2048, ELEN=64, write-through D-cache, and zero PMP entries. Tools were LLVM 20.1.0, VCS W-2024.09-SP1 Full64, and the Spike installation identified by revision prefix `5cf439b2`.

Set `ARA_ROOT` to the selected source copy, `PUBLIC_ROOT` to this material directory, `LLVM_ROOT` to the LLVM installation, and `SPIKE_ROOT` to the installed Spike prefix. Set `BUILD_DIR` and `RUN_DIR` to separate absolute output directories for each build and call. These variables select existing tools and paths. They do not request dependency updates.

## Software Make targets

The applicable official entry is `apps/<name>/main.c` with a `bin/<name>` Make target. Copy [segment_rs2/main.c](cases/segment_rs2/main.c) to `apps/segment_rs2/main.c` in the isolated checkout. Run:

```bash
cd "$ARA_ROOT"
llvm_args=(
  config=2_lanes "LLVM_INSTALL_DIR=$LLVM_ROOT"
  "LLVM_FLAGS=--target=riscv64-unknown-elf --sysroot=$LLVM_ROOT/riscv64-unknown-elf -isystem $LLVM_ROOT/riscv64-unknown-elf/include -march=rv64gcv_zfh_zvfh -mabi=lp64d -mno-relax -fuse-ld=lld --rtlib=compiler-rt -L$LLVM_ROOT/riscv64-unknown-elf/lib"
  "LLVM_V_FLAGS=-fno-vectorize -fno-slp-vectorize -mno-implicit-float"
  RISCV_STRIP=true
)
make -C apps "${llvm_args[@]}" \
  ENV_DEFINES= bin/segment_rs2
```

The official runtime enables vector state before user-mode `main`. The final C inputs do not write machine-mode `mstatus` from `main`. The first draft did so and produced two setup failures; those calls are not part of the earlier accepted results. The final programs use ordinary arrays and the official return path, with no private address alias or startup macro.

Compile each RTL input once and retain its exact ELF and hash across the compared RTL source variants. The ELF PC and memory addresses below belong to those retained bytes. A rebuild requires a new symbol and fence-PC check.

## RTL build

The official hardware Makefile provides Questa and Verilator entries, not a VCS target. The executed VCS recipe uses Bender's official target selection, the real `ara_tb`, and its DPI ELF loader. Generate the compile script from the selected source root:

```bash
cd "$ARA_ROOT"
mkdir -p "$BUILD_DIR"
bender --local --git-submodules false script vcs \
  -t rtl -t cv64a6_imafdcv_sv39 \
  -t tech_cells_generic_include_tc_sram -t tech_cells_generic_include_tc_clk \
  -t exclude_first_pass_decoder -t ara_test -t cva6_test \
  -D NR_LANES=2 -D VLEN=2048 -D ARIANE_ACCELERATOR_PORT=1 \
  --vlogan-args="-full64 -assert svaext +v2k -timescale=1ns/1ps" \
  > "$BUILD_DIR/compile.sh"
cd "$BUILD_DIR"
bash compile.sh
vlogan -full64 -sverilog -timescale=1ns/1ps -assert svaext \
  "$PUBLIC_ROOT/common/ara_observer.sv" \
  "$PUBLIC_ROOT/common/ara_mask_observer.sv"
vcs -full64 -sverilog -timescale=1ns/1ps -assert svaext \
  -top ara_tb -top ara_observer -top ara_mask_observer \
  "$ARA_ROOT/hardware/tb/dpi/elfloader.cc" \
  -CFLAGS "-I$SPIKE_ROOT/include" -o simv
```

The actual variant compile scripts reused the generated source list with only the source root changed. The extra tops are passive observers. No waveform or extra trace define was added; the official source list already contains `instr_tracer`.

## RTL runs and limits

Each recorded call had its own output directory. The existing UCLI command file, selected by `LIMIT_TCL`, contains:

```tcl
run 20 ms
run 0 ns
quit
```

The wall limit was 600 seconds per call. Use independent writable directories without waveforms. `ara_tb` accepts `PRELOAD`; it has no `TIMEOUT` or `MAX_CYCLES` plusarg. A UCLI cutoff can return host status zero and execute final export code without a native test completion. It is not a PASS.

For the rebuilt RTL ELF with SHA256 `78566c43a40623bc8a3a142e05f10b5378b162ccdb64d656cbf3705c1ee2d4bd`, the recorded probe arguments are:

```bash
probe_args=(
  -no_save -ucli -do "$LIMIT_TCL" "+PRELOAD=$ARA_ROOT/apps/bin/segment_rs2"
  +ARA_TEST_OBSERVER +ARA_TEST_VECTOR_COMMIT +ARA_TEST_FENCE_PC=8000108a +ARA_TEST_REGIONS=3
  +ARA_TEST_R0_BASE=80001f00 +ARA_TEST_R0_WORDS=916 +ARA_TEST_R0_NAME=ara_test_segment_rs2_observed
  +ARA_TEST_R1_BASE=80003210 +ARA_TEST_R1_WORDS=4 +ARA_TEST_R1_NAME=ara_test_segment_rs2_counts
  +ARA_TEST_R2_BASE=80003220 +ARA_TEST_R2_WORDS=8 +ARA_TEST_R2_NAME=ara_test_segment_rs2_first_error
)
mkdir -p "$RUN_DIR"
cd "$RUN_DIR"
timeout 600 "$BUILD_DIR/simv" "${probe_args[@]}"
```

Use the same probe arguments once with original build O and once with candidate build D. O reports native target 6, with six exact data errors. D reports target 0 and PASS. The two earlier accepted candidate-only regressions used no observer-enable plusarg. They are reused with unchanged input and production RTL; their command forms were:

```bash
timeout 600 "$BUILD_D/simv" -ucli -do "$LIMIT_TCL" \
  "+PRELOAD=$ARA_ROOT/apps/bin/rv64uv-ara-vl1r"
timeout 600 "$BUILD_D/simv" -ucli -do "$LIMIT_TCL" \
  "+PRELOAD=$ARA_ROOT/apps/bin/rv64uv-ara-vs1r"
```

## Real memory and final-fence selection

The passive [observer](common/ara_observer.sv) reads the real `ara_tb` DRAM array. It does not drive or stop the DUT. Region bases and complete sizes come from symbols in the actual retained ELF. The observer checks alignment and bounds against the real memory depth and exports each selected 32-bit word, including guards and status where specified.

For an accepted completed probe, require one selected retired fence and one native exit. At that fence, check the LSU committed-store queue, write-through buffer, combined empty indication, and accelerator consistency. Reject late writes to any exported region and observer consistency errors. Check the normal testbench finish, native target, full signature, intended vector instruction retirements where recorded, and unexpected assertions. Neither a fence alone nor host return code zero establishes success. Preserve the existing time-zero `axi_inval_filter` warning and the intended original/control failure warnings in the result classification.

The [export checker](common/review_ara_observer.py) checks ELF/symbol binding, completion, memory export, and observer consistency. The value checker independently checks architectural data. Run from `PUBLIC_ROOT`:

```bash
cd "$PUBLIC_ROOT"
python3 common/review_ara_observer.py --elf "$ARA_ROOT/apps/bin/segment_rs2" \
  --log "$RUN_O/ara_test_observer.log" --expect-target 6
python3 common/review_ara_observer.py --elf "$ARA_ROOT/apps/bin/segment_rs2" \
  --log "$RUN_D/ara_test_observer.log" --expect-target 0
python3 cases/check_segment_rs2.py --elf "$ARA_ROOT/apps/bin/segment_rs2" \
  --signature "$RUN_O/ara_test_region_0.hex" --variant baseline
python3 cases/check_segment_rs2.py --elf "$ARA_ROOT/apps/bin/segment_rs2" \
  --signature "$RUN_D/ara_test_region_0.hex" --variant candidate
```

## Separate Spike runtime and linker adaptation

The official app `.spike` target builds a separate CRT and ELF. It also changes that checkout's Spike startup code to enable vector state. Use the isolated checkout and inspect this change. Do not substitute the model ELF for the same-ELF RTL comparison.

LLVM emits suffixed sections. The initial Spike link left `.text.*`, `.data.*`, `.bss.*`, `.tdata.*`, and `.tbss.*` outside the intended sections after `_end`; the TLS putchar buffer then overlapped the signature. The [section-wildcard patch](common/spike_sections.patch) fixes that layout. Apply it only to the isolated model build and use the same `llvm_args` and app definitions:

```bash
cd "$ARA_ROOT"
git apply "$PUBLIC_ROOT/common/spike_sections.patch"
make -C apps "${llvm_args[@]}" bin/segment_rs2.spike
cd "$RUN_SPIKE"
"$SPIKE_ROOT/bin/spike" --isa=rv64gcv --varch=vlen:2048,elen:64 \
  --priv=msu --pmpregions=0 +signature=signature.hex +signature-granularity=4 \
  "$ARA_ROOT/apps/bin/segment_rs2.spike"
cd "$PUBLIC_ROOT"
python3 cases/check_segment_rs2.py --elf "$ARA_ROOT/apps/bin/segment_rs2.spike" \
  --signature "$RUN_SPIKE/signature.hex" --variant candidate
```

## Module implementation checks

The [DC instructions](common/dc/README.md) specify the fixed packages, dependency roots, all-port wrappers, and direct tool entries. The earlier campaign completed separate real-dispatcher O/D processes with analyze, elaborate, link, and `compile -no_map`; its [report](cases/REPRODUCTION.md#dispatcher-implementation-checks) records the accepted generic structural scope and retained warnings. Raw pre-compile black-box flags on native operators and constants were reviewed through their actual attributes; they were not treated as unresolved user IP.

The renamed passive observers were compiled again for this update. Mask-event logging was disabled in the segment calls and supplies no mask-event evidence. The current public C was compiled again; its source hash equals the actual software-build input hash. The previous public commit retains the old hashes, formatting checks, and original ELF bytes.

The DC script logic, production RTL, packages, and wrapper types are unchanged. Only public environment-variable labels changed. The structural results are earlier accepted generic checks, not new DC invocations.
