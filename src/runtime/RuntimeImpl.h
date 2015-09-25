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
#ifndef _RuntimeImpl_h_
#define _RuntimeImpl_h_ 1

#if defined(__KOS__)

#include "runtime/Runtime.h"
#include "runtime/Scheduler.h"
#include "kernel/Clock.h"
#include "kernel/Process.h"
#include "machine/Machine.h"

namespace Runtime {

  /**** debug output routines ****/

  template<typename... Args>
  static void debugI(const Args&... a) { DBG::outl(DBG::Idle, a...); }

  template<typename... Args>
  static void debugS(const Args&... a) { DBG::outl(DBG::Scheduler, a...); }

  template<typename... Args>
  static void debugT(const Args&... a) { DBG::outl(DBG::Threads, a...); }

  /**** AddressSpace-related interface ****/

  static MemoryContext currentAS() { return ::CurrAS(); }
  static MemoryContext defaultAS() { return ::defaultAS; }

  static vaddr allocThreadStack(size_t ss) {
    return kernelAS.allocStack(ss);
  }

  static void releaseThreadStack(vaddr vma, size_t ss) {
    kernelAS.releaseStack(vma, ss);
  }

  /**** idle loop ****/

  static void spin(VirtualProcessor& vp) {
    mword tick = Clock::now() + 10;
    while (vp.empty() && sword(tick - Clock::now()) > 0) CPU::Pause();
  }

  static void idle(VirtualProcessor& vp) {
    if (!CurrFM().zeroMemory()) {
      Runtime::debugI("idle halt");
      if (vp.getScheduler()->reportIdle(vp)) CPU::Halt();
    }
  }

  static void wake(VirtualProcessor& vp) {
    Runtime::debugI("send WakeIPI to ", vp.getIndex());
    vp.sendWakeIPI();
  }

  /**** thread switch ****/

  static MemoryContext preThreadSwitch() {
    CurrAS().preThreadSwitch();
    return CurrAS();
  }

  static void postThreadSwitch(Thread* prevThread, MemoryContext nextAS) {
    CHECK_LOCK_COUNT(1);
    AddressSpace& prevAS = nextAS.switchTo();
    nextAS.postThreadResume();
    if slowpath(prevThread) {            // cf. postSwitch() in Scheduler.cc
      prevThread->destroy();
      prevAS.postThreadDestroy();
    }
  }
}

#else
#error undefined platform: only __KOS__ supported at this time
#endif

#endif /* _RuntimeImpl_h_ */
