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
#ifndef _Paging_h_
#define _Paging_h_ 1

#include "generic/RegionSet.h"
#include "kernel/FrameManager.h"
#include "kernel/Output.h"
#include "machine/asmshare.h"
#include "machine/CPU.h"

#include <cstring>

/* page map access with recursive PML4: address of 8-byte aligned page entry
bit width:    9999 9 3
target vaddr: ABCD|x|y
PML4 self     XXXX|X|0
L4 entry      XXXX|A|0
L3 entry      XXXA|B|0
L2 entry      XXAB|C|0
L1 entry      XABC|D|0
with X = per-level bit pattern (position of recursive index in PML4). */

class Machine;

class Paging {
  static const BitString<uint64_t, 0, 1> P;      // page present
  static const BitString<uint64_t, 1, 1> RW;     // writable
  static const BitString<uint64_t, 2, 1> US;     // user-level
  static const BitString<uint64_t, 3, 1> PWT;    // write-through
  static const BitString<uint64_t, 4, 1> PCD;    // cache disable
  static const BitString<uint64_t, 5, 1> A;      // accessed
  static const BitString<uint64_t, 6, 1> D;      // dirty
  static const BitString<uint64_t, 7, 1> PS;     // page size (vs. table)
  static const BitString<uint64_t, 8, 1> G;      // no CR3-based TLB flush
  static const BitString<uint64_t,12, 1> PAT;    // memory typing (caching)
  static const BitString<uint64_t,12,40> ADDR;   // physical address
  static const BitString<uint64_t,63, 1> XD;     // execute-disable

  typedef uint64_t PageEntry;

public:
  struct PageEntryFmt {
    PageEntry t;
    PageEntryFmt( PageEntry t ) : t(t) {}
  };
  friend ostream& operator<<(ostream&, const PageEntryFmt&);

  struct PageFaultFlags {                        // exception_handler_0x0e
    uint64_t t;
    static const BitString<uint64_t, 0, 1> P;		 // page not present
    static const BitString<uint64_t, 1, 1> WR;   // write not allowed
    static const BitString<uint64_t, 2, 1> US;   // user access not allowed
    static const BitString<uint64_t, 3, 1> RSVD; // reserved flag in paging structure
    static const BitString<uint64_t, 4, 1> ID;   // instruction fetch
    static constexpr uint64_t testmask() { return P() | WR() | US(); }
    PageFaultFlags( uint64_t t ) : t(t) {}
  };
  friend ostream& operator<<(ostream&, const PageFaultFlags&);

  template<unsigned int N>
  struct PageVector {
    uint64_t t;
    PageVector( uint64_t t ) : t(t) {}
  };

  template<unsigned int N>
  friend ostream& operator<<(ostream& os, const PageVector<N>& v) {
    os << PageVector<N+1>(v.t) << ',' << getPageIndex<N>(v.t);
    return os;
  }

  enum PageType {
    // basic type, combine with owner
    Invalid    = 0,
    Code       = P(),
    RoData     = P() | XD(),
    Data       = P() | XD() | RW(),
    PageTable  = P() | RW(),         // G() for kernel tables upsets VirtualBox
    // owner, combine with basic type
    Kernel     = G(),
    User       = US(),
    // standalone page types
    MMapIO     = Data | PWT() | PCD(),
  };

  enum PageStatus {
    Available = 0x01,
    Guard     = 0x02,
    Lazy      = 0x04,
    Mapped    = 0x04,
  };

private:
  template<unsigned int N> static constexpr vaddr getPageEntryHelper( vaddr vma ) {
    static_assert( N > 0 && N <= pagelevels, "page level template violation" );
    return (recptindex << pagesizebits<pagelevels>()) | (getPageEntryHelper<N-1>(vma) >> pagetablebits);
  }

  template<unsigned int N> static constexpr vaddr getPageEntryInternal( vaddr vma ) {
    static_assert(recptindex >= ptentries / 2, "recptindex must be in upper half of VM");
    return align_down(canonPrefix | getPageEntryHelper<N>(vma & ~canonPrefix), sizeof(PageEntry));
  }

