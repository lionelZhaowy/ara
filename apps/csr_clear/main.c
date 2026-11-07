// SPDX-License-Identifier: Apache-2.0
#include <stdint.h>
#include "printf.h"
static unsigned checks, errors;
static void check(uint64_t a, uint64_t e) { checks++; if (a != e) errors++; }
int main(void) {
  uint64_t saved; asm volatile("csrr %0, vcsr" : "=r"(saved));
  { uint64_t old, combined, rm, sat;
    asm volatile("csrwi vcsr, 7\nli t0, 0\ncsrrc %0, vxrm, t0\ncsrr %1, vcsr\ncsrr %2, vxrm\ncsrr %3, vxsat"
      : "=&r"(old), "=&r"(combined), "=&r"(rm), "=&r"(sat) :: "t0", "memory");
    check(old, 3); check(combined, 7); check(rm, 3); check(sat, 1);
  }
  { uint64_t old, combined, rm, sat;
    asm volatile("csrwi vcsr, 7\ncsrrci %0, vxrm, 0\ncsrr %1, vcsr\ncsrr %2, vxrm\ncsrr %3, vxsat"
      : "=&r"(old), "=&r"(combined), "=&r"(rm), "=&r"(sat) :: "t0", "memory");
    check(old, 3); check(combined, 7); check(rm, 3); check(sat, 1);
  }
  { uint64_t old, combined, rm, sat;
    asm volatile("csrwi vcsr, 7\nli t0, 1\ncsrrc %0, vxrm, t0\ncsrr %1, vcsr\ncsrr %2, vxrm\ncsrr %3, vxsat"
      : "=&r"(old), "=&r"(combined), "=&r"(rm), "=&r"(sat) :: "t0", "memory");
    check(old, 3); check(combined, 5); check(rm, 2); check(sat, 1);
  }
  { uint64_t old, combined, rm, sat;
    asm volatile("csrwi vcsr, 7\ncsrrci %0, vxrm, 1\ncsrr %1, vcsr\ncsrr %2, vxrm\ncsrr %3, vxsat"
      : "=&r"(old), "=&r"(combined), "=&r"(rm), "=&r"(sat) :: "t0", "memory");
    check(old, 3); check(combined, 5); check(rm, 2); check(sat, 1);
  }
  { uint64_t old, combined, rm, sat;
    asm volatile("csrwi vcsr, 7\nli t0, 2\ncsrrc %0, vxrm, t0\ncsrr %1, vcsr\ncsrr %2, vxrm\ncsrr %3, vxsat"
      : "=&r"(old), "=&r"(combined), "=&r"(rm), "=&r"(sat) :: "t0", "memory");
    check(old, 3); check(combined, 3); check(rm, 1); check(sat, 1);
  }
  { uint64_t old, combined, rm, sat;
    asm volatile("csrwi vcsr, 7\ncsrrci %0, vxrm, 2\ncsrr %1, vcsr\ncsrr %2, vxrm\ncsrr %3, vxsat"
      : "=&r"(old), "=&r"(combined), "=&r"(rm), "=&r"(sat) :: "t0", "memory");
    check(old, 3); check(combined, 3); check(rm, 1); check(sat, 1);
  }
  { uint64_t old, combined, rm, sat;
    asm volatile("csrwi vcsr, 7\nli t0, 3\ncsrrc %0, vxrm, t0\ncsrr %1, vcsr\ncsrr %2, vxrm\ncsrr %3, vxsat"
      : "=&r"(old), "=&r"(combined), "=&r"(rm), "=&r"(sat) :: "t0", "memory");
    check(old, 3); check(combined, 1); check(rm, 0); check(sat, 1);
  }
  { uint64_t old, combined, rm, sat;
    asm volatile("csrwi vcsr, 7\ncsrrci %0, vxrm, 3\ncsrr %1, vcsr\ncsrr %2, vxrm\ncsrr %3, vxsat"
      : "=&r"(old), "=&r"(combined), "=&r"(rm), "=&r"(sat) :: "t0", "memory");
    check(old, 3); check(combined, 1); check(rm, 0); check(sat, 1);
  }
  { uint64_t old, combined, rm, sat;
    asm volatile("csrwi vcsr, 7\nli t0, 0\ncsrrc %0, vcsr, t0\ncsrr %1, vcsr\ncsrr %2, vxrm\ncsrr %3, vxsat"
      : "=&r"(old), "=&r"(combined), "=&r"(rm), "=&r"(sat) :: "t0", "memory");
    check(old, 7); check(combined, 7); check(rm, 3); check(sat, 1);
  }
  { uint64_t old, combined, rm, sat;
    asm volatile("csrwi vcsr, 7\ncsrrci %0, vcsr, 0\ncsrr %1, vcsr\ncsrr %2, vxrm\ncsrr %3, vxsat"
      : "=&r"(old), "=&r"(combined), "=&r"(rm), "=&r"(sat) :: "t0", "memory");
    check(old, 7); check(combined, 7); check(rm, 3); check(sat, 1);
  }
  { uint64_t old, combined, rm, sat;
    asm volatile("csrwi vcsr, 7\nli t0, 1\ncsrrc %0, vcsr, t0\ncsrr %1, vcsr\ncsrr %2, vxrm\ncsrr %3, vxsat"
      : "=&r"(old), "=&r"(combined), "=&r"(rm), "=&r"(sat) :: "t0", "memory");
    check(old, 7); check(combined, 6); check(rm, 3); check(sat, 0);
  }
  { uint64_t old, combined, rm, sat;
    asm volatile("csrwi vcsr, 7\ncsrrci %0, vcsr, 1\ncsrr %1, vcsr\ncsrr %2, vxrm\ncsrr %3, vxsat"
      : "=&r"(old), "=&r"(combined), "=&r"(rm), "=&r"(sat) :: "t0", "memory");
    check(old, 7); check(combined, 6); check(rm, 3); check(sat, 0);
  }
  { uint64_t old, combined, rm, sat;
    asm volatile("csrwi vcsr, 7\nli t0, 2\ncsrrc %0, vcsr, t0\ncsrr %1, vcsr\ncsrr %2, vxrm\ncsrr %3, vxsat"
      : "=&r"(old), "=&r"(combined), "=&r"(rm), "=&r"(sat) :: "t0", "memory");
    check(old, 7); check(combined, 5); check(rm, 2); check(sat, 1);
  }
  { uint64_t old, combined, rm, sat;
    asm volatile("csrwi vcsr, 7\ncsrrci %0, vcsr, 2\ncsrr %1, vcsr\ncsrr %2, vxrm\ncsrr %3, vxsat"
      : "=&r"(old), "=&r"(combined), "=&r"(rm), "=&r"(sat) :: "t0", "memory");
    check(old, 7); check(combined, 5); check(rm, 2); check(sat, 1);
  }
  { uint64_t old, combined, rm, sat;
    asm volatile("csrwi vcsr, 7\nli t0, 4\ncsrrc %0, vcsr, t0\ncsrr %1, vcsr\ncsrr %2, vxrm\ncsrr %3, vxsat"
      : "=&r"(old), "=&r"(combined), "=&r"(rm), "=&r"(sat) :: "t0", "memory");
    check(old, 7); check(combined, 3); check(rm, 1); check(sat, 1);
  }
  { uint64_t old, combined, rm, sat;
    asm volatile("csrwi vcsr, 7\ncsrrci %0, vcsr, 4\ncsrr %1, vcsr\ncsrr %2, vxrm\ncsrr %3, vxsat"
      : "=&r"(old), "=&r"(combined), "=&r"(rm), "=&r"(sat) :: "t0", "memory");
    check(old, 7); check(combined, 3); check(rm, 1); check(sat, 1);
  }
  { uint64_t old, combined, rm, sat;
    asm volatile("csrwi vcsr, 7\nli t0, 7\ncsrrc %0, vcsr, t0\ncsrr %1, vcsr\ncsrr %2, vxrm\ncsrr %3, vxsat"
      : "=&r"(old), "=&r"(combined), "=&r"(rm), "=&r"(sat) :: "t0", "memory");
    check(old, 7); check(combined, 0); check(rm, 0); check(sat, 0);
  }
  { uint64_t old, combined, rm, sat;
    asm volatile("csrwi vcsr, 7\ncsrrci %0, vcsr, 7\ncsrr %1, vcsr\ncsrr %2, vxrm\ncsrr %3, vxsat"
      : "=&r"(old), "=&r"(combined), "=&r"(rm), "=&r"(sat) :: "t0", "memory");
    check(old, 7); check(combined, 0); check(rm, 0); check(sat, 0);
  }
  asm volatile("csrw vcsr, %0" :: "r"(saved) : "memory");
  printf("CSR_CLEAR_DONE cases=18 checks=%u errors=%u\n", checks, errors);
  return errors ? 1 : 0;
}
