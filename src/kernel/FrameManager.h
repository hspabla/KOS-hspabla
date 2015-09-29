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

#include "kernel/Output.h"
#include "machine/Processor.h"

void FrameZero(vaddr vma, paddr pma, size_t offset, size_t size, _friend<FrameManager>);

template<size_t PL> class FrameMap : public FrameMap<PL+1> {
  SpinLock lock;
  HierarchicalBitmap<ptentries,framebits-pagesizebits<PL>()> frames;
  HierarchicalBitmap<ptentries,framebits-pagesizebits<PL>()> tozero;
  static const size_t fsize = pagesize<PL>();

  bool releaseInternal(size_t idx) {
    DBG::outl(DBG::Frame, "FM/release: ", FmtHex(idx * fsize), '/', FmtHex(fsize));
    frames.set(idx);
    if (frames.blockfull(idx)) { // full bitmap -> clear and mark larger frame
      if (FrameMap<PL+1>::releaseDirect(idx / ptentries)) frames.blockclr(idx);
    }
    return true;
  }

  bool zeroInternal(_friend<FrameManager> ff) {
    size_t idx = tozero.find();
    if (idx == limit<size_t>()) return false;
    tozero.clr(idx);
    lock.release();
    paddr pma = idx * fsize;
    paddr apma = align_down(pma, kernelps);
    vaddr vma = zeroBase + kernelps * LocalProcessor::getIndex();
    FrameZero(vma, apma, pma - apma, fsize, ff);
    lock.acquire();
    releaseInternal(idx);
    return true;
  }

  bool zeroLocked(_friend<FrameManager> ff) {
    ScopedLock<> sl(lock);
    return zeroInternal(ff);
  }

public:
  inline void print(ostream& os, paddr base, size_t range);

  bool releaseDirect(size_t idx) {
    ScopedLock<> sl(lock);
    return releaseInternal(idx);
  }

  bool release(size_t idx, size_t cnt) {
    while (cnt > 0) {
      if ( aligned(idx, ptentries) && cnt >= ptentries
        && FrameMap<PL+1>::release(idx / ptentries, cnt / ptentries) ) {
        idx += align_down(cnt, ptentries);
        cnt -= align_down(cnt, ptentries);
      } else {
        ScopedLock<> sl(lock);
        tozero.set(idx);
        idx += 1;
        cnt -= 1;
      }
    }
    return true;
  }

  bool zero(_friend<FrameManager> ff) {
    return zeroLocked(ff) || FrameMap<PL+1>::zero(ff);
  }

  size_t alloc(_friend<FrameManager> ff) {
    ScopedLock<> sl(lock);
    size_t idx;
    for (;;) {
      idx = frames.find();
      if (idx != limit<size_t>()) break;
      if (!zeroInternal(ff)) {
        idx = FrameMap<PL+1>::alloc(ff) * ptentries;
        frames.blockset(idx);
        break;
      }
    }
    frames.clr(idx);
    DBG::outl(DBG::Frame, "FM/alloc: ", FmtHex(idx * fsize), '/', FmtHex(fsize));
    return idx;
  }

  size_t allocRegion(size_t cnt, size_t lim, size_t range) {
    ScopedLock<> sl(lock);
    size_t idx = 0;
    for (;;) {
      size_t c = frames.findrange(idx, range / fsize);
      if (c == 0 || idx > lim) {
        idx = FrameMap<PL+1>::allocRegion(divup(cnt, ptentries), lim / ptentries, range) * ptentries;
        frames.blockset(idx);
        c = ptentries;
      }
      if (c >= cnt) {
        for (size_t i = 0; i < cnt; i += 1) frames.clr(idx + i);
        DBG::outl(DBG::Frame, "FM/alloc: ", FmtHex(idx * fsize), '/', (cnt * fsize));
        return idx;
      }
      idx += c;
    }
  }

  size_t allocsize(size_t range) {
    return frames.allocsize(divup(range, fsize))
         + tozero.allocsize(divup(range, fsize))
         + FrameMap<PL+1>::allocsize(range);
  }

  void init(size_t range, bufptr_t p) {
    frames.init(divup(range, fsize), p);
    p += frames.allocsize(divup(range, fsize));
    tozero.init(divup(range, fsize), p);
    p += frames.allocsize(divup(range, fsize));
    FrameMap<PL+1>::init(range, p);
  }
};

template<> class FrameMap<kernelpl+1> {
public:
  bool releaseDirect(size_t) { return false; }
  bool release(size_t, size_t) { return false; }
  bool zero(_friend<FrameManager>) { return false; }
  size_t alloc(_friend<FrameManager>) { KABORT1("OUT OF MEMORY"); }
  size_t allocRegion(size_t, size_t, size_t) { KABORT1("IMPOSSIBLE REGION ALLOCATION"); }
  void print(ostream&, paddr, size_t) {}
  size_t allocsize(size_t) { return 0; }
  void init(size_t, bufptr_t) {}
};

class FrameManager {
  friend ostream& operator<<(ostream&, const FrameManager&);
  FrameMap<smallpl> fmap;
  paddr baseAddress;
  size_t memRange;

public:
  size_t preinit( paddr base, paddr top ) {
    baseAddress = base;
    memRange = top - base;
    return fmap.allocsize(memRange);
  }

  void init( bufptr_t p ) {
    fmap.init(memRange, p);
  }

  void release( paddr addr, size_t size ) {
    KASSERT1(aligned(addr, smallps), FmtHex(addr));
    KASSERT1(aligned(size, smallps), FmtHex(size));
    fmap.release((addr - baseAddress) / smallps, size / smallps);
  }

  bool zeroMemory() { return fmap.zero(_friend<FrameManager>()); }

  template<size_t N>
  inline paddr allocFrame() {
    return baseAddress + fmap.FrameMap<N>::alloc(_friend<FrameManager>()) * pagesize<N>();
  }

  paddr allocRegion( size_t& size, paddr align, paddr limitAddress ) {
    KASSERT1(align <= smallps, FmtHex(align));
    size = align_up(size, smallps);
    KASSERT1(size <= pagesize<smallpl+1>(), FmtHex(size));
    limitAddress -= size;
    size_t cnt = size / smallps;
    size_t lim = (limitAddress - baseAddress) / smallps;
    return baseAddress + fmap.allocRegion(cnt, lim, memRange) * smallps;
  }

  static FrameManager& curr() {
    FrameManager* fm = LocalProcessor::getCurrFM(_friend<FrameManager>());
    KASSERT0(fm);
    return *fm;
  }
};

static FrameManager& CurrFM() { return FrameManager::curr(); }

#endif /* _FrameManager_h_ */
