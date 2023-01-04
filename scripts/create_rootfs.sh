#!/bin/bash

DISK1=hda.img
DISK_SIZE=2048M

if [[ `basename $PWD` != "nulix" ]]; then
	echo "This script must be run from main/root directory"
	exit 1
fi

# create first disk
rm -f $DISK1
dd if=/dev/zero of=$DISK1 bs=1 count=1 seek=$DISK_SIZE
mkfs.ext2 $DISK1

# mount disk
mkdir tmp >& /dev/null
sudo mount $DISK1 tmp

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
