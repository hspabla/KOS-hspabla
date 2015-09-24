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
#ifndef _Output_h_
#define _Output_h_ 1

#include "generic/Bitmap.h"
#include "kernel/SpinLock.h"

#include <cstdarg>

static const char kendl = '\n';

template<typename _CharT, typename _Traits = char_traits<_CharT>>
class OutputBuffer : public basic_streambuf<_CharT, _Traits> {
public:
  typedef _CharT														char_type;
  typedef _Traits														traits_type;
  typedef typename traits_type::int_type		int_type;
  typedef typename traits_type::pos_type		pos_type;
  typedef typename traits_type::off_type		off_type;

  typedef basic_streambuf<_CharT, _Traits>  BaseClass;

protected:
  virtual streamsize xsputn(const char_type* s, streamsize n) = 0;
  virtual int sync() { return BaseClass::sync(); }
};

class KernelOutput {
  OwnerLock olock;
  ostream os;

public:
  KernelOutput( OutputBuffer<char>& ob ) : os(&ob) {}

  void lock() { olock.acquire(); }
  void unlock() { olock.release(); }

  template <bool cpu> void print() {}

  template<bool cpu, typename T, typename... Args>
  void print( const T& msg, const Args&... a ) {
    if (cpu) os << 'C' << LocalProcessor::getIndex() << '/' << FmtHex(CPU::readCR3()) << ": ";
    os << msg;
    print<false>(a...);
  }

  template<bool cpu, typename T, typename... Args>
  void printl( const T& msg, const Args&... a ) {
    ScopedLock<OwnerLock> sl(olock);
    if (cpu) os << 'C' << LocalProcessor::getIndex() << '/' << FmtHex(CPU::readCR3()) << ": ";
    os << msg;
    print<false>(a...);
  }

  ssize_t write(const void *buf, size_t len) {
    ScopedLock<OwnerLock> sl(olock);
    os.write((cbufptr_t)buf, len);
    return len;
  }
};

extern KernelOutput StdOut;
extern KernelOutput StdErr;
extern KernelOutput StdDbg;

class DBG {
public:
  enum Level : size_t {
    Acpi = 0,
    AddressSpace,
    Boot,
    Basic,
    CDI,
    Devices,
    Error,
    Frame,
    File,
    GDBDebug,
    GDBEnable,
    Idle,
    KernMem,
    Libc,
    Lwip,
    MemAcpi,
    Paging,
    PCI,
    Perf,
    Process,
    Scheduler,
    Tests,
    Threads,
    VM,
    Warning,
    MaxLevel
  };

private:
  static Bitmap<> levels;
  static_assert(MaxLevel <= sizeof(levels) * 8, "too many debug levels");

public:
  static void init( char* dstring, bool msg );
  static bool test( Level c ) { return levels.test(c); }

  template<typename... Args> static void out1( Level c, const Args&... a ) {
    if (c && !test(c)) return;
    StdDbg.printl<false>(a...);
#if TESTING_DEBUG_STDOUT
    if (c) StdOut.printl<true>(a...);
#endif
  }
  template<typename... Args> static void outs( Level c, const Args&... a ) {
    if (c && !test(c)) return;
    StdDbg.printl<false>(a...);
#if TESTING_DEBUG_STDOUT
    if (c) StdOut.printl<true>(a...);
#endif
  }
  template<typename... Args> static void outl( Level c, const Args&... a ) {
    if (c && !test(c)) return;
    StdDbg.printl<true>(a..., kendl);
#if TESTING_DEBUG_STDOUT
    if (c) StdOut.printl<true>(a..., kendl);
#endif
  }
  static void outl( Level c ) {
    if (c && !test(c)) return;
    StdDbg.printl<false>(kendl);
#if TESTING_DEBUG_STDOUT
    if (c) StdOut.printl<false>(kendl);
#endif
  }
};

class KOUT {
public:
  template<typename... Args> static void out1( const Args&... a ) {
    StdOut.printl<false>(a...);
#if TESTING_STDOUT_DEBUG
    StdDbg.printl<true>(a...);
#endif
  }
  template<typename... Args> static void outl( const Args&... a ) {
    StdOut.printl<false>(a..., kendl);
#if TESTING_STDOUT_DEBUG
    StdDbg.printl<true>(a..., kendl);
#endif
  }
};

class KERR {
public:
  template<typename... Args> static void out1( const Args&... a ) {
    StdErr.printl<false>(a...);
#if TESTING_STDOUT_DEBUG
    StdDbg.printl<true>(a...);
#endif
  }
  template<typename... Args> static void outl( const Args&... a ) {
    StdErr.printl<false>(a..., kendl);
#if TESTING_STDOUT_DEBUG
    StdDbg.printl<true>(a..., kendl);
#endif
  }
};

template<typename... Args>
static inline void kassertprint1(const Args&... a) {
  StdErr.lock();
  StdErr.print<false>(a...);
  StdDbg.lock();
  StdDbg.print<true>(a...);
}

template<typename... Args>
static inline void kassertprint2(const Args&... a) {
  StdErr.print<false>(a..., kendl);
  StdErr.unlock();
  StdDbg.print<false>(a..., kendl);
  StdDbg.unlock();
}

#define KABORTN(args...)       {                      { kassertprints(  "KABORT: "       " in " __FILE__ ":", __LINE__, __func__); kassertprint2(" - ", args); Reboot(); } }
#define KASSERTN(expr,args...) { if slowpath(!(expr)) { kassertprints( "KASSERT: " #expr " in " __FILE__ ":", __LINE__, __func__); kassertprint2(" - ", args); Reboot(); } }
#define KCHECKN(expr,args...)  { if slowpath(!(expr)) { kassertprints(  "KCHECK: " #expr " in " __FILE__ ":", __LINE__, __func__); kassertprint2(" - ", args); } }

extern void ExternDebugPrintf(DBG::Level c, const char* fmt, va_list args);

#endif /* _Output_h_ */
