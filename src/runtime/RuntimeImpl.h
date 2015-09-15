/******************************************************************************
    Copyright � 2012-2015 Martin Karsten

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
#ifndef _RuntimeImpl_h_
#define _RuntimeImpl_h_ 1

#if defined(__KOS__)

#include "runtime/Runtime.h"
#include "runtime/Scheduler.h"
#include "kernel/Process.h"

namespace Runtime {

  template<size_t Levels>
  inline bool ReadyQueue<Levels>::push(Thread& t, mword prio) {
    ScopedLock<> sl(lock);
    readyQueue[prio].push_back(t);
    readyCount += 1;
    return (readyCount == 1);
  }

  template<size_t Levels>
  inline Thread* ReadyQueue<Levels>::pop(size_t maxlevel) {
    ScopedLock<> sl(lock);
    for (mword i = 0; i < maxlevel; i += 1) {
      if (!readyQueue[i].empty()) {
        readyCount -= 1;
        return readyQueue[i].pop_front();
      }
    }
    return nullptr;
  }

  template<size_t Levels>
  inline void ReadyQueue<Levels>::balanceWith(ReadyQueue& rq) {
  }

  static void idleLoop(Scheduler* s) {
    for (;;) {
      mword halt = s->preemption + 3;
      while (s->rq.empty() && sword(halt - s->preemption) > 0) CPU::Pause();
      halt = s->resumption;
      LocalProcessor::lock(true);
      s->preempt();
      LocalProcessor::unlock(true);
      while (halt == s->resumption) {
        if (!LocalProcessor::getFrameManager()->zeroMemory()) CPU::Halt();
      }
    }
    unreachable();
  }

  static AddressSpace& preThreadSwitch() {
    CurrAS().preThreadSwitch();
    return CurrAS();
  }

  static void postThreadResume(Thread* prevThread, AddressSpace& nextAS) {
    CHECK_LOCK_COUNT(1);
    AddressSpace* prevAS = nextAS.switchTo();
    nextAS.postThreadResume();
    if slowpath(prevThread) {            // cf. postSwitch() in Scheduler.cc
      prevThread->destroy();
      prevAS->postThreadDestroy();
    }
  }

}

#else
#error undefined platform: only __KOS__ supported at this time
#endif

#endif /* _RuntimeImpl_h_ */
