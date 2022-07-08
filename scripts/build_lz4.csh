#!/bin/csh

# setup environement
setenv MUSL_CC		`pwd`"/musl/musl-1.2.3-build/bin/musl-gcc"

# create port directory if needed
mkdir ports >& /dev/null
cd ports

# create directory if needed
mkdir lz4 >& /dev/null
cd lz4

# cleanup directories
rm -rf *

# download lz4 sources
wget https://github.com/lz4/lz4/archive/refs/tags/v1.9.3.tar.gz
tar -xzvf v1.9.3.tar.gz

# build lz4
cd lz4-1.9.3
make -j8 CC=$MUSL_CC CFLAGS="-static"
