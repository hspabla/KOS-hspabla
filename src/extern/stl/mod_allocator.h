#ifndef _mod_allocator_h_
#define _mod_allocator_h_

#include <cstdint>

template<typename T,size_t shift = 0> class InPlaceAllocator : public std::allocator<T> {
public:
  template<typename U> struct rebind { typedef InPlaceAllocator<U,shift> other; };
  InPlaceAllocator() = default;
  InPlaceAllocator(const InPlaceAllocator& x) = default;
  template<typename U> InPlaceAllocator (const InPlaceAllocator<U,shift>& x) : std::allocator<T>(x) {}
  ~InPlaceAllocator() = default;
  template<typename V>
  T* allocate(const V& val) {
		uintptr_t addr = (uintptr_t)(void*)(const_cast<V&>(val));
		addr = addr << shift;
		return reinterpret_cast<T*>(addr);
	}
  void deallocate(T*, size_t) {}
};

template<typename T> class IntrusiveAllocator : public std::allocator<T> {
public:
  template<typename U> struct rebind { typedef IntrusiveAllocator<U> other; };
  IntrusiveAllocator() = default;
  IntrusiveAllocator(const IntrusiveAllocator& x) = default;
  template<typename U> IntrusiveAllocator (const IntrusiveAllocator<U>& x) : std::allocator<T>(x) {}
  ~IntrusiveAllocator() = default;
  template<typename V>
  T* allocate(const V& val) { return reinterpret_cast<T*>(val->alloc()); }
  void deallocate(T*, size_t) {}
};

#endif /* _mod_allocator_h_ */
