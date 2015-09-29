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
#include "runtime/Scheduler.h"
#include "machine/Processor.h"

struct AddressSpaceMarker : public IntrusiveList<AddressSpaceMarker>::Link {
  sword enterEpoch;
};

class SystemProcessor : public Processor {
  friend class KernelAddressSpace;
  friend class AddressSpace;
  AddressSpaceMarker userASM;
  AddressSpaceMarker kernASM;
  Scheduler scheduler;
public:
  SystemProcessor() : Processor(this) {}
  void start(funcvoid0_t func);
  Scheduler& getScheduler() { return scheduler; }
};

#endif /* _SystemProcessor_h_ */
