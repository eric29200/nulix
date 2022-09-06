#!/bin/sh

NJOBS=`nproc`

# create a build directory
cd musl
rm -rf musl-build
cp -R musl-cross-make musl-build
cd musl-build

# build musl
make -j$NJOBS install TARGET=i386-linux-musl