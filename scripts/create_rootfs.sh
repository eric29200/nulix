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
sudo mkdir -p tmp/dev
sudo mkdir -p tmp/proc
sudo mkdir -p tmp/mnt
sudo mkdir -p tmp/tmp
sudo mkdir -p tmp/run
sudo mkdir -p tmp/var/run

# populate disk
sudo cp -R musl/musl-install/i386-linux-musl/lib tmp/
sudo cp -R musl/musl-install/i386-linux-musl/share tmp/
sudo cp -R musl/musl-install/i386-linux-musl/usr tmp/
sudo cp -R musl/musl-install/i386-linux-musl/etc tmp/
sudo cp -R musl/musl-install/i386-linux-musl/bin tmp/
sudo cp -R musl/musl-install/i386-linux-musl/sbin tmp/
sudo cp -R rootbase/etc/* tmp/etc/
sudo cp -R rootbase/root tmp/

# remove x86_64 binaries
sudo rm -f tmp/bin/pkgconf tmp/bin/ar tmp/bin/as tmp/bin/ld tmp/bin/ld.bfd tmp/bin/objdump
sudo rm -f tmp/bin/objcopy tmp/bin/ranlib tmp/bin/nm tmp/bin/readelf tmp/bin/strip

# create links
mkdir -p tmp/bin
sudo ln -s /usr/bin/bash tmp/bin/bash
sudo ln -s /usr/bin/bash tmp/bin/sh
sudo ln -s /proc/mounts tmp/etc/mtab

# chown root
sudo chown -R 0:0 tmp/

# unmount disk
sudo umount tmp
sudo rm -rf tmp

# detach loop device
sudo losetup -d $LOOP_DEVICE