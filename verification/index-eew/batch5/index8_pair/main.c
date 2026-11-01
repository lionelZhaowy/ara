/* SPDX-License-Identifier: Apache-2.0
 * Two consecutive ordered index8 loads, SEW32/m1, VL5.
 * Portable input for the official Ara app runtime.
 * AI tools assisted with this test preparation.
 */
#include <stdint.h>
#ifndef SPIKE
#include "printf.h"
#else
#include <stdio.h>
#endif

struct buffer {
  uint32_t before;
  uint32_t data[64];
  uint32_t after;
};
/* One complete model signature: observations, status, all six
 * index8_pair_buffers including guards, both alignment gaps, and the full index
 * register image. */
__asm__(".pushsection .data.index8_pair,\"aw\",@progbits\n"
        ".balign 64\n"
#ifdef SPIKE
        ".global begin_signature\nbegin_signature:\n"
#endif
        ".global index8_pair_observed\n.type index8_pair_observed,@object\n"
        "index8_pair_observed:\n.zero 1536\n.size index8_pair_observed,1536\n"
        ".global index8_pair_status\n.type index8_pair_status,@object\n"
        "index8_pair_status:\n.zero 48\n.size index8_pair_status,48\n"
        ".balign 64\n"
        ".global index8_pair_buffers\n.type index8_pair_buffers,@object\n"
        "index8_pair_buffers:\n.zero 1584\n.size index8_pair_buffers,1584\n"
        ".balign 64\n"
        ".global index8_pair_offsets\n.type index8_pair_offsets,@object\n"
        "index8_pair_offsets:\n.zero 256\n.size index8_pair_offsets,256\n"
#ifdef SPIKE
        ".global end_signature\nend_signature:\n"
#endif
        ".popsection\n");
extern struct buffer index8_pair_buffers[6];
extern uint8_t index8_pair_offsets[256];
extern volatile uint32_t index8_pair_observed[6 * 64];
extern volatile uint64_t index8_pair_status[6];

static uint32_t value(unsigned buffer, unsigned element) {
  return UINT32_C(0x53790000) ^ (buffer * UINT32_C(0x00110203)) ^
         (element * UINT32_C(0x01020407));
}

static uint8_t offset_value(unsigned i) {
  const uint8_t first[5] = {0, 12, 4, 16, 8};
  return i < 5 ? first[i] : (uint8_t)((i % 64) * 4);
}

int main(void) {
  uintptr_t returned, vl, vt, vs, vlenb;
  unsigned errors = 0;
  __asm__ volatile("csrw vstart, zero" ::: "memory");
  __asm__ volatile("csrr %0, vlenb" : "=r"(vlenb));
  if (vlenb != 256)
    return 2;
  for (unsigned b = 0; b < 6; ++b) {
    index8_pair_buffers[b].before = UINT32_C(0x13579bdf) ^ b;
    index8_pair_buffers[b].after = UINT32_C(0xeca86420) ^ b;
    for (unsigned i = 0; i < 64; ++i)
      index8_pair_buffers[b].data[i] = value(b, i);
  }
  for (unsigned i = 0; i < 256; ++i)
    index8_pair_offsets[i] = offset_value(i);
  __asm__ volatile(
      "fence rw, rw\n"
      "vsetvli t0, zero, e32, m1, tu, mu\n"
      "vle32.v v8, (%[old0])\nvle32.v v10, (%[old1])\n"
      "vsetvli t0, zero, e8, m1, tu, mu\nvle8.v v4, (%[index])\n"
      "vsetivli %[returned], 5, e32, m1, tu, mu\n"
      "vloxei8.v v8, (%[source0]), v4\n"
      "vloxei8.v v10, (%[source1]), v4\n"
      "csrr %[vl], vl\ncsrr %[vt], vtype\ncsrr %[vs], vstart\n"
      "vsetvli t0, zero, e32, m1, tu, mu\n"
      "vse32.v v8, (%[out0])\nvse32.v v10, (%[out1])\nfence rw, rw\n"
      :
      [returned] "=&r"(returned), [vl] "=&r"(vl), [vt] "=&r"(vt), [vs] "=&r"(vs)
      : [old0] "r"(index8_pair_buffers[2].data),
        [old1] "r"(index8_pair_buffers[3].data),
        [source0] "r"(index8_pair_buffers[0].data),
        [source1] "r"(index8_pair_buffers[1].data),
        [out0] "r"(index8_pair_buffers[4].data),
        [out1] "r"(index8_pair_buffers[5].data),
        [index] "r"(index8_pair_offsets)
      : "t0", "v4", "v8", "v10", "memory");
  errors += returned != 5;
  errors += vl != 5;
  errors += vt != 16;
  errors += vs != 0;
  for (unsigned i = 0; i < 256; ++i)
    errors += index8_pair_offsets[i] != offset_value(i);
  for (unsigned b = 0; b < 6; ++b) {
    errors += index8_pair_buffers[b].before != (UINT32_C(0x13579bdf) ^ b);
    errors += index8_pair_buffers[b].after != (UINT32_C(0xeca86420) ^ b);
    for (unsigned i = 0; i < 64; ++i) {
      uint32_t want = value(b, i);
      if (b >= 4)
        want = i < 5 ? value(b - 4, offset_value(i) / 4) : value(b - 2, i);
      index8_pair_observed[b * 64 + i] = index8_pair_buffers[b].data[i];
      errors += index8_pair_buffers[b].data[i] != want;
    }
  }
  index8_pair_status[0] = returned;
  index8_pair_status[1] = vl;
  index8_pair_status[2] = vt;
  index8_pair_status[3] = vs;
  index8_pair_status[4] = errors;
  index8_pair_status[5] = 1;
  __asm__ volatile("fence rw, rw" ::: "memory");
  printf("INDEX8_PAIR_DONE errors=%u completed=1\n", errors);
  return errors != 0;
}
