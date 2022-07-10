#!/bin/csh

# setup environement
setenv AR 		i686-linux-gnu-ar
setenv AS 		i686-linux-gnu-as
setenv CC 		i686-linux-gnu-gcc
setenv LD 		i686-linux-gnu-ld
setenv RANLIB 		i686-linux-gnu-ranlib
setenv TARGET		i386
setenv SYSROOT		`pwd`/sysroot
setenv CFLAGS		-DSYSCALL_NO_TLS=1
setenv NJOBS		8

# create directory if needed
mkdir musl >& /dev/null
cd musl

# cleanup directories
rm -rf *
mkdir musl-1.2.3-build

# download musl sources
wget https://musl.libc.org/releases/musl-1.2.3.tar.gz
tar -xzvf musl-1.2.3.tar.gz

# build musl
cd ./musl-1.2.3/
make clean
make distclean
./configure --target=$TARGET --prefix=$SYSROOT
make -j$NJOBS
make install
