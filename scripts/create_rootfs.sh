#!/bin/bash

DISK1=hda.img
DISK_SIZE=2048M

if [[ `basename $PWD` != "nulix" ]]; then
	echo "This script must be run from main/root directory"
	exit 1
fi

# compile user programs
make -C usr/

# create first disk
rm -f $DISK1
dd if=/dev/zero of=$DISK1 bs=1 count=1 seek=$DISK_SIZE
mkfs.minix -3 $DISK1

# mount disk
mkdir tmp >& /dev/null
sudo mount $DISK1 tmp

# create root folders
sudo mkdir -p tmp/etc
sudo mkdir -p tmp/dev
sudo mkdir -p tmp/dev/pts
sudo mkdir -p tmp/dev/input
sudo mkdir -p tmp/proc
sudo mkdir -p tmp/mnt
sudo mkdir -p tmp/tmp
sudo mkdir -p tmp/sbin

# populate disk
sudo cp -R musl/musl-install/i386-linux-musl/lib tmp/
sudo cp -R musl/musl-install/i386-linux-musl/share tmp/
sudo cp -R musl/musl-install/i386-linux-musl/usr tmp/
sudo cp -R musl/musl-install/i386-linux-musl/etc tmp/
sudo cp -R rootbase/etc/* tmp/etc/
sudo cp -R rootbase/root tmp/
sudo cp usr/init tmp/sbin/

# create links
mkdir -p tmp/bin
sudo ln -s /usr/bin/bash tmp/bin/sh
sudo ln -s /lib/libc.so tmp/lib/ld-musl-i386.so.1
sudo ln -s /proc/mounts tmp/etc/mtab

# create devices nodes
sudo mknod tmp/dev/null c 1 3
sudo mknod tmp/dev/zero c 1 5
sudo mknod tmp/dev/random c 1 8
sudo mknod tmp/dev/tty0 c 4 0
sudo mknod tmp/dev/tty1 c 4 1
sudo mknod tmp/dev/tty2 c 4 2
sudo mknod tmp/dev/tty3 c 4 3
sudo mknod tmp/dev/tty4 c 4 4
sudo mknod tmp/dev/tty c 5 0
sudo mknod tmp/dev/ptmx c 5 2
sudo mknod tmp/dev/mouse c 13 0
sudo mknod tmp/dev/fb0 c 29 0
sudo mknod tmp/dev/hda b 3 0
sudo mknod tmp/dev/hdb b 3 1
sudo ln -s ../mouse tmp/dev/input/mice

# chown root
sudo chown -R 0:0 tmp/

# unmount disk
sudo umount tmp
sudo rm -rf tmp
