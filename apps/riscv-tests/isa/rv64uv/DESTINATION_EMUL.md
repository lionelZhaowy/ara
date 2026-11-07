# OPMVV destination EMUL for multi-register reshuffling

Official tested base: Ara main `34bd3bc152421b4601a7bf3d6e8e91ffb545c99e`.
Two lanes, VLEN=2048, real `ara_tb`, CVA6, and locked dependencies.

mp-17's [PR #421](https://github.com/pulp-platform/ara/pull/421), head
`e7dee6ed7838dbba3f417ef2d616162db314a993`, already covers destination EMUL.
This branch supplies a narrow downstream classification alternative and
independent test evidence. It does not validate the complete PR head.
The full old author head has a separate zero TRANS_ID_BITS elaboration
problem with its CVA6 configuration forwarding. No interface workaround or
dependency upgrade is included here.

The ordinary OPMVV path leaves destination EMUL at LMUL1. Partial writes of
m2/m4 groups then reshuffle only the first destination register. The candidate
restores CSR LMUL for ordinary vector destinations, while preserving mask/
reduction single-register cases and explicit widening EMUL assignments.
The classification method overlaps the author's correction. This is not
presented as a new discovery or as the author's complete patch.

The test retains the official VMACC tests and adds three e32 partial-VL cases.
It initializes both vector sources as EW32 and the old destination with whole-register
EW8 loads. It checks VMACC output, both source memory buffers, destination memory, guards,
VL, and VSTART with independent unsigned modulo-32 multiplication/addition.
The official [RVV 1.0 specification](https://docs.riscv.org/reference/isa/extensions/vector/_attachments/riscv-v-spec.pdf),
section 11.13, specifies integer multiply-add. Section 5.2 defines register
groups and section 3.4.3 defines tail-undisturbed behavior. The final inactive
element must retain its old value under tu.

| LMUL / VL | Main errors | Candidate errors |
| --- | ---: | ---: |
| m1 / 63 | 0 | 0 |
| m2 / 127 | 64 | 0 |
| m4 / 255 | 192 | 0 |

The error counts aggregate output/source/guard/CSR checks. They are not
claimed as that many distinct wrong active multiply-add results.
Both RTL versions use the same frozen ELF, SHA256
`cbf00338e65290c1a5c7b758571e491ff63bb532d1be289da36b480c0c09a1a4`.
The public C SHA256 is
`757be212c3eb5e5333c75668704cceb1b0ad6182114f903e0c7eac53a0495dde`.
Main finishes normally at 98.0365 us with two failing cases and tohost=2.
The candidate finishes normally at 90.3155 us with zero errors and tohost=0.
Wall times were 15.60 s and 14.84 s. Neither reaches its deadline.

Use the official `make -C hardware checkout` dependency setup first. Keep
`Bender.lock` unchanged. The CVA6 HPDcache submodule is
`04de80896981527c34fbbd35d7b1ef787a082d7c`. Build the application from the
Ara checkout:

```sh
LLVM_ROOT=/path/to/llvm-20.1.0
make -C apps config=2_lanes LLVM_INSTALL_DIR="$LLVM_ROOT" \
  LLVM_FLAGS="--target=riscv64-unknown-elf --sysroot=$LLVM_ROOT/riscv64-unknown-elf -isystem $LLVM_ROOT/riscv64-unknown-elf/include -march=rv64gcv_zfh_zvfh -mabi=lp64d -mno-relax -fuse-ld=lld --rtlib=compiler-rt -L$LLVM_ROOT/riscv64-unknown-elf/lib" \
  LLVM_V_FLAGS="-fno-vectorize -fno-slp-vectorize -mno-implicit-float" \
  bin/rv64uv-ara-vmacc
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
  +PRELOAD=/absolute/path/to/frozen/destination-emul.elf > run.log 2>&1
```

The two bounds are limits, not measured run times. Require all three
`EMUL_CASE` lines with zero errors, one `EMUL_DONE completed=3 errors=0`,
tohost=0, normal `$finish`, and no assertion or runtime error. The existing
VMACC checks must also pass. A zero process return does not establish success.
Each parallel run needs its own parent and working directory because
`ara_tb` also creates `../gold_results.txt`.

Six unmodified official regressions pass: vadd, vmacc, vwmul, vsext, vzext,
and vredsum. VCS analysis and elaboration pass. Effective `+lint=all` reports the same 427 normalized diagnostics on both
versions. Real full-port dispatcher Design Compiler checks pass. The DC candidate has 738 flip-flops, zero latches,
zero post-compile blackboxes, and zero unexplained module references.
Analysis, elaboration, link, `compile -no_map`, design and loop checks pass.
There is no mapped timing or area claim.

The scope is the listed two-lane inputs. Source-group scanning, overlap-tail
fills, WAIT changes, WAW changes, and active-vstart rejection are excluded.
Original RTL/test authors and licenses remain. The modified VMACC test retains
its original SHL-0.51 license. AI assistance was used for adaptation, tests, and this explanation.
