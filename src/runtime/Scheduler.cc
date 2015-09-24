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
#include "runtime/Scheduler.h"
#include "runtime/VirtualProcessor.h"

inline void BaseScheduler::enqueue(Thread& t) {
  Runtime::debugS("Thread ", FmtHex(&t), " queueing on ", FmtHex(this));
  GENASSERT1(t.getPriority() < maxPriority, t.getPriority());
  AutoLock al(lock);
  readyCount += 1;
  readyQueue[t.getPriority()].push_back(t);
  if (readyCount == 1) wakeUp();
}

Thread* BaseScheduler::dequeue(size_t maxlevel) {
  AutoLock al(lock);
  for (mword i = 0; i < maxlevel; i += 1) {
    if (!readyQueue[i].empty()) {
      readyCount -= 1;
      return readyQueue[i].pop_front();
    }
  }
  return nullptr;
}

void BaseScheduler::ready(Thread& t) {
#if TESTING_NEVER_MIGRATE
#else /* migration enabled */
#if TESTING_ALWAYS_MIGRATE
  if (!t.hasAffinity()) peer->enqueue(t);
#else /* simple load balancing */
  if (!t.hasAffinity() && peer->readyCount + 2 < readyCount) peer->enqueue(t);
#endif
  else
#endif
  enqueue(t);
}

void Scheduler::wakeUp() {
  if (idler) {
    Runtime::wake(*idler);
    idler = nullptr;
  }
}

void GroupScheduler::wakeUp() {
}
