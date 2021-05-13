#!/bin/csh

# setup environement
setenv CC		`pwd`"/musl/musl-1.2.0-build/bin/musl-gcc"
setenv NJOBS		8

# create directory if needed
mkdir dash >& /dev/null
cd dash

# cleanup directories
rm -rf *
mkdir dash-0.5.10.2-build

# download dash sources
wget https://git.kernel.org/pub/scm/utils/dash/dash.git/snapshot/dash-0.5.10.2.tar.gz
tar -xzvf dash-0.5.10.2.tar.gz

# build dash
cd dash-0.5.10.2
./autogen.sh
./configure --enable-static --target i386-unknown-linux-gnu --host=x86_64-unknown-linux-gnu --prefix=`pwd`'/../dash-0.5.10.2-build'
make -j$NJOBS
make install
