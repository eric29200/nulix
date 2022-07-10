#!/bin/csh

# setup environement
setenv MUSL_CC		`pwd`"/musl/musl-1.2.3-build/bin/musl-gcc"
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
./configure --host=i386 --prefix=$INSTALL_DIR CC=$MUSL_CC CFLAGS="-static"
make uninstall
make -j8
make install
