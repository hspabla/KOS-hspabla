for target in run vbox bochs; do
	for compile in gcc gccdebug clang clangdebug; do
		echo -n > /tmp/KOS.serial
		make clean
		case $compile in
			gcc)        flags="";;
			gccdebug)   flags="OPTFLAGS=";;
			clang)      flags="CC=clang";;
			clangdebug) flags="CC=clang OPTFLAGS=";;
			*)          echo $compile; exit 1;
		esac
		make $flags $target &
		while ! fgrep -q "OUT OF MEMORY" /tmp/KOS.serial; do sleep 3; done
		case $target in
			run)   killall qemu-system-x86_64;;
			vbox)  killall VirtualBox;;
			bochs) killall bochs;;
			*)     echo $target; exit 1;;
		esac
		wait
		cp /tmp/KOS.serial /tmp/KOS.serial.$target.$compile
	done
done
echo "TESTSUITE FINISHED - SUCCESS"
exit 0
