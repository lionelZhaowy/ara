# Full VTYPE values and VILL restoration

Official tested base: `34bd3bc152421b4601a7bf3d6e8e91ffb545c99e`.
Two lanes, VLEN=2048, real `ara_tb`, CVA6, and locked dependencies.
Candidate source: flaviens's [PR #486](https://github.com/pulp-platform/ara/pull/486),
pinned head `77517f4ee1d4176fd3bacdeccd6b0dda7b7bae3f`. The PR remains open. This is additional test evidence.

The candidate uses the author's three full-XLEN/VILL corrections.
The test covers 56 individual bits 8 through 63, two legal register controls,
saved VILL restoration, and seven immediate cases. The immediate forms cover
bits 8/9/10 in vsetvli and bits 8/9 in vsetivli, with two legal controls.
Each operation starts from a known legal e32/m1 state and nonzero vstart.
It checks returned VL, VL/VTYPE/VSTART, VCSR preservation, and legal recovery.
The official application runs in U mode and does not access machine CSRs.
It checks all 66 configuration inputs.
RVV 1.0 section 6.1.1 requires checks on all VTYPE bits. Unsupported values
set canonical VILL and VL=0. Section 3.7 requires configuration instructions
to clear VSTART. Main truncates rs2 and ignores reserved immediate high bits.

The same ELF runs on both versions, SHA256
`8115a904dac4a8f6a2b91e0d03bfdc43ea7a39cf47905575b6f04a3c571fa3dd`. Both finish normally. Main reports
66 cases, 632 checks, 187 errors, and tohost=1.
The candidate reports the same counts with zero errors and tohost=0.

Use the official `make -C hardware checkout` dependency setup first. Keep
`Bender.lock` unchanged. The CVA6 HPDcache submodule is
`04de80896981527c34fbbd35d7b1ef787a082d7c`. Build the application from the
Ara checkout:

```sh
LLVM_ROOT=/path/to/llvm-20.1.0
make -C apps config=2_lanes LLVM_INSTALL_DIR="$LLVM_ROOT" RISCV_STRIP=true \
  LLVM_FLAGS="--target=riscv64-unknown-elf --sysroot=$LLVM_ROOT/riscv64-unknown-elf -isystem $LLVM_ROOT/riscv64-unknown-elf/include -march=rv64gcv_zfh_zvfh -mabi=lp64d -mno-relax -fuse-ld=lld --rtlib=compiler-rt -L$LLVM_ROOT/riscv64-unknown-elf/lib" \
  LLVM_V_FLAGS="-fno-vectorize -fno-slp-vectorize -mno-implicit-float" \
  bin/vtype
```

Freeze that ELF and use it on both RTL versions. The new SoC comparisons
reuse the frozen original/candidate executables. The following commands build
the same source versions in separate directories for reproduction:

```sh
ARA_ROOT=$PWD
BUILD_DIR="$PWD/build-vtype"
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
printf 'run 20 ms\nrun 0 ns\nquit\n' > control.tcl
timeout 600 "$BUILD_DIR/simv" -no_save -ucli -do control.tcl \
  +PRELOAD=/absolute/path/to/frozen/vtype.elf > run.log 2>&1
```

The two bounds are limits, not measured run times. Require one `VTYPE_DONE cases=66 checks=632 errors=0` line covering all 66 cases, tohost=0,
normal `$finish`, and no assertion or runtime error. A zero process return
does not establish success. Each parallel run needs its own parent and
working directory because `ara_tb` also creates `../gold_results.txt`.


The production RTL is unchanged by the public-name update. The test inputs
were rebuilt and the original/candidate comparisons above were rerun.
The following regression, lint, and DC results reuse their unchanged inputs
from the earlier accepted runs.

The candidate passes unmodified official vsetvl, vsetvli, vsetivli, and vadd tests.
Effective `+lint=all` reports the same 427 normalized diagnostics before/after.
The real dispatcher passes full-port Design Compiler checks. Both versions
have 738 flip-flops and zero latches.
Analysis, elaboration, link, `compile -no_map`, design and loop checks pass.
Post-compile blackboxes and unexplained module references are zero. Generic
internal operators remain unmapped. There is no mapped timing or area claim.
Other lane counts and all configuration combinations are outside this test.
The candidate retains the author's code and original RTL license.
New tests use Apache-2.0. AI assistance was used for adaptation and text.
