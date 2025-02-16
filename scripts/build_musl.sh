#!/bin/bash

NJOBS=`nproc`

if [[ `basename $PWD` != "nulix" ]]; then
	echo "This script must be run from main/root directory"
	exit 1
fi

# create a build directory
cd musl
rm -rf musl-build
cp -R musl-cross-make musl-build
cd musl-build

# build musl
make -j$NJOBS install TARGET=i386-linux-musl

# fix musl
mkdir output/i386-linux-musl/usr
cd output/i386-linux-musl/usr
ln -s ../include include
ln -s ../bin bin
ln -s ../lib lib
