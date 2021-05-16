#!/bin/csh

# setup environement
setenv CC		`pwd`"/musl/musl-1.2.0-build/bin/musl-gcc"
setenv NJOBS		8
setenv CFLAGS		-ffreestanding
setenv LDFLAGS		-static

# create directory if needed
mkdir coreutils >& /dev/null
cd coreutils

# cleanup directories
rm -rf *
mkdir coreutils-8.32-build

# download dash sources
wget ftp://ftp.gnu.org/gnu/coreutils/coreutils-8.32.tar.xz
tar -xvf coreutils-8.32.tar.xz

# build dash
cd coreutils-8.32
./configure --target i386-unknown-linux-gnu --host=x86_64-unknown-linux-gnu --prefix=`pwd`'/../coreutils-8.32-build' --disable-nls
make -j$NJOBS
make install
