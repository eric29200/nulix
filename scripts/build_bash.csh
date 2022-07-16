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
mkdir bash >& /dev/null
cd bash

# cleanup directories
rm -rf *

# download bash sources
wget https://ftp.gnu.org/gnu/bash/bash-5.1.16.tar.gz
tar -xzvf bash-5.1.16.tar.gz

# build bash
cd bash-5.1.16
./configure --host=$TARGET --disable-nls --without-gnu-malloc --prefix=$INSTALL_DIR
make -j$NJOBS install
