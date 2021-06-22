#!/bin/csh

# setup environement
setenv CC		`pwd`"/musl/musl-1.2.2-build/bin/musl-gcc"
setenv NJOBS		8
setenv CFLAGS		-ffreestanding
setenv LDFLAGS		-static
setenv PREFIX		`pwd`"/toybox/toybox-0.8.5-build"

# create directory if needed
mkdir toybox >& /dev/null
cd toybox

# cleanup directories
rm -rf *
mkdir toybox-0.8.5-build

# download toybox sources
wget http://landley.net/toybox/downloads/toybox-0.8.5.tar.gz
tar -xzvf toybox-0.8.5.tar.gz

# build toybox
cd toybox-0.8.5
make clean
cp ../../config/toybox-0.8.5.config .config
make -j8
make install_flat
