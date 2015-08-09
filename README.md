KOS
===

KOS (pronounce "Chaos") is an experimental operating system kernel that is designed to be simple and accessible to serve as a platform for research, experimentation, and teaching.
The current version runs on a bare-metal AMD/Intel 64-bit platform (x86-64), utilizing all CPU cores in 64-bit mode, and using virtual memory through 64-bit paging.
The system is comprised of a small runtime nucleus along with other subsystems typically found in an operating system kernel -- at various levels of maturity.
For example, KOS provides basic memory management, scheduling, threads, address spaces, processes, system calls, etc. It implements APIC programming, interrupt handling, and PCI enumeration. A few device drivers are included. It also comes with basic debugging support using gdb remote debugging. The system has been tested on the virtual platforms [qemu](http://www.qemu.org/), [bochs](http://bochs.sourceforge.net/), and [VirtualBox](https://www.virtualbox.org/), as well as a few actual machines.


### Installation

Installation instructions are provided in <tt>INSTALL.md</tt>.

Known bugs are described in <tt>BUGS.md</tt>.


### Documentation

TBD


### Contributors

The following students (in alphabetical order) have helped with getting various parts of the system off the ground:

- Sukown Oh (e1000, gdb)
- Behrooz Shafiee (elf, keyboard, lwip, pit, syscall/sysret)
- Priyaa Varshinee Srinivasan (synchronization)
- Alex Szlavik (bootstrap)
- Cameron White (clang support)


### License

KOS is currently distributed under the GNU GPL license, although this could change in the future.


### Third-Party Software

KOS is built using *gcc* with *newlib* as the C library, as well as *grub* for booting. KOS can also be compiled using *clang* (using *gcc*'s C++ library). Further, KOS integrates *acpica* and *lwip* as external software packages. Please see their respective license information when downloading the source code. Finally, KOS is distributed with and uses the following software packages, which can be found in the `src/extern/` directory.  Please see the source code for detailed license information.  The summary below is just a high-level overview of my interpretation of the license terms.

- *cdi*:        BSD-type license
- *dlmalloc*:   public domain / creative commons license
- *elfio*:      BSD-type license
- *multiboot*:  BSD-type license


### Feedback / Questions

Please send any questions or feedback to mkarsten|**at**|uwaterloo.ca.

