// SPDX-License-Identifier: Apache-2.0
// Full-width VTYPE checks adapted to the official U-mode application runtime.
#include <stdint.h>
#include "printf.h"
static unsigned num_checks, num_failed, num_vsets, num_cases;

enum { CASES=66, FIELDS=16, CHECKS=635, VSETS=141 };
#define VILL (UINT64_C(1)<<63)
/* id,kind,AVL,rs2,seed-return,seed-VL,seed-VTYPE,seed-VSTART,
 * return,VL,VTYPE,VSTART,target-PC,target-raw,VCSR,completed. */
volatile uint64_t v01_vtype_rows[CASES][FIELDS];
/* Saved mstatus/mtvec/vcsr/vstart/vl/vtype; restored same six;
 * vlenb; actual saved VILL; preparation return/VL/VTYPE/VSTART; finished. */
volatile uint64_t v01_vtype_context[19];
/* cases,checks,errors,vsets,finished. */
volatile uint64_t v01_vtype_counts[5];
volatile uint64_t v01_vtype_first_error[4];

static void check(unsigned id,unsigned field,uint64_t got,uint64_t want) {
    ++num_checks;
    if (got==want) return;
    if (!num_failed) {
        v01_vtype_first_error[0]=id;v01_vtype_first_error[1]=field;
        v01_vtype_first_error[2]=got;v01_vtype_first_error[3]=want;
    }
    ++num_failed;
}
struct result { uint64_t sr,sv,st,ss,r,v,t,s,pc,csr; };
static __attribute__((noinline)) struct result execute(uint64_t avl,uint64_t type) {
    struct result r;
    asm volatile(
        "li t0, 3\nvsetvli %[sr], t0, e32, m1, tu, mu\n"
        "csrr %[sv], vl\ncsrr %[st], vtype\n"
        "li t0, 1\ncsrw vstart, t0\ncsrr %[ss], vstart\n"
        "mv t1, %[avl]\nmv t2, %[type]\nlla %[pc], v01_vtype_target\n"
        ".balign 4\n.global v01_vtype_target\nv01_vtype_target:\n"
        "vsetvl t3, t1, t2\nmv %[r], t3\n"
        "csrr %[v], vl\ncsrr %[t], vtype\ncsrr %[s], vstart\ncsrr %[csr], vcsr\n"
        : [sr] "=&r"(r.sr),[sv] "=&r"(r.sv),[st] "=&r"(r.st),[ss] "=&r"(r.ss),
          [pc] "=&r"(r.pc),[r] "=&r"(r.r),[v] "=&r"(r.v),[t] "=&r"(r.t),
          [s] "=&r"(r.s),[csr] "=&r"(r.csr)
        : [avl] "r"(avl),[type] "r"(type) : "t0","t1","t2","t3","memory");
    num_vsets+=2;
    return r;
}
static void run(unsigned id,uint64_t saved_vill,uint64_t saved_csr) {
    const uint64_t avl=id==57 ? 5 : 3;
    const uint64_t type=id<56 ? (UINT64_C(1)<<(id+8))|UINT64_C(0x10) :
                        id==56 ? UINT64_C(0x10) : id==57 ? UINT64_C(0x19) : saved_vill;
    const uint64_t want_vl=id==56 ? 3 : id==57 ? 5 : 0;
    const uint64_t want_type=id==56 ? UINT64_C(0x10) : id==57 ? UINT64_C(0x19) : VILL;
    struct result r=execute(avl,type);
    const uint64_t row[FIELDS]={id,id+8,avl,type,r.sr,r.sv,r.st,r.ss,
        r.r,r.v,r.t,r.s,r.pc,*(const volatile uint32_t *)(uintptr_t)r.pc,r.csr,1};
    for(unsigned i=0;i<FIELDS;++i)v01_vtype_rows[id][i]=row[i];
    check(id,4,r.sr,3);check(id,5,r.sv,3);check(id,6,r.st,0x10);check(id,7,r.ss,1);
    check(id,8,r.r,want_vl);check(id,9,r.v,want_vl);
    check(id,10,r.t,want_type);check(id,11,r.s,0);check(id,14,r.csr,saved_csr);
    ++num_cases;
}
static uint64_t prepare_saved_vill(void) {
    uint64_t returned,vl,type,start;
    /* zimm=0x20 is unsupported SEW128/m1. It uses no high rs2 bits.
     * vsetvli t3,t0,0x20 = 0x0202fe57, a legal configuration instruction.
     * The unsupported configuration sets VILL/VL0 and never takes a trap. */
    asm volatile("li t0, 3\nli t1, 1\ncsrw vstart, t1\n"
        ".balign 4\n.global v01_vtype_make_vill\nv01_vtype_make_vill:\n"
        ".4byte 0x0202fe57\nmv %[r], t3\n"
        "csrr %[v], vl\ncsrr %[t], vtype\ncsrr %[s], vstart\n"
        : [r] "=&r"(returned),[v] "=&r"(vl),[t] "=&r"(type),[s] "=&r"(start)
        ::"t0","t1","t3","memory");
    ++num_vsets;
    v01_vtype_context[13]=type;
    v01_vtype_context[14]=returned;v01_vtype_context[15]=vl;
    v01_vtype_context[16]=type;v01_vtype_context[17]=start;
    check(CASES,1,returned,0);check(CASES,2,vl,0);
    check(CASES,3,type,VILL);check(CASES,4,start,0);
    return type;
}

