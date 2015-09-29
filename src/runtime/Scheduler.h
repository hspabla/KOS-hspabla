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
#include "runtime/Thread.h"

class BaseScheduler : public IntrusiveList<BaseScheduler>::Link {
protected:  // very simple N-class prio scheduling
  BasicLock lock;
  volatile size_t readyCount;
  IntrusiveList<Thread> readyQueue[maxPriority];

private:
  BaseScheduler* peer;
  BaseScheduler(const BaseScheduler&) = delete;            // no copy
  BaseScheduler& operator=(const BaseScheduler&) = delete; // no assignment

public:
  BaseScheduler() : readyCount(0), peer(this) {}
  virtual ~BaseScheduler() {}
  bool empty() const { return readyCount == 0; }
  void setPeer(BaseScheduler& p) { peer = &p; }

  Thread* dequeue(size_t maxlevel) {
    AutoLock al(lock);
    for (mword i = 0; i < maxlevel; i += 1) {
      if (!readyQueue[i].empty()) {
        readyCount -= 1;
        return readyQueue[i].pop_front();
      }
    }
    return nullptr;
  }

  void enqueue(Thread& t, _friend<Thread>) {
    Runtime::debugS("Thread ", FmtHex(&t), " queueing on ", FmtHex(this));
    GENASSERT1(t.getPriority() < maxPriority, t.getPriority());
    lock.acquire();
    bool wake = (readyCount == 0);
    readyCount += 1;
    readyQueue[t.getPriority()].push_back(t);
    lock.release();
    if (wake) wakeUp();
  }

  void enqueueBalanced(Thread& t, _friend<Thread> ft) {
#if TESTING_ALWAYS_MIGRATE
    t.setScheduler(*peer);
    peer->enqueue(t, ft);
#else /* simple load balancing */
    if (peer->readyCount + 2 < readyCount) {
      t.setScheduler(*peer);
      peer->enqueue(t, ft);
    } else {
      enqueue(t, ft);
    }
#endif
  }

  virtual void wakeUp() = 0;
};

class Scheduler : public BaseScheduler {
  SystemProcessor* idler;
public:
  Scheduler() : idler(nullptr) {}
  void reportIdle(SystemProcessor& sp) { idler = &sp; }
  virtual void wakeUp();
};

class GroupScheduler : public BaseScheduler {
  IntrusiveList<BaseScheduler> idleQueue;
public:
  bool reportIdle(BaseScheduler& bs) { return false; }
//  virtual void wakeUp();
};

#endif /* _Scheduler_h_ */
