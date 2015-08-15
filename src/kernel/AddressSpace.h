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
#ifndef _AddressSpace_h_
#define _AddressSpace_h_ 1

#include "generic/IntrusiveContainers.h"
#include "kernel/FrameManager.h"
#include "kernel/Output.h"
#include "machine/Paging.h"

struct AsyncUnmapMarker : public IntrusiveList<AsyncUnmapMarker>::Link {
  sword enterEpoch;
};

// TODO: store shared & swapped virtual memory regions in separate data
// structures - checked during page fault (swapped) resp. unmap (shared)
class AddressSpace : public Paging {
  AddressSpace(const AddressSpace&) = delete;            // no copy
  AddressSpace& operator=(const AddressSpace&) = delete; // no assignment

  struct AsyncUnmapDescriptor : public IntrusiveList<AsyncUnmapDescriptor>::Link {
    vaddr  vma;
    paddr  pma;
    size_t size;
    bool   alloc;
    AsyncUnmapDescriptor(vaddr v, paddr p, size_t s, bool a)
      : vma(v), pma(p), size(s), alloc(a) {}
    AsyncUnmapDescriptor(const AsyncUnmapDescriptor&) = default;
    AsyncUnmapDescriptor& operator=(const AsyncUnmapDescriptor&) = default;
  };

  SpinLock ulock;          // lock protecting page invalidation data
  sword unmapEpoch;        // unmap epoch counter
  IntrusiveList<AsyncUnmapDescriptor> unmapList; // list of pages to unmap
  IntrusiveList<AsyncUnmapMarker> markerList;    // list of CPUs active in AS
  HeapCache<sizeof(AsyncUnmapDescriptor)> audCache;

  SpinLock vlock;          // lock protecting virtual address range
  vaddr mapBottom, mapStart, mapTop;

  const bool kernel;       // is this the unique kernel address space?
  paddr pagetable;         // root page table *physical* address

  enum MapCode { NoAlloc, Alloc, Guard, Lazy };

  template<size_t N>
  void mapRegion( paddr pma, vaddr vma, size_t size, uint64_t type, MapCode mc ) {
    static_assert( N > 0 && N < pagelevels, "page level template violation" );
    KASSERT1( NoAlloc <= mc && mc <= Lazy, mc );
    KASSERT1( aligned(vma,  pagesize<N>()), vma );
    KASSERT1( aligned(size, pagesize<N>()), size );
    if (mc == Lazy && kernel) mc = Alloc;
#if TESTING_NEVER_ALLOC_LAZY
    if (mc == Lazy) mc = Alloc;
#endif
    for (vaddr end = vma + size; vma < end; vma += pagesize<N>()) {
      if (mc == Alloc) pma = CurrFM().allocFrame<N>();
      DBG::outl(DBG::VM, "AS(", FmtHex(pagetable), ")/map<", N, ',', mc, ">: ", FmtHex(vma), " -> ", FmtHex(pma), " flags:", Paging::PageEntryFmt(type));
      switch (mc) {
        case NoAlloc:
        case Alloc:   Paging::mapPage<N>(vma, pma, type | owner()); break;
        case Guard:   Paging::mapToGuard<N>(vma); break;
        case Lazy:    Paging::mapToLazy<N>(vma); break;
        default:      KABORT0();
      }
      if (mc == NoAlloc) pma += pagesize<N>();
    }
  }

  template<size_t N>
  void unmapRegion( vaddr vma, size_t size, MapCode mc ) {
    static_assert( N > 0 && N < pagelevels, "page level template violation" );
    KASSERT1( NoAlloc <= mc && mc <= Alloc, mc );
    KASSERT1( aligned(vma, pagesize<N>()), vma );
    KASSERT1( aligned(size, pagesize<N>()), size );
    for (vaddr end = vma + size; vma < end; vma += pagesize<N>()) {
      paddr pma = Paging::unmap<N>(vma);
      bool alloc = (mc == Alloc && pma != guardPage && pma != lazyPage);
      DBG::outl(DBG::VM, "AS(", FmtHex(pagetable), ")/post: ", FmtHex(vma), '/', FmtHex(pagesize<N>()), " -> ", FmtHex(pma), " epoch:", unmapEpoch);
      ScopedLock<> sl(ulock);
      AsyncUnmapDescriptor* aud = new (audCache.allocate()) AsyncUnmapDescriptor(vma, pma, pagesize<N>(), alloc);
      unmapList.push_back(*aud);
      unmapEpoch += 1;
    }
  }

  void enter(AsyncUnmapMarker& aum) {
    aum.enterEpoch = unmapEpoch;
    markerList.push_back(aum);
  }

