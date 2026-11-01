/* Draft for public review. Adapted input: no previous ELF result applies.
 * AI tools assisted with this adaptation.
 */
/* ordered indexed segment loads/stores, portable draft for the official Ara app
 * runtime. One ELF per NFIELDS=2..8; SEW32/m1, VL3, CSR vstart0, tu/mu. The
 * segment sequencer creates internal nonzero vstart and indexed addrgen
 * documents incomplete vstart handling. This is a specific test risk, not an
 * assumed expected failure or exclusion. Ordered semantics apply between
 * segments. No within-segment field order is required by this test; every field
 * has an independent exact final oracle. SPDX-License-Identifier: Apache-2.0
 */
#include <stdint.h>
#ifndef SPIKE
#include "printf.h"
#else
#include <stdio.h>
#endif
#define STR_I(x) #x
#define STR(x) STR_I(x)
#ifndef V01_NFIELDS
#define V01_NFIELDS 2
#endif
#if V01_NFIELDS < 2 || V01_NFIELDS > 8
#error V01_NFIELDS_must_be_2_through_8
#endif
#define ELEMENTS 64
#define MEMORY_WORDS 32
#define CASES 16
#ifndef V01_FIRST
#define V01_FIRST 12
#endif
#ifndef V01_COUNT
#define V01_COUNT 1
#endif
#define HEADER_WORDS 12
#define CASE_WORDS (52 + V01_NFIELDS * ELEMENTS)
#define SIGNATURE_WORDS (HEADER_WORDS + CASES * CASE_WORDS)
#define SIGNATURE_BYTES (SIGNATURE_WORDS * 4)

/* Full actual destination and memory results, not hashes. UART exports only
 * summary/first error; Spike +signature and RTL CPU stores can retain all
 * words. Header:
 * magic/version/cases/NFIELDS/SEW/VL/vlenb/checks/errors/completed/
 *         record_words/signature_words.
 * Each record: 16 metadata words, NFIELDS*64 words from v8..v(7+NFIELDS),
 *              32 memory words and four guards. Vector results are field-major.
 * Metadata: id/store/indexEEW/masked/state, returnedVL(lo,hi), VL(lo,hi),
 * vtype(lo,hi), vstart(lo,hi), errors, indexVLMAX, active element count.
 * state: 0 not started, 1 started, 2 checked successfully, 3 checked with
 * errors.
 */
__asm__(".pushsection .data.v01_ordered_segment_signature,\"aw\",@progbits\n"
        ".balign 64\n"
#ifdef SPIKE
        ".global begin_signature\nbegin_signature:\n"
#endif
        ".global v01_ordered_segment_signature\n.type "
        "v01_ordered_segment_signature,@object\n"
        "v01_ordered_segment_signature:\n.zero " STR(
            SIGNATURE_BYTES) "\n"
                             ".size "
                             "v01_ordered_segment_signature,.-v01_ordered_"
                             "segment_signature\n"
#ifdef SPIKE
                             ".global end_signature\nend_signature:\n"
#endif
                             ".popsection\n");
extern volatile uint32_t v01_ordered_segment_signature[SIGNATURE_WORDS];

struct vector_words {
  uint32_t before, data[V01_NFIELDS * ELEMENTS], after;
};
struct memory_words {
  uint32_t before, data[MEMORY_WORDS], after;
};
struct indices {
  uint64_t before;
  uint8_t data[ELEMENTS * 8];
  uint64_t after;
};
struct mask_buffer {
  uint64_t before;
  uint8_t data[ELEMENTS / 8];
  uint64_t after;
};
struct fixture {
  struct vector_words old, out;
  struct memory_words memory;
  struct indices index;
  struct mask_buffer mask;
};
static struct fixture storage __attribute__((aligned(64)));
static unsigned checks, failures, completed;
/* First error is inspectable on both platforms, including when UART is absent.
 */
volatile uint64_t v01_ordered_segment_first_error[5];
struct result {
  uintptr_t returned, vl, vtype, vstart, first, index_max;
};

