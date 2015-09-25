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
#ifndef _Scheduler_h_
#define _Scheduler_h_ 1

#include "generic/IntrusiveContainers.h"
#include "runtime/Runtime.h"

class BaseScheduler : public IntrusiveList<BaseScheduler>::Link {
protected:  // very simple N-class prio scheduling
  BasicLock lock;
  volatile size_t readyCount;
  IntrusiveList<Thread> readyQueue[maxPriority];

private:
  BaseScheduler* parent;
  BaseScheduler* peer;
  BaseScheduler(const BaseScheduler&) = delete;            // no copy
  BaseScheduler& operator=(const BaseScheduler&) = delete; // no assignment
  inline void enqueue(Thread& t);

public:
  BaseScheduler() : readyCount(0), parent(this), peer(this) {}
  virtual ~BaseScheduler() {}
  bool empty() { return readyCount == 0; }
  void setPeer(BaseScheduler* p) { peer = p; }
  void setParent(BaseScheduler* p) { parent = p; }
  Thread* dequeue(size_t maxlevel);
  void ready(Thread& t, _friend<VirtualProcessor>);
  void resume(Thread& t);
  virtual void wakeUp() = 0;
};

class Scheduler : public BaseScheduler {
  VirtualProcessor* idler;
public:
  Scheduler() : idler(nullptr) {}
  bool reportIdle(VirtualProcessor& vp) {
    AutoLock al(lock);
    if (!empty()) return false;
    idler = &vp;
    return true;
  }
  virtual void wakeUp();
};

class GroupScheduler : public BaseScheduler {
  IntrusiveList<BaseScheduler> idleQueue;
public:
  bool reportIdle(BaseScheduler& bs) { return false; }
  virtual void wakeUp();
};

#endif /* _Scheduler_h_ */
