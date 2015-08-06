KOS
===

KOS (pronounce "Chaos") is an experimental operating system kernel that is designed to be simple and accessible to serve as a platform for research, experimentation, and teaching.
The current version runs on a bare-metal AMD/Intel 64-bit platform (x86-64), utilizing all CPU cores in 64-bit mode, and using virtual memory through 64-bit paging.
The system is comprised of a small runtime nucleus along with other subsystems typically found in an operating system kernel -- at various levels of maturity.
For example, KOS provides basic memory management, scheduling, threads, address spaces, processes, system calls, etc. It implements APIC programming, interrupt handling, and PCI enumeration. A few device drivers are included. It also comes with basic debugging support using gdb remote debugging. The system has been tested on the virtual platforms [qemu](http://www.qemu.org/), [bochs](http://bochs.sourceforge.net/), and [VirtualBox](https://www.virtualbox.org/), as well as a few actual machines.

Installation instructions are provided in <tt>INSTALL.md</tt>.

