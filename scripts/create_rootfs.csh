#!/bin/csh

set DISK		= hdd.img
set DISK_SIZE		= 512M

# create disk
dd if=/dev/zero of=$DISK bs=1 count=1 seek=$DISK_SIZE
mkfs.minix -2 $DISK

# mount disk
mkdir tmp >& /dev/null
sudo mount $DISK tmp

# create root folders
sudo mkdir -p tmp/bin
sudo mkdir -p tmp/sbin
sudo mkdir -p tmp/usr/bin
sudo mkdir -p tmp/usr/sbin
sudo mkdir -p tmp/dev
sudo mkdir -p tmp/dev/pts
sudo mkdir -p tmp/proc

# copy root folders
sudo cp -Rv root/etc tmp/

# cp user binaries
sudo cp usr/init tmp/sbin/
sudo cp usr/xserver tmp/usr/bin/

# cp busybox binaries
sudo cp -P busybox/busybox-1.33.1/_install/bin/* tmp/bin
sudo cp -P busybox/busybox-1.33.1/_install/sbin/* tmp/sbin
sudo cp -P busybox/busybox-1.33.1/_install/usr/bin/* tmp/usr/bin
sudo cp -P busybox/busybox-1.33.1/_install/usr/sbin/* tmp/usr/sbin

# create devices nodes
sudo mknod tmp/dev/null c 1 3
sudo mknod tmp/dev/zero c 1 5
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
sudo mknod tmp/dev/fb c 29 0
sudo mknod tmp/dev/mouse c 13 0

# chown root
sudo chown -R 0.0 tmp/

# unmount disk
sudo umount tmp
sudo rm -rf tmp
