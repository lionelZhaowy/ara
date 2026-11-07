// SPDX-License-Identifier: Apache-2.0
#include <stdint.h>
#include "printf.h"
struct region { uint64_t before[4]; int32_t data[64]; uint64_t after[4]; };
static volatile struct region a,b,out;
static unsigned checks, errors;
static void check(uint64_t got,uint64_t want) {checks++; if(got!=want)errors++;}
static int32_t values(unsigned i) {
 const int32_t x[]={INT32_MAX,INT32_MAX-1,INT32_MIN,INT32_MIN+1,-1,0,1,2};
 return x[i%8];
}
static int32_t expected(int32_t x,int32_t y,unsigned mode) {
 int64_t sum=(int64_t)x+(int64_t)y;
 int64_t floor=sum>=0?sum/2:-((-sum+1)/2);
 unsigned odd=(uint64_t)sum&1;
 unsigned bit=(uint64_t)floor&1;
 unsigned round=mode==0?odd:mode==1?(odd&bit):mode==2?0:(odd&!bit);
 return (int32_t)(floor+round);
}
#define EXEC(INSTRUCTION,RM) \
 asm volatile("li t0,64\nvsetvli %[vl],t0,e32,m1,tu,mu\n" \
  "vle32.v v8,(%[a])\nvle32.v v16,(%[b])\ncsrwi vxrm," #RM "\n" \
  INSTRUCTION "\nvse32.v v24,(%[o])\ncsrr %[after],vl\n" \
  "csrr %[start],vstart\ncsrr %[csr],vcsr\nfence rw,rw" \
  : [vl]"=&r"(vl),[after]"=&r"(after),[start]"=&r"(start),[csr]"=&r"(csr) \
  : [a]"r"(a.data),[b]"r"(b.data),[o]"r"(out.data),[scalar]"r"((intptr_t)scalar) \
  : "t0","v8","v16","v24","memory")
#define MODES(INST) do {switch(mode) { \
 case 0: EXEC(INST,0);break;case 1: EXEC(INST,1);break; \
 case 2: EXEC(INST,2);break;default: EXEC(INST,3);break;}}while(0)
static void run(unsigned form,unsigned mode,unsigned id) {
 unsigned old=errors;uintptr_t vl,after,start,csr;
 int32_t scalar=values(id);
 volatile struct region *regions[]={&a,&b,&out};
 for(unsigned r=0;r<3;r++) {
  for(unsigned i=0;i<4;i++){regions[r]->before[i]=UINT64_C(0x123456789abcdef0)^(r*4+i);regions[r]->after[i]=UINT64_C(0xfedcba9876543210)^(r*4+i);}
  for(unsigned i=0;i<64;i++)regions[r]->data[i]=r==0?values(i+id):r==1?values(i*3+id):0x13579bdf;
 }
 if(form){MODES("vaadd.vx v24,v8,%[scalar]");}else{MODES("vaadd.vv v24,v8,v16");}
 check(vl,64);check(after,64);check(start,0);check(csr,mode*2);
 for(unsigned i=0;i<64;i++) {
  check((uint32_t)out.data[i],(uint32_t)expected(values(i+id),form?scalar:values(i*3+id),mode));
  check((uint32_t)a.data[i],(uint32_t)values(i+id));check((uint32_t)b.data[i],(uint32_t)values(i*3+id));
 }
 for(unsigned r=0;r<3;r++)for(unsigned i=0;i<4;i++){
  check(regions[r]->before[i],UINT64_C(0x123456789abcdef0)^(r*4+i));check(regions[r]->after[i],UINT64_C(0xfedcba9876543210)^(r*4+i));
 }
 printf("VAADD_CASE form=%u mode=%u id=%u errors=%u\n",form,mode,id,errors-old);
}
int main(void) {
 uint64_t saved;asm volatile("csrr %0,vcsr\ncsrwi vcsr,0":"=r"(saved)::"memory");
 for(unsigned form=0;form<2;form++)for(unsigned mode=0;mode<4;mode++)for(unsigned id=0;id<8;id++)run(form,mode,id);
 asm volatile("csrw vcsr,%0"::"r"(saved):"memory");
 printf("VAADD_DONE cases=64 checks=%u errors=%u\n",checks,errors);return errors?1:0;
}
