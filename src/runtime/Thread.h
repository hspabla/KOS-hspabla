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
#ifndef _Thread_h_
#define _Thread_h_ 1

#include "generic/IntrusiveContainers.h" 
#include "runtime/Runtime.h"
#include "runtime/Stack.h"

class BaseScheduler;
class UnblockInfo;

static const mword  topPriority = 0;
static const mword  defPriority = 1;
static const mword idlePriority = 2;
static const mword  maxPriority = 3;

class Thread : public IntrusiveList<Thread>::Link {
  vaddr stackPointer;       // holds stack pointer while thread inactive
  vaddr stackBottom;        // bottom of allocated memory for thread/stack
  size_t stackSize;         // size of allocated memory

  BaseScheduler* scheduler; // default: resume on same scheduler
  bool affinity;            // stick with scheduler
  mword priority;           // scheduling priority

  enum State { Running, Cancelled, Finishing } state;
  UnblockInfo* unblockInfo; // unblock vs. timeout vs. cancel

  Runtime::MemoryContext memctx;
  Runtime::ThreadStats stats;

  Thread(const Thread&) = delete;
  const Thread& operator=(const Thread&) = delete;

  template<typename... Args>
  inline void switchThread(bool yield, Args&... a);
  inline void switchThread2();
  static Thread* postYield(Thread* prevThread);
  static Thread* postSuspend(Thread* prevThread);

protected:
  Thread(vaddr sb, size_t ss, Runtime::MemoryContext& mc) :
    stackPointer(vaddr(this)), stackBottom(sb), stackSize(ss),
    scheduler(nullptr), affinity(false), priority(defPriority),
    state(Running), unblockInfo(nullptr), memctx(mc) {}

public:
  static Thread* create(size_t ss);
  static Thread* create() { return create(defaultStack); }
  void destroy();

  void direct(ptr_t func, ptr_t p1 = nullptr, ptr_t p2 = nullptr, ptr_t p3 = nullptr, ptr_t p4 = nullptr) {
    if (!affinity) scheduler = CurrThread()->scheduler;
    stackDirect(stackPointer, func, p1, p2, p3, p4);
  }
  void setup(ptr_t func, ptr_t p1 = nullptr, ptr_t p2 = nullptr, ptr_t p3 = nullptr, ptr_t p4 = nullptr) {
    if (!affinity) scheduler = CurrThread()->scheduler;
    stackPointer = stackInit(stackPointer, func, p1, p2, p3, p4);
  }
  void start(ptr_t func, ptr_t p1 = nullptr, ptr_t p2 = nullptr, ptr_t p3 = nullptr, ptr_t p4 = nullptr) {
    setup(func, p1, p2, p3, p4);
    resume();
  }

  void yield();
  void preempt();
  void suspend(BasicLock& lk);
  void suspend(BasicLock& lk1, BasicLock& lk2);
  void terminate() __noreturn;

  void resume();
  void cancel();

  bool block(UnblockInfo* ubi) {
    GENASSERT1(this == CurrThread(), FmtHex(CurrThread()));
    GENASSERT1(unblockInfo == nullptr, FmtHex(unblockInfo));
    GENASSERT1(state != Finishing, state);
    if (state == Cancelled) return false;
    __atomic_store_n( &unblockInfo, ubi, __ATOMIC_SEQ_CST );
    return true;
  }

  UnblockInfo* getUnblockInfo() {
    GENASSERT1(this != CurrThread(), CurrThread());
    return __atomic_exchange_n( &unblockInfo, nullptr, __ATOMIC_SEQ_CST );
  }

  bool cancelled() const       { return state == Cancelled; }
  bool finishing() const       { return state == Finishing; }
  Thread* setPriority(mword p) { priority = p; return this; }
  mword getPriority() const    { return priority; }
  Thread* setAffinity(bool a)  { affinity = a; return this; }
  bool getAffinity() const     { return affinity; }

  Thread* setScheduler(BaseScheduler& s) { scheduler = &s; return this; }

  Runtime::MemoryContext getMemCtx() { return memctx; }
  const Runtime::ThreadStats& getStats() const { return stats; }
};

#endif /* _Thread_h_ */
