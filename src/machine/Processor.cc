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
#include "runtime/RuntimeImpl.h"
#include "runtime/Thread.h"
#include "kernel/AddressSpace.h"
#include "kernel/Output.h"
#include "machine/APIC.h"
#include "machine/Processor.h"

void Processor::init(paddr pml4, bool output, InterruptDescriptor* idtTable, size_t idtSize) {
  DBG::Level dl = output ? DBG::Basic : DBG::MaxLevel;
  DBG::outl(dl, "*********** CPU INFO ***********");
  DBG::out1(dl, "checking BSP capabilities:");
  KASSERT0(__atomic_always_lock_free(sizeof(mword),0));
  KASSERT0(CPU::CPUID());      DBG::out1(dl, " CPUID");
  KASSERT0(CPUID::MSR());      DBG::out1(dl, " MSR");
  KASSERT0(CPUID::APIC());     DBG::out1(dl, " APIC");
  KASSERT0(CPUID::NX());       DBG::out1(dl, " NX");
  KASSERT0(CPUID::SYSCALL());  DBG::out1(dl, " SYSCALL");
  KASSERT0(CPUID::FXSR());     DBG::out1(dl, " FXSR");
  if (CPUID::MWAIT())          DBG::out1(dl, " MWAIT");
  if (CPUID::X2APIC())         DBG::out1(dl, " X2A");
  if (CPUID::POPCNT())         DBG::out1(dl, " POPCNT");
  if (CPUID::TSCD())           DBG::out1(dl, " TSC");
  if (CPUID::ARAT())           DBG::out1(dl, " ARAT");
  if (CPUID::FSGSBASE())       DBG::out1(dl, " FSGSBASE");
  if (CPUID::Page1G())         DBG::out1(dl, " Page1G");
  DBG::outl(dl);

  MSR::enableNX();                                   // enable NX paging bit
  CPU::writeCR4(CPU::readCR4() | CPU::PGE());        // enable  G paging bit
//  CPU::writeCR4(CPU::readCR4() | CPU::FSGSBASE());    // enable fs/gs base instructions
  CPU::writeCR4(CPU::readCR4() | CPU::OSFXSR());     // enable FP context switch
  CPU::writeCR4(CPU::readCR4() | CPU::OSXMMEXCPT()); // enable FP exceptions
  // CPU::writeCR0(CPU::readCR0() | CPU::MP());         // enable monitor coprocessor
  CPU::writeCR0(CPU::readCR0() & ~(CPU::EM()));      // disable x87 emulation
  if (pml4 != topaddr) CPU::writeCR3(pml4);          // install page tables

  Context::install();

  memset(gdt, 0, sizeof(gdt)); // set up GDT
  setupGDT(kernCS, 0, true);
  setupGDT(kernDS, 0, false);  // DPL *mostly* ignored for data selectors (except for iret)
  setupGDT(userDS, 3, false);  // DPL *mostly* ignored for data selectors (except for iret)
  setupGDT(userCS, 3, true);
  setupTSS(tssSel, (vaddr)&tss);
  loadGDT(gdt, sizeof(gdt));
  loadTR(tssSel * sizeof(SegmentDescriptor));
  clearLDT();                  // LDT is not used
  loadIDT(idtTable, idtSize);  // install interrupt table
}

void Processor::startup(BaseScheduler& sched, funcvoid0_t func) {
  MSR::enableSYSCALL();                               // enable syscall/sysret
  // top  16 bits: index 2 = userDS - 1 = userCS - 2; * 8 (selector size) + 3 (CPL); userDS follows this index, followed by userCS
  // next 16 bits: index 1 = kernCS     = kernDS - 1; * 8 (selector size) + 0 (CPL); kernCS is at   this index, followed by kernDS
  MSR::write(MSR::SYSCALL_STAR, mword(0x0013000800000000));
  MSR::write(MSR::SYSCALL_LSTAR, mword(syscall_wrapper));
  MSR::write(MSR::SYSCALL_CSTAR, 0x0);
  MSR::write(MSR::SYSCALL_SFMASK, CPU::RFlags::IF()); // disable interrupts during syscall

  // set up TSS: rsp[0] is set to per-thread kernel stack before sysretq
  memset(&tss, 0, sizeof(TaskStateSegment));

  // dedicated fault stack for some exceptions handlers
  vaddr fstack = kernelAS.allocStack(faultStack) + faultStack;
  tss.ist[nmiIST-1] = fstack;
  tss.ist[dbfIST-1] = fstack;
  tss.ist[stfIST-1] = fstack;
  DBG::outl(DBG::Basic, "Fault Stack for ", index, " at ", FmtHex(fstack));

  Thread* idleThread = Thread::create(idleStack);
  idleThread->setScheduler(sched)->setAffinity(true)->setPriority(idlePriority);
  idleThread->setup((ptr_t)Runtime::idleLoop);
  idleThread->resume();
  currThread = Thread::create(defaultStack);
  currThread->setScheduler(sched)->setAffinity(true)->direct((ptr_t)func);
}

void Processor::setupGDT(unsigned int number, unsigned int dpl, bool code) {
  KASSERT1(number < maxGDT, number);
  KASSERT1(dpl < 4, dpl);
  gdt[number].RW = 1;
  gdt[number].C = code ? 1 : 0;
  gdt[number].S = 1;
  gdt[number].DPL = dpl;
  gdt[number].P = 1;
  gdt[number].L = 1;
}

void Processor::setupTSS(unsigned int number, paddr address) {
  SystemDescriptor* tssDesc = (SystemDescriptor*)&gdt[number];
  tssDesc->Limit00 = 0xffff;
  tssDesc->Base00 = (address & 0x000000000000FFFF);
  tssDesc->Base16 = (address & 0x0000000000FF0000) >> 16;
  tssDesc->Type = 0x9; // label as 'available 64-bit TSS'
  tssDesc->P = 1;
  tssDesc->Base24 = (address & 0x00000000FF000000) >> 24;
  tssDesc->Base32 = (address & 0xFFFFFFFF00000000) >> 32;
}

void LocalProcessor::initInterrupts(bool irqs) {
  MappedAPIC()->setFlatMode();         // set flat logical destination mode
  MappedAPIC()->setLogicalDest(irqs ? 0x01 : 0x00); // join irq group
  MappedAPIC()->setTaskPriority(0x00); // accept all interrupts
	MappedAPIC()->enable(0xff);         // enable APIC, set spurious IRQ to 0xff
	unlock(1);
}