static void check(unsigned id, unsigned field, unsigned i, uint64_t actual,
                  uint64_t expected) {
  ++checks;
  if (actual != expected) {
    if (!failures) {
      v01_ordered_segment_first_error[0] = id;
      v01_ordered_segment_first_error[1] = field;
      v01_ordered_segment_first_error[2] = i;
      v01_ordered_segment_first_error[3] = actual;
      v01_ordered_segment_first_error[4] = expected;
#ifndef SPIKE
      printf("V01_ORDERED_SEG_FIRST_FAIL id=%u field=%u i=%u got=%08x:%08x "
             "want=%08x:%08x\n",
             id, field, i, (unsigned)(actual >> 32), (unsigned)actual,
             (unsigned)(expected >> 32), (unsigned)expected);
#endif
    }
    ++failures;
  }
}
static uint32_t old_value(unsigned id, unsigned field, unsigned i) {
  return UINT32_C(0x91400000) ^ (id * UINT32_C(0x00010001)) ^
         (field * UINT32_C(0x10001001)) ^ (i * UINT32_C(0x01010103));
}
static uint32_t memory_value(unsigned id, unsigned i) {
  return UINT32_C(0x52340000) ^ (id * UINT32_C(0x00010101)) ^
         (i * UINT32_C(0x01020407));
}
static uint32_t guard(unsigned id, unsigned which, unsigned after) {
  return UINT32_C(0x13579bdf) ^ (id << 16) ^ (which << 8) ^
         (after ? UINT32_C(0xffffffff) : 0);
}
static uint64_t index_guard(unsigned id, unsigned after) {
  return UINT64_C(0x123456789abcdef0) ^ ((uint64_t)id << 40) ^
         (after ? UINT64_C(0xffffffffffffffff) : 0);
}
static uint64_t mask_guard(unsigned id, unsigned after) {
  return UINT64_C(0x0fedcba987654321) ^ ((uint64_t)id << 32) ^
         (after ? UINT64_C(0xffffffffffffffff) : 0);
}
/* Indexed instructions use byte offsets, not element numbers. All offsets,
 * including inactive/tail entries, are initialized and stay inside memory.
 * Store indices repeat: masked mode 0xaa makes element 1 the final writer;
 * element 2 must not write. Unmasked mode makes element 2 the final writer.
 */
static unsigned offset(unsigned is_store, unsigned i) {
  if (is_store)
    return 8;
  if (i == 0)
    return 8;
  if (i == 1)
    return 0;
  if (i == 2)
    return 12;
  return 4 * (i % 16);
}
static uint8_t index_byte(unsigned id, unsigned is_store, unsigned eew,
                          unsigned byte) {
  const unsigned bytes = eew / 8;
  if (byte < ELEMENTS * bytes)
    return (uint8_t)((uint64_t)offset(is_store, byte / bytes) >>
                     (8 * (byte % bytes)));
  return (uint8_t)(0x69U ^ id ^ (byte * 37U));
}

/* Index register VLMAX is 64 in all four configurations. v4-v5 is aligned for
 * EMUL2 at indexEEW64; lower widths use mf4/mf2/m1 and only v4. Data v8 and
 * mask v0 are disjoint. Exact index EEW is loaded first, avoiding a deliberate
 * old-EEW reshuffle dependency in this addressing-mode test.
 */
#define FIELDS_2(A) A(v8, 0) A(v9, 256)
#define FIELDS_3(A) FIELDS_2(A) A(v10, 512)
#define FIELDS_4(A) FIELDS_3(A) A(v11, 768)
#define FIELDS_5(A) FIELDS_4(A) A(v12, 1024)
#define FIELDS_6(A) FIELDS_5(A) A(v13, 1280)
#define FIELDS_7(A) FIELDS_6(A) A(v14, 1536)
#define FIELDS_8(A) FIELDS_7(A) A(v15, 1792)
#define FIELD_LIST_I(N, A) FIELDS_##N(A)
#define FIELD_LIST(N, A) FIELD_LIST_I(N, A)
#define LOAD_FIELD(R, OFFSET)                                                  \
  "addi t1, %[old], " STR(OFFSET) "\n"                                         \
                                  "vle32.v " STR(R) ", (t1)\n"
#define STORE_FIELD(R, OFFSET)                                                 \
  "addi t1, %[out], " STR(OFFSET) "\n"                                         \
                                  "vse32.v " STR(R) ", (t1)\n"
#define EXEC(W, M, INSN)                                                       \
  __asm__ volatile(                                                            \
      "fence rw, rw\ncsrw vstart, zero\n"                                      \
      "vsetvli %[imax], zero, e" STR(W) ", " STR(                              \
          M) ", tu, mu\n"                                                      \
             "vle" STR(W) ".v v4, (%[index])\n"                                \
                          "vsetvli t0, zero, e32, m1, tu, mu\n" FIELD_LIST(    \
                              V01_NFIELDS,                                     \
                              LOAD_FIELD) "vlm.v v0, (%[mask])\n"              \
                                          "vsetvli %[returned], %[avl], e32, " \
                                          "m1, tu, mu\n" INSN                  \
                                          "\nfence rw, rw\n"                   \
                                          "csrr %[vl], vl\ncsrr %[vt], "       \
                                          "vtype\ncsrr %[vs], vstart\n"        \
                                          "vmv.x.s %[first], v8\n"             \
                                          "vsetvli t0, zero, e32, m1, tu, "    \
                                          "mu\n" FIELD_LIST(                   \
                                              V01_NFIELDS,                     \
                                              STORE_FIELD) "fence rw, rw\n"    \
      : [returned] "=&r"(r.returned), [vl] "=&r"(r.vl), [vt] "=&r"(r.vtype),   \
        [vs] "=&r"(r.vstart), [first] "=&r"(r.first),                          \
        [imax] "=&r"(r.index_max)                                              \
      : [index] "r"(p->index.data), [old] "r"(p->old.data),                    \
        [mask] "r"(p->mask.data), [memory] "r"(p->memory.data),                \
        [out] "r"(p->out.data), [avl] "r"((uintptr_t)3)                        \
      : "t0", "t1", "v0", "v4", "v5", "v8", "v9", "v10", "v11", "v12", "v13",  \
        "v14", "v15", "memory")
