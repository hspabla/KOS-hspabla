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
#ifndef _VirtualProcessor_h_
#define _VirtualProcessor_h_ 1

#include "runtime/Scheduler.h"
#include "kernel/SystemProcessor.h"

class VirtualProcessor : public IntrusiveList<VirtualProcessor>::Link, public SystemProcessor {
  Scheduler scheduler;
  Thread* volatile currThread;
  volatile mword epoch;
  template<typename... Args>
  inline void switchThread(bool yield, Args&... a);
  inline void switchThread2();
  static Thread* postYield(Thread* prevThread);
  static Thread* postSuspend(Thread* prevThread);
  static void idleLoop(VirtualProcessor*);
  VirtualProcessor(const VirtualProcessor&) = delete;            // no copy
  VirtualProcessor& operator=(const VirtualProcessor&) = delete; // no assignment
public:
  VirtualProcessor() : currThread(nullptr), epoch(0) {}
  void yield();
  void preempt();
  void suspend(BasicLock& lk);
  void suspend(BasicLock& lk1, BasicLock& lk2);
  void terminate() __noreturn;
  void start(funcvoid0_t func);
  Thread* getCurrThread() { return currThread; }
  Scheduler* getScheduler() { return &scheduler; }
};

#endif /* _VirtualProcessor_h_ */
