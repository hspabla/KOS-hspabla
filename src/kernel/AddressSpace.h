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

#include "runtime/Runtime.h"
#include "kernel/SystemProcessor.h"
#include "kernel/FrameManager.h"
#include "machine/Paging.h"

#include "extern/stl/mod_set"

// TODO: store shared & swapped virtual memory regions in separate data
// structures - checked during page fault (swapped) resp. unmap (shared)
class BaseAddressSpace : public Paging {
  BaseAddressSpace(const BaseAddressSpace&) = delete;            // no copy
  BaseAddressSpace& operator=(const BaseAddressSpace&) = delete; // no assignment

  struct MemoryDescriptor : public IntrusiveList<MemoryDescriptor>::Link {
    vaddr  vma;
    paddr  pma;
    size_t size;
    bool   alloc;
    mword  dummy; // sizeof(MemoryDescriptor) >= sizeof(UnmapDescriptor)
    MemoryDescriptor(vaddr v, paddr p, size_t s, bool a)
      : vma(v), pma(p), size(s), alloc(a) {}
    MemoryDescriptor(const MemoryDescriptor&) = default;
    MemoryDescriptor& operator=(const MemoryDescriptor&) = default;
  };

  struct UnmapDescriptor : public mod_set_elem<UnmapDescriptor*> {
    vaddr start;
    vaddr end;
    UnmapDescriptor(vaddr s, vaddr e) : start(s), end(e) {}
    UnmapDescriptor(const UnmapDescriptor&) = default;
    UnmapDescriptor& operator=(const UnmapDescriptor&) = default;
    bool operator<(const UnmapDescriptor& u) { return start < u.start; }
  };

  static_assert(sizeof(MemoryDescriptor) >= sizeof(UnmapDescriptor), "type sizes");

protected:
  SpinLock ulock;                      // lock protecting page invalidation data
  sword unmapEpoch;                             // unmap epoch counter
  IntrusiveList<MemoryDescriptor> memoryList;   // list of pages to unmap
  IntrusiveList<AddressSpaceMarker> markerList; // list of CPUs active in AS
  HeapCache<sizeof(MemoryDescriptor)> mdCache;
  IntrusiveStdSet<UnmapDescriptor*> unmapSet;

  SpinLock vlock;                      // lock protecting virtual address range
  vaddr mapBottom, mapStart, mapTop;

  enum MapCode { NoAlloc, Alloc, Guard, Lazy };

  BaseAddressSpace() : unmapEpoch(0), mapBottom(0), mapStart(0), mapTop(0) {}
  virtual ~BaseAddressSpace() {
    KASSERT0(memoryList.empty());
    KASSERT0(markerList.empty());
  }

  void setup(vaddr bot, vaddr top) {
    mapBottom = bot;
    mapStart = mapTop = top;
  }

  static void verifyPT(paddr pt) {
    KASSERTN( pt == CPU::readCR3(), FmtHex(pt), ' ', FmtHex(CPU::readCR3()));
  }

  static constexpr MapCode AllocCode(PageType owner) {
#if TESTING_NEVER_ALLOC_LAZY
    return Alloc;
#else
    return owner == Kernel ? Alloc : Lazy;
#endif
  }

  template<size_t N>
  vaddr getVmRange(vaddr addr, size_t& size) {
    KASSERT1(mapBottom < mapTop, "no AS memory break set yet");
    vaddr end = (addr >= mapBottom) ? align_up(addr + size, pagesize<N>()) : align_down(mapStart, pagesize<N>());
    vaddr start = align_down(end - size, pagesize<N>());
    if (start < mapBottom) goto allocFailed;
    if (start < mapStart) mapStart = start;
    size = end - start;
    DBG::outl(DBG::VM, "AS(", FmtHex(this), ")/get: ", FmtHex(start), '-', FmtHex(end));
    return start;
allocFailed:
    KABORT0();
  }

