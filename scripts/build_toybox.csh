#!/bin/csh

# setup environement
setenv CC		`pwd`"/musl/musl-1.2.0-build/bin/musl-gcc"
setenv NJOBS		8
setenv CFLAGS		-ffreestanding
setenv LDFLAGS		-static
setenv PREFIX		`pwd`"/toybox/toybox-0.8.4-build"

# create directory if needed
mkdir toybox >& /dev/null
cd toybox

# cleanup directories
rm -rf *
mkdir toybox-0.8.4-build

# download toybox sources
wget http://landley.net/toybox/downloads/toybox-0.8.4.tar.gz
tar -xzvf toybox-0.8.4.tar.gz

# build toybox
cd toybox-0.8.4
make bsd_defconfig
make -j8
make install
