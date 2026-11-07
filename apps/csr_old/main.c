// SPDX-License-Identifier: Apache-2.0
#include <stdint.h>
#include "printf.h"
static unsigned checks, errors;
static void check(uint64_t a, uint64_t e) { checks++; if (a != e) errors++; }
int main(void) {
  uint64_t saved; asm volatile("csrr %0, vcsr" : "=r"(saved));
  { uint64_t old, combined, rm, sat;
    asm volatile("csrwi vcsr, 7\ncsrrwi %0, vcsr, 0\ncsrr %1, vcsr\ncsrr %2, vxrm\ncsrr %3, vxsat"
      : "=&r"(old), "=&r"(combined), "=&r"(rm), "=&r"(sat) :: "memory");
    check(old, 7); check(combined, 0); check(rm, 0); check(sat, 0);
  }
  { uint64_t old, combined, rm, sat;
    asm volatile("csrwi vcsr, 6\ncsrrwi %0, vcsr, 1\ncsrr %1, vcsr\ncsrr %2, vxrm\ncsrr %3, vxsat"
      : "=&r"(old), "=&r"(combined), "=&r"(rm), "=&r"(sat) :: "memory");
    check(old, 6); check(combined, 1); check(rm, 0); check(sat, 1);
  }
  { uint64_t old, combined, rm, sat;
    asm volatile("csrwi vcsr, 5\ncsrrwi %0, vcsr, 7\ncsrr %1, vcsr\ncsrr %2, vxrm\ncsrr %3, vxsat"
      : "=&r"(old), "=&r"(combined), "=&r"(rm), "=&r"(sat) :: "memory");
    check(old, 5); check(combined, 7); check(rm, 3); check(sat, 1);
  }
  { uint64_t old, combined, rm, sat;
    asm volatile("csrwi vcsr, 4\ncsrrwi %0, vcsr, 2\ncsrr %1, vcsr\ncsrr %2, vxrm\ncsrr %3, vxsat"
      : "=&r"(old), "=&r"(combined), "=&r"(rm), "=&r"(sat) :: "memory");
    check(old, 4); check(combined, 2); check(rm, 1); check(sat, 0);
  }
  { uint64_t old, combined, rm, sat;
    asm volatile("csrwi vcsr, 3\ncsrrwi %0, vcsr, 4\ncsrr %1, vcsr\ncsrr %2, vxrm\ncsrr %3, vxsat"
      : "=&r"(old), "=&r"(combined), "=&r"(rm), "=&r"(sat) :: "memory");
    check(old, 3); check(combined, 4); check(rm, 2); check(sat, 0);
  }
  { uint64_t old, combined, rm, sat;
    asm volatile("csrwi vcsr, 2\ncsrrwi %0, vcsr, 6\ncsrr %1, vcsr\ncsrr %2, vxrm\ncsrr %3, vxsat"
      : "=&r"(old), "=&r"(combined), "=&r"(rm), "=&r"(sat) :: "memory");
    check(old, 2); check(combined, 6); check(rm, 3); check(sat, 0);
  }
  { uint64_t old, combined, rm, sat;
    asm volatile("csrwi vcsr, 1\ncsrrwi %0, vcsr, 3\ncsrr %1, vcsr\ncsrr %2, vxrm\ncsrr %3, vxsat"
      : "=&r"(old), "=&r"(combined), "=&r"(rm), "=&r"(sat) :: "memory");
    check(old, 1); check(combined, 3); check(rm, 1); check(sat, 1);
  }
  { uint64_t old, combined, rm, sat;
    asm volatile("csrwi vcsr, 0\ncsrrwi %0, vcsr, 5\ncsrr %1, vcsr\ncsrr %2, vxrm\ncsrr %3, vxsat"
      : "=&r"(old), "=&r"(combined), "=&r"(rm), "=&r"(sat) :: "memory");
    check(old, 0); check(combined, 5); check(rm, 2); check(sat, 1);
  }
  asm volatile("csrw vcsr, %0" :: "r"(saved) : "memory");
  printf("CSR_OLD_DONE cases=8 checks=%u errors=%u\n", checks, errors);
  return errors ? 1 : 0;
}
