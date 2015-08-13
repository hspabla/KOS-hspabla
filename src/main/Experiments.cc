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
#include "runtime/Scheduler.h"
#include "runtime/Thread.h"
#include "kernel/Output.h"
#include "machine/APIC.h"
#include "machine/Machine.h"

extern void (*tipiHandler)(void);

// simple IPI measurements, TODO: should do IPI ping pong
namespace IPI_Experiment {

static volatile bool done = false;

static volatile mword tipiCount = 0;
static void tipiCounter() { tipiCount += 1; }

static mword rCount = 0;
static void receiver() {
  Machine::setAffinity(*Runtime::getCurrThread(), 1);
  KOUT::outl("IPI receiver @ core ", LocalProcessor::getIndex());
  Runtime::getScheduler()->yield();
  KOUT::outl("IPI receiver @ core ", LocalProcessor::getIndex());
  while (!done) rCount += 1;
}

static mword sCount = 0;
static mword tscStart, tscEnd;
static void sender() {
  Machine::setAffinity(*Runtime::getCurrThread(), 2);
  KOUT::outl("IPI sender @ core ", LocalProcessor::getIndex());
  Runtime::getScheduler()->yield();
  KOUT::outl("IPI sender @ core ", LocalProcessor::getIndex());
  tscStart = CPU::readTSC();
  for (int i = 0; i < 1000; i += 1) {
    mword tc = tipiCount;
    Machine::sendIPI(1, APIC::TestIPI);
    while (tc == tipiCount) sCount += 1;
  }
  tscEnd = CPU::readTSC();
  done = true;
}

static void run() {
  KOUT::outl("IPI experiment running...");
  tipiHandler = tipiCounter;
  Thread* t = Thread::create();
//  Machine::setAffinity(*t, 1);
  t->start((ptr_t)receiver);
  t = Thread::create();
//  Machine::setAffinity(*t, 2);
  t->start((ptr_t)sender);
  while (!done);
  KOUT::outl("IPI experiment results: ", tipiCount, ' ', sCount, ' ', rCount, ' ', tscEnd - tscStart);
}

} // namespace IPI_Experiment

int Experiments() {
  IPI_Experiment::run();
  return 0;
}
