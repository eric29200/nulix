#!/bin/csh

# setup environement
setenv CC		`pwd`"/musl/musl-1.2.0-build/bin/musl-gcc"
setenv LDFLAGS		"-static"
setenv TARGET		i386
setenv NJOBS		8

# create directory if needed
mkdir bash >& /dev/null
cd bash

# cleanup directories
rm -rf bash-5.1 bash-5.1-build bash-5.1.tar.gz
mkdir bash-5.1-build

# download bash sources
wget ftp://ftp.gnu.org/gnu/bash/bash-5.1.tar.gz
tar -xzvf bash-5.1.tar.gz

# build bash
cd ./bash-5.1
./configure --host=$TARGET --enable-static-link --disable-nls --without-bash-malloc --prefix=`pwd`'/../bash-5.1-build'
make -j$NJOBS
make install
