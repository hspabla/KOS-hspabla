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
#include "runtime/RuntimeImpl.h"
#include "runtime/VirtualProcessor.h"

static inline void unlock() {}

template<typename... Args>
static inline void unlock(BasicLock &l, Args&... a) {
  l.release();
  unlock(a...);
}

template<typename... Args>
inline void VirtualProcessor::switchThread(bool yield, Args&... a) {
  CHECK_LOCK_MIN(sizeof...(Args));
  GENASSERT0(currThread);
  Thread* nextThread = scheduler.dequeue(yield ? idlePriority : maxPriority);
  unlock(a...); // REMEMBER: unlock early, because suspend/resume to same CPU!
  Runtime::debugS("Thread switch <", (yield ? 'Y' : 'S'), ">: ", FmtHex(currThread), " to ", FmtHex(nextThread));
  Thread* prevThread = nullptr;
  MemoryContext memctx = Runtime::preThreadSwitch();
  if (nextThread) {
    epoch += 1;
    GENASSERTN(nextThread != currThread, FmtHex(currThread), ' ', FmtHex(nextThread));
    currThread->nextScheduler = &scheduler;
    prevThread = currThread;
    currThread = nextThread;
    prevThread = stackSwitch(prevThread, yield ? postYield : postSuspend, &prevThread->stackPointer, currThread->stackPointer);
  } else {
    // no next thread, run post-switch anyway (AS switch/destroy, thread cancel)
    GENASSERT0(yield);
  }
  // REMEMBER: Thread might have migrated from other processor: 'this' is
  //           prevThread's processor -> separate switchThread2() routine!
  Runtime::postThreadSwitch(prevThread, memctx);
  Runtime::thisProcessor()->switchThread2();
}

// check if resumed thread is cancelled
inline void VirtualProcessor::switchThread2() {
  if (currThread->state == Thread::Cancelled) {
    currThread->state = Thread::Finishing;
    switchThread(false);
    unreachable();
  }
}

// return 'prevThread' only if the previous thread needs to be destroyed
Thread* VirtualProcessor::postYield(Thread* prevThread) {
  CHECK_LOCK_COUNT(1);
  GENASSERT1(!prevThread->finishing(), FmtHex(prevThread));
  prevThread->nextScheduler->ready(*prevThread, _friend<VirtualProcessor>());
  return nullptr;
}

// return 'prevThread' only if the previous thread needs to be destroyed
Thread* VirtualProcessor::postSuspend(Thread* prevThread) {
  CHECK_LOCK_COUNT(1);
  if slowpath(prevThread->finishing()) return prevThread;
  return nullptr;
}

extern "C" void invokeThread(Thread* prevThread, MemoryContext memctx, funcvoid3_t func, ptr_t arg1, ptr_t arg2, ptr_t arg3) {
  Runtime::postThreadSwitch(prevThread, memctx);
  Runtime::EnablePreemption();
  func(arg1, arg2, arg3);
  Runtime::thisProcessor()->terminate();
}

void VirtualProcessor::idleLoop(VirtualProcessor* This) {
  for (;;) {
    Runtime::spin(*This);
    mword e = This->epoch;
    for (;;) {
      This->yield();
      if (e != This->epoch) break;
      Runtime::idle(*This);
      if (e != This->epoch) break;
    }
  }
  unreachable();
}

void VirtualProcessor::yield() {
  Runtime::DisablePreemption dp(true);
  switchThread(true);
}

void VirtualProcessor::preempt() { // expect preemption already disabled
  switchThread(true);
}

void VirtualProcessor::suspend(BasicLock& lk) {
  Runtime::DisablePreemption dp;
  switchThread(false, lk);
}

void VirtualProcessor::suspend(BasicLock& lk1, BasicLock& lk2) {
  Runtime::DisablePreemption dp;
  switchThread(false, lk1, lk2);
}

void VirtualProcessor::terminate() {
  Runtime::DisablePreemption dp(true);
  GENASSERT1(currThread->state != Thread::Blocked, currThread->state);
  currThread->state = Thread::Finishing;
  switchThread(false);
  unreachable();
}

void VirtualProcessor::start(funcvoid0_t func) {
  Thread* idleThread = Thread::create(idleStack);
  idleThread->setAffinity(&scheduler)->setPriority(idlePriority);
  idleThread->setup(Runtime::defaultAS(), (ptr_t)idleLoop, this, nullptr, nullptr);
  idleThread->resume();
  currThread = Thread::create(defaultStack);
  currThread->setAffinity(&scheduler)->direct((ptr_t)func, nullptr);
}
