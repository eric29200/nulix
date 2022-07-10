#!/bin/csh

# setup environement
setenv AR 		i686-linux-gnu-ar
setenv AS 		i686-linux-gnu-as
setenv CC 		i686-linux-gnu-gcc
setenv LD 		i686-linux-gnu-ld
setenv RANLIB 		i686-linux-gnu-ranlib
setenv TARGET		i386
setenv SYSROOT		`pwd`/sysroot
setenv MUSL_CC		$SYSROOT"/bin/musl-gcc"
setenv INSTALL_DIR	`pwd`/root/

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
make clean
make -j8 CC=$MUSL_CC LDFLAGS="-static"
make install INSTALL_TOP=$INSTALL_DIR