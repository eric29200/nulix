#!/bin/csh

# setup environement
setenv AR 		i686-linux-gnu-ar
setenv AS 		i686-linux-gnu-as
setenv CC 		i686-linux-gnu-gcc
setenv LD 		i686-linux-gnu-ld
setenv RANLIB 		i686-linux-gnu-ranlib

setenv TARGET		i386
setenv CFLAGS		-DSYSCALL_NO_TLS=1
setenv NJOBS		8

# create directory if needed
mkdir musl >& /dev/null
cd musl

# cleanup directories
rm -rf musl-1.2.0 musl-1.2.0-build musl-1.2.0.tar.gz
mkdir musl-1.2.0-build

# download musl sources
wget https://musl.libc.org/releases/musl-1.2.0.tar.gz
tar -xzvf musl-1.2.0.tar.gz

# build musl
cd ./musl-1.2.0/
make clean
make distclean
./configure --target=$TARGET --prefix=`pwd`'/../musl-1.2.0-build'
make -j$NJOBS
make install