/* The original16-field row remains unchanged. New immediate rows use
 * kind=id+8, AVL=3 and type=the actual encoded zimm (not an rs2 value).
 * Every new target is followed by a distinct legal recovery instruction.
 * Recovery fields16..19 name first-error diagnostics only. Their actual
 * return/VL/VTYPE/VSTART producers are checked in the retired CPU trace. */
#define V01_STRING1(x) #x
#define V01_STRING(x) V01_STRING1(x)
#define V01_IMMEDIATE_BODY(ID, RAW) \
    asm volatile( \
        "li t0, 3\nvsetvli %[sr], t0, e32, m1, tu, mu\n" \
        "csrr %[sv], vl\ncsrr %[st], vtype\n" \
        "li t0, 1\ncsrw vstart, t0\ncsrr %[ss], vstart\n" \
        "li t0, 3\nlla %[pc], v01_vtype_imm_case" V01_STRING(ID) "\n" \
        ".balign 4\n.global v01_vtype_imm_case" V01_STRING(ID) "\n" \
        "v01_vtype_imm_case" V01_STRING(ID) ":\n" \
        ".4byte " V01_STRING(RAW) "\nmv %[r], t3\n" \
        "csrr %[v], vl\ncsrr %[t], vtype\ncsrr %[s], vstart\ncsrr %[csr], vcsr\n" \
        : [sr] "=&r"(r.sr),[sv] "=&r"(r.sv),[st] "=&r"(r.st),[ss] "=&r"(r.ss), \
          [pc] "=&r"(r.pc),[r] "=&r"(r.r),[v] "=&r"(r.v),[t] "=&r"(r.t), \
          [s] "=&r"(r.s),[csr] "=&r"(r.csr) \
        :: "t0","t1","t2","t3","memory")
