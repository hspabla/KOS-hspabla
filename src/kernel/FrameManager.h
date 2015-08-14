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

class Machine;

class FrameManager {
  FrameManager(const FrameManager&) = delete;            // no copy
  FrameManager& operator=(const FrameManager&) = delete; // no assignment

  friend ostream& operator<<(ostream&, const FrameManager&);

  SpinLock lock;
  HierarchicalBitmap<ptentries,framebits-pagesizebits<kernelpl>()> largeFrames;
  HierarchicalBitmap<ptentries,framebits-pagesizebits<1>()> smallFrames;

  static const size_t largeSize = kernelps;
  static const size_t smallSize = pagesize<1>();

  AddressSpace* zeroAS;
  struct ZeroDescriptor : public IntrusiveList<ZeroDescriptor>::Link {
    paddr addr;
    size_t size;
    ZeroDescriptor(paddr a, size_t s) : addr(a), size(s) {}
  };
  IntrusiveList<ZeroDescriptor> zeroQueue;
  HeapCache<sizeof(ZeroDescriptor)> zdCache;

  paddr baseAddress;
  paddr topAddress;

  void releaseInternal(paddr addr, size_t size) {
    DBG::outl(DBG::Frame, "FM/release: ", FmtHex(addr), '/', FmtHex(size));
    KASSERT1( aligned(addr, smallSize), addr );
    KASSERT1( aligned(size, smallSize), size );
    while (size > 0) {
      if (aligned(addr, largeSize) && size >= largeSize) {
        largeFrames.set(addr / largeSize);
        addr += largeSize;
        size -= largeSize;
      } else {
        size_t idx = addr / smallSize;
        smallFrames.set(idx);
        addr += smallSize;
        size -= smallSize;
        // check for full small bitmap -> mark large page as free
        if (aligned(addr, largeSize) && smallFrames.blockfull(idx)) {
          smallFrames.blockclr(idx);
          largeFrames.set(idx / ptentries);
        }
      }
    }
  }

  size_t allocLargeIdx() {
    size_t idx = largeFrames.find();
    KASSERT1(idx != limit<size_t>(), "OUT OF MEMORY");
    largeFrames.clr(idx);
    DBG::outl(DBG::Frame, "FM/allocL: ", FmtHex(idx * largeSize), '/', FmtHex(largeSize));
    return idx;
  }

  paddr allocSmallIdx() {
    size_t idx = smallFrames.find();
    if (idx == limit<size_t>()) {
      idx = allocLargeIdx() * ptentries;
      smallFrames.blockset(idx);
    }
    smallFrames.clr(idx);
    DBG::outl(DBG::Frame, "FM/allocS: ", FmtHex(idx * smallSize), '/', FmtHex(smallSize));
    return idx;
  }

public:
  FrameManager() : zeroAS(nullptr) {} // do not initialize baseAddress, topAddress!

  size_t preinit( paddr bot, paddr top ) {
    baseAddress = bot;
    topAddress = top;
    return largeFrames.allocsize(divup(top - bot, largeSize))
         + smallFrames.allocsize(divup(top - bot, smallSize));
  }

  void init( bufptr_t p ) {
    size_t size = topAddress - baseAddress;
    largeFrames.init(divup(size, largeSize), p);
    p += largeFrames.allocsize(divup(size, largeSize));
    smallFrames.init(divup(size, smallSize), p);
  }

  template<size_t N>
  inline paddr allocFrame() {
    ScopedLock<> sl(lock);
    paddr result;
    switch (N) {
      case 1:        result = baseAddress + allocSmallIdx() * smallSize; break;
      case kernelpl: result = baseAddress + allocLargeIdx() * largeSize; break;
      default: KABORT1(N);
    }
    // check for impending memory shortage -> synchronous zeroing!
    while (largeFrames.empty() && !zeroQueue.empty()) zeroInternal();
    return result;
  }

  template<bool zero=true>
  void release( paddr addr, size_t size ) {
    ZeroDescriptor* zd;
    // allocate ZD object before acquiring lock
    if (zero) zd = new (zdCache.allocate()) ZeroDescriptor(addr, size);
    ScopedLock<> sl(lock);
    if (zero) zeroQueue.push_back(*zd);
    else releaseInternal(addr, size);
  }

  paddr allocRegion( size_t& size, paddr align, paddr lim );
  void initZero(_friend<Machine>);
  void zeroInternal();
  bool zeroMemory();
};

static inline FrameManager& CurrFM() {
  FrameManager* fm = LocalProcessor::getFrameManager();
  KASSERT0(fm);
  return *fm;
}

#endif /* _FrameManager_h_ */
