#!/bin/csh

# go to ports directory
cd `dirname $0`

# setup env
setenv TARGET		i386
setenv SYSROOT		`realpath ../sysroot`

# get port name
set PORT = "linux-headers"

# check if port is available
if ( ! -e "$PORT/install" ) then
	echo "Error : port $PORT is not available"
	exit 1
endif

# get port configuration
source $PORT"/install"

# create build directory
rm -rf build/$PORT
mkdir -p build/$PORT
cd build/$PORT

# download sources
wget $URL"/"$SRC_FILENAME

# extract sources
set SRC_EXTENSION = `echo $SRC_FILENAME | awk -F '.' '{ print $(NF-1)"."$NF }'`
if ($SRC_EXTENSION == "tar.gz") then
	tar -xzvf $SRC_FILENAME
	set SRC_DIR = `tar --list -zf $SRC_FILENAME | head -1`
else if ($SRC_EXTENSION == "tar.bz2") then
	tar -xjvf $SRC_FILENAME
	set SRC_DIR = `tar --list -jf $SRC_FILENAME | head -1`
else if ($SRC_EXTENSION == "tar.xz") then
	tar -xvf $SRC_FILENAME
	set SRC_DIR = `tar --list -f $SRC_FILENAME | head -1`
else
	echo "Error : cannot extract file $SRC_FILENAME"
	exit 1
endif

# patch
foreach PATCH (`find ../../$PORT -name "*.patch"`)
	patch -p0 < $PATCH
end

# install
cd $SRC_DIR
make ARCH=$TARGET prefix="" DESTDIR=$SYSROOT install