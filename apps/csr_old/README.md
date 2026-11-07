# Immediate VCSR old-value return

Official tested base: `34bd3bc152421b4601a7bf3d6e8e91ffb545c99e`.
Two lanes, VLEN=2048, real `ara_tb`, CVA6, and locked dependencies.
Candidate source: flaviens's [PR #489](https://github.com/pulp-platform/ara/pull/489),
pinned head `9f0506ebdebcff8be800c86fea4d39f4e2713e7a`. The PR remains open. This is additional test evidence.

The candidate uses the author's one-line full VCSR readback correction.
The test covers all eight old VCSR values with immediate writes. It checks
the old returned value and the new VCSR, VXRM, and VXSAT views. The seed
write uses rd=x0, so the readback defect cannot affect setup. Zicsr requires
CSRRWI to return the old CSR. RVV 1.0 section 3.11 defines VCSR as VXRM and
VXSAT together. Main returns only VXSAT. No other CSR patch is included.

The same ELF runs on both versions, SHA256
`4b5b3f5c76eeea2e3ae77d4ca33adc59dea7204ff66a525d7e8f81cfcce3d404`. Both finish normally. Main reports
8 cases, 32 checks, 6 errors, and tohost=1.
The candidate reports the same counts with zero errors and tohost=0.

Use the official `make -C hardware checkout` dependency setup first. Keep
`Bender.lock` unchanged. The CVA6 HPDcache submodule is
`04de80896981527c34fbbd35d7b1ef787a082d7c`. Build the application from the
Ara checkout:

```sh
LLVM_ROOT=/path/to/llvm-20.1.0
make -C apps config=2_lanes LLVM_INSTALL_DIR="$LLVM_ROOT" \
  LLVM_FLAGS="--target=riscv64-unknown-elf --sysroot=$LLVM_ROOT/riscv64-unknown-elf -isystem $LLVM_ROOT/riscv64-unknown-elf/include -march=rv64gcv_zfh_zvfh -mabi=lp64d -mno-relax -fuse-ld=lld --rtlib=compiler-rt -L$LLVM_ROOT/riscv64-unknown-elf/lib" \
  LLVM_V_FLAGS="-fno-vectorize -fno-slp-vectorize -mno-implicit-float" \
  bin/csr_old
```

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
  +PRELOAD=/absolute/path/to/frozen/csr_old.elf > run.log 2>&1
```

The two bounds are limits, not measured run times. Require one `CSR_OLD_DONE cases=8 checks=32 errors=0` line covering all 8 cases, tohost=0,
normal `$finish`, and no assertion or runtime error. A zero process return
does not establish success. Each parallel run needs its own parent and
working directory because `ara_tb` also creates `../gold_results.txt`.


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
