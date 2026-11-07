# Integer-extension source EMUL with mixed register EEWs

Tested base: Ara main `34bd3bc152421b4601a7bf3d6e8e91ffb545c99e`.
Configuration: two lanes, VLEN=2048, official `ara_tb`, CVA6, locked dependencies.

Each case creates a four-register EW16 source group v16-v19. It then rewrites
v17, v18, and v19 with EW8 while preserving the logical source bytes.
The first register retains EW16. The destination v24-v31 is disjoint.
The test executes `vzext.vf2` or `vsext.vf2` at e32/m8, VL=512.
An independent byte formula checks every result, all three regions' guards,
VL, and vstart. RVV 1.0 section 11.3 specifies the extension operation.
Source EMUL is four registers, while destination LMUL is eight registers.

| Case | Main errors | Narrow candidate errors |
| --- | ---: | ---: |
| Zero extension | 384 | 0 |
| Sign extension | 384 | 0 |

Both versions run the same ELF, SHA256
`b02671db783b2bff5125ef55e9ddf0e9ca32a9ca3bba237b551f97c87acd164a`.
Each makes 1126 checks and finishes normally. Main returns tohost=1 with
768 errors. The candidate returns tohost=0 with zero errors.
The passive observer confirms both initial EEW layouts and exactly six
reshuffle requests, all restricted to v17-v19. It checks the source counter
limit and excludes registers outside v16-v19. It never drives or forces RTL.
Compile it after the generated source list and add
`-top extension_source_observer` at elaboration for this directed test.

Main checks only the first source register's EEW before deciding whether to
reshuffle. That register already has the requested EW16 and hides later
mismatches. The candidate checks each physical source register and limits
reshuffle counters to source EMUL. It applies only to integer extensions.
It has no inactive-element fill change and no destination-overlap change.

mp-17's PR #421 already covers the broader source-group EEW check and source
EMUL correction. This narrow downstream alternative overlaps that work.
It is provided as additional evidence on current main. These runs do not
validate the complete #421 head `e7dee6ed7838dbba3f417ef2d616162db314a993`.
No duplicate PR is proposed.

Use the official `make -C hardware checkout` dependency setup first. Keep
`Bender.lock` unchanged. The CVA6 HPDcache submodule is
`04de80896981527c34fbbd35d7b1ef787a082d7c`. Build the application from the
Ara checkout:

```sh
LLVM_ROOT=/path/to/llvm-20.1.0
make -C apps config=2_lanes LLVM_INSTALL_DIR="$LLVM_ROOT" \
  LLVM_FLAGS="--target=riscv64-unknown-elf --sysroot=$LLVM_ROOT/riscv64-unknown-elf -isystem $LLVM_ROOT/riscv64-unknown-elf/include -march=rv64gcv_zfh_zvfh -mabi=lp64d -mno-relax -fuse-ld=lld --rtlib=compiler-rt -L$LLVM_ROOT/riscv64-unknown-elf/lib" \
  LLVM_V_FLAGS="-fno-vectorize -fno-slp-vectorize -mno-implicit-float" \
  bin/extension_source
```

For the candidate-only passive checks, run this command after `bash compile.sh`:

```sh
vlogan -full64 -sverilog -timescale=1ns/1ps -assert svaext \
  "$ARA_ROOT/apps/extension_source/observer.sv"
```

Add `-top extension_source_observer` to the VCS elaboration command below.
Use the normal command without this observer on main and on ordinary regressions.

Freeze that ELF and use it on both RTL versions. Generate a distinct VCS
build directory for each checkout:

```sh
ARA_ROOT=$PWD
BUILD_DIR="$PWD/build-ext-hazards"
SPIKE_ROOT=/path/to/spike
mkdir "$BUILD_DIR"
bender --local --git-submodules false script vcs \
  -t rtl -t cv64a6_imafdcv_sv39 \
  -t tech_cells_generic_include_tc_sram -t tech_cells_generic_include_tc_clk \
  -t exclude_first_pass_decoder -t ara_test -t cva6_test \
  -D NR_LANES=2 -D VLEN=2048 -D ARIANE_ACCELERATOR_PORT=1 \
  --vlogan-args="-full64 -assert svaext +v2k -timescale=1ns/1ps" \
  > "$BUILD_DIR/compile.sh"
cd "$BUILD_DIR"
bash compile.sh
vcs -full64 -sverilog -timescale=1ns/1ps -assert svaext -top ara_tb \
  "$ARA_ROOT/hardware/tb/dpi/elfloader.cc" \
  -CFLAGS "-I$SPIKE_ROOT/include" -o simv
mkdir -p run/work
cd run/work
printf 'run 20ms\nquit\n' > control.tcl
timeout 600 "$BUILD_DIR/simv" -ucli -i control.tcl \
  +PRELOAD=/absolute/path/to/frozen/extension_source.elf > run.log 2>&1
```

The two bounds are limits, not measured run times. Require both
`SOURCE_CASE` lines, `SOURCE_DONE cases=2 checks=1126 errors=0`, tohost=0,
normal `$finish`, and no assertion or runtime error. A zero process return
does not establish success. Each parallel run needs its own parent and
working directory because `ara_tb` also creates `../gold_results.txt`.


The candidate passes the unmodified official vadd, vsext, vzext, and vwmul
tests. Effective `+lint=all` reports the same 427 normalized diagnostics.
Real full-port dispatcher checks pass Design Compiler analysis, elaboration,
link, `compile -no_map`, design and loop checks. Both have 738 flip-flops,
zero latches, and zero unexplained module references after compilation.
This generic structure check has no mapped timing or area result.
The RTL retains its authors and Solderpad license. New test/observer files
use Apache-2.0. AI assistance was used for adaptation, tests, and this text.
