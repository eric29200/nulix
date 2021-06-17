#!/bin/csh

set DISK		= hdd.img
set DISK_SIZE		= 512M

# create disk
dd if=/dev/zero of=$DISK bs=1 count=1 seek=$DISK_SIZE
mkfs.minix -2 $DISK

# mount disk
mkdir tmp >& /dev/null
sudo mount $DISK tmp

# create folders
sudo mkdir tmp/sbin
sudo mkdir tmp/bin
sudo mkdir tmp/proc
sudo mkdir tmp/etc

# cp init process
sudo cp usr/init tmp/sbin/

# cp dash binary
sudo cp dash/dash-0.5.10.2-build/bin/dash tmp/bin

# cp toybox binaries
sudo cp toybox/toybox-0.8.4-build/* tmp/bin

# create devices nodes
sudo mkdir tmp/dev
sudo mknod tmp/dev/tty0 c 4 0
sudo mknod tmp/dev/tty1 c 4 1
sudo mknod tmp/dev/tty2 c 4 2
sudo mknod tmp/dev/tty3 c 4 3
sudo mknod tmp/dev/tty4 c 4 4
sudo mknod tmp/dev/tty c 5 0

# create resolv.conf
sudo sh -c 'echo "nameserver 192.168.1.1" > tmp/etc/resolv.conf'

# unmount disk
sudo umount tmp
sudo rm -rf tmp
