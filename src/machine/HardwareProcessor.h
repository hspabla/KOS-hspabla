/******************************************************************************
    Copyright © 2012-2015 Martin Karsten

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
******************************************************************************/
#ifndef _HardwareProcessor_h_
#define _HardwareProcessor_h_ 1

#include "machine/asmdecl.h"
#include "machine/asmshare.h"
#include "machine/APIC.h"
#include "machine/CPU.h"
#include "machine/Descriptors.h"
#include "machine/Memory.h" // cloneBase, zeroBase, apicAddr, ioApicAddr

class VirtualProcessor;
class Thread;

static APIC* MappedAPIC() { return   (APIC*)apicAddr; }
static IOAPIC* MappedIOAPIC() { return (IOAPIC*)ioApicAddr; }

class HardwareProcessor {
  friend class Machine;             // init and setup routines
  friend class LocalProcessor;      // member offsets for %gs-based access

  /* execution context */
  mword lockCount;

  /* processor information */
  mword index;
  mword apicID;
  mword systemID;
  VirtualProcessor* lproc;

  /* task state segment: kernel stack for interrupts/exceptions */
  static const unsigned int nmiIST = 1;
  static const unsigned int dbfIST = 2;
  static const unsigned int stfIST = 3;
  TaskStateSegment tss;

  // layout for syscall/sysret, because of (strange) rules for SYSCALL_LSTAR
  // SYSCALL_LSTAR essentially forces userCS = userDS + 1
  // layout for sysenter/sysexit would be: kernCS, kernDS, userCS, userDS
  // also: kernDS = 2 consistent with convention and segment setup in boot.S
  static const unsigned int kernCS  = 1;
  static const unsigned int kernDS  = 2;
  static const unsigned int userDS  = 3;
  static const unsigned int userCS  = 4;
  static const unsigned int tssSel  = 5; // TSS entry uses 2 entries
  static const unsigned int maxGDT  = 7;
  SegmentDescriptor gdt[maxGDT];

  void setupGDT(uint32_t n, uint32_t dpl, bool code)    __section(".boot.text");
  void setupTSS(uint32_t num, paddr addr)               __section(".boot.text");

  HardwareProcessor(const HardwareProcessor&) = delete;            // no copy
  HardwareProcessor& operator=(const HardwareProcessor&) = delete; // no assignment

  void setup(mword i, mword a, mword s) {
    index = i;
    apicID = a;
    systemID = s;
  }

protected:
  void init0()                                          __section(".boot.text");
  static void init1(paddr pml4, bool bootstrap)         __section(".boot.text");
  void init2(InterruptDescriptor*, size_t)              __section(".boot.text");
  void init3(VirtualProcessor& vp) { lproc = &vp; }
  void init4()                                          __section(".boot.text");

public:
  HardwareProcessor() : lockCount(1), index(0), apicID(0), systemID(0), lproc(nullptr) {}

  void sendIPI(uint8_t vec) { MappedAPIC()->sendIPI(apicID, vec); }
  void sendWakeIPI() { sendIPI(APIC::WakeIPI); }
  void sendPreemptIPI() { sendIPI(APIC::PreemptIPI); }
} __packed __caligned;

class LocalProcessor {
  static void enableInterrupts() { asm volatile("sti" ::: "memory"); }
  static void disableInterrupts() { asm volatile("cli" ::: "memory"); }
  static void incLockCount() {
    asm volatile("addq $1, %%gs:%c0" :: "i"(offsetof(HardwareProcessor, lockCount)) : "cc");
  }
  static void decLockCount() {
    asm volatile("subq $1, %%gs:%c0" :: "i"(offsetof(HardwareProcessor, lockCount)) : "cc");
  }

  template<typename T, mword offset> static T get() {
    T x;
    asm volatile("movq %%gs:%c1, %0" : "=r"(x) : "i"(offset));
    return x;
  }

  template<typename T, mword offset> static void set(T x) {
    asm volatile("movq %0, %%gs:%c1" :: "r"(x), "i"(offset));
  }

public:
  static void initInterrupts(bool irqs);

  static mword getLockCount() {
    return get<mword, offsetof(HardwareProcessor, lockCount)>();
  }
  static mword checkLock() {
    KASSERT1(CPU::interruptsEnabled() == (getLockCount() == 0), getLockCount());
    return getLockCount();
  }
  static void lockFake() {
    KASSERT0(!CPU::interruptsEnabled());
    incLockCount();
  }
  static void unlockFake() {
    KASSERT0(!CPU::interruptsEnabled());
    decLockCount();
  }
  static void lock(bool check = false) {
    if (check) KASSERT1(checkLock() == 0, getLockCount());
    // despite looking like trouble, I believe this is safe: race could cause
    // multiple calls to disableInterrupts(), but this is no problem!
    if slowpath(getLockCount() == 0) disableInterrupts();
    incLockCount();
  }
  static void unlock(bool check = false) {
    if (check) KASSERT1(checkLock() == 1, getLockCount());
    decLockCount();
    // no races here (interrupts disabled)
    if slowpath(getLockCount() == 0) enableInterrupts();
  }

  static mword getIndex() {
    return get<mword, offsetof(HardwareProcessor, index)>();
  }
  static mword getApicID() {
    return get<mword, offsetof(HardwareProcessor, apicID)>();
  }
  static mword getSystemID() {
    return get<mword, offsetof(HardwareProcessor, systemID)>();
  }
  static VirtualProcessor* self() {
    return get<VirtualProcessor*, offsetof(HardwareProcessor, lproc)>();
  }
  static vaddr getCloneAddr() {
    return cloneBase + pagetableps * getIndex();
  }
  static vaddr getZeroAddr() {
    return zeroBase + kernelps * getIndex();
  }
  static void setKernelStack(Thread* currThread) {
    const mword o = offsetof(HardwareProcessor, tss) + offsetof(TaskStateSegment, rsp);
    static_assert(o == TSSRSP, "TSSRSP");
    set<Thread*, o>(currThread);            // Thread* = top of stack
  }
};

#endif /* HardwareProcessor_h_ */
