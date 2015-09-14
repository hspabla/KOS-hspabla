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
#ifndef _KernelHeap_h_
#define _KernelHeap_h_ 1

#include "machine/Memory.h"
#include "machine/SpinLock.h"

extern "C" void free(void* p);
extern "C" void* malloc(size_t);

/* KernelHeap is where complex memory algorithms could be implemented */
class KernelHeap : public NoObject {
  friend void free(void*);
  friend void* malloc(size_t);
  static ptr_t legacy_malloc(size_t s);
  static void legacy_free(ptr_t p);

public:
  static void init0( vaddr p, size_t s );
  static void reinit( vaddr p, size_t s );
  static vaddr alloc( size_t s ) { return (vaddr)legacy_malloc(s); }
  static void release( vaddr p, size_t s ) { legacy_free((ptr_t)p); }
};

template<typename T>
T* kmalloc(size_t n = 1) {
  return (T*)KernelHeap::alloc(n * sizeof(T));
}

template<typename T>
void kfree(T* p, size_t n = 1) {
  KernelHeap::release((vaddr)p, n * sizeof(T));
}

template<typename T, typename... Args>
T* knew(Args&&... a) {
  return new (kmalloc<T>()) T(std::forward<Args>(a)...);
}

template<typename T>
T* knewN(size_t n) {
  return new (kmalloc<T>(n)) T[n];
}

template<typename T>
void kdelete(T* p, size_t n = 1) {
  for (size_t i = 0; i < n; i += 1) p[i].~T();
  kfree<T>(p, n);
}

template<typename T> class KernelAllocator : public allocator<T> {
public:
  template<typename U> struct rebind { typedef KernelAllocator<U> other; };
  KernelAllocator() = default;
  KernelAllocator(const KernelAllocator& x) = default;
  template<typename U> KernelAllocator (const KernelAllocator<U>& x) : allocator<T>(x) {}
  ~KernelAllocator() = default;
  T* allocate(size_t n, const void* = 0) { return kmalloc<T>(n); }
  void deallocate(T* p, size_t n) { kfree<T>(p, n); }
};

template<size_t Size, int ID=0> class HeapCache {
  HeapCache(const HeapCache&) = delete;                        // no copy
  HeapCache& operator=(const HeapCache&) = delete;             // no assignment
  struct Free { Free* next; };
  Free* freestack;
  static_assert(Size >= sizeof(Free*), "Size < sizeof(Free*)");
public:
  HeapCache() : freestack(nullptr) {}
  ~HeapCache() {
    while (freestack) KernelHeap::release(vaddr(allocate()), Size);
  }
  ptr_t allocate() {
    if (!freestack) return (Free*)KernelHeap::alloc(Size);
    Free* ptr = freestack;
    freestack = freestack->next;
    return ptr;
  }
  void deallocate(ptr_t p) {
#if TESTING_MEMORY_NO_CACHE
    KernelHeap::release(vaddr(p), Size);
#else
    ((Free*)p)->next = freestack;
    freestack = (Free*)p;
#endif
  }
};

#endif /* _KernelHeap_h_ */
