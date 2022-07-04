#!/bin/csh

# setup environement
setenv MUSL_CC		`pwd`"/musl/musl-1.2.3-build/bin/musl-gcc"

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
make clean
cp ../../config/busybox.config .config
make -j8 CC=$MUSL_CC
make -j8 install CC=$MUSL_CC
