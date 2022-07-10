#!/bin/csh

# setup environement
setenv AR 		i686-linux-gnu-ar
setenv AS 		i686-linux-gnu-as
setenv CC 		i686-linux-gnu-gcc
setenv LD 		i686-linux-gnu-ld
setenv RANLIB 		i686-linux-gnu-ranlib
setenv TARGET		i386
setenv SYSROOT		`pwd`/sysroot
setenv MUSL_CC		$SYSROOT"/bin/musl-gcc"

# create port directory if needed
mkdir ports >& /dev/null
cd ports

# create directory if needed
mkdir busybox >& /dev/null
cd busybox

# cleanup directories
rm -rf *

# download busybox sources
wget https://busybox.net/downloads/busybox-1.35.0.tar.bz2
tar -xjvf busybox-1.35.0.tar.bz2

# build busybox
cd busybox-1.35.0
cp ../../../config/busybox.config .config
make uninstall
make -j8 CC=$MUSL_CC
make -j8 install CC=$MUSL_CC
