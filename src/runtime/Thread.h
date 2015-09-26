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
#include "runtime/Stack.h"
#include "runtime/VirtualProcessor.h"

class BaseScheduler;
class UnblockInfo;

class Thread;

static inline Thread* CurrThread() {
  Thread* t = Runtime::thisProcessor()->getCurrThread();
  GENASSERT0(t);
  return t;
}

class Thread : public IntrusiveList<Thread>::Link {
  friend class VirtualProcessor; // stackPointer, nextScheduler

  vaddr stackPointer;            // holds stack pointer while thread inactive
  vaddr stackBottom;             // bottom of allocated memory for thread/stack
  size_t stackSize;              // size of allocated memory

  BaseScheduler* nextScheduler;  // default: resume on same scheduler
  bool affinity;                 // stick with scheduler
  mword priority;                // scheduling priority

  Runtime::ThreadStats stats;

  Thread(const Thread&) = delete;
  const Thread& operator=(const Thread&) = delete;

protected:
  enum State { Running, Cancelled, Finishing } state;
  UnblockInfo* unblockInfo; // unblock vs. timeout vs. cancel

  Thread(vaddr sb, size_t ss) :
    stackPointer(vaddr(this)), stackBottom(sb), stackSize(ss),
    nextScheduler(Runtime::thisProcessor()->getScheduler()), affinity(false),
    priority(defPriority), state(Running), unblockInfo(nullptr) {}

public:
  static Thread* create(size_t ss);
  static Thread* create();
  void destroy();
  void direct(ptr_t func, ptr_t p1 = nullptr, ptr_t p2 = nullptr, ptr_t p3 = nullptr, ptr_t p4 = nullptr) {
    stackDirect(stackPointer, func, p1, p2, p3, p4);
  }
  void setup(MemoryContext ctx, ptr_t func, ptr_t p1 = nullptr, ptr_t p2 = nullptr, ptr_t p3 = nullptr) {
    stackPointer = stackInit(stackPointer, ctx, func, p1, p2, p3);
  }
  void start(ptr_t func, ptr_t p1 = nullptr, ptr_t p2 = nullptr, ptr_t p3 = nullptr);
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

  void resume() {
    GENASSERT0(nextScheduler);
    nextScheduler->resume(*this);
  }

  bool finishing()             { return state == Finishing; }

  Thread* setPriority(mword p) { priority = p; return this; }
  mword getPriority() const    { return priority; }

  BaseScheduler* getNextScheduler() const { return nextScheduler; }

  Thread* setAffinity(BaseScheduler* s)   { affinity = (nextScheduler = s); return this; }
  BaseScheduler* getAffinity() const      { return affinity ? nextScheduler : nullptr; }
  bool hasAffinity() const                { return affinity; }

  const Runtime::ThreadStats& getStats() const { return stats; }
};

#endif /* _Thread_h_ */
