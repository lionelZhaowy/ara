/* Portable segment rs2 regression for the official Ara app runtime.
 * AI tools assisted with the platform adaptation.
 */
/* segment rs2=x8/v8 decode regression.
 * Six memory encodings, each paired with an alternative register.
 * Every target uses e8/m1, VL1, unmasked and architectural vstart=0.
 * The index EEW equals data SEW; internal segment vstart remains zero.
 * SPDX-License-Identifier: Apache-2.0
 */
#include <stdint.h>
#ifndef SPIKE
#include "printf.h"
#else
#include <stdio.h>
#endif

enum { CASES = 12, CHECKS_PER_CASE = 38, CHECKS = 458, VSETS = 36 };
struct bytes16 {
  uint64_t before;
  uint8_t data[16];
  uint64_t after;
};
struct bytes4 {
  uint64_t before;
  uint8_t data[4];
  uint64_t after;
};
struct bytes2 {
  uint64_t before;
  uint8_t data[2];
  uint64_t after;
};
struct fixture {
  struct bytes16 memory;
  struct bytes4 out;
  struct bytes2 index;
  struct bytes4 index_out;
};
static struct fixture storage __attribute__((aligned(64)));
static unsigned cases, checks, vsets, errors;
volatile uint32_t v01_segment_rs2_counts[4];
volatile uint64_t v01_segment_rs2_first_error[4];

/* Every comparison's actual value, in comparison order. No hashes.
 * The independent Python oracle builds the expected 458 values without C
 * conditionals or RTL predicates. The same bytes are exported by Spike.
 */
__asm__(".pushsection .data.v01_segment_rs2_observed,\"aw\",@progbits\n"
        ".balign 64\n"
#ifdef SPIKE
        ".global begin_signature\nbegin_signature:\n"
#endif
        ".global v01_segment_rs2_observed\n"
        ".type v01_segment_rs2_observed,@object\n"
        "v01_segment_rs2_observed:\n.zero 3664\n"
        ".size v01_segment_rs2_observed,.-v01_segment_rs2_observed\n"
#ifdef SPIKE
        ".global end_signature\nend_signature:\n"
#endif
        ".popsection\n");
extern volatile uint64_t v01_segment_rs2_observed[CHECKS];

static uint64_t guard(unsigned id, unsigned member, unsigned side) {
  return UINT64_C(0x5a3c91e700000000) | ((uint64_t)id << 16) |
         ((uint64_t)member << 8) | side;
}

static void check(unsigned id, unsigned field, uint64_t got, uint64_t want) {
  if (checks < CHECKS)
    v01_segment_rs2_observed[checks] = got;
  ++checks;
  if (got != want) {
    if (!errors) {
      v01_segment_rs2_first_error[0] = id;
      v01_segment_rs2_first_error[1] = field;
      v01_segment_rs2_first_error[2] = got;
      v01_segment_rs2_first_error[3] = want;
    }
    ++errors;
  }
}

struct result {
  uintptr_t setup, target, exported, vl, vt, vs;
};
#define EXEC(INSN)                                                             \
  __asm__ volatile(                                                            \
      "fence rw, rw\ncsrw vstart, zero\n"                                      \
      "vsetivli %[setup], 2, e8, m1, tu, mu\n"                                 \
      "vle8.v v8, (%[index])\nvle8.v v12, (%[index])\n"                        \
      "vmv.v.x v16, %[init0]\nvmv.v.x v17, %[init1]\n"                         \
      "li s0, 4\nli s1, 4\n"                                                   \
      "vsetivli %[target], 1, e8, m1, tu, mu\n" INSN "\n"                      \
      "csrr %[vl], vl\ncsrr %[vt], vtype\ncsrr %[vs], vstart\n"                \
      "vsetivli %[exported], 2, e8, m1, tu, mu\n"                              \
      "vse8.v v16, (%[out])\naddi t0, %[out], 2\nvse8.v v17, (t0)\n"           \
      "vse8.v v8, (%[index_out])\n"                                            \
      "addi t0, %[index_out], 2\nvse8.v v12, (t0)\nfence rw, rw\n"             \
      : [setup] "=&r"(r.setup), [target] "=&r"(r.target),                      \
        [exported] "=&r"(r.exported), [vl] "=&r"(r.vl), [vt] "=&r"(r.vt),      \
        [vs] "=&r"(r.vs)                                                       \
      : [memory] "r"(p->memory.data), [out] "r"(p->out.data),                  \
        [index] "r"(p->index.data), [index_out] "r"(p->index_out.data),        \
        [init0] "r"(initial0), [init1] "r"(initial1)                           \
      : "t0", "s0", "s1", "v8", "v12", "v16", "v17", "memory")

