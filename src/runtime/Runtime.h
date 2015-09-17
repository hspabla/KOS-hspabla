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
#ifndef _Runtime_h_
#define _Runtime_h_ 1

#if defined(__KOS__)

#include "kernel/AddressSpace.h"
#include "kernel/Output.h"
#include "machine/Machine.h"
#include "machine/Processor.h"
#include "machine/SpinLock.h"

typedef SpinLock BasicLock;
typedef ScopedLock<BasicLock> AutoLock;

class Scheduler;
class Thread;

static const mword  topPriority = 0;
static const mword  defPriority = 1;
static const mword idlePriority = 2;
static const mword  maxPriority = 3;

#define CHECK_LOCK_MIN(x) \
  KASSERT1(LocalProcessor::checkLock() > (x), LocalProcessor::getLockCount())

#define CHECK_LOCK_COUNT(x) \
  KASSERT1(LocalProcessor::checkLock() == (x), LocalProcessor::getLockCount())

namespace Runtime {

  /**** platform-specific ready queue ****/

  template<size_t Levels>
  class ReadyQueue {
    BasicLock lock;
    size_t readyCount;
    size_t cpuCount;
    IntrusiveList<Thread> readyQueue[Levels];
  public:
    ReadyQueue(size_t c) : readyCount(0), cpuCount(c) {}
    bool empty() { return 0 == (volatile size_t&)readyCount; }
    size_t size() { return readyCount; }
    inline bool push(Thread& t);
    inline Thread* pop(size_t maxlevel);
    inline void balanceWith(ReadyQueue& rq);
  };

  /**** additional thread fields that depend on runtime system ****/

  struct ThreadStats {
    mword tscLast;
    mword tscTotal;
    ThreadStats() : tscLast(0), tscTotal(0) {}
    void update(ThreadStats& next) {
      mword tsc = CPU::readTSC();
      tscTotal += tsc - tscLast;
      next.tscLast = tsc;
    }
    mword getCycleCount() const  { return tscTotal; }
  };

  /**** preemption enable/disable/fake ****/

  struct DisablePreemption {
    DisablePreemption() { LocalProcessor::lockFake(); }     // just inflate lock count
    DisablePreemption(bool) { LocalProcessor::lock(true); } // disable IRQs
    ~DisablePreemption() { LocalProcessor::unlock(true); }  // full unlock
  };

  static void EnablePreemption() { LocalProcessor::unlock(true); } // full unlock

  /**** AddressSpace-related interface ****/

  typedef AddressSpace MemoryContext;

  static MemoryContext& getCurrentMemoryContext() { return CurrAS(); }
  static MemoryContext& getDefaultMemoryContext() { return defaultAS; }

  static vaddr allocThreadStack(size_t ss) {
    return kernelAS.allocStack(ss);
  }

  static void releaseThreadStack(vaddr vma, size_t ss) {
    kernelAS.releaseStack(vma, ss);
  }

  /**** obtain/use context information ****/

  static Thread* getCurrThread() { return LocalProcessor::getCurrThread(); }
  static void setCurrThread(Thread* t) { LocalProcessor::setCurrThread(t); }
  static Scheduler* getScheduler() { return LocalProcessor::getScheduler(); }
  static void wakeUp(Scheduler* s) { Machine::sendWakeIPI(s); }

  /**** helper routines for scheduler ****/

  static void idleLoop(Scheduler*);

  /**** debug output routines ****/

  template<typename... Args>
  static void debugT(const Args&... a) { DBG::outl(DBG::Threads, a...); }

  template<typename... Args>
  static void debugS(const Args&... a) { DBG::outl(DBG::Scheduler, a...); }
}

#else
#error undefined platform: only __KOS__ supported at this time
#endif

#endif /* _Runtime_h_ */
