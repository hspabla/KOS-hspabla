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

#include "generic/basics.h"
#include "kernel/SpinLock.h"

typedef SpinLock BasicLock;
typedef ScopedLock<BasicLock> AutoLock;

class AddressSpace;
class Scheduler;
class Thread;
class VirtualProcessor;

typedef AddressSpace& MemoryContext;

static const mword  topPriority = 0;
static const mword  defPriority = 1;
static const mword idlePriority = 2;
static const mword  maxPriority = 3;

#define CHECK_LOCK_MIN(x) \
  KASSERT1(LocalProcessor::checkLock() > (x), LocalProcessor::getLockCount())

#define CHECK_LOCK_COUNT(x) \
  KASSERT1(LocalProcessor::checkLock() == (x), LocalProcessor::getLockCount())

namespace Runtime {

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

  /**** obtain/use context information ****/

  static VirtualProcessor* thisProcessor() { return LocalProcessor::self(); }

}

#else
#error undefined platform: only __KOS__ supported at this time

class SystemProcessor{};
class MemoryContext{};

#endif

#endif /* _Runtime_h_ */
