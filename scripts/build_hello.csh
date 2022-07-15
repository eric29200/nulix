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
mkdir hello >& /dev/null
cd hello

# cleanup directories
rm -rf *

# download hello sources
wget https://ftp.gnu.org/gnu/hello/hello-2.12.1.tar.gz
tar -xzvf hello-2.12.1.tar.gz

# build hello
cd hello-2.12.1
./configure --host=$TARGET --disable-nls --prefix=$INSTALL_DIR
make -j$NJOBS install