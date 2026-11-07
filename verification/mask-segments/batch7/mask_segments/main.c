/* Draft for public review. Adapted input: no previous ELF result applies.
 * AI tools assisted with this adaptation.
 */
/* Masked segment input for the official Ara app runtime.
 * Three phases per ELF: original mask, inverse mask, then unmasked; m1/vstart0.
 * Full field images, memory, mask/index checks, guards and signature retained.
 * This independent NF2 cross-word fixture does not replace the NF5 failure.
 * SPDX-License-Identifier: Apache-2.0
 */
#include <stdint.h>
#ifndef SPIKE
#include "printf.h"
#else
#include <stdio.h>
#endif
#define STR_I(x) #x
#define STR(x) STR_I(x)
/* Exactly two diagnostic shapes, independently built for unit load/store. */
#ifndef V01_FAMILY
#error V01_FAMILY_required
#endif
#if V01_FAMILY != 1 && V01_FAMILY != 2
#error diagnostic_requires_unit_load_or_store
#endif
#ifndef V01_NF5_E64_VL3
#define V01_NF5_E64_VL3 0
#endif
#ifndef V01_NF2_E8_VL129
#define V01_NF2_E8_VL129 0
#endif
#if (V01_NF5_E64_VL3 != 0 && V01_NF5_E64_VL3 != 1) ||                          \
    (V01_NF2_E8_VL129 != 0 && V01_NF2_E8_VL129 != 1) ||                        \
    (V01_NF5_E64_VL3 + V01_NF2_E8_VL129 != 1)
#error select_exactly_one_fixed_diagnostic
#endif
#if V01_NF5_E64_VL3
#define V01_FIXED_NFIELDS 5
#define V01_SELECTED_VL 3
#define V01_SELECTED_CASE 7
#else
#define V01_FIXED_NFIELDS 2
#define V01_SELECTED_VL 129
#define V01_SELECTED_CASE 1
#endif
#ifndef V01_NFIELDS
#define V01_NFIELDS V01_FIXED_NFIELDS
#endif
#if V01_NFIELDS != V01_FIXED_NFIELDS
#error NF_must_match_the_selected_fixed_diagnostic
#endif
#ifndef V01_EEW_MASK
#define V01_EEW_MASK 15
#endif
#if V01_EEW_MASK != 15
#error preserve_original_compiled_EEW_mask
#endif
#define IS_STORE ((V01_FAMILY % 2) == 0)
#define IS_INDEXED (V01_FAMILY >= 5)
#define REGISTER_BYTES 256
#define MEMORY_BYTES 512
#define INDEX_BYTES 512
#define MASK_BYTES 256
#define WIDTHS                                                                 \
  (((V01_EEW_MASK >> 0) & 1) + ((V01_EEW_MASK >> 1) & 1) +                     \
   ((V01_EEW_MASK >> 2) & 1) + ((V01_EEW_MASK >> 3) & 1))
#define CASES 3
#define HEADER_WORDS 16
#define METADATA_WORDS 16
#define GUARD_WORDS 28
#define CASE_WORDS (METADATA_WORDS + V01_NFIELDS * 64 + 128 + GUARD_WORDS)
#define FOOTER_WORDS 10
#define SIGNATURE_WORDS (HEADER_WORDS + CASES * CASE_WORDS + FOOTER_WORDS)
#define SIGNATURE_BYTES (4 * SIGNATURE_WORDS)

/* Header words: magic,version,family,NF,EEWmask,case_count,vlenb,checks,
 * errors,completed,record_words,signature_words,VL,reserved,reserved,reserved.
 * Record: 16 metadata words, NF full 256-byte register images, 512 memory
 * bytes, seven before/after uint64 guard pairs. Metadata: id,EEW,SEW,masked,
 * state,errors, returnedVL(lo,hi),VL(lo,hi),vtype(lo,hi),vstart(lo,hi),
 * dataVLMAX,indexVLMAX. State 1=start,2=checked,3=checked with errors.
 * Footer is the first error's five uint64 values (id,field,byte,got,want).
 * All payload bytes are packed little-endian in words; no result hashes.
 */
__asm__(
    ".pushsection .data.v01_memory_gap_signature,\"aw\",@progbits\n"
    ".balign 64\n"
#ifdef SPIKE
    ".global begin_signature\nbegin_signature:\n"
#endif
    ".global v01_memory_gap_signature\n.type v01_memory_gap_signature,@object\n"
    "v01_memory_gap_signature:\n.zero " STR(
        SIGNATURE_BYTES) "\n"
                         ".size "
                         "v01_memory_gap_signature,.-v01_memory_gap_signature\n"
#ifdef SPIKE
                         ".global end_signature\nend_signature:\n"
#endif
                         ".popsection\n");