  template<bool invalidate>
  void leave(AsyncUnmapMarker& aum) {
    KASSERT0(!markerList.empty());
    bool front = (&aum == markerList.front());
    IntrusiveList<AsyncUnmapMarker>::remove(aum);

    // explicit TLB invalidation of TLB entries for pages removed since
    // 'enterEpoch'.  Invalidation iterates backwards -> easier!
    // In kernelSpace, pages are G(lobal), so TLBs not flushed at 'mov cr3'.
    if (invalidate) {
      AsyncUnmapDescriptor* aud = unmapList.back();
      for (sword e = unmapEpoch; e - aum.enterEpoch > 0; e -= 1) {
        KASSERT0(aud != unmapList.fence());
        DBG::outl(DBG::VM, "AS(", FmtHex(pagetable), ")/kinv: ", FmtHex(aud->vma), '/', FmtHex(aud->size), " -> ", FmtHex(aud->pma), " epoch:", e-1);
        CPU::InvTLB(aud->vma);
        aud = IntrusiveList<AsyncUnmapDescriptor>::prev(*aud);
      }
    } else KASSERT0(!kernel);

    // All unmap entries between 'oldest' and 'second-oldest' CPU are
    // flushed from all TLBs and can be removed from list.
    if (front) {
      sword end = markerList.empty() ? unmapEpoch : markerList.front()->enterEpoch;
      for (sword e = aum.enterEpoch; end - e > 0; e += 1) {
        KASSERT0(!unmapList.empty());
        AsyncUnmapDescriptor* aud = unmapList.front();
        DBG::outl(DBG::VM, "AS(", FmtHex(pagetable), ")/inv: ", FmtHex(aud->vma), '/', FmtHex(aud->size), " -> ", FmtHex(aud->pma), (aud->alloc ? "a" : ""), " epoch:", e);
        putVmRange(aud->vma, aud->size);
        if (aud->alloc) CurrFM().release(aud->pma, aud->size);
        IntrusiveList<AsyncUnmapDescriptor>::remove(*aud);
        audCache.deallocate(aud);
      }
    }
  }

  template<size_t N>
  vaddr getVmRange(vaddr addr, size_t& size) {
    KASSERT1(mapBottom < mapTop, "no AS memory break set yet");
    vaddr end = (addr >= mapBottom) ? align_up(addr + size, pagesize<N>()) : align_down(mapStart, pagesize<N>());
    vaddr start = align_down(end - size, pagesize<N>());
    if (start < mapBottom) goto allocFailed;
    if (start < mapStart) mapStart = start;
    size = end - start;
    DBG::outl(DBG::VM, "AS(", FmtHex(pagetable), ")/get: ", FmtHex(start), '-', FmtHex(end));
    return start;
allocFailed:
    KABORT0();
  }

  void putVmRange(vaddr addr, size_t size) {
    vaddr end = addr + size;
    ScopedLock<> sl(vlock);
    if (addr <= mapStart && end >= mapStart) {
      while ((size = Paging::test(end, Available)) && end + size <= mapTop) end += size;
      DBG::outl(DBG::VM, "AS(", FmtHex(pagetable), ")/put: ", FmtHex(mapStart), '-', FmtHex(end));
      mapStart = end;
    }
  }

  Paging::PageType owner() const { return kernel ? Kernel : User; }

public:
  inline AddressSpace(const bool k = false);

  ~AddressSpace() {
    KASSERT0(!kernel); // kernelSpace is never destroyed
    KASSERT0(pagetable != topaddr);
    DBG::outl(DBG::VM, "AS(", FmtHex(pagetable), ")/destruct:", FmtHex(pagetable));
    KASSERT1(pagetable != CPU::readCR3(), FmtHex(CPU::readCR3()));
    KASSERT0(unmapList.empty());
    KASSERT0(markerList.empty());
    CurrFM().release(pagetable, pagetableps);
  }

  bool user() const { return !kernel; }

  void initKernel(vaddr bot, vaddr top, paddr pt) {
    KASSERT0(kernel);
    pagetable = pt;
    mapBottom = bot;
    mapStart = mapTop = top;
  }

  void initUser(vaddr bssEnd) {
    KASSERT0(!kernel);
    mapBottom = bssEnd;
    mapStart = mapTop = usertop;
  }

  void initProcessor(AsyncUnmapMarker& aum) {
    KASSERT0(kernel);
    ScopedLock<> sl(ulock);
    if (markerList.empty()) {
      enter(aum);
      leave<true>(aum);
    }
    enter(aum);
  }

  inline void runInvalidation();

  AddressSpace& switchTo(bool destroyPrev = false) {
    KASSERT0(LocalProcessor::checkLock());
    AddressSpace* prevAS = LocalProcessor::getCurrAS();
    KASSERT0(prevAS);
    KASSERTN(prevAS->pagetable == CPU::readCR3(), FmtHex(prevAS->pagetable), ' ', FmtHex(CPU::readCR3()));
    KASSERT0(pagetable != topaddr);
    if (prevAS != this) {
      DBG::outl(DBG::Scheduler, "AS(", FmtHex(prevAS->pagetable), ")/switchTo: ", FmtHex(pagetable));
      AsyncUnmapMarker* aum = LocalProcessor::getUserAUM();
      if (prevAS->user()) {
        if (destroyPrev) {
          DBG::outl(DBG::VM, "AS(", FmtHex(pagetable), ")/destroy:", *prevAS);
          Paging::clear(align_down(userbot, pagesize<pagelevels>()), align_up(usertop, pagesize<pagelevels>()), CurrFM());
        }
        ScopedLock<> sl(prevAS->ulock);
        prevAS->leave<false>(*aum);
      } else {
        KASSERT0(!destroyPrev);
        prevAS->runInvalidation(); // prevAS == &kernelSpace
      }
      Paging::installPagetable(pagetable);
      LocalProcessor::setCurrAS(this);
      if (user()) {
        ScopedLock<> sl(ulock);
        enter(*aum);
      }
    } else {
      KASSERT0(!destroyPrev);
      runInvalidation();
    }
    return *prevAS;
  }

