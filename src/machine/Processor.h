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
#ifndef _Processor_h_
#define _Processor_h_ 1

#include "machine/asmdecl.h"
#include "machine/asmshare.h"
#include "machine/APIC.h"
#include "machine/CPU.h"
#include "machine/Descriptors.h"
#include "machine/Memory.h" // apicAddr, ioApicAddr

class Thread;
class FrameManager;
class BaseScheduler;
struct Runtime;

static APIC*   MappedAPIC()   { return   (APIC*)apicAddr; }
static IOAPIC* MappedIOAPIC() { return (IOAPIC*)ioApicAddr; }

class SystemProcessor;

class Context {
  friend class LocalProcessor;      // member offsets for %gs-based access
protected:
  mword            index;
  mword            apicID;
  mword            systemID;
  mword            lockCount;
  Thread*          currThread;
  FrameManager*    currFM;
  mword            epoch;
  SystemProcessor* sproc;
  TaskStateSegment tss;
  Context(SystemProcessor* s) : index(0), apicID(0), systemID(0), lockCount(1),
    currThread(nullptr), currFM(nullptr), epoch(0), sproc(s) {}
  void install() {
    MSR::write(MSR::GS_BASE, mword(this)); // store 'this' in gs
    MSR::write(MSR::KERNEL_GS_BASE, 0);    // later: store user value in shadow gs
  }
};

class Processor : public Context {
  friend class LocalProcessor;      // member offsets for %gs-based access
  friend class Machine;             // init and setup routines

  /* task state segment: kernel stack for interrupts/exceptions */
  static const unsigned int nmiIST = 1;
  static const unsigned int dbfIST = 2;
  static const unsigned int stfIST = 3;

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

  Processor(const Processor&) = delete;            // no copy
  Processor& operator=(const Processor&) = delete; // no assignment

  void setup(mword i, mword a, mword s, FrameManager& fm) {
    index = i;
    apicID = a;
    systemID = s;
    currFM = &fm;
  }

  void init(paddr, bool, InterruptDescriptor*, size_t)  __section(".boot.text");

protected:
  void startup(BaseScheduler& sched, funcvoid0_t func)  __section(".boot.text");

public:
  Processor(SystemProcessor* s) : Context(s) {}
  mword getIndex() const { return index; }
  void sendIPI(uint8_t vec) { MappedAPIC()->sendIPI(apicID, vec); }
  void sendWakeIPI() { sendIPI(APIC::WakeIPI); }
  void sendPreemptIPI() { sendIPI(APIC::PreemptIPI); }
	void timer_perf() { MappedAPIC()->timer_perf(); }

} __packed __caligned;

class LocalProcessor {
  static void enableInterrupts() { asm volatile("sti" ::: "memory"); }
  static void disableInterrupts() { asm volatile("cli" ::: "memory"); }
  static void incLockCount() {
    asm volatile("addq $1, %%gs:%c0" :: "i"(offsetof(Context, lockCount)) : "cc");
  }
  static void decLockCount() {
    asm volatile("subq $1, %%gs:%c0" :: "i"(offsetof(Context, lockCount)) : "cc");
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

  static mword getIndex() {
    return get<mword, offsetof(Context, index)>();
  }
  static mword getApicID() {
    return get<mword, offsetof(Context, apicID)>();
  }
  static mword getSystemID() {
    return get<mword, offsetof(Context, systemID)>();
  }

  static mword getLockCount() {
    return get<mword, offsetof(Context, lockCount)>();
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

  static Thread* getCurrThread(_friend<Runtime>) {
    return get<Thread*, offsetof(Context, currThread)>();
  }
  static void setCurrThread(Thread* x, _friend<Runtime>) {
    set<Thread*, offsetof(Context, currThread)>(x);
  }
  static FrameManager* getCurrFM(_friend<FrameManager>) {
    return get<FrameManager*, offsetof(Context, currFM)>();
  }
//  static void setCurrFM(FrameManager* x) {
//    set<FrameManager*, offsetof(Context, currFM)>(x);
//  }
  static mword getEpoch() {
    return get<mword, offsetof(Context, epoch)>();
  }
  static void incEpoch(_friend<Runtime>) {
    asm volatile("addq $1, %%gs:%c0" :: "i"(offsetof(Context, epoch)) : "cc");
  }

  static SystemProcessor* self() {
    KASSERT0(checkLock());
    return get<SystemProcessor*, offsetof(Context, sproc)>();
  }
  static void setKernelStack(Thread* currThread) {
    const mword o = offsetof(Context, tss) + offsetof(TaskStateSegment, rsp);
    static_assert(o == TSSRSP, "TSSRSP");
    set<Thread*, o>(currThread);            // Thread* = top of stack
  }
};

#endif /* Processor_h_ */
