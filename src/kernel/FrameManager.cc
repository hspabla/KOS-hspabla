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
#include "kernel/FrameManager.h"

paddr FrameManager::allocRegion(size_t& size, paddr align, paddr limit) {
  size = align_up(size, defaultps);
  ScopedLock<> sl(lock);
  if (size < kernelps) { // search in small frames first
    for (auto it = smallFrames.begin(); it != smallFrames.end(); ++it) {
      size_t idx = it->second.findset();
      mword found = 0;
      size_t baseidx;
      paddr baseaddr;
      for (;;) { 
        if (found) {
          found += defaultps;
        } else {
          baseidx = idx;
          baseaddr = it->first * kernelps + baseidx * defaultps;
    if (baseaddr + size > limit) goto use_large_frame;
          if (aligned(baseaddr, align)) found = defaultps;
        }
        if (found >= size) {
          for (size_t i = baseidx; i <= idx; i += 1) it->second.clear(i);
          if (it->second.empty()) smallFrames.erase(it);
          return baseaddr;
        }
        for (;;) { // keeps found while consecutive bits are set
          idx += 1;
      if (idx >= ptentries) goto next_small_set;
        if (it->second.test(idx)) break;
          found = 0;
        }
      }
next_small_set:;
    }
use_large_frame:
    size_t idx = largeFrames.findset();
    if (idx * kernelps + size > limit) goto allocFailed;
    largeFrames.clear(idx);
    auto it = smallFrames.emplace(idx, Bitmap<ptentries>::filled()).first;
    for (size_t i = 0; i < size/defaultps; i += 1) it->second.clear(i);
    KASSERT0(!it->second.empty());
    return idx * kernelps;
  } else { // search in large frames by index, same as above
    size = align_up(size, kernelps);
    size_t idx = largeFrames.findset();
    mword found = 0;
    size_t baseidx;
    paddr baseaddr;
    for (;;) {
      if (found) {
        found += kernelps;
      } else {
        baseidx = idx;
        baseaddr = idx * kernelps;
    if (baseaddr + size > limit) goto allocFailed;
        if (aligned(baseaddr, align)) found = kernelps;
      }
      if (found >= size) {
        for (size_t i = baseidx; i <= idx; i += 1) largeFrames.clear(i);
        return baseaddr;
      }
      for (;;) { // keeps found while consecutive bits are set
        idx += 1;
    if (idx >= largeFrameCount) goto allocFailed;
      if (largeFrames.test(idx)) break;
        found = 0;
      }
    }
  }
allocFailed:
  KABORTN(FmtHex(size), ' ', FmtHex(align), ' ', FmtHex(limit));
}

void FrameManager::initZero() {
  zeroAS = new AddressSpace;
  zeroAS->initUser(userbot);
}

void FrameManager::zeroInternal() {
  // get memory region from queue
  ZeroDescriptor* zd = zeroQueue.front();
  zeroQueue.pop_front();
  // drop FM lock, then delete ZD object without holding lock
  lock.release();
  paddr addr = zd->addr;
  size_t size = zd->size;
  kdelete(zd);
  // map and zero memory without hold FM lock
  DBG::outl(DBG::Frame, "FM/zero: ", FmtHex(addr), '/', FmtHex(size));
  paddr astart = align_down(addr, kernelps);
  size_t offset = addr - astart;
  size_t asize = align_up(size + offset, kernelps);
  // all mappings use 2M pages
  vaddr v = CurrAS().kmap<kernelpl,false>(0, asize, astart);
  memset((ptr_t)(v + offset), 0,  size);
  CurrAS().kunmap<kernelpl,false>(v, asize);
  // re-acquire FM lock and make frames available for future allocation
  lock.acquire();
  releaseInternal(addr, size);
}

bool FrameManager::zeroMemory() {
  AddressSpace::Temp ast(*zeroAS);
  ScopedLock<> sl(lock);
  if (zeroQueue.empty()) return false;
  zeroInternal();
  return true;
}

ostream& operator<<(ostream& os, const FrameManager& fm) {
  ScopedLock<> sl(const_cast<FrameManager&>(fm).lock);
  size_t start = fm.largeFrames.findset();
  size_t end = start;
  for (;;) {
    if (start >= fm.largeFrameCount) break;
    end = fm.largeFrames.getrange(start, fm.largeFrameCount);
    os << ' ' << FmtHex(start*kernelps) << '-' << FmtHex(end*kernelps);
    if (end >= fm.largeFrameCount) break;
    start = fm.largeFrames.getrange(end, fm.largeFrameCount);
  }
  start = topaddr;
  for (	auto it = fm.smallFrames.begin(); it != fm.smallFrames.end(); ++it ) {
    if (start != topaddr && it->first * kernelps != end) {
      os << ' ' << FmtHex(start) << '-' << FmtHex(end);
      start = topaddr;
    }
    for (size_t idx = 0; idx < ptentries; idx += 1) {
      if (it->second.test(idx)) {
        if (start == topaddr) end = start = it->first * kernelps + idx * pagesize<1>();
        end += pagesize<1>();
      } else if (start != topaddr) {
        os << ',' << FmtHex(start) << '-' << FmtHex(end);
        start = topaddr;
      }
    }
  }
  return os;
}
