#!/bin/bash

function runtest() {
	make $1 iso
	echo -n > /tmp/KOS.serial
	make $1 $2 &
	sleep 3
	case $2 in
		qemu)  pid=$(pgrep -f "qemu-system-x86_64.*KOS");;
		vbox)  pid=$(pgrep -f ".*/lib/.*VirtualBox.*KOS");;
		bochs) pid=$(pgrep -f "bochs.*kos");;
		*)     echo $2; exit 1;;
	esac
	trap "kill $pid; exit 1" SIGHUP SIGINT SIGQUIT SIGTERM
	while ! fgrep -q "OUT OF MEMORY" /tmp/KOS.serial; do sleep 3; done
	kill $pid
	wait
}

if [ $# -ge 1 ]; then
	while true; do
		for target in ${*}; do
			runtest "" $target
		done
	done
	exit 0
fi
		
for compile in gcc clang # gccdebug clangdebug
do
	make clean
	case $compile in
		gcc)        flags="";;
		gccdebug)   flags="OPTFLAGS=";;
		clang)      flags="CC=clang";;
		clangdebug) flags="CC=clang OPTFLAGS=";;
		*)          echo $compile; exit 1;
	esac
	make $flags all
	for target in qemu vbox bochs
	do
		runtest "$flags" "$target"
		cp /tmp/KOS.serial /tmp/KOS.serial.$target.$compile
	done
done
echo "TESTSUITE FINISHED - SUCCESS"
exit 0
