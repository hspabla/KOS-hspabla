SRCDIR:=$(CURDIR)
include $(SRCDIR)/Makefile.config
SOURCEDIRS=generic runtime kernel machine devices world main gdb tools
SOURCES=$(wildcard $(addsuffix /*.cc,$(SOURCEDIRS)))
BSOURCES=$(notdir $(SOURCES))
OBJECTS=$(BSOURCES:%.cc=%.o)
SOBJECTS=$(BSOURCES:%.cc=%.s)
EOBJECTS=$(BSOURCES:%.cc=%.e)
AS_SOURCES=$(wildcard $(addsuffix /*.S,$(SOURCEDIRS)))
AS_OBJECTS=$(subst .S,.o,$(notdir $(AS_SOURCES)))
KERNLIBS=\
	extern/acpica/libacpica.a\
	extern/cdi/libcdi.o\
	extern/dlmalloc/malloc.o\
	extern/lwip/liblwip.o
USERLIBS=ulib/libKOS.a
SUBDIRS=main user $(dir $(KERNLIBS) $(USERLIBS))

vpath %.cc $(SOURCEDIRS)
vpath %.S $(SOURCEDIRS)

.PHONY: all iso userfiles Makefile.state .FORCE

KOS=kernel.sys userfiles

all: $(KOS)

iso: $(ISO)

########################## run targets ##########################

.PHONY: run qemu qpxe debug rnet rnetdebug gdb gdbdebug tgdb bochs vbox vboxd vgdb

run: qemu

qemu: $(ISO)
	$(QEMU) $(QEMU_UNET) $(QEMU_IMG) $(QEMU_SER)

qpxe: tftp
	$(QEMU) $(QEMU_UNET) $(QEMU_PXE) $(QEMU_SER)

debug: $(ISO)
	# xterm/nohup: shield qemu from Ctrl-C sent to gdb
	xterm -e nohup $(QEMU) $(QEMU_UNET) $(QEMU_IMG) $(QEMU_SER) -s -S &
	$(GDB) -x $(CFGDIR)/gdbinit.qemu kernel.sys.debug

rnet: $(ISO)
	touch $(TMPFILES) # make sure tmp files are owned by user
	su -c "scripts/qemu-ifup.sh; $(QEMU) $(QEMU_RNET) $(QEMU_IMG) $(QEMU_SER); scripts/qemu-ifdown.sh"

rnetdebug: $(ISO)
	touch $(TMPFILES) # make sure tmp files are owned by user
	# xterm/su: also shields qemu from Ctrl-C sent to gdb
	xterm -e su -c "scripts/qemu-ifup.sh; $(QEMU) $(QEMU_NET) $(QEMU_IMG) $(QEMU_SER) -s -S; scripts/qemu-ifdown.sh" &
	$(GDB) -x $(CFGDIR)/gdbinit.qemu kernel.sys.debug

gdb:
	rm -f $(ISO)
	$(MAKE) RGDB=$@ $(ISO)
	xterm -e nohup $(QEMU) $(QEMU_IMG) $(QEMU_GDB) &
	$(GDB) -x $(CFGDIR)/gdbinit.tcp kernel.sys.debug

gdbdebug:
	rm -f $(ISO)
	$(MAKE) RGDB=$@ $(ISO)
	xterm -e $(GDB) -x $(CFGDIR)/gdbinit.qemu kernel.sys.debug &
	xterm -e nohup $(QEMU) $(QEMU_IMG) $(QEMU_GDB) -s -S &
	$(GDB) -x $(CFGDIR)/gdbinit.tcp kernel.sys.debug

tgdb:
	$(GDB) -x $(CFGDIR)/gdbinit.ttyS0 kernel.sys.debug

bochs: $(ISO)
	$(TOOLSDIR)/bin/bochs -f $(CFGDIR)/bochsrc -q -rc $(CFGDIR)/bochsrc.dbg

vbox: $(ISO)
	scripts/vbox-run.sh

vboxd: $(ISO)
	scripts/vbox-run.sh debug

vgdb:
	rm -f $(ISO)
	$(MAKE) RGDB=gdb $(ISO)
	rm -f /tmp/KOS.pipe
	scripts/vbox-run.sh gdb

ulib: $(USERLIBS)

######################### stage targets #########################

.PHONY: pxe tftp usb

$(ISO): $(KOS) scripts/setup_boot.sh
	rm -rf $(STAGE)
	./scripts/setup_boot.sh iso $@ $(STAGE) kernel.sys $(RGDB) user/exec/*

pxe: $(KOS) scripts/setup_boot.sh
	rm -rf $(STAGE)
	./scripts/setup_boot.sh pxe $(STAGE) kernel.sys $(RGDB) user/exec/*

$(IMAGE): $(KOS) scripts/setup_boot.sh
	dd if=/dev/zero of=$@ bs=516096c count=64 2>/dev/null
	su -c "./scripts/setup_boot.sh img $@ $(STAGE) kernel.sys $(RGDB) user/exec/*" || { rm -f $@; false; }

tftp: pxe
#	ssh $(TFTPSERVER) rm -rf $(TFTPDIR)/boot
	$(GRUBDIR)/bin/grub-mknetdir --net-directory=$(STAGE)
	rsync -lprtuvz $(STAGE)/boot/ $(TFTPSERVER):$(TFTPDIR)/boot/ --delete
	@echo "ATTENTION: You also need pxelinux.0 pxelinux.cfg/default vesamenu.c32"
	@echo "           in $(TFTPSERVER):$(TFTPDIR)"

usb: $(KOS) scripts/setup_boot.sh
	@test -b $(USBDEV)1 || { echo no USB device found at $(USBDEV) && false; }
	su -c "./scripts/setup_boot.sh usb $(USBDEV) $(STAGE) kernel.sys $(RGDB) user/exec/*"

############ build targets ############

kernel.sys: kernel.sys.debug
	$(STRIP) $@.debug -o $@

kernel.sys.debug: linker.ld $(AS_OBJECTS) $(OBJECTS) $(KERNLIBS)
	rm -f $@
	$(LD) $(LDFLAGS) -T linker.ld -o $@ $(filter-out $<,$^) $(LDDEF) $(LIBS)

$(OBJECTS): %.o: %.cc
	$(CXX) -c $(CXXFLAGS) $<
	
$(AS_OBJECTS): %.o: %.S
	$(AS) $(ASFLAGS) $< -o $@

$(SOBJECTS): %.s: %.cc
	$(CXX) -S $(PREFLAGS) $<
	
$(EOBJECTS): %.e: %.cc
	$(CXX) -E $(PREFLAGS) $< > $@

$(AS_OBJECTS) $(OBJECTS) $(SOBJECTS) $(EOBJECTS): Makefile.config

main/UserMain.h: .FORCE
	$(MAKE) -C main

$(KERNLIBS) $(USERLIBS): .FORCE
	$(MAKE) -C $(dir $@) $(notdir $@)

userfiles: $(USERLIBS)
	$(MAKE) -C user

######################### housekeeping targets #########################

.PHONY: defines echo clean vclean dep depend

defines:
	$(CXX) -dM -E - < /dev/null
	
echo:
	@echo SOURCES: $(SOURCES)
	@echo BSOURCES: $(BSOURCES)
	@echo OBJECTS: $(OBJECTS)
	@echo SUBDIRS: $(SUBDIRS)

clean:
	@for d in $(SUBDIRS); do $(MAKE) -C $$d $@; done
	rm -f kernel.sys.debug kernel.sys $(VDISK) $(ISO)\
		$(AS_OBJECTS) $(OBJECTS) $(SOBJECTS) $(EOBJECTS)
# rm -f $(BSOURCES:%.cc=%.{e,o,s})
	rm -rf $(STAGE)
	rm -f Makefile.o.dep Makefile.s.dep Makefile.e.dep
	rm -f nohup.out

vclean: clean
	@for d in $(SUBDIRS); do $(MAKE) -C $$d $@; done
	rm -f Makefile.state Makefile.dep
	rm -f /tmp/KOS.dbg /tmp/KOS.serial /tmp/KOS.pipe /tmp/qemu.log
	rm -f /tmp/KOS.serial.*

dep depend Makefile.dep: main/UserMain.h
	@for d in $(SUBDIRS); do $(MAKE) -C $$d $@; done
	$(CXX) -MM $(CXXFLAGS) $(SOURCES) > Makefile.o.dep
	cat Makefile.o.dep | sed -e "/.o:/s//.s:/" > Makefile.s.dep
	cat Makefile.o.dep | sed -e "/.o:/s//.e:/" > Makefile.e.dep
	cat Makefile.o.dep Makefile.s.dep Makefile.e.dep > Makefile.dep
	rm -f Makefile.o.dep Makefile.s.dep Makefile.e.dep

include Makefile.state

Makefile.state:
	@echo "CC_SAVE=$(CC)" > Makefile.state
	@echo "RGDB_SAVE=$(RGDB)" >> Makefile.state
ifneq ($(CC),$(CC_SAVE))
	@$(MAKE) clean
endif
ifneq ($(RGDB),$(RGDB_SAVE))
	@rm -f $(ISO)
endif

boot.o: machine/asmshare.h
isr_wrapper.o: generic/regsave.h
syscall_wrapper.o: machine/asmshare.h
Stack.o: generic/regsave.h

-include Makefile.dep
-include Makefile.local # development targets, not for release
