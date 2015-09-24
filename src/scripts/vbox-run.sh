#!/bin/bash
source ../config
CFGDIR=$(dirname $0)/../../cfg
GDB="$GDBDIR/bin/$TARGET-gdb -x $CFGDIR/gdbinit.cfg"

function leave() {
	echo "arp -d 192.168.56.200"
	echo "arp -d 192.168.56.201"
	echo "iptables -D FORWARD -i vboxnet0 -j ACCEPT"
	echo "iptables -D INPUT -i vboxnet0 -j ACCEPT"
	echo "ifconfig vboxnet0 down"
	vboxmanage unregistervm KOS
}

trap "leave; exit 0" SIGHUP SIGINT SIGQUIT SIGTERM

vboxmanage unregistervm KOS
rm -f /tmp/KOS.vbox.tmp.*
tmpcfg=$(mktemp /tmp/KOS.vbox.tmp.XXXXXXXX)
cp $CFGDIR/KOS.vbox.default $tmpcfg
vboxmanage registervm $tmpcfg
echo "arp -s 192.168.56.200 0800272BA956"
echo "arp -s 192.168.56.201 0800272BA957"
echo "iptables -I FORWARD -i vboxnet0 -j ACCEPT"
echo "iptables -I INPUT -i vboxnet0 -j ACCEPT"

if [ "$1" = "debug" ]; then
	VirtualBox --startvm KOS --debug 2> >(fgrep -v libpng)
elif [ "$1" = "gdb" ]; then
	VirtualBox --startvm KOS --dbg 2> >(fgrep -v libpng) & sleep 5
	socat -b1 TCP-LISTEN:2345,reuseaddr UNIX-CONNECT:/tmp/KOS.pipe & sleep 1
	$GDB -x $CFGDIR/gdbinit.tcp kernel.sys.debug
	kill %1
else
	VirtualBox --startvm KOS --dbg 2> >(fgrep -v libpng)
fi

leave
exit 0
