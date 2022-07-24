#!/bin/csh

set URL			= "https://musl.libc.org/releases/"
set VERSION		= "1.2.3"
set SRC_FILENAME	= "musl-"$VERSION".tar.gz"

# go to musl directory
cd `dirname $0`

# setup env
setenv TARGET		i386
setenv SYSROOT		`realpath ../sysroot`
setenv AR 		i686-linux-gnu-ar
setenv AS 		i686-linux-gnu-as
setenv CC 		i686-linux-gnu-gcc
setenv LD 		i686-linux-gnu-ld
setenv RANLIB 		i686-linux-gnu-ranlib
setenv CFLAGS		-DSYSCALL_NO_TLS=1
setenv NJOBS		`nproc`

# create build directory
rm -rf build
mkdir -p build
cd build/

# download sources
wget $URL"/"$SRC_FILENAME

# extract sources
tar -xzvf $SRC_FILENAME
set SRC_DIR = `tar --list -zf $SRC_FILENAME | head -1`

# patch
foreach PATCH (`find ../ -name "*.patch"`)
	patch -p0 < $PATCH
end

# configure
cd $SRC_DIR
./configure --host=$TARGET --prefix=$SYSROOT

# build
make -j$NJOBS
make install