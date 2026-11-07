// SPDX-License-Identifier: Apache-2.0
#include <stdint.h>
#include "printf.h"
struct region { uint64_t before[8], data[256], after[8]; };
static volatile struct region source, destination, output;
static unsigned checks, errors;
static uint8_t pattern(unsigned b, unsigned i, unsigned id) {
  return (uint8_t)(0x31u + b * 71u + i * 29u + (i >> 3) * 17u + id * 23u);
}
static void check(uint64_t actual, uint64_t expected) {
  ++checks;
  if (actual != expected) ++errors;
}
static void initialize(unsigned id) {
  volatile struct region *regions[] = {&source, &destination, &output};
  for (unsigned b = 0; b < 3; ++b) {
    for (unsigned i = 0; i < 8; ++i) {
      regions[b]->before[i] = UINT64_C(0x13579bdf2468ace0) ^ (b * 17u + i);
      regions[b]->after[i] = UINT64_C(0xfedcba9876543210) ^ (b * 17u + i);
    }
    for (unsigned i = 0; i < 256; ++i) {
      uint64_t value = 0;
      for (unsigned j = 0; j < 8; ++j)
        value |= (uint64_t)pattern(b, i * 8u + j, id) << (j * 8u);
      regions[b]->data[i] = value;
    }
  }
}
#define EXEC(OP)                                                       \
  asm volatile("csrw vstart, zero\nli t0, 512\n"                             \
               "vsetvli t0, t0, e16, m4, tu, mu\n"                           \
               "vle16.v v16, (%[src])\ncsrr t0, vl\n"                        \
               "li t0, 256\nvsetvli t0, t0, e8, m1, tu, mu\n"               \
               "vle8.v v17, (%[s1])\ncsrr t0, vl\n"                         \
               "vle8.v v18, (%[s2])\ncsrr t0, vl\n"                         \
               "vle8.v v19, (%[s3])\ncsrr t0, vl\n"                         \
               "li t0, 512\nvsetvli t0, t0, e32, m8, tu, mu\n"               \
               "vle32.v v24, (%[old])\ncsrr t0, vl\n"                        \
               OP "\ncsrr %[after], vl\ncsrr %[start], vstart\n"             \
               "csrr %[vl], vl\nvs8r.v v24, (%[out])\nfence rw, rw\n"         \
               : [vl] "=&r"(vl), [after] "=&r"(after), [start] "=&r"(start)    \
               : [old] "r"(destination.data), [src] "r"(source.data),          \
                 [s1] "r"((volatile uint8_t *)source.data + 256),              \
                 [s2] "r"((volatile uint8_t *)source.data + 512),              \
                 [s3] "r"((volatile uint8_t *)source.data + 768),              \
                 [out] "r"(output.data)                                       \
               : "t0", "v16", "v17", "v18", "v19", "v24", "v25", "v26",    \
                 "v27", "v28", "v29", "v30", "v31", "memory")
static void run(unsigned id) {
  uintptr_t vl, after, start;
  const unsigned before = errors;
  initialize(id);
  if (id == 2) { EXEC("vzext.vf2 v24, v16"); }
  else { EXEC("vsext.vf2 v24, v16"); }
  check(vl, 512); check(after, 512); check(start, 0);
  for (unsigned i = 0; i < 512; ++i) {
    uint32_t expected = (uint32_t)pattern(0, 2 * i, id) |
                        ((uint32_t)pattern(0, 2 * i + 1, id) << 8);
    if (id == 3 && (expected & 32768u)) expected |= UINT32_MAX << 16;
    uint32_t actual = output.data[i / 2] >> ((i % 2) * 32);
    check(actual, expected);
  }
  volatile struct region *regions[] = {&source, &destination, &output};
  for (unsigned b = 0; b < 3; ++b)
    for (unsigned i = 0; i < 8; ++i) {
      check(regions[b]->before[i], UINT64_C(0x13579bdf2468ace0) ^ (b * 17u + i));
      check(regions[b]->after[i], UINT64_C(0xfedcba9876543210) ^ (b * 17u + i));
    }
  printf("SOURCE_CASE %u checks=%u errors=%u\n", id, checks, errors - before);
}
int main(void) {
  run(2); run(3);
  printf("SOURCE_DONE cases=2 checks=%u errors=%u\n", checks, errors);
  return errors ? 1 : 0;
}