extern volatile uint32_t v01_memory_gap_signature[SIGNATURE_WORDS];

struct register_image {
  uint64_t before;
  uint8_t data[V01_NFIELDS * REGISTER_BYTES];
  uint64_t after;
};
struct memory_image {
  uint64_t before;
  uint8_t data[MEMORY_BYTES];
  uint64_t after;
};
struct index_image {
  uint64_t before;
  uint8_t data[INDEX_BYTES];
  uint64_t after;
};
struct mask_image {
  uint64_t before;
  uint8_t data[MASK_BYTES];
  uint64_t after;
};
struct fixture {
  struct register_image old, out;
  struct memory_image memory;
  struct index_image index, index_out;
  struct mask_image mask, mask_out;
};
static struct fixture storage __attribute__((aligned(64)));
static unsigned checks, failures, completed;
volatile uint64_t v01_memory_gap_first_error[5];
/* Passive observers may read this symbol. It never controls the DUT. */
volatile uint32_t v01_memory_gap_phase = UINT32_MAX;
struct result {
  uintptr_t returned, vl, vt, vs, data_max, index_max;
};
_Static_assert(REGISTER_BYTES == 2048 / 8, "fixed Ara VLEN");
_Static_assert(SIGNATURE_BYTES <= 22000, "bounded public signature");
_Static_assert(sizeof(struct fixture) <= 6400, "bounded fixture");
_Static_assert(__builtin_offsetof(struct register_image, data) % 8 == 0,
               "vle64 alignment");
_Static_assert(__builtin_offsetof(struct memory_image, data) % 8 == 0,
               "memory alignment");

static void check(unsigned id, unsigned field, unsigned byte, uint64_t got,
                  uint64_t want) {
  ++checks;
  if (got != want) {
    if (!failures) {
      v01_memory_gap_first_error[0] = id;
      v01_memory_gap_first_error[1] = field;
      v01_memory_gap_first_error[2] = byte;
      v01_memory_gap_first_error[3] = got;
      v01_memory_gap_first_error[4] = want;
#ifndef SPIKE
      printf("V01_MEMGAP_FIRST_FAIL family=%u nf=%u id=%u field=%u byte=%u "
             "got=%08x:%08x want=%08x:%08x\n",
             V01_FAMILY, V01_NFIELDS, id, field, byte, (unsigned)(got >> 32),
             (unsigned)got, (unsigned)(want >> 32), (unsigned)want);
#endif
    }
    ++failures;
  }
}
static uint8_t old_byte(unsigned id, unsigned field, unsigned byte) {
  return (uint8_t)(0x91U ^ (id * 37U) ^ (field * 29U) ^ (byte * 11U) ^
                   (byte >> 3));
}
static uint8_t memory_byte(unsigned id, unsigned byte) {
  return (uint8_t)(0x53U ^ (id * 19U) ^ (byte * 23U) ^ (byte >> 2));
}
/* Same explicit mask bytes feed initialization, complete mask readback and
 * the independent per-element activity oracle. Tail selection short-circuits
 * before any mask-bit lookup. The e8 case changes its second 128-bit word.
 */
static uint8_t original_mask_byte(unsigned byte) {
#if V01_NF5_E64_VL3
  (void)byte;
  return UINT8_C(0xa6);
#else
  return byte < 16 ? UINT8_C(0xa6)
                   : (byte < 32 ? UINT8_C(0x59) : UINT8_C(0xc9));
#endif
}
/* Compute the inverse with scalar C. Do not insert a MASKU instruction. */
static uint8_t diagnostic_mask_byte(unsigned phase, unsigned byte) {
  const uint8_t original = original_mask_byte(byte);
  return phase == 0   ? original
         : phase == 1 ? (uint8_t)~original
                      : UINT8_C(0xff);
}
static unsigned diagnostic_active(unsigned phase, unsigned element,
                                  unsigned masked) {
  return element < V01_SELECTED_VL &&
         (!masked ||
          ((diagnostic_mask_byte(phase, element / 8) >> (element % 8)) & 1U));
}
static uint64_t guard(unsigned id, unsigned which, unsigned after) {
  return UINT64_C(0x123456789abcdef0) ^ ((uint64_t)id << 40) ^
         ((uint64_t)which * UINT64_C(0x0102030405060708)) ^
         (after ? UINT64_C(0xffffffffffffffff) : 0);
}
/* Bytes between fields of one segment and between different segments never
 * overlap. The three active offsets fit even indexEEW8 at NF8. Tail indices
 * also stay in bounds so wrong-tail writes have an exact in-fixture oracle.
 */
