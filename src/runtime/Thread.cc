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
#include "runtime/BlockingSync.h"
#include "runtime/RuntimeImpl.h"
#include "runtime/Scheduler.h"
#include "runtime/Thread.h"
#include "kernel/Output.h"

Thread* Thread::create(size_t ss) {
  vaddr mem = Runtime::allocThreadStack(ss);
  vaddr This = mem + ss - sizeof(Thread);
  Runtime::debugT("Thread create: ", FmtHex(mem), '/', FmtHex(ss), '/', FmtHex(This));
  return new (ptr_t(This)) Thread(mem, ss, defaultAS);
}

void Thread::destroy() {
  GENASSERT1(state == Finishing, state);
  GENASSERT1(unblockInfo == nullptr, FmtHex(unblockInfo));
  Runtime::debugT("Thread destroy: ", FmtHex(stackBottom), '/', FmtHex(stackSize), '/', FmtHex(this));
  Runtime::releaseThreadStack(stackBottom, stackSize);
}

static inline void unlock() {}

template<typename... Args>
static inline void unlock(BasicLock &l, Args&... a) {
  l.release();
  unlock(a...);
}

template<typename... Args>
inline void Thread::switchThread(bool yield, Args&... a) {
  CHECK_LOCK_MIN(sizeof...(Args));
  Thread* nextThread = scheduler->dequeue(yield ? idlePriority : maxPriority);
  unlock(a...); // REMEMBER: unlock early, because suspend/resume to same CPU!
  Runtime::debugS("Thread switch <", (yield ? 'Y' : 'S'), ">: ", FmtHex(this), " to ", FmtHex(nextThread));
  if (nextThread) {
    GENASSERTN(this != nextThread, FmtHex(this), ' ', FmtHex(nextThread));
    Runtime::preThreadSwitch(nextThread);
    Thread* prevThread = stackSwitch(this, yield ? postYield : postSuspend, &stackPointer, nextThread->stackPointer);
    Runtime::postThreadSwitch(prevThread);
  } else {
    // no next thread, run post-switch anyway (AS switch/destroy, thread cancel)
    GENASSERT0(yield);
    Runtime::preThreadSwitch(this);
  }
  if (state == Cancelled) {
    state = Finishing;
    switchThread(false);
    unreachable();
  }
}

// return 'prevThread' only if the previous thread needs to be destroyed
Thread* Thread::postYield(Thread* prevThread) {
  CHECK_LOCK_COUNT(1);
  GENASSERT1(!prevThread->finishing(), FmtHex(prevThread));
#if TESTING_NEVER_MIGRATE
  prevThread->scheduler->enqueue(*prevThread, _friend<Thread>());
#else /* migration enabled */
  if (prevThread->affinity) prevThread->scheduler->enqueue(*prevThread, _friend<Thread>());
  else prevThread->scheduler->enqueueBalanced(*prevThread, _friend<Thread>());
#endif
  return nullptr;
}

// return 'prevThread' only if the previous thread needs to be destroyed
Thread* Thread::postSuspend(Thread* prevThread) {
  CHECK_LOCK_COUNT(1);
  if slowpath(prevThread->finishing()) return prevThread;
  return nullptr;
}

extern "C" void invokeThread(Thread* prevThread, funcvoid3_t func, ptr_t arg1, ptr_t arg2, ptr_t arg3) {
  Runtime::postThreadSwitch(prevThread);
  Runtime::EnablePreemption();
  func(arg1, arg2, arg3);
  CurrThread()->terminate();
}

void Thread::yield() {
  Runtime::DisablePreemption dp(true);
  switchThread(true);
}

void Thread::preempt() { // expect preemption already disabled
  switchThread(true);
}

void Thread::suspend(BasicLock& lk) {
  Runtime::DisablePreemption dp;
  switchThread(false, lk);
}

void Thread::suspend(BasicLock& lk1, BasicLock& lk2) {
  Runtime::DisablePreemption dp;
  switchThread(false, lk1, lk2);
}

void Thread::terminate() {
  Runtime::DisablePreemption dp(true);
  state = Thread::Finishing;
  switchThread(false);
  unreachable();
}

void Thread::resume() {
  GENASSERT0(scheduler);
  scheduler->enqueue(*this, _friend<Thread>());
}

void Thread::cancel() {
  GENASSERT1(this != CurrThread(), CurrThread());
  state = Cancelled;
  UnblockInfo* ubi = getUnblockInfo();
  if (ubi) {
    ubi->cancelTimeout();
    ubi->cancelEvent(*this);
    resume();
  }
}
