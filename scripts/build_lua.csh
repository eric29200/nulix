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
mkdir lua >& /dev/null
cd lua

# cleanup directories
rm -rf *

# download lua sources
wget http://www.lua.org/ftp/lua-5.4.4.tar.gz
tar -xzvf lua-5.4.4.tar.gz

# build lua
cd lua-5.4.4
make -j$NJOBS CC=$CC LDFLAGS=$LDFLAGS
make install INSTALL_TOP=$INSTALL_DIR
