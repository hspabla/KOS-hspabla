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
#include "runtime/RuntimeImpl.h"
#include "runtime/Scheduler.h"
#include "runtime/Stack.h"
#include "runtime/Thread.h"
#include "kernel/Output.h"

void Scheduler::init(Scheduler& p) {
  partner = &p;
  Thread* idleThread = Thread::create(idleStack);
  idleThread->setAffinity(this)->setPriority(idlePriority);
  // use low-level routines, since runtime context might not exist
  idleThread->stackPointer = stackInit(idleThread->stackPointer, &Runtime::getDefaultMemoryContext(), (ptr_t)Runtime::idleLoop, this, nullptr, nullptr);
  rq.push(*idleThread, idlePriority);
}

static inline void unlock() {}

template<typename... Args>
static inline void unlock(BasicLock &l, Args&... a) {
  l.release();
  unlock(a...);
}

// very simple N-class prio scheduling!
template<typename... Args>
inline bool Scheduler::switchThread(Scheduler* target, Args&... a) {
  preemption += 1;
  CHECK_LOCK_MIN(sizeof...(Args));
  Thread* nextThread = rq.pop((target == this) ? idlePriority : maxPriority);
  if (!nextThread) {
    GENASSERT1(target == this, FmtHex(target));
    GENASSERT0(!sizeof...(Args));
    return false;                                 // return to current thread
  }

  resumption += 1;
  Thread* currThread = Runtime::getCurrThread();
  GENASSERTN(currThread && nextThread && nextThread != currThread, currThread, ' ', nextThread);

  if (target) currThread->nextScheduler = target; // yield/preempt to given processor
  else currThread->nextScheduler = this;          // suspend/resume to same processor
  unlock(a...);                                   // ...thus can unlock now
  CHECK_LOCK_COUNT(1);
  Runtime::debugS("Thread switch <", (target ? 'Y' : 'S'), ">: ", FmtHex(currThread), " to ", FmtHex(nextThread));

  Runtime::MemoryContext& ctx = Runtime::preThreadSwitch();
  Runtime::setCurrThread(nextThread);
  Thread* prevThread = stackSwitch(currThread, target, &currThread->stackPointer, nextThread->stackPointer);
  // REMEMBER: Thread might have migrated from other processor, so 'this'
  //           might not be currThread's Scheduler object anymore.
  //           However, 'this' points to prevThread's Scheduler object.
  Runtime::postThreadResume(prevThread, ctx);
  if (currThread->state == Thread::Cancelled) {
    currThread->state = Thread::Finishing;
    switchThread(nullptr);
    unreachable();
  }
  return true;
}

// return 'prevThread' only if the previous thread needs to be destroyed
extern "C" Thread* postSwitch(Thread* prevThread, Scheduler* target) {
  CHECK_LOCK_COUNT(1);
  if slowpath(prevThread->finishing()) return prevThread;
  if fastpath(target) Scheduler::resume(*prevThread);
  return nullptr;
}

extern "C" void invokeThread(Thread* prevThread, Runtime::MemoryContext* ctx, funcvoid3_t func, ptr_t arg1, ptr_t arg2, ptr_t arg3) {
  Runtime::postThreadResume(prevThread, *ctx);
  Runtime::EnablePreemption();
  func(arg1, arg2, arg3);
  Runtime::getScheduler()->terminate();
}

void Scheduler::enqueue(Thread& t) {
  GENASSERT1(t.priority < maxPriority, t.priority);
  bool wake = rq.push(t, t.priority);
  Runtime::debugS("Thread ", FmtHex(&t), " queued on ", FmtHex(this));
  if (wake) Runtime::wakeUp(this);
}

void Scheduler::resume(Thread& t) {
  GENASSERT1(&t != Runtime::getCurrThread(), Runtime::getCurrThread());
  if (t.nextScheduler) t.nextScheduler->enqueue(t);
  else Runtime::getScheduler()->enqueue(t);
}

bool Scheduler::yield() {
  Runtime::DisablePreemption dp(true);
  return preempt();
}

bool Scheduler::preempt() {        // expect IRQs disabled, lock count inflated
#if TESTING_NEVER_MIGRATE
  return switchThread(this);
#else /* migration enabled */
  Scheduler* target = Runtime::getCurrThread()->getAffinity();
#if TESTING_ALWAYS_MIGRATE
  if (!target) target = partner;
#else /* simple load balancing */
  if (!target) target = (partner->rq.size() + 2 < rq.size()) ? partner : this;
#endif
  return switchThread(target);
#endif
}

void Scheduler::suspend(BasicLock& lk) {
  Runtime::DisablePreemption dp;
  switchThread(nullptr, lk);
}

void Scheduler::suspend(BasicLock& lk1, BasicLock& lk2) {
  Runtime::DisablePreemption dp;
  switchThread(nullptr, lk1, lk2);
}

void Scheduler::terminate() {
  Runtime::DisablePreemption dp(true);
  Thread* thr = Runtime::getCurrThread();
  GENASSERT1(thr->state != Thread::Blocked, thr->state);
  thr->state = Thread::Finishing;
  switchThread(nullptr);
  unreachable();
}
