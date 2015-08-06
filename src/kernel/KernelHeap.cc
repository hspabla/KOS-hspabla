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
#include "kernel/AddressSpace.h"
#include "kernel/KernelHeap.h"
#include "kernel/Output.h"

#include "extern/dlmalloc/malloc_glue.h"
#include "extern/dlmalloc/malloc.h"

static SpinLock mallocLock;
static void*    mallocSpace;

struct DeferredUnmapDescriptor : public IntrusiveList<DeferredUnmapDescriptor>::Link {
  vaddr addr;
  size_t len;
  DeferredUnmapDescriptor(vaddr a, size_t l) : addr(a), len(l) {}
};

IntrusiveList<DeferredUnmapDescriptor>* mallocUnmapList = nullptr;

static_assert(DEFAULT_GRANULARITY == kernelps, "dlmalloc DEFAULT_GRANULARITY != kernelps");

void* dl_mmap(void* addr, size_t len, int, int, int, _off64_t) {
  KASSERT1( aligned(len, size_t(DEFAULT_GRANULARITY)), len);
  vaddr va = kernelSpace.kmap<kernelpl>(vaddr(addr), len);
  return (void*)va;
}

int dl_munmap(void* addr, size_t len) {
  KASSERT1( aligned(len, size_t(DEFAULT_GRANULARITY)), len);
  if (mallocUnmapList) {
    DeferredUnmapDescriptor* dud = new (addr) DeferredUnmapDescriptor(vaddr(addr), len);
    mallocUnmapList->push_back(*dud);
  } else {
    kernelSpace.kunmap<kernelpl>(vaddr(addr), len);
  }
  return 0;
}

void KernelHeap::init0( vaddr p, size_t s ) {
  mallocSpace = create_mspace_with_base((ptr_t)p, s, 0);
}

void KernelHeap::reinit( vaddr p, size_t s ) {
  destroy_mspace(mallocSpace);
  mallocSpace = create_mspace_with_base((ptr_t)p, s, 0);
}

ptr_t KernelHeap::legacy_malloc(size_t s) {
  ScopedLock<> sl(mallocLock);
  return mspace_malloc(mallocSpace, s);
}

// page unmapping can lead to allocation, which would lead to deadlock, thus
// must be deferred to after mallocLock is released
void KernelHeap::legacy_free(ptr_t p) {
  IntrusiveList<DeferredUnmapDescriptor> ulist;
  mallocLock.acquire();
  mallocUnmapList = &ulist;
  mspace_free(mallocSpace, p);
  mallocUnmapList = nullptr;
  mallocLock.release();
  while (!ulist.empty()) {
    DeferredUnmapDescriptor* dud = ulist.front();
    ulist.pop_front();
    kernelSpace.kunmap<kernelpl>(dud->addr, dud->len);
  }
}
