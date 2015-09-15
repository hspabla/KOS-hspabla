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
#include "kernel/FrameManager.h"
#include "machine/Paging.h"

ostream& operator<<(ostream& os, const FrameManager& fm) {
  ScopedLock<> sl(const_cast<FrameManager&>(fm).lock);
  size_t top = (fm.topAddress - fm.baseAddress) / fm.largeSize;
  size_t idx = 0;
  size_t cnt;
  while ((cnt = fm.largeFrames.findrange(idx, top))) {
    os << ' ' << FmtHex(fm.baseAddress + fm.largeSize * idx)
       << '-' << FmtHex(fm.baseAddress + fm.largeSize * (idx+cnt));
    idx += cnt;
  }
  top = (fm.topAddress - fm.baseAddress) / fm.smallSize;
  idx = 0;
  while ((cnt = fm.smallFrames.findrange(idx, top))) {
    os << ' ' << FmtHex(fm.baseAddress + fm.smallSize * idx)
       << '-' << FmtHex(fm.baseAddress + fm.smallSize * (idx+cnt));
    idx += cnt;
  }
  return os;
}

paddr FrameManager::allocRegion(size_t& size, paddr align, paddr lim) {
  KASSERT1(lim >= baseAddress, FmtHex(lim));
  ScopedLock<> sl(lock);
  if (size < largeSize) {                // search in small frames first
    size = align_up(size, smallSize);
    lim = lim - size;
    size_t target = size / smallSize;
    size_t top = (topAddress - baseAddress) / smallSize;
    for (size_t c, idx = 0; (c = smallFrames.findrange(idx, top)); idx += c) {
      paddr addr = baseAddress + smallSize * idx;
      if ((c >= target) && (addr <= lim)) {
        // clear entries in small frame bitmap
        for (size_t i = 0; i < target; i += 1) smallFrames.clr(idx + i);
        DBG::outl(DBG::Frame, "FM/allocC1: ", FmtHex(addr), '/', size);
        return addr;
      }
    }
    size_t idx = largeFrames.find();     // try splitting a large frame
    if (idx != limit<size_t>()) {
      paddr addr = largeSize * idx;
      if (addr <= lim) {
        // remove from large frame bitmap
        largeFrames.clr(idx);
        // convert from large to small idx
        idx = idx * ptentries;
        // set leftover entries in small frame bitmap
        for (size_t i = target; i < ptentries; i += 1) smallFrames.set(idx + i);
        DBG::outl(DBG::Frame, "FM/allocC2: ", FmtHex(addr), '/', size);
        return addr;
      }
    }
  } else {                               // search for block in large frames
    size = align_up(size, largeSize);
    lim = lim - size;
    size_t target = size / largeSize;
    size_t top = (topAddress - baseAddress) / largeSize;
    for (size_t c, idx = 0; (c = largeFrames.findrange(idx, top)); idx += c) {
      paddr addr = baseAddress + largeSize * idx;
      if ((c >= target) && (addr <= lim)) {
        for (size_t i = 0; i < target; i += 1) largeFrames.clr(idx + i);
        DBG::outl(DBG::Frame, "FM/allocC3: ", FmtHex(addr), '/', FmtHex(size));
        return addr;
      }
    }
  }
  KABORTN(FmtHex(size), ' ', FmtHex(align), ' ', FmtHex(lim));
}

void FrameManager::zeroInternal() {
  // get memory region from queue
  ZeroDescriptor* zd = zeroQueue.front();
  zeroQueue.pop_front();
  // drop FM lock, then delete ZD object without holding lock
  lock.release();
  paddr addr = zd->addr;
  size_t size = zd->size;
  zdCache.deallocate(zd);

  // map (using 2M pages) and zero memory without holding FM lock
  while (size > 0) {
    paddr astart = align_down(addr, largeSize);
    size_t offset = addr - astart;
    size_t zsize = min(size, largeSize - offset);
    vaddr v = LocalProcessor::getZeroAddr();
    Paging::mapPage<kernelpl>(v, astart, Paging::Data, _friend<FrameManager>());
    DBG::outl(DBG::Frame, "FM/zero: ", FmtHex(astart), '/', FmtHex(addr), '/', FmtHex(zsize));
    memset((ptr_t)(v + offset), 0, zsize);
    Paging::unmap<kernelpl>(v, _friend<FrameManager>());
    // re-acquire FM lock and make frames available for future allocation
    lock.acquire();
    releaseInternal(addr, zsize);
    lock.release();
    addr += zsize;
    size -= zsize;
  }
}
