#!/bin/bash

for compile in gcc clang gccdebug clangdebug; do

	make clean
	case $compile in
		gcc)        flags="";;
		gccdebug)   flags="OPTFLAGS=";;
		clang)      flags="CC=clang";;
		clangdebug) flags="CC=clang OPTFLAGS=";;
		*)          echo $compile; exit 1;
	esac
	make $flags all

	for target in run vbox bochs; do
		echo -n > /tmp/KOS.serial
		make $flags $target &
		sleep 1
		case $target in
			run)   pid=$(pgrep -f "qemu-system-x86_64.*KOS");;
			vbox)  pid=$(pgrep -f ".*/lib/.*VirtualBox.*KOS");;
			bochs) pid=$(pgrep -f "bochs.*kos");;
			*)     echo $target; exit 1;;
		esac
		trap "kill $pid; exit 1" SIGHUP SIGINT SIGQUIT SIGTERM
		while ! fgrep -q "OUT OF MEMORY" /tmp/KOS.serial; do sleep 3; done
		kill $pid
		wait
		cp /tmp/KOS.serial /tmp/KOS.serial.$target.$compile
	done

done
echo "TESTSUITE FINISHED - SUCCESS"
exit 0
