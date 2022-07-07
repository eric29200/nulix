#!/bin/csh

# setup environement
setenv MUSL_CC		`pwd`"/musl/musl-1.2.3-build/bin/musl-gcc"

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
make clean
./configure --host=i386 CC=$MUSL_CC CFLAGS="-static"
make -j8