  template<unsigned int N> static constexpr PageEntry* getPageEntry( vaddr vma ) {
    return (PageEntry*)getPageEntryInternal<N>(vma);
  }

  template <unsigned int N> static constexpr PageEntry* getPageTable( vaddr vma ) {
    return getPageEntry<N>(align_down(vma, pagesize<N+1>()));
  }

  template<unsigned int N> static constexpr mword getPageIndex( vaddr vma ) {
    static_assert( N > 0 && N <= pagelevels, "page level template violation" );
    return (vma & bitmask<mword>(pagesizebits<N+1>())) >> pagesizebits<N>();
  }

  template<unsigned int N> static void setPageEntry( vaddr vma, PageEntry pe ) {
    *getPageEntry<N>(vma) = pe;
  }

  template<unsigned int N> static constexpr PageEntry xPS() {
    static_assert( N > 0 && N < pagelevels, "page level template violation" );
    return N > 1 ? PS() : 0;
  }

  template<unsigned int N> static constexpr bool isPage(const PageEntry& pe) {
    static_assert( N > 0 && N <= pagelevels, "page level template violation" );
    return (N < pagelevels) && (N == 1 || PS.get(pe));
  }

	template<unsigned int N> static bool hasPAT(const PageEntry& pe) {
    static_assert( N > 0 && N < pagelevels, "page level template violation" );
		return N > 1 ? PAT.get(pe) : PS.get(pe);
  }

  template <unsigned int N, bool output = true>
  static bool mapInternal( vaddr vma, PageEntry pe, PageEntry expected ) {
    static_assert( N >= 1 && N <= pagelevels, "page level template violation" );
    KASSERT1( aligned(vma, pagesize<N>()), FmtHex(vma) );
    PageEntry* curr = getPageEntry<N>(vma);
    bool success = __atomic_compare_exchange_n(curr, &expected, pe, 0, __ATOMIC_ACQ_REL, __ATOMIC_RELAXED);
    if (output) DBG::outl(DBG::Paging, "Paging::map<", N, ">: ", FmtHex(vma), '/', FmtHex(pagesize<N>()), " ->", PageEntryFmt(*curr), (success ? " new" : " old"));
    return success;
  }

  template <unsigned int N>
  static bool mapFromLazyInternal( vaddr vma, uint64_t type, uint64_t pff, FrameManager& fm ) {
    PageEntry* pe = getPageEntry<N>(vma);
    if (P.get(*pe) && !isPage<N>(*pe)) return mapFromLazyInternal<N-1>(vma, type, pff, fm);
    paddr pma = fm.allocFrame<N>();
    vma = align_down(vma, pagesize<N>());
    if (!mapInternal<N>(vma, pma | type | xPS<N>(), lazyPage)) {
      fm.release(pma, pagesize<N>());
    }
    return (*getPageEntry<N>(vma) & pff) == (pff & PageFaultFlags::testmask());
  }

  template <unsigned int N>
  static size_t testInternal( vaddr vma, uint64_t status ) {
    static_assert( N > 0 && N <= pagelevels, "page level template violation" );
    PageEntry* pe = getPageEntry<N>(vma);
    PageStatus s;
    if (P.get(*pe)) {
      if (isPage<N>(*pe)) s = Mapped;
      else return testInternal<N-1>(vma, status);
    } else switch(*pe) {
      case 0:         s = Available; break;
      case guardPage: s = Guard; break;
      case lazyPage:  s = Lazy; break;
      default:        KABORT1(*pe); break;
    }
    return (s & status) ? pagesize<N>() : 0;
  }

