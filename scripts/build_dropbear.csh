#!/bin/csh

# setup environement
setenv CC		`pwd`"/musl/musl-1.2.2-build/bin/musl-gcc"

# create directory if needed
mkdir dropbear >& /dev/null
cd dropbear

# cleanup directories
rm -rf *
mkdir dropbear-2020.81-build

# download dropbear sources
wget https://mirror.dropbear.nl/mirror/releases/dropbear-2020.81.tar.bz2
tar -xjvf dropbear-2020.81.tar.bz2

# build dropbear
cd dropbear-2020.81
./configure --host=i386 --enable-static --disable-zlib --prefix=`pwd`'/../dropbear-2020.81-build'
make -j8
make install
