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
#ifndef _FrameManager_h_
#define _FrameManager_h_ 1

#include "generic/Bitmap.h"
#include "generic/IntrusiveContainers.h"
#include "kernel/KernelHeap.h"
#include "kernel/Output.h"

#include <map>

class Machine;

class FrameManager {
  FrameManager(const FrameManager&) = delete;            // no copy
  FrameManager& operator=(const FrameManager&) = delete; // no assignment

  friend ostream& operator<<(ostream&, const FrameManager&);

  SpinLock lock;
  size_t largeFrameCount; // for output and scanning in contig allocation
  typedef HierarchicalBitmap<ptentries,framebits-pagesizebits<kernelpl>()> LFBM;
  LFBM largeFrames;
  map<paddr,Bitmap<ptentries>,less<paddr>,KernelAllocator<paddr>> smallFrames;

  AddressSpace* zeroAS;
  struct ZeroDescriptor : public IntrusiveList<ZeroDescriptor>::Link {
    paddr addr;
    size_t size;
    ZeroDescriptor(paddr a, size_t s) : addr(a), size(s) {}
  };
  IntrusiveList<ZeroDescriptor> zeroQueue;

  void releaseInternal(paddr addr, size_t size) {
    DBG::outl(DBG::Frame, "FM/release: ", FmtHex(addr), '/', FmtHex(size));
    KASSERT1( aligned(addr, defaultps), addr );
    KASSERT1( aligned(size, defaultps), size );
    while (size > 0) {
      size_t idxL =  addr / kernelps;
      size_t idxS = (addr % kernelps) / defaultps;
      if (idxS == 0 && size >= kernelps) {
        addr += kernelps;
        size -= kernelps;
        largeFrames.set(idxL);
      } else {
        // find appropriate bitmap in smallFrames, or create empty bitmap
        auto it = smallFrames.lower_bound(idxL);
        if (it == smallFrames.end() || it->first != idxL) it = smallFrames.emplace_hint(it, idxL, Bitmap<ptentries>());
        // set bits in bitmap, limited to this bitmap
        while (size > 0 && idxS < ptentries) {
          it->second.set(idxS);
          idxS += 1;
          size -= defaultps;
          addr += defaultps;
        }
        // check for full bitmap; one small page bitmap represents one large page
        if (it->second.full()) {
          smallFrames.erase(it);
          largeFrames.set(idxL);
        }
      }
    }
  }

  size_t allocLargeIdx() {
    size_t idx = largeFrames.findset();
    KASSERT1(idx < largeFrameCount, "OUT OF MEMORY");
    largeFrames.clear(idx);
    DBG::outl(DBG::Frame, "FM/allocL: ", FmtHex(idx * kernelps), '/', FmtHex(kernelps));
    return idx;
  }

  paddr allocSmall() {
    if (smallFrames.empty()) {
      size_t idx = allocLargeIdx();
      smallFrames.emplace(idx, Bitmap<ptentries>::filled());
    }
    auto it = smallFrames.begin();
    size_t idx2 = it->second.findset();
    it->second.clear(idx2);
    if (it->second.empty()) smallFrames.erase(it);
    paddr addr = it->first * kernelps + idx2 * defaultps;
    DBG::outl(DBG::Frame, "FM/allocS: ", FmtHex(addr), '/', FmtHex(defaultps));
    return addr;
  }

public:
  FrameManager() : largeFrameCount(0), zeroAS(nullptr) {}

  static constexpr size_t getSize( paddr top ) {
    return LFBM::allocsize(divup(top, kernelps));
  }

  void init( bufptr_t p, paddr top ) {
    largeFrameCount = divup(top, kernelps);
    largeFrames.init(largeFrameCount, p);
  }

  template<size_t N>
  inline paddr allocFrame() {
    ScopedLock<> sl(lock);
    paddr result;
    switch (N) {
      case defaultpl: result = allocSmall(); break;
      case kernelpl:  result = allocLargeIdx() * kernelps; break;
      default: KABORT0();
    }
    // check for impending memory shortage -> synchronous zeroing!
    while (largeFrames.empty() && !zeroQueue.empty()) zeroInternal();
    return result;
  }

  paddr allocRegion( size_t& size, paddr align, paddr limit );

  template<bool zero=true>
  void release( paddr addr, size_t size ) {
    ZeroDescriptor* zd;
    // allocate ZD object before acquiring lock
    if (zero) zd = knew<ZeroDescriptor>(addr, size);
    ScopedLock<> sl(lock);
    if (zero) zeroQueue.push_back(*zd);
    else releaseInternal(addr, size);
  }

  void initZero();
  void zeroInternal();
  bool zeroMemory();
};

static inline FrameManager& CurrFM() {
  FrameManager* fm = LocalProcessor::getFrameManager();
  KASSERT0(fm);
  return *fm;
}

#endif /* _FrameManager_h_ */
