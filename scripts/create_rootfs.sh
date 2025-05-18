#!/bin/bash

DISK=hda.img
DISK_SIZE=2G

if [[ `basename $PWD` != "nulix" ]]; then
	echo "This script must be run from main/root directory"
	exit 1
fi

# create disk
qemu-img create -f raw $DISK $DISK_SIZE

# mount device
LOOP_DEVICE=`sudo losetup -Pf --show $DISK`

# create partition table
sudo parted --script $LOOP_DEVICE mktable msdos
sudo parted --script $LOOP_DEVICE mkpart primary 2048s 100%

# create file system
sudo mkfs.ext2 ${LOOP_DEVICE}p1

# mount disk
mkdir tmp >& /dev/null
sudo mount ${LOOP_DEVICE}p1 tmp

# create root folders
sudo mkdir -p tmp/etc
sudo mkdir -p tmp/dev/pts tmp/dev/shm
sudo mkdir -p tmp/proc
sudo mkdir -p tmp/mnt
sudo mkdir -p tmp/tmp
sudo mkdir -p tmp/run
sudo mkdir -p tmp/var/run tmp/var/log

# populate disk
sudo cp -R musl/musl-install/i386-linux-musl/lib tmp/
sudo cp -R musl/musl-install/i386-linux-musl/share tmp/
sudo cp -R musl/musl-install/i386-linux-musl/usr tmp/
sudo cp -R musl/musl-install/i386-linux-musl/etc tmp/
sudo cp -R musl/musl-install/i386-linux-musl/bin tmp/
sudo cp -R musl/musl-install/i386-linux-musl/sbin tmp/
sudo cp -R musl/musl-install/i386-linux-musl/root tmp/
sudo cp -R rootbase/etc/* tmp/etc/
sudo cp -R rootbase/root tmp/

# remove x86_64 binaries
sudo rm -f tmp/bin/pkgconf tmp/bin/ar tmp/bin/as tmp/bin/ld tmp/bin/ld.bfd tmp/bin/objdump
sudo rm -f tmp/bin/objcopy tmp/bin/ranlib tmp/bin/nm tmp/bin/readelf tmp/bin/strip

# create links
mkdir -p tmp/bin
sudo ln -s /proc/mounts tmp/etc/mtab
sudo ln -s /usr/sbin/init tmp/sbin/init
sudo ln -s /usr/sbin/getty tmp/sbin/getty
sudo ln -s /usr/bin/sh tmp/bin/sh
sudo ln -s /usr/bin/mount tmp/bin/mount
sudo ln -s /usr/X11 tmp/usr/X11R6

# create devices
sudo mknod tmp/dev/null c 1 3
sudo mknod tmp/dev/zero c 1 5
sudo mknod tmp/dev/random c 1 8
sudo mknod tmp/dev/urandom c 1 9
sudo mknod tmp/dev/tty0 c 4 0
sudo mknod tmp/dev/tty1 c 4 1
sudo mknod tmp/dev/tty2 c 4 2
sudo mknod tmp/dev/tty3 c 4 3
sudo mknod tmp/dev/tty4 c 4 4
sudo mknod tmp/dev/tty5 c 4 5
sudo mknod tmp/dev/tty6 c 4 6
sudo mknod tmp/dev/tty7 c 4 7
sudo mknod tmp/dev/tty c 5 0
sudo mknod tmp/dev/console c 5 1
sudo mknod tmp/dev/ptmx c 5 2
sudo mknod tmp/dev/mouse c 13 0
sudo mknod tmp/dev/fb0 c 29 0
sudo mknod tmp/dev/hda b 3 0
sudo mknod tmp/dev/hda1 b 3 1
sudo mknod tmp/dev/hdc b 3 32
sudo ln -s /dev/hda1 tmp/dev/root

# chown root
sudo chown -R 0:0 tmp/

# unmount disk
sudo umount tmp
sudo rm -rf tmp

# detach loop device
sudo losetup -d $LOOP_DEVICE
