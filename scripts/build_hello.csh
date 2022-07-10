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
mkdir hello >& /dev/null
cd hello

# cleanup directories
rm -rf *

# download hello sources
wget https://ftp.gnu.org/gnu/hello/hello-2.12.1.tar.gz
tar -xzvf hello-2.12.1.tar.gz

# build hello
cd hello-2.12.1
./configure --host=$TARGET --prefix=$INSTALL_DIR CC=$MUSL_CC CFLAGS="-static"
make uninstall
make -j8
make install