  void putVmRange(MemoryDescriptor* md) {
    UnmapDescriptor* ud = new (md) UnmapDescriptor(md->vma, md->vma + md->size);
    unmapSet.insert(ud);
    ScopedLock<> sl(vlock);
    do {
      auto iter = unmapSet.begin();
      ud = *iter;
      if (ud->start > mapStart) break;
      KASSERTN(ud->start == mapStart, FmtHex(mapStart), ' ', FmtHex(ud->start));
      KASSERTN(ud->end    > mapStart, FmtHex(mapStart), ' ', FmtHex(ud->end));
      DBG::outl(DBG::VM, "AS(", FmtHex(this), ")/put: ", FmtHex(mapStart), '-', FmtHex(ud->end));
      mapStart = ud->end;
      unmapSet.erase(iter);
      mdCache.deallocate(reinterpret_cast<MemoryDescriptor*>(ud));
      // TODO: clear page tables in range [mapStart...ud->end]
    } while (!unmapSet.empty());
  }

  template<size_t N, MapCode mc, PageType owner>
  void mapRegion( paddr pma, vaddr vma, size_t size, PageType type ) {
    static_assert( N > 0 && N < pagelevels, "page level template violation" );
    KASSERT1( NoAlloc <= mc && mc <= Lazy, mc );
    KASSERT1( aligned(vma,  pagesize<N>()), vma );
    KASSERT1( aligned(size, pagesize<N>()), size );
    for (vaddr end = vma + size; vma < end; vma += pagesize<N>()) {
      if (mc == Alloc) pma = CurrFM().allocFrame<N>();
      DBG::outl(DBG::VM, "AS(", FmtHex(this), ")/map<", N, ',', mc, ">: ", FmtHex(vma), " -> ", FmtHex(pma), " flags:", Paging::PageEntryFmt(type));
      switch (mc) {
        case NoAlloc:
        case Alloc:   Paging::mapPage<N>(vma, pma, type | owner); break;
        case Guard:   Paging::mapToGuard<N>(vma); break;
        case Lazy:    Paging::mapToLazy<N>(vma); break;
        default:      KABORT0();
      }
      if (mc == NoAlloc) pma += pagesize<N>();
    }
  }

  template<size_t N, MapCode mc>
  void unmapRegion( vaddr vma, size_t size ) {
    static_assert( N > 0 && N < pagelevels, "page level template violation" );
    KASSERT1( NoAlloc <= mc && mc <= Alloc, mc );
    KASSERT1( aligned(vma, pagesize<N>()), vma );
    KASSERT1( aligned(size, pagesize<N>()), size );
    for (vaddr end = vma + size; vma < end; vma += pagesize<N>()) {
      paddr pma = Paging::unmap<N,false>(vma); // TLB invalidated separately
      bool alloc = (mc == Alloc && pma != guardPage && pma != lazyPage);
      DBG::outl(DBG::VM, "AS(", FmtHex(this), ")/post: ", FmtHex(vma), '/', FmtHex(pagesize<N>()), " -> ", FmtHex(pma), " epoch:", unmapEpoch);
      ScopedLock<> sl(ulock);
      MemoryDescriptor* md = new (mdCache.allocate()) MemoryDescriptor(vma, pma, pagesize<N>(), alloc);
      memoryList.push_back(*md);
      unmapEpoch += 1;
    }
  }

  template<size_t N, bool alloc, PageType owner>
  vaddr bmap(vaddr addr, size_t size, paddr pma) {
    ScopedLock<> sl(vlock);
    addr = getVmRange<N>(addr, size);
    const MapCode mc = alloc ? AllocCode(owner) : NoAlloc;
    mapRegion<N,mc,owner>(pma, addr, size, Data);
    return addr;
  }

  template<PageType owner>
  vaddr ballocStack(size_t ss) {
    KASSERT1(ss >= minimumStack, ss);
    size_t size = ss + stackGuardPage;
    ScopedLock<> sl(vlock);
    vaddr vma = getVmRange<stackpl>(0, size);
    KASSERTN(size == ss + stackGuardPage, ss, ' ', size);
    mapRegion<stackpl,Guard,owner>(0, vma, stackGuardPage, Data);
    vma += stackGuardPage;
    mapRegion<stackpl,AllocCode(owner),owner>(0, vma, ss, Data);
    return vma;
  }

