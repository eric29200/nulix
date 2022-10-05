#!/bin/bash

DISK1=hda.img
DISK2=hdb.img
DISK_SIZE=512M

# create first disk
rm -f $DISK1
dd if=/dev/zero of=$DISK1 bs=1 count=1 seek=$DISK_SIZE
mkfs.ext2 -b 4096 $DISK1

# create second disk
rm -f $DISK2
dd if=/dev/zero of=$DISK2 bs=1 count=1 seek=$DISK_SIZE
mkfs.minix -3 $DISK2

# mount disk
mkdir tmp >& /dev/null
sudo mount $DISK1 tmp

# create root folders
sudo mkdir -p tmp/etc
sudo mkdir -p tmp/dev
sudo mkdir -p tmp/dev/pts
sudo mkdir -p tmp/proc
sudo mkdir -p tmp/mnt
sudo mkdir -p tmp/tmp
sudo mkdir -p tmp/root

# create config files
sudo sh -c 'echo "root::0:" > tmp/etc/group'
sudo sh -c 'echo "nulix" > tmp/etc/issue.net'
sudo sh -c 'echo "root::0:0:root:/root:/bin/bash" > tmp/etc/passwd'
sudo sh -c 'echo "nameserver 192.168.1.1" > tmp/etc/resolv.conf'
sudo ln -s /proc/mounts tmp/etc/mtab
sudo sh -c 'echo "manpath /man" > tmp/etc/man.conf'
sudo sh -c 'echo "manpath /usr/man" > tmp/etc/man.conf'
sudo sh -c 'echo "manpath /usr/share/man" > tmp/etc/man.conf'
sudo sh -c 'echo "manpath /share/man" >> tmp/etc/man.conf'
sudo sh -c 'echo "manpath /usr/local/man" >> tmp/etc/man.conf'

# create .bashrc
sudo sh -c 'echo "export HOME=/root" > tmp/root/.bashrc'
sudo sh -c 'echo "export TERM=linux" >> tmp/root/.bashrc'
sudo sh -c 'echo "export PATH=/bin:/sbin:/usr/bin:/usr/sbin:/usr/local/bin:/usr/local/sbin" >> tmp/root/.bashrc'
sudo sh -c 'echo "alias ls=\"ls --color\"" >> tmp/root/.bashrc'
sudo sh -c 'echo "alias vi=\"vim\"" >> tmp/root/.bashrc'

# copy root folders
sudo cp -R sysroot/* tmp/

# cp user binaries
sudo mkdir -p tmp/sbin/
sudo cp usr/init tmp/sbin/

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
sudo mknod tmp/dev/pts/1 c 136 1
sudo mknod tmp/dev/pts/2 c 136 2
sudo mknod tmp/dev/pts/3 c 136 3
sudo mknod tmp/dev/pts/4 c 136 4
sudo mknod tmp/dev/mouse c 13 0
sudo mknod tmp/dev/fb0 c 29 0
sudo mknod tmp/dev/hda b 3 0
sudo mknod tmp/dev/hdb b 3 1

# chown root
sudo chown -R 0.0 tmp/

# unmount disk
sudo umount tmp
sudo rm -rf tmp
