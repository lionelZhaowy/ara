# Ara commands for the index EEW comparison

## Fixed source and tools

Use separate source copies at Ara `34bd3bc152421b4601a7bf3d6e8e91ffb545c99e`. Keep the unchanged `Bender.lock` dependencies and CVA6 HPDcache gitlink. The source variants contain only the patches stated in each batch report. Do not add an unrelated repair stack.

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

## Software and result checks

The [actual input definitions and commands](batch5/REPRODUCTION.md#actual-commands) identify all five software builds, the fifteen RTL calls, the five model ELFs, memory regions, and independent gold. Copy each C input to its named official `apps/<name>/main.c` location. The official startup enables vector state before user-mode main. Keep the compiled-run source hashes distinct from the current public formatting copies.

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

The real-memory observer source was unchanged across batches 5 and 6. The mask observer was compiled but not enabled in those calls. Its source was later revised for batch 7. Thus a build with the current mask observer has a new executable hash; it is not the retained batch 5/6 executable or evidence of mask-event coverage in those runs. Each batch's recorded executable and source hashes identify the version actually used.

## Serial runtime and limits

Each call used its own output directory, a 600-second wall limit, and this UCLI file:

```text
run 20 ms
quit
```

The real top reads `+PRELOAD=<ELF>`. It does not read TIMEOUT or MAX_CYCLES plusargs. The [input-specific arguments](batch5/REPRODUCTION.md#actual-commands) enable the passive memory observer. The selected FENCE PC follows every signature store on the completion path. Obtain PCs and region boundaries from the actual ELF after a rebuild. Address order alone does not identify the final fence because LLVM can reorder basic blocks.

The observer reads complete symbol regions from the real DRAM and records accelerator consistency, LSU/store-buffer drain, one final fence, one native exit, and absence of later result writes. The independent byte-address checker checks the exported values. Process exit zero, a fence by itself, or an AXI data value without an address is insufficient.

The scale-only NF2 control returns target 96 with one wrong data word and three error/status changes. The two incomplete original runs use `--allow-incomplete`, reached the 20 ms HDL cutoff, and are not PASS results. All raw data used for the complete-value reviews can be reconstructed from `batch5/results.json`.

## Separate Spike runtime

Apply [spike_sections.patch](common/spike_sections.patch) in the isolated checkout for LLVM suffixed sections. The official `.spike` Make target uses a separate startup and ELF, and enables vector state there. Use the software flags and definitions from the input report with the corresponding `.spike` target. Each actual model call used a 60-second wall limit and this command form:

```bash
"$SPIKE_ROOT/bin/spike" --isa=rv64gcv --varch=vlen:2048,elen:64 \
  --priv=msu --pmpregions=0 +signature=signature.hex +signature-granularity=4 \
  "$ARA_ROOT/apps/bin/idx_nf2.spike"
```

## Direct DC checks

The [DC instructions](common/dc/README.md) specify the exact packages, all-port wrapper, and isolated output directories. O and I completed real lane-sequencer analyze, elaborate, link, and generic `compile -no_map`. No S DC call was planned or executed. This is a generic structural check, not timing or physical signoff.

The bundled mask observer is the original inactive version, SHA256 `0fdc3df529202cc431170a106a72d2e9460d3fa926f4d483136ebe200459416d`. It was compiled but supplied no mask-event evidence for these calls. The active real-memory observer retained its recorded source.