  template<size_t N, bool alloc=true>
  vaddr kmap(vaddr addr, size_t size, paddr pma = topaddr) {
    ScopedLock<> sl(vlock);
    addr = getVmRange<N>(addr, size);
    MapCode mc = alloc ? (kernel ? Alloc : Lazy) : NoAlloc;
    mapRegion<N>(pma, addr, size, Data, mc);
    return addr;
  }

  template<size_t N, bool alloc=true>
  void kunmap(vaddr addr, size_t size) {
    KASSERT1(aligned(addr, pagesize<N>()), addr);
    size = align_up(size, pagesize<N>());
    unmapRegion<N>(addr, size, alloc ? Alloc : NoAlloc);
  }

  bool tablefault(vaddr vma, uint64_t pff) {
    return Paging::mapTable<pagetablepl>(vma, pff, CurrFM());
  }

  bool pagefault(vaddr vma, uint64_t pff) {
    return Paging::mapFromLazy(vma, Data | owner(), pff, CurrFM());
  }

  vaddr allocStack(size_t ss) {
    KASSERT1(ss >= minimumStack, ss);
    size_t size = ss + stackGuardPage;
    ScopedLock<> sl(vlock);
    vaddr vma = getVmRange<stackpl>(0, size);
    KASSERTN(size == ss + stackGuardPage, ss, ' ', size);
    mapRegion<stackpl>(0, vma, stackGuardPage, Data, Guard);
    vma += stackGuardPage;
    MapCode mc = kernel ? Alloc : Lazy;
    mapRegion<stackpl>(0, vma, ss, Data, mc);
    return vma;
  }

  void releaseStack(vaddr vma, size_t ss) {
    unmapRegion<stackpl>(vma - stackGuardPage, ss + stackGuardPage, Alloc);
  }

  // allocate memory and map to specific virtual address: ELF loading
  template<size_t N, bool check=true>
  void allocDirect( vaddr vma, size_t size, PageType t ) {
    KASSERT1(!check || vma < mapBottom || vma > mapTop, vma);
    mapRegion<N>(0, vma, size, t, Alloc);
  }

  // map memory to specific virtual address: ELF loading
  template<size_t N, bool check=true>
  void mapDirect( paddr pma, vaddr vma, size_t size, PageType t ) {
    KASSERT1(!check || vma < mapBottom || vma > mapTop, vma);
    mapRegion<N>(pma, vma, size, t, NoAlloc);
  }

  // allocate and map contiguous physical memory: device buffers -> set MMapIO?
  vaddr allocContig( size_t size, paddr align, paddr limit) {
    paddr pma = CurrFM().allocRegion(size, align, limit);
    if (size < kernelps) return kmap<1,false>(0, size, pma);
    else return kmap<kernelpl,false>(0, size, pma);
  }

  void print(ostream& os) const;

  class Temp {
    AddressSpace* orig;
  public:
    Temp(AddressSpace& temp) {
      LocalProcessor::lock(true);
      orig = &temp.switchTo();
      LocalProcessor::unlock(true);
    }
    ~Temp() {
      LocalProcessor::lock(true);
      orig->switchTo();
      LocalProcessor::unlock(true);
    }
  };
};

inline ostream& operator<<(ostream& os, const AddressSpace& as) {
  as.print(os);
  return os;
}

extern AddressSpace kernelSpace;

inline AddressSpace::AddressSpace(const bool k) : unmapEpoch(0),
  mapBottom(0), mapStart(0), mapTop(0), kernel(k) {
  if (kernel) {
    pagetable = topaddr;
  } else { // shallow copy; if clone from user AS -> make deep copy!
    pagetable = CurrFM().allocFrame<pagetablepl>();
    Paging::clone(pagetable);
    DBG::outl(DBG::VM, "AS(", FmtHex(kernelSpace.pagetable), ")/cloned: ", FmtHex(pagetable));
  }
}

inline void AddressSpace::runInvalidation() {
  if (!kernel) kernelSpace.runInvalidation();
  ScopedLock<> sl(ulock);
  AsyncUnmapMarker* aum = kernel ? LocalProcessor::getKernAUM() : LocalProcessor::getUserAUM();
  leave<true>(*aum);
  enter(*aum);
}

static inline AddressSpace& CurrAS() {
  AddressSpace* as = LocalProcessor::getCurrAS();
  KASSERT0(as);
  return *as;
}

#endif /* _AddressSpace_h_ */