#define WIDTH_CASES(BASE, W, M)                                                \
  case BASE:                                                                   \
    EXEC(W, M,                                                                 \
         "vloxseg" STR(V01_NFIELDS) "ei" STR(W) ".v v8, (%[memory]), v4");     \
    break;                                                                     \
  case BASE + 1:                                                               \
    EXEC(W, M,                                                                 \
         "vloxseg" STR(V01_NFIELDS) "ei" STR(                                  \
             W) ".v v8, (%[memory]), v4, v0.t");                               \
    break;                                                                     \
  case BASE + 2:                                                               \
    EXEC(W, M,                                                                 \
         "vsoxseg" STR(V01_NFIELDS) "ei" STR(W) ".v v8, (%[memory]), v4");     \
    break;                                                                     \
  case BASE + 3:                                                               \
    EXEC(W, M,                                                                 \
         "vsoxseg" STR(V01_NFIELDS) "ei" STR(                                  \
             W) ".v v8, (%[memory]), v4, v0.t");                               \
    break

static struct result execute(unsigned id, volatile struct fixture *p) {
  struct result r = {0};
  switch (id) {
    WIDTH_CASES(0, 8, mf4);
    WIDTH_CASES(4, 16, mf2);
    WIDTH_CASES(8, 32, m1);
    WIDTH_CASES(12, 64, m2);
  }
  return r;
}
static void put64(unsigned position, uint64_t value) {
  v01_ordered_segment_signature[position] = (uint32_t)value;
  v01_ordered_segment_signature[position + 1] = (uint32_t)(value >> 32);
}
static void run(unsigned id) {
  const unsigned eew = 8U << (id / 4);
  const unsigned is_store = (id % 4) / 2;
  const unsigned masked = id & 1;
  const unsigned base = HEADER_WORDS + id * CASE_WORDS;
  const unsigned previous = failures;
  volatile struct fixture *p = (&storage);
  v01_ordered_segment_signature[base] = id;
  v01_ordered_segment_signature[base + 1] = is_store;
  v01_ordered_segment_signature[base + 2] = eew;
  v01_ordered_segment_signature[base + 3] = masked;
  v01_ordered_segment_signature[base + 4] = 1;
  for (unsigned f = 0; f < V01_NFIELDS; ++f)
    for (unsigned i = 0; i < ELEMENTS; ++i) {
      p->old.data[f * ELEMENTS + i] = old_value(id, f, i);
      p->out.data[f * ELEMENTS + i] = UINT32_C(0xcccccccc);
    }
  for (unsigned i = 0; i < MEMORY_WORDS; ++i)
    p->memory.data[i] = memory_value(id, i);
  p->old.before = guard(id, 0, 0);
  p->old.after = guard(id, 0, 1);
  p->memory.before = guard(id, 1, 0);
  p->memory.after = guard(id, 1, 1);
  p->out.before = guard(id, 2, 0);
  p->out.after = guard(id, 2, 1);
  p->index.before = index_guard(id, 0);
  p->index.after = index_guard(id, 1);
  p->mask.before = mask_guard(id, 0);
  p->mask.after = mask_guard(id, 1);
  for (unsigned i = 0; i < sizeof(p->index.data); ++i)
    p->index.data[i] = index_byte(id, is_store, eew, i);
  for (unsigned i = 0; i < sizeof(p->mask.data); ++i)
    p->mask.data[i] = masked ? UINT8_C(0xaa) : UINT8_C(0xff);

  const struct result r = execute(id, p);
  check(id, 0, 0, r.returned, 3);
  check(id, 1, 0, r.vl, 3);
  check(id, 2, 0, r.vtype, 16); /* SEW32/m1, tu/mu, vill=0. */
  check(id, 3, 0, r.vstart, 0);
  check(id, 4, 0, r.index_max, ELEMENTS);
  for (unsigned f = 0; f < V01_NFIELDS; ++f)
    for (unsigned i = 0; i < ELEMENTS; ++i) {
      const unsigned active = i < 3 && (!masked || (i & 1));
      const unsigned element = f * ELEMENTS + i;
      const uint32_t wanted = !is_store && active
                                  ? memory_value(id, offset(0, i) / 4 + f)
                                  : old_value(id, f, i);
      check(id, i < 3 ? 5 : 6, element, p->out.data[element], wanted);
      check(id, 8, element, p->old.data[element], old_value(id, f, i));
      if (element == 0)
        check(id, 9, 0, (uint32_t)r.first, wanted);
      v01_ordered_segment_signature[base + 16 + element] = p->out.data[element];
    }
  for (unsigned i = 0; i < MEMORY_WORDS; ++i) {
    const uint32_t wanted = is_store && i >= 2 && i < 2 + V01_NFIELDS
                                ? old_value(id, i - 2, masked ? 1 : 2)
                                : memory_value(id, i);
    check(id, 7, i, p->memory.data[i], wanted);
    v01_ordered_segment_signature[base + 16 + V01_NFIELDS * ELEMENTS + i] =
        p->memory.data[i];
  }
  check(id, 10, 0, p->old.before, guard(id, 0, 0));
  check(id, 11, 0, p->old.after, guard(id, 0, 1));
  check(id, 10, 1, p->memory.before, guard(id, 1, 0));
  check(id, 11, 1, p->memory.after, guard(id, 1, 1));
  check(id, 10, 2, p->out.before, guard(id, 2, 0));
  check(id, 11, 2, p->out.after, guard(id, 2, 1));
  check(id, 12, 0, p->index.before, index_guard(id, 0));
  check(id, 13, 0, p->index.after, index_guard(id, 1));
  check(id, 14, 0, p->mask.before, mask_guard(id, 0));
  check(id, 15, 0, p->mask.after, mask_guard(id, 1));
  for (unsigned i = 0; i < sizeof(p->index.data); ++i)
    check(id, 16, i, p->index.data[i], index_byte(id, is_store, eew, i));
  for (unsigned i = 0; i < sizeof(p->mask.data); ++i)
    check(id, 17, i, p->mask.data[i], masked ? UINT8_C(0xaa) : UINT8_C(0xff));
  put64(base + 5, r.returned);
  put64(base + 7, r.vl);
  put64(base + 9, r.vtype);
  put64(base + 11, r.vstart);
  v01_ordered_segment_signature[base + 13] = failures - previous;
  v01_ordered_segment_signature[base + 14] = (uint32_t)r.index_max;
  v01_ordered_segment_signature[base + 15] = masked ? 1 : 3;
  const unsigned guards = base + 16 + V01_NFIELDS * ELEMENTS + MEMORY_WORDS;
  v01_ordered_segment_signature[guards] = p->memory.before;
  v01_ordered_segment_signature[guards + 1] = p->memory.after;
  v01_ordered_segment_signature[guards + 2] = p->out.before;
  v01_ordered_segment_signature[guards + 3] = p->out.after;
  v01_ordered_segment_signature[base + 4] = failures == previous ? 2 : 3;
  ++completed;
}