  void enter(AddressSpaceMarker& marker) {
    marker.enterEpoch = unmapEpoch;
    markerList.push_back(marker);
  }

  template<bool invalidate>
  void leave(AddressSpaceMarker& marker) {
    KASSERT0(!markerList.empty());
    bool front = (&marker == markerList.front());
    IntrusiveList<AddressSpaceMarker>::remove(marker);

    // explicit TLB invalidation of TLB entries for pages removed since
    // 'enterEpoch'.  Invalidation iterates backwards -> easier!
    // In kernelSpace, pages are G(lobal), so TLBs not flushed at 'mov cr3'.
    if (invalidate) {
      MemoryDescriptor* md = memoryList.back();
      for (sword e = unmapEpoch; e - marker.enterEpoch > 0; e -= 1) {
        KASSERT0(md != memoryList.fence());
        DBG::outl(DBG::VM, "AS(", FmtHex(this), ")/kinv: ", FmtHex(md->vma), '/', FmtHex(md->size), " -> ", FmtHex(md->pma), " epoch:", e-1);
        CPU::InvTLB(md->vma);
        md = IntrusiveList<MemoryDescriptor>::prev(*md);
      }
    }

    // All unmap entries between 'oldest' and 'second-oldest' CPU are
    // flushed from all TLBs and can be removed from list.
    if (front) {
      sword end = markerList.empty() ? unmapEpoch : markerList.front()->enterEpoch;
      for (sword e = marker.enterEpoch; end - e > 0; e += 1) {
        KASSERT0(!memoryList.empty());
        MemoryDescriptor* md = memoryList.front();
        DBG::outl(DBG::VM, "AS(", FmtHex(this), ")/inv: ", FmtHex(md->vma), '/', FmtHex(md->size), " -> ", FmtHex(md->pma), (md->alloc ? "a" : ""), " epoch:", e);
        if (md->alloc) CurrFM().release(md->pma, md->size);
        IntrusiveList<MemoryDescriptor>::remove(*md);
        putVmRange(md);
      }
    }
  }

public:
  void setup(vaddr bot, vaddr top, _friend<Machine>) {
    setup(bot, top);
  }

  void initInvalidation(AddressSpaceMarker& marker) {
    ScopedLock<> sl(ulock);
    if (markerList.empty()) {
      enter(marker);
      leave<true>(marker);
    }
    enter(marker);
  }

  template<size_t N, bool alloc=true>
  void munmap(vaddr addr, size_t size) {
    KASSERT1(aligned(addr, pagesize<N>()), addr);
    size = align_up(size, pagesize<N>());
    unmapRegion<N,alloc ? Alloc : NoAlloc>(addr, size);
  }

  void releaseStack(vaddr vma, size_t ss) {
    unmapRegion<stackpl,Alloc>(vma - stackGuardPage, ss + stackGuardPage);
  }

  static bool tablefault(vaddr vma, uint64_t pff) {
    return Paging::mapTable<pagetablepl>(vma, pff, CurrFM());
  }
};

class KernelAddressSpace : public BaseAddressSpace {
public:
  template<size_t N, bool alloc=true>
  vaddr mmap(vaddr addr, size_t size, paddr pma = topaddr) {
    return BaseAddressSpace::bmap<N,alloc,Kernel>(addr, size, pma);
  }

  vaddr allocStack(size_t ss) {
    return BaseAddressSpace::ballocStack<Kernel>(ss);
  }

  // allocate and map contiguous physical memory: device buffers -> set MMapIO?
  vaddr allocContig( size_t size, paddr align, paddr limit) {
    paddr pma = CurrFM().allocRegion(size, align, limit);
    if (size < kernelps) return mmap<1,false>(0, size, pma);
    else return mmap<kernelpl,false>(0, size, pma);
  }

  void runInvalidation() {
    ScopedLock<> sl(ulock);
    AddressSpaceMarker& marker = LocalProcessor::self()->kernASM;
    leave<true>(marker);
    enter(marker);
  }

};

extern KernelAddressSpace kernelAS;

