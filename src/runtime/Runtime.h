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

#include "kernel/Output.h"
#include "kernel/SpinLock.h"

typedef SpinLock BasicLock;
typedef ScopedLock<BasicLock> AutoLock;

class AddressSpace;
class FrameManager;
class Thread;
class SystemProcessor;

#define CHECK_LOCK_MIN(x) \
  KASSERT1(LocalProcessor::checkLock() > (x), LocalProcessor::getLockCount())

#define CHECK_LOCK_COUNT(x) \
  KASSERT1(LocalProcessor::checkLock() == (x), LocalProcessor::getLockCount())

struct Runtime {
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

  /**** debug output ****/

  template<typename... Args>
  static inline void debugS(const Args&... a) { DBG::outl(DBG::Scheduler, a...); }

  template<typename... Args>
  static inline void debugT(const Args&... a) { DBG::outl(DBG::Threads, a...); }

  /**** AddressSpace-related interface ****/

  typedef AddressSpace& MemoryContext;

  static inline vaddr allocThreadStack(size_t ss);
  static inline void releaseThreadStack(vaddr vma, size_t ss);

  /**** idle loop ****/

  static inline void idleLoop();
  static inline void wake(SystemProcessor& sp);

  /**** thread switch ****/

  static inline Thread* CurrThread() {
    Thread* t = LocalProcessor::getCurrThread(_friend<Runtime>());
    KASSERT0(t);
    return t;
  }
  static inline void preThreadSwitch(Thread* nextThread);
  static inline void postThreadSwitch(Thread* prevThread);
};

static Thread* CurrThread() { return Runtime::CurrThread(); }

#else
#error undefined platform: only __KOS__ supported at this time

class SystemProcessor{};
class MemoryContext{};

#endif

#endif /* _Runtime_h_ */