  template <unsigned int N>
  static void clearInternal( vaddr start, vaddr end, FrameManager& fm ) {
    static_assert( N >= 1 && N <= pagelevels, "page level template violation" );
    for (vaddr vma = start; vma < end; vma += pagesize<N>()) {
      PageEntry* pe = getPageEntry<N>(vma);
      if (P.get(*pe)) {
        paddr pma = *pe & ADDR();
        if (isPage<N>(*pe)) {
          DBG::outl(DBG::Paging, "Paging::clearP<", N, ">: ", FmtHex(vma), '/', FmtHex(pagesize<N>()), " -> ", PageEntryFmt(*pe));
          KASSERT1(N < pagelevels, N);
          fm.release(pma, pagesize<N>());
        } else {
          clearInternal<N-1>(vma, vma + pagesize<N>(), fm);
          DBG::outl(DBG::Paging, "Paging::clearT<", N-1, ">: ", FmtHex(vma), '/', FmtHex(pagesize<N>()), " -> ", PageEntryFmt(*pe));
          fm.release(pma, pagetableps);
        }
      }
      *pe = 0;
    }
  }

  template<unsigned int N>
  static paddr vtopInternal( vaddr vma ) {
    static_assert( N > 0 && N <= pagelevels, "page level template violation" );
    PageEntry* pe = getPageEntry<N>(vma);
    KASSERT1(P.get(*pe), FmtHex(vma));
    if (!isPage<N>(*pe)) return vtopInternal<N-1>(vma);
    return (*pe & ADDR()) + pageoffset<N>(vma);
  }

protected:
  static const paddr guardPage = topaddr & ADDR();
  static const paddr lazyPage  = guardPage - pagesize<1>();

  template <unsigned int N>
  static bool mapToGuard( vaddr vma ) {
    return mapInternal<N>(vma, guardPage, 0);
  }

  template <unsigned int N>
  static bool mapToLazy( vaddr vma ) {
    return mapInternal<N>(vma, lazyPage, 0);
  }

  template <unsigned int N>
  static bool mapPage( vaddr vma, paddr pma, uint64_t type ) {
    KASSERT1( ADDR.excl(pma) == 0, FmtHex(pma) );
    return mapInternal<N>(vma, pma | type | xPS<N>(), 0);
  }

  template <unsigned int N>
  static inline bool mapTable( vaddr vma, uint64_t pff, FrameManager& fm );

  template <unsigned int N = pagelevels-1>
  static bool mapFromLazy( vaddr vma, uint64_t type, uint64_t pff, FrameManager& fm ) {
    CPU::LoadFence();
    return mapFromLazyInternal<N>(vma, type, pff, fm);
  }

  template <unsigned int N = pagelevels>
  static size_t test( vaddr vma, uint64_t status ) {
    CPU::LoadFence();
    return testInternal<N>(vma, status);
  }

  static size_t testfree( vaddr vma ) { return test(vma, Available); }
  static size_t testused( vaddr vma ) { return test(vma, Guard|Lazy|Mapped); }

  template <unsigned int N>
  static paddr unmap( vaddr vma ) {
    static_assert( N >= 1 && N <= pagelevels, "page level template violation" );
    KASSERT1(aligned(vma, pagesize<N>()), FmtHex(vma));
    PageEntry* pe = getPageEntry<N>(vma);
    CPU::LoadFence();
    KASSERT1(isPage<N>(*pe), FmtHex(vma));
    paddr pma = *pe & ADDR();
    *pe = 0;
    DBG::outl(DBG::Paging, "Paging::unmap<", N, ">: ", FmtHex(vma), '/', FmtHex(pagesize<N>()), " -> ", FmtHex(pma));
    CPU::StoreFence();
    return pma;
  }

  template <unsigned int N = pagelevels>
  static void clear( vaddr start, vaddr end, FrameManager& fm ) {
    CPU::LoadFence();
    clearInternal<N>(start, end, fm);
    CPU::StoreFence();
  }

  static void installPagetable(paddr pt) {
    CPU::writeCR3(pt);
  }

  static inline void clone(paddr pt);

  Paging() = default;
  Paging(const Paging&) = delete;            // no copy
  Paging& operator=(const Paging&) = delete; // no assignment

public:
  static inline paddr bootstrap(vaddr kernelBase, vaddr kernelData, vaddr kernelEnd, RegionSet<Region<paddr>>& mem, _friend<Machine>);
  static inline vaddr bootstrap2(RegionSet<Region<paddr>>& mem, size_t maxHeap, size_t startHeap, _friend<Machine>);

