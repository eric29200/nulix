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
sudo mkdir tmp/usr
sudo mkdir tmp/usr/bin
sudo mkdir tmp/usr/sbin

# cp init process
sudo cp usr/init tmp/sbin/

# cp busybox binaries
sudo cp busybox/busybox-1.33.1/_install/bin/* tmp/bin
sudo cp busybox/busybox-1.33.1/_install/sbin/* tmp/sbin
sudo cp busybox/busybox-1.33.1/_install/usr/bin/* tmp/usr/bin
sudo cp busybox/busybox-1.33.1/_install/usr/sbin/* tmp/usr/sbin

# create devices nodes
sudo mkdir tmp/dev
sudo mkdir tmp/dev/pts
sudo mknod tmp/dev/tty0 c 4 0
sudo mknod tmp/dev/tty1 c 4 1
sudo mknod tmp/dev/tty2 c 4 2
sudo mknod tmp/dev/tty3 c 4 3
sudo mknod tmp/dev/tty4 c 4 4
sudo mknod tmp/dev/tty c 5 0
sudo mknod tmp/dev/ptmx c 5 2
sudo mknod tmp/dev/pts/1 c 136 1
sudo mknod tmp/dev/pts/2 c 136 2
sudo mknod tmp/dev/pts/3 c 136 3
sudo mknod tmp/dev/pts/4 c 136 4

# create resolv.conf
sudo sh -c 'echo "nameserver 192.168.1.1" > tmp/etc/resolv.conf'

# unmount disk
sudo umount tmp
sudo rm -rf tmp
