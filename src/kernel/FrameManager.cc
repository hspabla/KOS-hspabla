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

void FrameZero(vaddr vma, paddr pma, size_t offset, size_t size, _friend<FrameManager> ff) {
  Paging::mapPage<kernelpl>(vma, pma, Paging::Data, ff);
  DBG::outl(DBG::Frame, "FM/zero: ", FmtHex(pma + offset), '/', FmtHex(size));
  memset((ptr_t)(vma + offset), 0, size);
  Paging::unmap<kernelpl>(vma, ff);
}

template<size_t PL>
inline void FrameMap<PL>::print(ostream& os, paddr base, size_t range) {
  ScopedLock<> sl(lock);
  size_t max = range / fsize;
  size_t idx = 0;
  size_t cnt;
  while ((cnt = frames.findrange(idx, max))) {
    os << ' ' << FmtHex(base + fsize * idx)
       << '-' << FmtHex(base + fsize * (idx+cnt));
    idx += cnt;
  }
  FrameMap<PL+1>::print(os, base, range);
}

ostream& operator<<(ostream& os, const FrameManager& fm) {
  const_cast<FrameManager&>(fm).fmap.print(os, fm.baseAddress, fm.memRange);
  return os;
}
