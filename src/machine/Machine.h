/******************************************************************************
    Copyright � 2012-2015 Martin Karsten

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
#ifndef _Machine_h_
#define _Machine_h_ 1

#include "generic/basics.h"
#include "kernel/SystemProcessor.h"

class Scheduler;
class Thread;

class Machine : public NoObject {
  friend void initGdb(mword); // initGdb calls setupIDT to redirect exception handlers

  static SystemProcessor* processorTable;
  static mword processorCount;

  static void setupIDT(uint32_t, paddr, uint32_t = 0)  __section(".boot.text");
  static void setupIDTable()                           __section(".boot.text");

  static void mapIrq(mword irq, mword vector);
  static void asyncIrqLoop();

  static void initAP2()                                __section(".boot.text");
  static void initBSP2()                               __section(".boot.text");
  static void bootCleanup();

public:
  static void initAP(mword idx)                        __section(".boot.text");
  static void initBSP(mword mag, vaddr mb, mword idx)  __section(".boot.text");
  static void bootMain();

  static mword getProcessorCount() { return processorCount; }
  static SystemProcessor& getProcessor(mword idx) { return processorTable[idx]; }

  static void registerIrqSync(mword irq, mword vec);
  static void registerIrqAsync(mword irq, funcvoid1_t handler, ptr_t ctx);
  static void deregisterIrqAsync(mword irq, funcvoid1_t handler);

  static constexpr inline mword kernCS();
};
void Breakpoint2(vaddr ia = 0) __ninline;
static inline void Breakpoint(vaddr ia = 0) {
  asm volatile( "xchg %%bx, %%bx" ::: "memory" );
  Breakpoint2(ia);
}

#endif /* _Machine_h_ */