class AddressSpace : public BaseAddressSpace {
  paddr pagetable;         // root page table *physical* address

public:
  AddressSpace(int) : pagetable(topaddr) {}
  AddressSpace() {
    pagetable = CurrFM().allocFrame<pagetablepl>();
    Paging::clone(pagetable);
    DBG::outl(DBG::VM, "AS(", FmtHex(this), ")/cloned: ", FmtHex(pagetable));
    BaseAddressSpace::setup(usertop - kernelps, usertop);
  }

  void init(paddr p, _friend<Machine>) {
    KASSERT1(pagetable == topaddr, FmtHex(pagetable));
    pagetable = p;
    setup(usertop);
  }

  void setup(vaddr bssEnd) {
    BaseAddressSpace::setup(bssEnd, usertop);
  }

  ~AddressSpace() {
    KASSERT0(pagetable != topaddr);
    DBG::outl(DBG::VM, "AS(", FmtHex(this), ")/destroy:", FmtHex(pagetable));
    KASSERT1(pagetable != CPU::readCR3(), FmtHex(CPU::readCR3()));
    CurrFM().release(pagetable, pagetableps);
  }

  void clean() {
    DBG::outl(DBG::VM, "AS(", FmtHex(this), ")/clean:", *this);
    verifyPT(pagetable);
    Paging::clear(align_down(userbot, pagesize<pagelevels>()), align_up(usertop, pagesize<pagelevels>()), CurrFM());
  }

  template<size_t N, bool alloc=true>
  vaddr mmap(vaddr addr, size_t size, paddr pma = topaddr) {
    verifyPT(pagetable);
    return BaseAddressSpace::bmap<N,alloc,User>(addr, size, pma);
  }

  template<size_t N, bool alloc=true>
  void munmap(vaddr addr, size_t size) {
    verifyPT(pagetable);
    BaseAddressSpace::munmap<N,alloc>(addr, size);
  }

  vaddr allocStack(size_t ss) {
    verifyPT(pagetable);
    return BaseAddressSpace::ballocStack<User>(ss);
  }

  void releaseStack(vaddr vma, size_t ss) {
    verifyPT(pagetable);
    BaseAddressSpace::releaseStack(vma, ss);
  }

  bool pagefault(vaddr vma, uint64_t pff) {
    verifyPT(pagetable);
    return Paging::mapFromLazy(vma, Data | User, pff, CurrFM());
  }

  // allocate memory and map to specific virtual address: ELF loading
  template<size_t N, bool check=true>
  void allocDirect( vaddr vma, size_t size, PageType t ) {
    KASSERT1(!check || vma < mapBottom || vma > mapTop, vma);
    mapRegion<N,Alloc,User>(0, vma, size, t);
  }

  // map memory to specific virtual address: ELF loading
  template<size_t N, bool check=true>
  void mapDirect( paddr pma, vaddr vma, size_t size, PageType t ) {
    KASSERT1(!check || vma < mapBottom || vma > mapTop, vma);
    mapRegion<N,NoAlloc,User>(pma, vma, size, t);
  }

  void switchTo(AddressSpace& nextAS) {
    KASSERT0(LocalProcessor::checkLock());
    KASSERT0(pagetable != topaddr);
    verifyPT(pagetable);
    AddressSpaceMarker& marker = LocalProcessor::self()->userASM;
    if (pagetable != nextAS.pagetable) {
      DBG::outl(DBG::AddressSpace, "AS(", FmtHex(pagetable), ")/switchTo: ", FmtHex(nextAS.pagetable));
      Paging::installPagetable(nextAS.pagetable);
      ulock.acquire();
      leave<false>(marker);
      ulock.release();
      ScopedLock<> sl(nextAS.ulock);
      nextAS.enter(marker);
    } else {
      ScopedLock<> sl(ulock);
      leave<true>(marker);
      enter(marker);
    }
    kernelAS.runInvalidation();
  }

  virtual void preThreadSwitch() {}
  virtual void postThreadResume() {}
  virtual void postThreadDestroy() {}

  void print(ostream& os) const;
};

inline ostream& operator<<(ostream& os, const AddressSpace& as) {
  as.print(os);
  return os;
}

extern AddressSpace defaultAS;

#endif /* _AddressSpace_h_ */