static unsigned index_offset(unsigned element) {
  return (element % 3) * (V01_NFIELDS + 1) * 4;
}
static uint8_t index_byte(unsigned eew, unsigned byte) {
  const unsigned bytes = eew / 8;
  return (uint8_t)((uint64_t)index_offset(byte / bytes) >>
                   (8 * (byte % bytes)));
}
static unsigned element_address(unsigned eew, unsigned element,
                                unsigned field) {
  const unsigned bytes = IS_INDEXED ? 4 : eew / 8;
#if V01_FAMILY <= 2
  return (element * V01_NFIELDS + field) * bytes;
#elif V01_FAMILY <= 4
  return (element * (V01_NFIELDS + 1) + field) * bytes;
#else
  return index_offset(element) + field * bytes;
#endif
}

#define FIELDS_2(A, W) A(v8, 0, W) A(v9, 256, W)
#define FIELDS_3(A, W) FIELDS_2(A, W) A(v10, 512, W)
#define FIELDS_4(A, W) FIELDS_3(A, W) A(v11, 768, W)
#define FIELDS_5(A, W) FIELDS_4(A, W) A(v12, 1024, W)
#define FIELDS_6(A, W) FIELDS_5(A, W) A(v13, 1280, W)
#define FIELDS_7(A, W) FIELDS_6(A, W) A(v14, 1536, W)
#define FIELDS_8(A, W) FIELDS_7(A, W) A(v15, 1792, W)
#define FIELD_LIST_I(N, A, W) FIELDS_##N(A, W)
#define FIELD_LIST(N, A, W) FIELD_LIST_I(N, A, W)
#define LOAD_FIELD(R, OFFSET, W)                                               \
  "addi t1, %[old], " STR(OFFSET) "\n"                                         \
                                  "vle" STR(W) ".v " STR(R) ", (t1)\n"
#define STORE_FIELD(R, OFFSET, W)                                              \
  "addi t1, %[out], " STR(OFFSET) "\n"                                         \
                                  "vse" STR(W) ".v " STR(R) ", (t1)\n"
#if IS_INDEXED
#define INDEX_PREP(W, M)                                                       \
  "vsetvli t0, zero, e" STR(                                                   \
      W) ", m1, tu, mu\n"                                                      \
         "vle" STR(W) ".v v4, (%[index])\n"                                    \
                      "addi t1, %[index], 256\n"                               \
                      "vle" STR(W) ".v v5, (t1)\n"                             \
                                   "vsetvli %[imax], zero, e" STR(W) ", " STR( \
                                       M) ", tu, mu\n"
#define INDEX_SAVE(W)                                                          \
  "vsetvli t0, zero, e" STR(W) ", m1, tu, mu\n"                                \
                               "vse" STR(W) ".v v4, (%[index_out])\n"          \
                                            "addi t1, %[index_out], 256\n"     \
                                            "vse" STR(W) ".v v5, (t1)\n"
#define DATA_WIDTH(W) 32
#else
#define INDEX_PREP(W, M) "li %[imax], 0\n"
#define INDEX_SAVE(W) ""
#define DATA_WIDTH(W) W
#endif
#if V01_FAMILY == 1
#define TARGET(W) "vlseg" STR(V01_NFIELDS) "e" STR(W) ".v v8, (%[memory])"
#elif V01_FAMILY == 2
#define TARGET(W) "vsseg" STR(V01_NFIELDS) "e" STR(W) ".v v8, (%[memory])"
#elif V01_FAMILY == 3
#define TARGET(W)                                                              \
  "vlsseg" STR(V01_NFIELDS) "e" STR(W) ".v v8, (%[memory]), %[stride]"
#elif V01_FAMILY == 4
#define TARGET(W)                                                              \
  "vssseg" STR(V01_NFIELDS) "e" STR(W) ".v v8, (%[memory]), %[stride]"
#elif V01_FAMILY == 5
#define TARGET(W)                                                              \
  "vluxseg" STR(V01_NFIELDS) "ei" STR(W) ".v v8, (%[memory]), v4"
#else
#define TARGET(W)                                                              \
  "vsuxseg" STR(V01_NFIELDS) "ei" STR(W) ".v v8, (%[memory]), v4"