static struct result execute(unsigned id, volatile struct fixture *p) {
  struct result r = {0};
  const uintptr_t initial0 = ((id / 2) & 1) ? 0xa1 : 0xd1;
  const uintptr_t initial1 = ((id / 2) & 1) ? 0xb2 : 0xe2;
  switch (id) {
  case 0:
    EXEC("vlsseg2e8.v v16, (%[memory]), s0");
    break;
  case 1:
    EXEC("vlsseg2e8.v v16, (%[memory]), s1");
    break;
  case 2:
    EXEC("vssseg2e8.v v16, (%[memory]), s0");
    break;
  case 3:
    EXEC("vssseg2e8.v v16, (%[memory]), s1");
    break;
  case 4:
    EXEC("vluxseg2ei8.v v16, (%[memory]), v8");
    break;
  case 5:
    EXEC("vluxseg2ei8.v v16, (%[memory]), v12");
    break;
  case 6:
    EXEC("vsuxseg2ei8.v v16, (%[memory]), v8");
    break;
  case 7:
    EXEC("vsuxseg2ei8.v v16, (%[memory]), v12");
    break;
  case 8:
    EXEC("vloxseg2ei8.v v16, (%[memory]), v8");
    break;
  case 9:
    EXEC("vloxseg2ei8.v v16, (%[memory]), v12");
    break;
  case 10:
    EXEC("vsoxseg2ei8.v v16, (%[memory]), v8");
    break;
  case 11:
    EXEC("vsoxseg2ei8.v v16, (%[memory]), v12");
    break;
  }
  vsets += 3;
  return r;
}

static void run(unsigned id) {
  volatile struct fixture *p = (&storage);
  const unsigned is_store = (id / 2) & 1, offset = id < 4 ? 0 : 3;
  for (unsigned i = 0; i < 16; ++i)
    p->memory.data[i] = (uint8_t)(0x40 + i);
  for (unsigned i = 0; i < 4; ++i) {
    p->out.data[i] = 0xcc;
    p->index_out.data[i] = 0xcc;
  }
  p->index.data[0] = 3;
  p->index.data[1] = 7;
#define INIT(M, N)                                                             \
  p->M.before = guard(id, N, 0);                                               \
  p->M.after = guard(id, N, 1)
  INIT(memory, 0);
  INIT(out, 1);
  INIT(index, 2);
  INIT(index_out, 3);
#undef INIT
  const struct result r = execute(id, p);
  check(id, 0, r.setup, 2);
  check(id, 1, r.target, 1);
  check(id, 2, r.exported, 2);
  check(id, 3, r.vl, 1);
  check(id, 4, r.vt, 0);
  check(id, 5, r.vs, 0);
  for (unsigned i = 0; i < 16; ++i) {
    uint8_t want = (uint8_t)(0x40 + i);
    if (is_store && i == offset)
      want = 0xa1;
    if (is_store && i == offset + 1)
      want = 0xb2;
    check(id, 6 + i, p->memory.data[i], want);
  }
  for (unsigned f = 0; f < 2; ++f)
    for (unsigned i = 0; i < 2; ++i) {
      uint8_t want = is_store ? (f ? 0xb2 : 0xa1) : (f ? 0xe2 : 0xd1);
      if (!is_store && i == 0)
        want = (uint8_t)(0x40 + offset + f);
      check(id, 22 + 2 * f + i, p->out.data[2 * f + i], want);
    }
  for (unsigned i = 0; i < 4; ++i)
    check(id, 26 + i, p->index_out.data[i], (i & 1) ? 7 : 3);
#define GUARD(M, N)                                                            \
  check(id, 30 + 2 * N, p->M.before, guard(id, N, 0));                         \
  check(id, 31 + 2 * N, p->M.after, guard(id, N, 1))
  GUARD(memory, 0);
  GUARD(out, 1);
  GUARD(index, 2);
  GUARD(index_out, 3);
#undef GUARD
  ++cases;
}

int main(void) {
  uintptr_t vlenb;
  __asm__ volatile("csrw vstart, zero\ncsrr %0, vlenb"
                   : "=r"(vlenb)
                   :
                   : "memory");
  check(CASES, 0, vlenb, 256);
  for (unsigned id = 0; id < CASES; ++id)
    run(id);
  check(CASES, 1, cases, CASES);
  v01_segment_rs2_counts[0] = cases;
  v01_segment_rs2_counts[1] = checks;
  v01_segment_rs2_counts[2] = vsets;
  v01_segment_rs2_counts[3] = errors;
  __asm__ volatile("fence rw, rw" : : : "memory");
  printf("V01_SEG_RS2_DONE cases=%u checks=%u vsets=%u errors=%u\n", cases,
         checks, vsets, errors);
  return (int)errors;
}
