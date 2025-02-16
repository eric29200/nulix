#!/bin/bash

DISK=hdb.img
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

# populate disk
cd tmp
sudo tar -xvf ../buildroot/build/buildroot-2024.02.10/output/images/rootfs.tar.xz
cd ..

# chown root
sudo chown -R 0:0 tmp/

# unmount disk
sudo umount tmp
sudo rm -rf tmp

# detach loop device
sudo losetup -d $LOOP_DEVICE