#!/bin/csh

# setup environement
setenv TARGET		i386
setenv SYSROOT		`pwd`/sysroot
setenv CC		$SYSROOT"/bin/musl-gcc"
setenv LD		$SYSROOT"/bin/musl-gcc"
setenv INSTALL_DIR	`pwd`/root/
setenv CFLAGS		"-static"
setenv LDFLAGS		"-static"
setenv NJOBS		8

# create port directory if needed
mkdir ports >& /dev/null
cd ports

# create directory if needed
mkdir coreutils >& /dev/null
cd coreutils

# cleanup directories
rm -rf *

# download coreutils sources
wget https://ftp.gnu.org/gnu/coreutils/coreutils-9.1.tar.xz
tar -xvf coreutils-9.1.tar.xz

# build coreutils
cd coreutils-9.1
./configure --host=$TARGET --disable-nls --disable-threads --enable-no-install-program=factor,ptx --prefix=$INSTALL_DIR
make -j$NJOBS install