  template <unsigned int N>
  static bool mapPage( vaddr vma, paddr pma, uint64_t type, _friend<Machine> ) {
    return mapPage<N>(vma, pma, type);
  }

  template <unsigned int N>
  static paddr unmap( vaddr vma, _friend<Machine> ) {
    return unmap<N>(vma);
  }

  template<unsigned int N = pagelevels>
  static paddr vtop( vaddr vma ) {
    CPU::LoadFence();
    return vtopInternal<N>(vma);
  }

  // must be defined after Paging::getPageEntryHelper<0> specialization
  static inline constexpr vaddr start();
  static inline constexpr vaddr end();
};

// corner cases that need specific template instantiations
template<> inline vaddr  Paging::getPageEntryHelper<0>(vaddr vma) { return vma; }
template<> inline ostream& operator<<(ostream& os, const Paging::PageVector<pagelevels>& v) {
  os << Paging::getPageIndex<pagelevels>(v.t);
  return os;
}

// corner cases that need dummy template instantiations
template<> inline bool   Paging::mapFromLazyInternal<0>(vaddr, uint64_t, uint64_t, FrameManager&) { KABORT0(); return false; }
template<> inline size_t Paging::testInternal<0>(vaddr, uint64_t ) { KABORT0(); return 0; }
template<> inline void   Paging::clearInternal<0>(vaddr, vaddr, FrameManager&) { KABORT0(); }
template<> inline paddr  Paging::vtopInternal<0>(vaddr) { KABORT0(); return 0; }

// must be defined after Paging::getPageEntryHelper<0> specialization
inline constexpr vaddr Paging::start() {
  return getPageEntryInternal<1>(0);
}
inline constexpr vaddr Paging::end() {
  return getPageEntryInternal<1>(topaddr) + sizeof(PageEntry);
}

template <unsigned int N>
inline bool Paging::mapTable( vaddr vma, uint64_t pff, FrameManager& fm ) {
  KASSERT1( pff == 0 || pff == PageFaultFlags::WR(), FmtHex(pff) );
  paddr pma = fm.allocFrame<pagetablepl>();
  vma = align_down(vma, pagetableps);
  bool success = mapInternal<pagetablepl,false>(vma, pma | PageTable | User, 0);
  if (!success) fm.release(pma, pagetableps);
  DBG::outl(DBG::Paging, "Paging::mapTable<", N, ">: [", PageVector<N>(vma), "] -> ", success ? FmtHex(pma) : FmtHex(topaddr));
  return *getPageEntry<pagetablepl>(vma) & P();
}

inline void Paging::clone(paddr newpt) {
  DBG::outl(DBG::Paging, "Paging::clone - PML4: ", FmtHex(newpt));
  // obtain per-processor clone address and lock processor
  vaddr cloneAddr = LocalProcessor::getCloneAddr();
  LocalProcessor::lock();
  // map PML4 page
  mapPage<pagetablepl>(cloneAddr, newpt, Data | Kernel);
  PageEntry* clonedPE = (PageEntry*)cloneAddr;
  PageEntry* kernelPE = getPageEntry<pagelevels>(0);
  // copy top half of PML4
  memcpy(clonedPE + ptentries/2, kernelPE + ptentries/2, pagetableps / 2);
  // set recursive page mapping entry
  clonedPE[recptindex] = newpt | PageTable;
  // unmap PML4 page, invalidate mapping, unlock
  unmap<pagetablepl>(cloneAddr);
  CPU::InvTLB(cloneAddr);
  LocalProcessor::unlock();
}

