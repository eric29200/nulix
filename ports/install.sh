#!/bin/bash

# base ports, needed to build other ports
BASE_PORTS=("pkgconf" "libncurses" "zlib" "openssl" "libpng" "expat" "freetype2")

if [[ `basename $PWD` != "nulix" ]]; then
	echo "This script must be run from main/root directory"
	exit 1
fi

################################
######### install musl #########
################################
function install_musl() {
	# create musl install directory
	rm -rf ../musl/musl-install
	cp -R ../musl/musl-build/output ../musl/musl-install
}

##############################################
######### check if port is available #########
##############################################
function check_port() {
	if [[ ! -f "$PORT/install" ]]; then
		echo "Error : port $PORT is not available"
		exit 1
	fi

	# load port environ
	URL_GIT=0
	source $PORT"/install"
}

###########################################
####### Download and extract a port #######
###########################################
function download_and_extract_port() {
	# create build directory
	rm -rf build/$PORT
	mkdir -p build/$PORT
	cd build/$PORT

	# download sources
	if [[ $URL_GIT == 1 ]]; then
		SRC_DIR=$SRC_FILENAME
		git clone $URL
		return
	else
		wget $URL"/"$SRC_FILENAME
	fi

	# extract sources
	SRC_EXTENSION=`echo $SRC_FILENAME | awk -F '.' '{ print $(NF-1)"."$NF }'`
	if [[ $SRC_EXTENSION == "tar.gz" ]]; then
		tar -xzvf $SRC_FILENAME
		SRC_DIR=`tar --list -zf $SRC_FILENAME | head -1 | awk -F '/' '{ print $1 }'`
	elif [[ $SRC_EXTENSION == "tar.bz2" ]]; then
		tar -xjvf $SRC_FILENAME
		SRC_DIR=`tar --list -jf $SRC_FILENAME | head -1 | awk -F '/' '{ print $1 }'`
	elif [[ $SRC_EXTENSION == "tar.xz" ]]; then
		tar -xvf $SRC_FILENAME
		SRC_DIR=`tar --list -f $SRC_FILENAME | head -1 | awk -F '/' '{ print $1 }'`
	else
		echo "Error : cannot extract file $SRC_FILENAME"
		exit 1
	fi
}

##########################################
############## Patch a port ##############
##########################################
function patch_port() {
	for PATCH in `find ../../$PORT -name "*.patch"`; do
		patch -p0 < $PATCH
	done
}



# check number of arguments
if [[ $# == 0 ]]; then
	echo "Usage : ports/install.sh port_name (or all)"
	exit 1
fi

# go to ports directory
cd `dirname $0`
BASE_DIR=`pwd`

# install musl directory
if [[ ! -d "../musl/musl-install" ]]; then
	install_musl
fi

# global environ
export TARGET=i386-elf
export HOST=i386-linux-musl
export NJOBS=`nproc`
export SYSROOT=`realpath ../musl/musl-install/i386-linux-musl`
export CC=`realpath ../musl/musl-install/bin/i386-linux-musl-gcc`
export CXX=`realpath ../musl/musl-install/bin/i386-linux-musl-g++`
export LD=`realpath ../musl/musl-install/bin/i386-linux-musl-ld`
export AR=`realpath ../musl/musl-install/bin/i386-linux-musl-ar`
export AS=`realpath ../musl/musl-install/bin/i386-linux-musl-as`
export RANLIB=`realpath ../musl/musl-install/bin/i386-linux-musl-ranlib`
export READELF=`realpath ../musl/musl-install/bin/i386-linux-musl-readelf`
export PKG_CONFIG=$SYSROOT"/bin/pkgconf"
export PKG_CONFIG_PATH=$SYSROOT"/lib/pkgconfig:"$SYSROOT"/usr/lib/pkgconfig"
export PKG_CONFIG_LIBDIR=$SYSROOT"/lib/pkgconfig"

# get ports list
PORTS=()
BUILD_ALL=0
for PORT in $@; do
	if [[ $PORT == "all" ]]; then
		BUILD_ALL=1
	fi

	PORTS+=($PORT)
done

# get ports list
if [[ $BUILD_ALL == 1 ]]; then
	# add base ports first
	PORTS=${BASE_PORTS[@]}

	# get other ports
	for INSTALL_FILE in `ls */install`; do
		# get port
		PORT=`echo $INSTALL_FILE | awk -F '/' '{ print $1 }'`

		# check if port is in base ports
		IN_BASE_PORT=0
		for BASE_PORT in ${BASE_PORTS[@]}; do
			if [[ $BASE_PORT == $PORT ]]; then
				IN_BASE_PORT=1
			fi
		done

		# add port if needed
		if [[ $IN_BASE_PORT == 0 ]]; then
			PORTS+=($PORT)
		fi
	done

	# reset musl installation directory
	install_musl
fi

# for each port
for PORT in ${PORTS[@]}; do
	cd $BASE_DIR

	# download and extract
	check_port
	download_and_extract_port
	patch_port

	# build and install port
	cd $SRC_DIR
	configure_port
	build_port
	install_port
done
