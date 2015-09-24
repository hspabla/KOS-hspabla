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
#include "kernel/SystemProcessor.h"

void SystemProcessor::init0(FrameManager& fm) {
  frameManager = &fm;
  currAS = &defaultAS;
}

void SystemProcessor::initAll(paddr pml4, InterruptDescriptor* idtTable, size_t idtSize, VirtualProcessor& vp, FrameManager& fm) {
  HardwareProcessor::init0();
  HardwareProcessor::init1(pml4, false);
  HardwareProcessor::init2(idtTable, idtSize);
  HardwareProcessor::init3(vp);
  SystemProcessor::init0(fm);
  HardwareProcessor::init4();
  // init async TLB invalidation on this processor
  kernelAS.initInvalidation(kernASM);
  defaultAS.initInvalidation(userASM);
}
