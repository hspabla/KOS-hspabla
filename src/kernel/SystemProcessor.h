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
#ifndef _SystemProcessor_h_
#define _SystemProcessor_h_ 1

#include "generic/IntrusiveContainers.h"

struct AddressSpaceMarker : public IntrusiveList<AddressSpaceMarker>::Link {
  sword enterEpoch;
};

class AddressSpace;
class FrameManager;

class SystemProcessor : public HardwareProcessor {
  friend class KernelAddressSpace;
  friend class AddressSpace;
  friend class FrameManager;
  friend class Machine;
  AddressSpace* volatile currAS;
  FrameManager* frameManager;
  AddressSpaceMarker userASM;
  AddressSpaceMarker kernASM;
  void init0(FrameManager& fm)                          __section(".boot.text");
  void initAll(paddr pml4, InterruptDescriptor* idtTable, size_t idtSize,
       VirtualProcessor& vp, FrameManager& fm)          __section(".boot.text");
protected:
  SystemProcessor() : currAS(nullptr), frameManager(nullptr) {}
};

#endif /* _SystemProcessor_h_ */
