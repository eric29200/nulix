#!/bin/sh

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