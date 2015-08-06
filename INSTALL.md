KOS - Installation
==================

### Tool Chain

KOS is built using a custom tool chain. To prepare the tool chain, edit the first few lines of `setup_crossenv.sh` and `config`.  You will need to download the appropriate source packages from your friendly neighbourhood mirror. If you run into problems, please see [Installing GCC](https://gcc.gnu.org/install/) and [Prerequisites for GCC](https://gcc.gnu.org/install/prerequisites.html) from the official GCC documentation.

A somewhat recent version of *xorriso* (>-1.0.0) is necessary for making ISO boot images using *grub-mkrescue*. The package might also be called *libisoburn*.

Run `setup_crossenv.sh gcc gdb grub 2>&1 | tee setup.out` (bash syntax) to build the complete tool chain. The output should contain the following (among other output; grep for SUCCESS):

`SUCCESS: gcc-4.9.2 install`  
`SUCCESS: gdb-7.9.1 install`  
`SUCCESS: grub-2.02~beta2 install`

Note that the cross compiler expects the KOS system library (`libKOS.a`) in the `src/ulib` directory when linking user-level programs.  This configuration setting is hard-coded in the *link_libgcc* and *libgcc* specs of the cross compiler.  This is controlled by the variable `ULDIR` in `setup_crossenv.sh`.


### Hardware Emulation

The script `setup_crossenv.sh` also contains suggested configurations to build *bochs* and *qemu* and will build and install those in `TOOLSDIR` (default is `/usr/local`) when invoked with the corresponding command-line argument.  Building *qemu* requires the development version of the Simple DirectMedia Layer package (http://www.libsdl.org).  The package might be called *sdl* or *libsdl-dev*, depending on the Linux distribution.


### Building KOS

To prepare the KOS source code (starting from the main directory), download and unpack the *acpica* and *lwip* packages:

```
cd src/extern
wget https://acpica.org/sites/acpica/files/acpica-unix-20150717.tar.gz
tar xaf acpica-unix-20150717.tar.gz
rm acpica-unix-20150717.tar.gz
mv acpica-unix-20150717 acpica
patch -d acpica -p1 < ../../patches/acpica.patch
wget http://download.savannah.gnu.org/releases/lwip/lwip-1.4.1.zip
unzip -d lwip lwip-1.4.1.zip
rm lwip-1.4.1.zip
mv lwip/lwip-1.4.1 lwip/lwip
patch -d lwip/lwip -p1 < ../../patches/lwip-1.4.1.patch
cd ../..
```

You should be back in the main directory. Run `make` to get a list of build targets. These targets should also work from within the `src/` directory. Make sure to **not** have any of *gcc*'s or *ld*'s `PATH` environment variables set, such as `C_INCLUDE_PATH` or `LD_LIBRARY_PATH`, since those might interfere with the cross compiler setup. Run `make all` to build the kernel binary and/or `make iso` to build the ISO image.


### Running KOS

#### Hardware Emulation

To execute KOS with hardware emulation (qemu, bochs, or VirtualBox), run either of these commands:

`make run`  
`make bochs`  
`make vbox` -- requires VirtualBox setup, start with `cfg/KOS.vbox.default`

When executing KOS, the system should show a number of bootstrap messages and then get into a split-screen mode, where the first 20 lines are showing output from several threads running on several cores and the bottom 5 lines show characters as keys are pressed. If the *memoryhog* user application is executed (see `main/InitProcess.cc`), the system eventually stops with an "OUT OF MEMORY" error message.

Running KOS in *qemu* creates several log files that can be used to diagnose problems:

`/tmp/KOS.serial`  
`/tmp/KOS.dbg`  
`/tmp/qemu.log` -- currently disabled, see `QEMU_LOG` in `Makefile.config`

`KOS.dbg` and `KOS.serial` are two different output channels internally, but currently contain essentially the same information.  Running KOS with *bochs* or *VirtualBox* only produces `/tmp/KOS.serial`.


#### PXE Boot

`make pxe` stages a *grub*-based boot setup in `src/stage/boot`. `make tftp` uploads this setup to a TFTP server, according to the setting of `TFTPSERVER` and `TFTDDIR` in the main `config` file. The boot process can be invoked, for example, through chain-loading from syslinux (via the `pxelinux.0` binary) using the following boot entry in `/tftpboot/pxelinux.cfg/default`:

```
label kosgrub
  menu label kosgrub
  menu default
  kernel boot/grub/i386-pc/core.0
```


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