static __attribute__((noinline)) struct result execute_immediate(unsigned id) {
    struct result r;
    switch (id) {
    case 59: V01_IMMEDIATE_BODY(59, 0x0102fe57); break;
    case 60: V01_IMMEDIATE_BODY(60, 0x1102fe57); break;
    case 61: V01_IMMEDIATE_BODY(61, 0x2102fe57); break;
    case 62: V01_IMMEDIATE_BODY(62, 0x4102fe57); break;
    case 63: V01_IMMEDIATE_BODY(63, 0xc101fe57); break;
    case 64: V01_IMMEDIATE_BODY(64, 0xd101fe57); break;
    case 65: V01_IMMEDIATE_BODY(65, 0xe101fe57); break;
    default: __builtin_unreachable();
    }
    num_vsets+=2;
    return r;
}
#undef V01_IMMEDIATE_BODY
#undef V01_STRING
#undef V01_STRING1
struct recovery { uint64_t returned,vl,type,start; };
static __attribute__((noinline)) struct recovery execute_immediate_recovery(void) {
    struct recovery r;
    /* vsetvli t3,t0,e32,m1,tu,mu. Non-x0 t0 supplies the real AVL3. */
    asm volatile("li t0, 3\n.balign 4\n.global v01_vtype_imm_restore\n"
        "v01_vtype_imm_restore:\n.4byte 0x0102fe57\nmv %[r], t3\n"
        "csrr %[v], vl\ncsrr %[t], vtype\ncsrr %[s], vstart\n"
        : [r] "=&r"(r.returned),[v] "=&r"(r.vl),[t] "=&r"(r.type),[s] "=&r"(r.start)
        :: "t0","t3","memory");
    ++num_vsets;
    return r;
}
static void run_immediate(unsigned id,uint64_t saved_csr) {
    static const uint64_t zimm[7]={0x10,0x110,0x210,0x410,0x10,0x110,0x210};
    const unsigned index=id-59;
    const uint64_t want_vl=(index==0 || index==4) ? 3 : 0;
    const uint64_t want_type=(index==0 || index==4) ? UINT64_C(0x10) : VILL;
    struct result r=execute_immediate(id);
    const uint64_t row[FIELDS]={id,id+8,3,zimm[index],r.sr,r.sv,r.st,r.ss,
        r.r,r.v,r.t,r.s,r.pc,*(const volatile uint32_t *)(uintptr_t)r.pc,r.csr,1};
    for(unsigned i=0;i<FIELDS;++i)v01_vtype_rows[id][i]=row[i];
    check(id,4,r.sr,3);check(id,5,r.sv,3);check(id,6,r.st,0x10);check(id,7,r.ss,1);
    check(id,8,r.r,want_vl);check(id,9,r.v,want_vl);
    check(id,10,r.t,want_type);check(id,11,r.s,0);check(id,14,r.csr,saved_csr);
    struct recovery recovered=execute_immediate_recovery();
    check(id,16,recovered.returned,3);check(id,17,recovered.vl,3);
    check(id,18,recovered.type,0x10);check(id,19,recovered.start,0);
    ++num_cases;
}

int main(void) {
    uint64_t csr,start,vl,type,vlenb;
    asm volatile("csrr %0, vcsr\ncsrr %1, vstart\ncsrr %2, vl\ncsrr %3, vtype"
        : "=&r"(csr),"=&r"(start),"=&r"(vl),"=&r"(type) :: "memory");
    asm volatile("csrr %0, vlenb" : "=r"(vlenb));
    check(CASES,0,vlenb,256);
    for(unsigned id=0;id<58;++id)run(id,0,csr);
    uint64_t saved_vill=prepare_saved_vill();run(58,saved_vill,csr);
    for(unsigned id=59;id<CASES;++id)run_immediate(id,csr);
    asm volatile("mv t0, %0\nmv t1, %1\ncsrw vstart, zero\nvsetvl zero, t0, t1\ncsrw vcsr, %2\ncsrw vstart, %3"
        :: "r"(vl),"r"(type),"r"(csr),"r"(start) : "t0","t1","memory");
    uint64_t rc,rs,rv,rt;
    asm volatile("csrr %0, vcsr\ncsrr %1, vstart\ncsrr %2, vl\ncsrr %3, vtype"
        : "=&r"(rc),"=&r"(rs),"=&r"(rv),"=&r"(rt) :: "memory");
    check(CASES,5,rc,csr);check(CASES,6,rs,start);check(CASES,7,rv,vl);check(CASES,8,rt,type);
    check(CASES,9,num_cases,CASES);
    printf("VTYPE_DONE cases=%u checks=%u errors=%u\n",num_cases,num_checks,num_failed);
    return num_failed ? 1 : 0;
}
