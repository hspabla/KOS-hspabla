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
#include "machine/Processor.h"

class Machine;

class FrameManager {
  FrameManager(const FrameManager&) = delete;            // no copy
  FrameManager& operator=(const FrameManager&) = delete; // no assignment

  friend ostream& operator<<(ostream&, const FrameManager&);

  SpinLock lock;
  HierarchicalBitmap<ptentries,framebits-pagesizebits<kernelpl>()> largeFrames;
  HierarchicalBitmap<ptentries,framebits-pagesizebits<1>()> smallFrames;
  HierarchicalBitmap<ptentries,framebits-pagesizebits<1>()> zeroFrames;

  static const size_t largeSize = kernelps;
  static const size_t smallSize = pagesize<1>();

  paddr baseAddress;
  paddr topAddress;

  void releaseInternal(size_t idx) {
    DBG::outl(DBG::Frame, "FM/release: ", FmtHex(idx * smallSize));
    smallFrames.set(idx);
    // check for full small bitmap -> mark large page as free
    if (smallFrames.blockfull(idx)) {
      smallFrames.blockclr(idx);
      largeFrames.set(idx / ptentries);
    }
  }

  size_t allocLargeIdx() {
    size_t idx;
    for (;;) {
      idx = largeFrames.find();
      if (idx != limit<size_t>()) break;
      if (!zeroInternal()) KABORT1("OUT OF MEMORY");
    }
    largeFrames.clr(idx);
    DBG::outl(DBG::Frame, "FM/allocL: ", FmtHex(idx * largeSize), '/', FmtHex(largeSize));
    return idx;
  }

  paddr allocSmallIdx() {
    size_t idx;
    for (;;) {
      idx = smallFrames.find();
      if (idx != limit<size_t>()) break;
      if (!zeroInternal()) {
        idx = allocLargeIdx() * ptentries;
        smallFrames.blockset(idx);
        break;
      }
    }
    smallFrames.clr(idx);
    DBG::outl(DBG::Frame, "FM/allocS: ", FmtHex(idx * smallSize), '/', FmtHex(smallSize));
    return idx;
  }

  bool zeroInternal();

public:
  FrameManager() {} // do not initialize baseAddress, topAddress!

  size_t preinit( paddr bot, paddr top ) {
    baseAddress = bot;
    topAddress = top;
    return largeFrames.allocsize(divup(top - bot, largeSize))
         + smallFrames.allocsize(divup(top - bot, smallSize))
         +  zeroFrames.allocsize(divup(top - bot, smallSize));
  }

  void init( bufptr_t p ) {
    size_t size = topAddress - baseAddress;
    largeFrames.init(divup(size, largeSize), p);
    p += largeFrames.allocsize(divup(size, largeSize));
    smallFrames.init(divup(size, smallSize), p);
    p += smallFrames.allocsize(divup(size, smallSize));
    zeroFrames.init(divup(size, smallSize), p);
  }

  template<size_t N>
  inline paddr allocFrame() {
    ScopedLock<> sl(lock);
    switch (N) {
      case 1:        return baseAddress + allocSmallIdx() * smallSize;
      case kernelpl: return baseAddress + allocLargeIdx() * largeSize;
      default:       KABORT1(N);
    }
  }

  paddr allocRegion( size_t& size, paddr align, paddr lim );

  void release( paddr addr, size_t size ) {
    KASSERT1( aligned(addr, smallSize), addr );
    KASSERT1( aligned(size, smallSize), size );
    ScopedLock<> sl(lock);
    while (size > 0) {
      zeroFrames.set(addr / smallSize);
      addr += smallSize;
      size -= smallSize;
    }
  }

  bool zeroMemory() {
    ScopedLock<> sl(lock);
    return zeroInternal();
  }

  static FrameManager& curr() {
    FrameManager* fm = LocalProcessor::getCurrFM(_friend<FrameManager>());
    KASSERT0(fm);
    return *fm;
  }
};

static FrameManager& CurrFM() { return FrameManager::curr(); }

#endif /* _FrameManager_h_ */
