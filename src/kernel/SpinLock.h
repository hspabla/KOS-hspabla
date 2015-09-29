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
#ifndef _SpinLock_h_
#define _SpinLock_h_ 1

#include "machine/CPU.h"
#include "machine/Processor.h"

/* TODO: add (exponential) backoff spin to spinlocks */

class BinarySpinLock {
  volatile bool locked;
public:
  BinarySpinLock() : locked(false) {}
  bool check() const { return locked; }
  bool tryAcquire() {
    KASSERT0(!CPU::interruptsEnabled());
    return !__atomic_test_and_set(&locked, __ATOMIC_ACQUIRE);
  }
  void acquire() {
    KASSERT0(!CPU::interruptsEnabled());
    for (;;) {
      if fastpath(!__atomic_test_and_set(&locked, __ATOMIC_ACQUIRE)) return;
      while (locked) CPU::Pause();
    }
  }
  void release() {
    KASSERT0(!CPU::interruptsEnabled());
    KASSERT0(check());
    __atomic_clear(&locked, __ATOMIC_RELEASE);
  }
} __caligned;

template<typename T, T noOwner>
class OwnerSpinLock {
  volatile T owner;
  mword counter;
public:
  OwnerSpinLock() : owner(noOwner) {}
  bool check() const { return owner != noOwner; }
  bool tryAcquire(T requestor) {
    KASSERT0(!CPU::interruptsEnabled());
    if (owner != requestor) {
      T expected = noOwner;
      if slowpath(!__atomic_compare_exchange_n(&owner, &expected, requestor, 0, __ATOMIC_ACQUIRE, __ATOMIC_RELAXED)) {
        return false;
      }
    }
    counter += 1;
    return true;
  }
  void acquire(T requestor) {
    KASSERT0(!CPU::interruptsEnabled());
    if (owner != requestor) {
      for (;;) {
        T expected = noOwner;
        if fastpath(__atomic_compare_exchange_n(&owner, &expected, requestor, 0, __ATOMIC_ACQUIRE, __ATOMIC_RELAXED)) break;
        while (owner != noOwner) CPU::Pause();
      }
    }
    counter += 1;
  }
  void release(T requestor) {
    KASSERT0(!CPU::interruptsEnabled());
    KASSERT0(owner == requestor);
    counter -= 1;
    if (counter == 0) __atomic_store_n(&owner, noOwner, __ATOMIC_RELEASE);
  }
} __caligned;

class TicketSpinLock {
  volatile mword serving;
  mword ticket;
public:
  TicketSpinLock() : serving(0), ticket(0) {}
  bool check() const { return sword(ticket-serving) > 0; }
  bool tryAcquire() {
    KASSERT0(!CPU::interruptsEnabled());
    mword tryticket = serving;
    return __atomic_compare_exchange_n(&ticket, &tryticket, tryticket + 1, 0, __ATOMIC_ACQUIRE, __ATOMIC_RELAXED);
  }
  void acquire() {
    KASSERT0(!CPU::interruptsEnabled());
    mword myticket = __atomic_fetch_add(&ticket, 1, __ATOMIC_ACQUIRE);
    while (myticket != serving) CPU::Pause();
  }
  void release() {
    KASSERT0(!CPU::interruptsEnabled());
    KASSERT0(check());
    __atomic_fetch_add(&serving, 1, __ATOMIC_RELEASE);
  }
};

class SpinLock : protected BinarySpinLock {
public:
  bool tryAcquire() {
    LocalProcessor::lock();
    if (BinarySpinLock::tryAcquire()) return true;
    LocalProcessor::unlock();
    return false;
  }
  void acquire(SpinLock* l = nullptr) {
    LocalProcessor::lock();
    BinarySpinLock::acquire();
    if (l) l->release();
  }
  void release() {
    BinarySpinLock::release();
    LocalProcessor::unlock();
  }
  bool check() const { return BinarySpinLock::check(); }
};

class OwnerLock : protected OwnerSpinLock<mword,limit<mword>()> {
public:
  bool tryAcquire() {
    LocalProcessor::lock();
    if (OwnerSpinLock::tryAcquire(LocalProcessor::getIndex())) return true;
    LocalProcessor::unlock();
    return false;
  }
  void acquire(SpinLock* l = nullptr) {
    LocalProcessor::lock();
    OwnerSpinLock::acquire(LocalProcessor::getIndex());
    if (l) l->release();
  }
  void release() {
    OwnerSpinLock::release(LocalProcessor::getIndex());
    LocalProcessor::unlock();
  }
  bool check() const { return OwnerSpinLock::check(); }
};

class NoLock {
public:
  void acquire() {}
  void release() {}
};

template <typename Lock = SpinLock>
class ScopedLock {
  Lock& lk;
public:
  ScopedLock(Lock& lk) : lk(lk) { lk.acquire(); }
  ~ScopedLock() { lk.release(); }
};

template <>
class ScopedLock<LocalProcessor> {
public:
  ScopedLock() { LocalProcessor::lock(); }
  ~ScopedLock() { LocalProcessor::unlock(); }
};

#endif /* _SpinLock_h_ */