int main(void) {
  uintptr_t vlenb;
  __asm__ volatile("csrw vstart, zero" ::: "memory");
  __asm__ volatile("csrr %0, vlenb" : "=r"(vlenb));
  for (unsigned i = 0; i < SIGNATURE_WORDS; ++i)
    v01_ordered_segment_signature[i] = 0;
  v01_ordered_segment_signature[0] = UINT32_C(0x56314f53);
  v01_ordered_segment_signature[1] = 1;
  v01_ordered_segment_signature[2] = CASES;
  v01_ordered_segment_signature[3] = V01_NFIELDS;
  v01_ordered_segment_signature[4] = 32;
  v01_ordered_segment_signature[5] = 3;
  v01_ordered_segment_signature[6] = (uint32_t)vlenb;
  v01_ordered_segment_signature[10] = CASE_WORDS;
  v01_ordered_segment_signature[11] = SIGNATURE_WORDS;
  check(CASES, 18, 0, vlenb, 256);
  check(CASES, 19, 0, ((uintptr_t)&storage % 64) == 0, 1);
  if (!failures)
    for (unsigned id = V01_FIRST; id < V01_FIRST + V01_COUNT; ++id)
      run(id);
  check(CASES, 20, 0, completed, V01_COUNT);
  v01_ordered_segment_signature[7] = checks;
  v01_ordered_segment_signature[8] = failures;
  v01_ordered_segment_signature[9] = completed;
  __asm__ volatile("fence rw, rw" : : : "memory");
#ifndef SPIKE
  printf("V01_ORDERED_SEG_DONE nf=%u cases=%u checks=%u errors=%u\n",
         V01_NFIELDS, completed, checks, failures);
#endif
  return failures ? 96 : 0;
}