#endif
#define EXEC(W, M, SUFFIX)                                                     \
  __asm__ volatile(                                                            \
      "fence rw, rw\ncsrw vstart, zero\n"                                      \
      "vsetvli t0, zero, e8, m1, tu, mu\n"                                     \
      "vle8.v v0, (%[mask])\n"                                                 \
      "li t1, 23\nvmv.v.x v1, t1\n"                                            \
      "li t1, 59\nvmv.v.x v2, t1\n"                                            \
      "li t1, 109\nvmv.v.x v3, t1\n"                                           \
      "li t1, 225\nvmv.v.x v4, t1\n" INDEX_PREP(                               \
          W, M) "vsetvli %[dmax], "                                            \
                "zero, e" STR(DATA_WIDTH(W)) ", m1, tu, mu\n" FIELD_LIST(      \
                    V01_NFIELDS, LOAD_FIELD,                                   \
                    DATA_WIDTH(W)) "vsetvli "                                  \
                                   "%["                                        \
                                   "returned],"                                \
                                   " %[avl], "                                 \
                                   "e" STR(DATA_WIDTH(                         \
                                       W)) ", m1, tu, mu\n" TARGET(W) SUFFIX   \
      "\n"                                                                     \
      "fence rw, rw\n"                                                         \
      "csrr %[vl], vl\ncsrr %[vt], vtype\ncsrr %[vs], vstart\n"                \
      "vsetvli t0, zero, e" STR(DATA_WIDTH(W)) ", m1, tu, mu\n" FIELD_LIST(    \
          V01_NFIELDS, STORE_FIELD, DATA_WIDTH(W))                             \
          INDEX_SAVE(W) "vsetvli t0, zero, e8, m1, tu, mu\n"                   \
                        "vse8.v v0, (%[mask_out])\n"                           \
                        "fence rw, rw\n"                                       \
      : [returned] "=&r"(r.returned), [vl] "=&r"(r.vl), [vt] "=&r"(r.vt),      \
        [vs] "=&r"(r.vs), [dmax] "=&r"(r.data_max), [imax] "=&r"(r.index_max)  \
      : [old] "r"(p->old.data), [out] "r"(p->out.data),                        \
        [memory] "r"(p->memory.data), [mask] "r"(p->mask.data),                \
        [mask_out] "r"(p->mask_out.data), [index] "r"(p->index.data),          \
        [index_out] "r"(p->index_out.data),                                    \
        [avl] "r"((uintptr_t)V01_SELECTED_VL),                                 \
        [stride] "r"((uintptr_t)((V01_NFIELDS + 1) * (W / 8)))                 \
      : "t0", "t1", "v0", "v1", "v2", "v3", "v4", "v5", "v8", "v9", "v10",     \
        "v11", "v12", "v13", "v14", "v15", "memory")
#define WIDTH_CASE(BIT, W, M)                                                  \
  case 2 * BIT:                                                                \
    EXEC(W, M, "");                                                            \
    break;                                                                     \
  case 2 * BIT + 1:                                                            \
    EXEC(W, M, ", v0.t");                                                      \
    break
