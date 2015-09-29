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
#include "kernel/AddressSpace.h"
#include "kernel/Clock.h"

inline vaddr Runtime::allocThreadStack(size_t ss) {
  return kernelAS.allocStack(ss);
}

inline void Runtime::releaseThreadStack(vaddr vma, size_t ss) {
  kernelAS.releaseStack(vma, ss);
}

void Runtime::idleLoop() {
  for (;;) {
    mword e = LocalProcessor::getEpoch();
    mword tick = Clock::now() + 10;
    while (e == LocalProcessor::getEpoch()) {
      if (Clock::now() < tick) CPU::Pause();
      else if (!CurrFM().zeroMemory()) {
        DBG::outl(DBG::Idle, "idle halt");
        CPU::Halt();
      }
      CurrThread()->yield();
    }
  }
}

inline void Runtime:: wake(SystemProcessor& sp) {
  DBG::outl(DBG::Idle, "send WakeIPI to ", sp.getIndex());
  sp.sendWakeIPI();
}

inline void Runtime::preThreadSwitch(Thread* nextThread) {
  CHECK_LOCK_COUNT(1);
  AddressSpace& nextAS = nextThread->getMemCtx();
  AddressSpace& currAS = CurrThread()->getMemCtx();
  currAS.preThreadSwitch();
  currAS.switchTo(nextAS);
  LocalProcessor::setCurrThread(nextThread, _friend<Runtime>());
}

inline void Runtime::postThreadSwitch(Thread* prevThread) {
  CHECK_LOCK_COUNT(1);
  AddressSpace& nextAS = CurrThread()->getMemCtx();
  nextAS.postThreadResume();
  if slowpath(prevThread) {            // cf. postSwitch() in Scheduler.cc
    AddressSpace& prevAS = prevThread->getMemCtx();
    prevThread->destroy();
    prevAS.postThreadDestroy();
  }
}

#else
#error undefined platform: only __KOS__ supported at this time
#endif

#endif /* _RuntimeImpl_h_ */
