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
#ifndef _IntrusiveContainer_h_
#define _IntrusiveContainer_h_ 1

#include "generic/basics.h"

template<typename T,size_t ID=0> class IntrusiveStack {
public:
  class Link {
    friend class IntrusiveStack<T,ID>;
    T* next;
  public:
    constexpr Link() : next(nullptr) {}
    bool onStack() { return next != nullptr; }
  } __packed;

private:
  T* head;

public:
  IntrusiveStack() : head(nullptr) {}
  bool empty() const { return head == nullptr; }

  T*              peek()       { return head; }
  const T*        peek() const { return head; }

  static T*       next(      T& elem) { return elem.Link::next; }
  static const T* next(const T& elem) { return elem.Link::next; }

  void push(T& elem) {
    GENASSERT1(!elem.onStack(), &elem);
    elem.Link::next = head;
    head = elem;
  }

  void push(T& first, T& last) {
    GENASSERT1(!last.onStack(), &first);
    last.Link::next = head;
    head = first;
  }

  T* pop(size_t count = 1) {
    GENASSERT1(!empty(), this);
    T* last = head;
    for (size_t i = 0; i < count; i += 1) {
      if slowpath(next(*last) == nullptr) break;
      last = next(*last);
    }
    head = next(*last);
    last->Link::next = nullptr;
    return last;
  }
} __packed;


template<typename T, size_t ID=0> class IntrusiveQueue {
public:
  class Link {
    friend class IntrusiveQueue<T,ID>;
    T* next;
  } __packed;

private:
  T* head;
  T* tail;

public:
  IntrusiveQueue() : head(nullptr), tail(nullptr) {}
  bool empty() const {
    GENASSERT1((head == nullptr) == (tail == nullptr), this);
    return head == nullptr;
  }

  T*              peek_front()       { return head; }
  const T*        peek_front() const { return head; }
  T*              peek_back()        { return tail; }
  const T*        peek_back()  const { return tail; }

  static T*       next(      T& elem) { return elem.Link::next; }
  static const T* next(const T& elem) { return elem.Link::next; }

  void push(T& first, T& last) {
    if slowpath(!head) head = &first;
    else {
      GENASSERT1(tail != nullptr, this);
      tail->Link::next = &first;
    }
    tail = &last;
  }

  void push(T& elem) {
    push(elem, elem);
  }

  T* pop(size_t count = 1) {
    GENASSERT1(!empty(), this);
    T* last = head;
    for (size_t i = 0; i < count; i += 1) {
      if slowpath(next(*last) == nullptr) break;
      last = next(*last);
    }
    head = next(*last);
    if slowpath(tail == last) tail = nullptr;
    return last;
  }

  void transfer(IntrusiveQueue& eq, size_t count) {
    T* first = eq.peek_front();
    T* last = eq.pop(count);
    push(*first, *last);
  }
} __packed;

// NOTE WELL: this simple design (using anchor) only works, if Link is first in T
template<typename T, size_t ID=0> class IntrusiveList {
public:
  class Link {
    friend class IntrusiveList<T,ID>;
    Link* prev;
    Link* next;
  public:
    constexpr Link() : prev(nullptr), next(nullptr) {}
#if 0
    ~Link() {
      if fastpath(onList()) {
        prev->next = next;
        next->prev = prev;
      }
    }
#endif
    bool onList() {
      GENASSERT1((prev == nullptr) == (next == nullptr), this);
      return next != nullptr;
    }
  } __packed;

private:
  Link anchor;

public:
  IntrusiveList() { anchor.next = anchor.prev = &anchor; }

  bool            empty() const { return &anchor == anchor.next; }
  Link*           fence()       { return &anchor; }
  const Link*     fence() const { return &anchor; }

  T*              front()       { return       (T*)anchor.next; }
  const T*        front() const { return (const T*)anchor.next; }
  T*              back()        { return       (T*)anchor.prev; }
  const T*        back()  const { return (const T*)anchor.prev; }

  static T*       next(      T& elem) { return       (T*)elem.Link::next; }
  static const T* next(const T& elem) { return (const T*)elem.Link::next; }
  static T*       prev(      T& elem) { return       (T*)elem.Link::prev; }
  static const T* prev(const T& elem) { return (const T*)elem.Link::prev; }

  static void insert_before(T& next, T& elem) {
    GENASSERT1(!elem.onList(), &elem);
    GENASSERT1(next.onList(), &prev);
    next.Link::prev->Link::next = &elem;
    elem.Link::prev = next.Link::prev;
    next.Link::prev = &elem;
    elem.Link::next = &next;
  }

  static void insert_after(T& prev, T& first, T&last) {
    GENASSERT1(first.Link::prev == nullptr, &first);
    GENASSERT1(last.Link::next == nullptr, &last);
    GENASSERT1(prev.onList(), &prev);
    prev.Link::next->Link::prev = &last;
    last.Link::next = prev.Link::next;
    prev.Link::next = &first;
    first.Link::prev = &prev;
  }

  static void insert_after(T& prev, T& elem) {
    insert_after(prev, elem, elem);
  }

  static T* remove(T& elem) {
    GENASSERT1(elem.onList(), &elem);
    elem.Link::prev->Link::next = elem.Link::next;
    elem.Link::next->Link::prev = elem.Link::prev;
    elem.Link::prev = nullptr;
    elem.Link::next = nullptr;
    return &elem;
  }

  T* remove(T& first, size_t& count) {
    GENASSERT1(first.onList(), &first);
    T* last = &first;
    for (size_t i = 0; i < count; i += 1) {
      if slowpath(next(*last) == fence()) count = i; // breaks loop and sets count
      else last = next(*last);
    }
    first.Link::prev->Link::next = last->Link::next;
    last->Link::next->Link::prev = first.Link::prev;
    first.Link::prev = nullptr;
    last->Link::next = nullptr;
    return last;
  }

  void push_front(T& elem)           { insert_before(*front(), elem); }
  void push_back(T& elem)            { insert_after (*back(),  elem); }
  void splice_back(T& first, T&last) { insert_after (*back(),  first, last); }

  T* pop_front() { GENASSERT1(!empty(), this); return remove(*front()); }
  T* pop_back()  { GENASSERT1(!empty(), this); return remove(*back()); }

  T* split_front(size_t& count) {
    GENASSERT1(!empty(), this);
    return remove(*front(), count);
  }

  void transfer(IntrusiveList& el, size_t& count) {
    T* first = el.front();
    T* last = el.split_front(count);
    splice_back(*first, *last);
  }
} __packed;

#endif /* _IntrusiveContainer_h_ */