static struct result execute(unsigned id, volatile struct fixture *p) {
  struct result r = {0};
  switch (id) {
#if V01_EEW_MASK & 1
    WIDTH_CASE(0, 8, mf4);
#endif
#if V01_EEW_MASK & 2
    WIDTH_CASE(1, 16, mf2);
#endif
#if V01_EEW_MASK & 4
    WIDTH_CASE(2, 32, m1);
#endif
#if V01_EEW_MASK & 8
    WIDTH_CASE(3, 64, m2);
#endif
  }
  return r;
}
static void put64(unsigned word, uint64_t value) {
  v01_memory_gap_signature[word] = (uint32_t)value;
  v01_memory_gap_signature[word + 1] = (uint32_t)(value >> 32);
}
static void save_bytes(unsigned word, volatile const uint8_t *bytes,
                       unsigned size) {
  for (unsigned i = 0; i < size; i += 4)
    v01_memory_gap_signature[word + i / 4] =
        (uint32_t)bytes[i] | ((uint32_t)bytes[i + 1] << 8) |
        ((uint32_t)bytes[i + 2] << 16) | ((uint32_t)bytes[i + 3] << 24);
}
static void run(unsigned phase) {
  const unsigned id = V01_SELECTED_CASE + 8 * phase;
  const unsigned eew = 8U << (V01_SELECTED_CASE / 2), masked = phase < 2;
  v01_memory_gap_phase = phase;
  const unsigned sew = IS_INDEXED ? 32 : eew, bytes = sew / 8;
  const unsigned base = HEADER_WORDS + completed * CASE_WORDS;
  const unsigned previous = failures;
  volatile struct fixture *p = (&storage);
  v01_memory_gap_signature[base] = id;
  v01_memory_gap_signature[base + 1] = eew;
  v01_memory_gap_signature[base + 2] = sew;
  v01_memory_gap_signature[base + 3] = masked;
  v01_memory_gap_signature[base + 4] = 1;
  for (unsigned f = 0; f < V01_NFIELDS; ++f)
    for (unsigned i = 0; i < REGISTER_BYTES; ++i) {
      p->old.data[f * REGISTER_BYTES + i] = old_byte(id, f, i);
      p->out.data[f * REGISTER_BYTES + i] = UINT8_C(0xcc);
    }
  for (unsigned i = 0; i < MEMORY_BYTES; ++i)
    p->memory.data[i] = memory_byte(id, i);
  for (unsigned i = 0; i < INDEX_BYTES; ++i) {
    p->index.data[i] = index_byte(eew, i);
    p->index_out.data[i] = UINT8_C(0xcc);
  }
  for (unsigned i = 0; i < MASK_BYTES; ++i) {
    p->mask.data[i] = diagnostic_mask_byte(phase, i);
    p->mask_out.data[i] = UINT8_C(0xcc);
  }
#define INIT_GUARD(MEMBER, WHICH)                                              \
  p->MEMBER.before = guard(id, WHICH, 0);                                      \
  p->MEMBER.after = guard(id, WHICH, 1)
  INIT_GUARD(old, 0);
  INIT_GUARD(out, 1);
  INIT_GUARD(memory, 2);
  INIT_GUARD(index, 3);
  INIT_GUARD(index_out, 4);
  INIT_GUARD(mask, 5);
  INIT_GUARD(mask_out, 6);
#undef INIT_GUARD

#ifndef SPIKE
  printf("V01_MEMGAP_PHASE_BEGIN phase=%u id=%u family=%u nf=%u masked=%u\n",
         phase, id, V01_FAMILY, V01_NFIELDS, masked);
#endif
  /* Every setup/export instruction is unmasked. No reset or queue clear. */
  const struct result r = execute(V01_SELECTED_CASE - (masked ? 0 : 1), p);
  check(id, 0, 0, r.returned, V01_SELECTED_VL);
  check(id, 1, 0, r.vl, V01_SELECTED_VL);
  check(id, 2, 0, r.vt, (IS_INDEXED ? 2 : V01_SELECTED_CASE / 2) * 8);
  check(id, 3, 0, r.vs, 0);
  check(id, 4, 0, r.data_max, REGISTER_BYTES / bytes);
  check(id, 5, 0, r.index_max, IS_INDEXED ? 64 : 0);
  for (unsigned f = 0; f < V01_NFIELDS; ++f)
    for (unsigned i = 0; i < REGISTER_BYTES; ++i) {
      const unsigned element = i / bytes, subbyte = i % bytes;
      const unsigned active = diagnostic_active(phase, element, masked);
      const uint8_t wanted =
          !IS_STORE && active
              ? memory_byte(id, element_address(eew, element, f) + subbyte)
              : old_byte(id, f, i);
      check(id, active ? 6 : 7, f * REGISTER_BYTES + i,
            p->out.data[f * REGISTER_BYTES + i], wanted);
      check(id, 8, f * REGISTER_BYTES + i, p->old.data[f * REGISTER_BYTES + i],
            old_byte(id, f, i));
    }
  for (unsigned i = 0; i < MEMORY_BYTES; ++i) {
    uint8_t wanted = memory_byte(id, i);
    if (IS_STORE)
      for (unsigned e = 0; e < V01_SELECTED_VL; ++e)
        if (diagnostic_active(phase, e, masked))
          for (unsigned f = 0; f < V01_NFIELDS; ++f) {
            const unsigned address = element_address(eew, e, f);
            if (i >= address && i < address + bytes)
              wanted = old_byte(id, f, e * bytes + i - address);
          }
    check(id, 9, i, p->memory.data[i], wanted);
  }
  for (unsigned i = 0; i < INDEX_BYTES; ++i) {
    check(id, 10, i, p->index.data[i], index_byte(eew, i));
    check(id, 11, i, p->index_out.data[i],
          IS_INDEXED ? index_byte(eew, i) : UINT8_C(0xcc));
  }
  for (unsigned i = 0; i < MASK_BYTES; ++i) {
    check(id, 12, i, p->mask.data[i], diagnostic_mask_byte(phase, i));
    check(id, 13, i, p->mask_out.data[i], diagnostic_mask_byte(phase, i));
  }
  unsigned guards = base + METADATA_WORDS + V01_NFIELDS * 64 + 128;
#define CHECK_GUARD(MEMBER, WHICH)                                             \
  check(id, 14, 2 * WHICH, p->MEMBER.before, guard(id, WHICH, 0));             \
  check(id, 14, 2 * WHICH + 1, p->MEMBER.after, guard(id, WHICH, 1));          \
  put64(guards + 4 * WHICH, p->MEMBER.before);                                 \
  put64(guards + 4 * WHICH + 2, p->MEMBER.after)
  CHECK_GUARD(old, 0);
  CHECK_GUARD(out, 1);
  CHECK_GUARD(memory, 2);
  CHECK_GUARD(index, 3);
  CHECK_GUARD(index_out, 4);
  CHECK_GUARD(mask, 5);
  CHECK_GUARD(mask_out, 6);
#undef CHECK_GUARD
  save_bytes(base + METADATA_WORDS, p->out.data, V01_NFIELDS * REGISTER_BYTES);
  save_bytes(base + METADATA_WORDS + V01_NFIELDS * 64, p->memory.data,
             MEMORY_BYTES);
  v01_memory_gap_signature[base + 5] = failures - previous;
  put64(base + 6, r.returned);
  put64(base + 8, r.vl);
  put64(base + 10, r.vt);
  put64(base + 12, r.vs);
  v01_memory_gap_signature[base + 14] = (uint32_t)r.data_max;
  v01_memory_gap_signature[base + 15] = (uint32_t)r.index_max;
  v01_memory_gap_signature[base + 4] = failures == previous ? 2 : 3;
  ++completed;
#ifndef SPIKE
  printf(
      "V01_MEMGAP_PHASE_DONE phase=%u id=%u checks=%u errors=%u completed=%u\n",
      phase, id, checks, failures - previous, completed);
#endif
}
int main(void) {
  uintptr_t vlenb;
  __asm__ volatile("csrw vstart, zero" ::: "memory");
  __asm__ volatile("csrr %0, vlenb" : "=r"(vlenb));
  for (unsigned i = 0; i < SIGNATURE_WORDS; ++i)
    v01_memory_gap_signature[i] = 0;
  v01_memory_gap_signature[0] = UINT32_C(0x56314d47);
  v01_memory_gap_signature[1] = 2;
  v01_memory_gap_signature[2] = V01_FAMILY;
  v01_memory_gap_signature[3] = V01_NFIELDS;
  v01_memory_gap_signature[4] = V01_EEW_MASK;
  v01_memory_gap_signature[5] = CASES;
  v01_memory_gap_signature[6] = (uint32_t)vlenb;
  v01_memory_gap_signature[10] = CASE_WORDS;
  v01_memory_gap_signature[11] = SIGNATURE_WORDS;
  v01_memory_gap_signature[12] = V01_SELECTED_VL;
  check(8, 15, 0, vlenb, REGISTER_BYTES);
  check(8, 16, 0, (uintptr_t)&storage % 64 == 0, 1);
  if (!failures) {
    /* A completed numerical failure does not suppress later phases. */
    for (unsigned phase = 0; phase < CASES; ++phase)
      run(phase);
  }
#ifndef SPIKE
  else
    printf("V01_MEMGAP_ENVIRONMENT_FAIL checks=%u errors=%u\n", checks,
           failures);
#endif
  check(8, 17, 0, completed, CASES);
  v01_memory_gap_signature[7] = checks;
  v01_memory_gap_signature[8] = failures;
  v01_memory_gap_signature[9] = completed;
  for (unsigned i = 0; i < 5; ++i)
    put64(HEADER_WORDS + CASES * CASE_WORDS + 2 * i,
          v01_memory_gap_first_error[i]);
  __asm__ volatile("fence rw, rw" ::: "memory");
#ifndef SPIKE
  printf("V01_MEMGAP_DONE family=%u nf=%u eewmask=%u cases=%u checks=%u "
         "errors=%u\n",
         V01_FAMILY, V01_NFIELDS, V01_EEW_MASK, completed, checks, failures);
#endif
  return failures ? 97 : 0;
}
