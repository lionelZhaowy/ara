# Full-width signed averaging-add boundaries

Official tested base: `34bd3bc152421b4601a7bf3d6e8e91ffb545c99e`.
Two lanes, VLEN=2048, real `ara_tb`, CVA6, and locked dependencies.
Candidate source: David Harris (davidharrishmc)'s [PR #513](https://github.com/pulp-platform/ara/pull/513),
pinned head `3cde4f928e1cf3b914d14b6848ac5f3fe79795be`. The PR remains open. This is additional test evidence.

The candidate uses only David Harris's addition subset from #513.
It includes no subtraction change. Main forms a widened unsigned sum and then
uses the old operand-width sign bit. Signed addition overflow therefore
changes the average. The patch sign-extends both operands before addition.
RVV 1.0 section 12.2 defines signed averaging addition without sum overflow.
Section 3.9 defines the four fixed-point rounding modes.
The SoC test covers signed e32 vv/vx forms, four VXRM modes, and positive/
negative boundaries. An independent 64-bit signed sum computes floor-half
and the rounding increment. Every case checks all 64 outputs, both sources,
guards, VL/VSTART, and VCSR. No masks or nonzero active VSTART are tested.

The same ELF runs on both versions, SHA256
`e73377c18e240f9b0e40cf834d5658f7eb9ec58246ccdb2d810239ed554576a2`. Both finish normally. Main reports
64 cases, 14080 checks, 1152 errors, and tohost=1.
The candidate reports the same counts with zero errors and tohost=0.

The real combinational simd_alu also runs 256 frozen packed boundary rows:
SEW8/16/32/64, four VXRM modes, 513 comparisons. Main has 160 errors.
The candidate has zero. A one-bit wrong-gold control has exactly one error.
All finish at 256 ns, before the 1 us HDL deadline, with a 60 s wall limit.
`verify_vectors.py` checks every packed expected word with Python unbounded
signed arithmetic. `vectors.txt` has no model-generated RTL values.
The unit test does not establish masked/sticky CSR or architectural restart.

Use the official `make -C hardware checkout` dependency setup first. Keep
`Bender.lock` unchanged. The CVA6 HPDcache submodule is
`04de80896981527c34fbbd35d7b1ef787a082d7c`. Build the application from the
Ara checkout:

```sh
LLVM_ROOT=/path/to/llvm-20.1.0
make -C apps config=2_lanes LLVM_INSTALL_DIR="$LLVM_ROOT" RISCV_STRIP=true \
  LLVM_FLAGS="--target=riscv64-unknown-elf --sysroot=$LLVM_ROOT/riscv64-unknown-elf -isystem $LLVM_ROOT/riscv64-unknown-elf/include -march=rv64gcv_zfh_zvfh -mabi=lp64d -mno-relax -fuse-ld=lld --rtlib=compiler-rt -L$LLVM_ROOT/riscv64-unknown-elf/lib" \
  LLVM_V_FLAGS="-fno-vectorize -fno-slp-vectorize -mno-implicit-float" \
  bin/vaadd_boundary
```

Freeze that ELF and use it on both RTL versions. The new SoC comparisons
reuse the frozen original/candidate executables. The following commands build
the same source versions in separate directories for reproduction:

```sh
ARA_ROOT=$PWD
BUILD_DIR="$PWD/build-vaadd-boundary"
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
  +PRELOAD=/absolute/path/to/frozen/vaadd_boundary.elf > run.log 2>&1
```

The two bounds are limits, not measured run times. Require one `VAADD_DONE cases=64 checks=14080 errors=0` line covering all 64 cases, tohost=0,
normal `$finish`, and no assertion or runtime error. A zero process return
does not establish success. Each parallel run needs its own parent and
working directory because `ara_tb` also creates `../gold_results.txt`.


For the combinational test, compile the real ALU and its packages directly.
Use a separate empty build directory for each RTL version. ARA_ROOT selects
that version; INPUT_ROOT selects one frozen public input copy for both builds.
These are the source arguments used for the recorded unit calls:

```sh
INPUT_ROOT=/path/to/frozen/apps/vaadd_boundary
vcs -full64 -sverilog -timescale=1ns/1ps +define+NR_LANES=2+VLEN=2048 \
  +incdir+"$ARA_ROOT/hardware/deps/axi/include" \
  +incdir+"$ARA_ROOT/hardware/deps/common_cells/include" \
  +incdir+"$ARA_ROOT/hardware/include" \
  "$ARA_ROOT/hardware/deps/common_cells/src/cf_math_pkg.sv" \
  "$ARA_ROOT/hardware/deps/axi/src/axi_pkg.sv" \
  "$ARA_ROOT/hardware/deps/fpnew/src/fpnew_pkg.sv" \
  "$ARA_ROOT/hardware/deps/cva6/core/include/config_pkg.sv" \
  "$ARA_ROOT/hardware/deps/cva6/core/include/cv64a6_imafdcv_sv39_config_pkg.sv" \
  "$ARA_ROOT/hardware/deps/cva6/core/include/build_config_pkg.sv" \
  "$ARA_ROOT/hardware/deps/cva6/core/include/riscv_pkg.sv" \
  "$ARA_ROOT/hardware/include/rvv_pkg.sv" \
  "$ARA_ROOT/hardware/include/ara_pkg.sv" \
  "$ARA_ROOT/hardware/src/lane/simd_alu.sv" \
  "$INPUT_ROOT/tb_vaadd_alu.sv" -top tb_ara_test_vaadd_alu -o alu_simv
python3 "$INPUT_ROOT/verify_vectors.py"
printf 'run 1000 ns\nrun 0 ns\nquit\n' > control.tcl
timeout 60 ./alu_simv -no_save -ucli -do control.tcl \
  +ARA_TEST_VAADD_VECTORS="$INPUT_ROOT/vectors.txt" > unit.log 2>&1
timeout 60 ./alu_simv -no_save -ucli -do control.tcl \
  +ARA_TEST_VAADD_VECTORS="$INPUT_ROOT/vectors.txt" \
  +ARA_TEST_WRONG_GOLD > wrong_gold.log 2>&1
```

Require exactly `cases=256 checks=513 errors=0 wrong_gold=0` on the
candidate. Main must report 160 errors. The candidate wrong-gold run must
report exactly one error and the expected failure marker. The TB has an
explicit 1 us HDL deadline. All accepted runs end at 256 ns.

The production RTL is unchanged by the public-name update. The test inputs
were rebuilt and the original/candidate comparisons above were rerun.
The following regression, lint, and DC results reuse their unchanged inputs
from the earlier accepted runs.

The candidate passes unmodified official vadd, vaadd, and vaaddu tests.
Effective `+lint=all` reports the same 427 normalized diagnostics before/after.
The real VALU, including the modified simd_alu, passes full-port Design
Compiler checks. Both versions have 1010 flip-flops and zero latches.
Analysis, elaboration, link, `compile -no_map`, design and loop checks pass.
Post-compile blackboxes and unexplained module references are zero. Generic
internal operators remain unmapped. There is no mapped timing or area claim.
Other lane counts and all configuration combinations are outside this test.
The candidate retains the author's code and original RTL license.
New tests use Apache-2.0. AI assistance was used for adaptation and text.
