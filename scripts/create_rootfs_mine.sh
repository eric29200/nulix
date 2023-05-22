#!/bin/bash

DISK=hda_mine.img
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
sudo mkdir -p tmp/dev
sudo mkdir -p tmp/proc
sudo mkdir -p tmp/mnt
sudo mkdir -p tmp/tmp
sudo mkdir -p tmp/bin
sudo mkdir -p tmp/sbin

# populate disk
sudo cp -R usr/build/* tmp/
sudo cp -R rootbase/* tmp/

# chown root
sudo chown -R 0:0 tmp/

# unmount disk
sudo umount tmp
sudo rm -rf tmp

# detach loop device
sudo losetup -d $LOOP_DEVICE