// bootstrap relies on identity paging in place, but memory not yet zeroed
inline paddr Paging::bootstrap(vaddr kernelBase, vaddr kernelData, vaddr kernelEnd, RegionSet<Region<paddr>>& mem, _friend<Machine>) {
  // PML4 setup
  PageEntry* pml4 = (PageEntry*)mem.retrieve_front(pagetableps);
  memset(pml4, 0, pagetableps);
  pml4[recptindex] = paddr(pml4) | PageTable;
  DBG::outl(DBG::Paging, "Paging::bootstrap - PML4: ", FmtHex(pml4));

  // PDPT setup for kernel image
  PageEntry* pdpt = (PageEntry*)mem.retrieve_front(pagetableps);
  memset(pdpt, 0, pagetableps);
  size_t kernindex4 = getPageIndex<kernelpl+2>(kernelBase);
  pml4[kernindex4] = paddr(pdpt) | PageTable;
  DBG::outl(DBG::Paging, "Paging::bootstrap - PML4 [", kernindex4, "] PDPT: ", FmtHex(pdpt));

  // PD and 2M page setup for kernel image
  for (vaddr addr = kernelBase; addr < kernelEnd; ) {
    PageEntry* pd = (PageEntry*)mem.retrieve_front(pagetableps);
    memset(pd, 0, pagetableps);
    size_t kernindex3 = getPageIndex<kernelpl+1>(addr);
    pdpt[kernindex3] = paddr(pd) | PageTable;
    DBG::outl(DBG::Paging, "Paging::bootstrap - PDPT [", kernindex4, ',', kernindex3, "] PD: ", FmtHex(pd));
    vaddr end = min(kernelEnd, addr + pagesize<kernelpl+1>());
    for (; addr < end; addr += kernelps) {
      size_t kernindex2 = getPageIndex<kernelpl>(addr);
      PageType ptype = addr < kernelData ? Code : Data;
      pd[kernindex2] = (addr - kernelBase) | ptype | Kernel | xPS<kernelpl>();
      DBG::outl(DBG::Paging, "Paging::bootstrap - PD   [", kernindex4, ',', kernindex3, ',', kernindex2, "] Page: ", FmtHex(addr), " -> ", PageEntryFmt(pd[kernindex2]));
    }
  }

  // install new page table
  installPagetable(paddr(pml4));
  return paddr(pml4);
}

inline vaddr Paging::bootstrap2(RegionSet<Region<paddr>>& mem, size_t maxHeap, size_t initHeap, _friend<Machine>) {
  // PDPT setup for maximum kernel heap -> ready for cloning later
  vaddr bottom = align_down(kerneltop - max(maxHeap,initHeap), pagesize<kernelpl+2>());
  vaddr addr = bottom;
  for (; addr < kerneltop; addr += pagesize<kernelpl+2>()) {
    paddr pdpt = mem.retrieve_front(pagetableps);
    setPageEntry<kernelpl+2>(addr, pdpt | PageTable);
    memset(getPageTable<kernelpl+1>(addr), 0, pagetableps);
    DBG::outl(DBG::Paging, "Paging::bootstrap - PML4 [", PageVector<kernelpl+2>(addr), "] PDPT: ", FmtHex(pdpt));
  }

  // PD setup for initial kernel heap -> FrameManager memory & bootHeap
  addr = align_down(kerneltop - initHeap, pagesize<kernelpl+1>());
  for (; addr < kerneltop; addr += pagesize<kernelpl+1>()) {
    paddr pd = mem.retrieve_front(pagetableps);
    setPageEntry<kernelpl+1>(addr, pd | PageTable);
    memset(getPageTable<kernelpl>(addr), 0, pagetableps);
    DBG::outl(DBG::Paging, "Paging::bootstrap - PDPT [", PageVector<kernelpl+1>(addr), "] PD: ", FmtHex(pd));
  }

  // 2M page setup initial kernel heap
  KASSERT1(aligned(initHeap, kernelps), FmtHex(initHeap));
  addr = kerneltop - initHeap;
  for (; addr < kerneltop; addr += kernelps) {
    paddr page = mem.retrieve_front(kernelps);
    setPageEntry<kernelpl>(addr, page | Kernel | Data | xPS<kernelpl>());
    memset((ptr_t)addr, 0, kernelps);
    DBG::outl(DBG::Paging, "Paging::bootstrap - PD   [", PageVector<kernelpl>(addr), "] Page: ", FmtHex(addr), "-> ", FmtHex(page));
  }

  return bottom;
}

#endif /* _Paging_h_ */
