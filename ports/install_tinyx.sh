#!/bin/bash

if [[ `basename $PWD` != "nulix" ]]; then
	echo "This script must be run from main/root directory"
	exit 1
fi

# create tinyx build directory
cd ports
mkdir -p build/tinyx/src

# global environ
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
export PKG_CONFIG_PATH=$SYSROOT"/lib/pkgconfig:"$SYSROOT"/usr/lib/pkgconfig:"$SYSROOT"/usr/X11/lib/pkgconfig:"$SYSROOT"/usr/X11/share/pkgconfig"
export PKG_CONFIG_LIBDIR=$SYSROOT"/lib/pkgconfig:"$SYSROOT"/usr/lib/pkgconfig:"$SYSROOT"/usr/X11/lib"

# go to tinyx build directory
cd build/tinyx
BASE_DIR=`pwd`

# define packages
PACKAGES_URLS=(
	"https://www.x.org/pub/individual/util/util-macros-1.19.3.tar.bz2"
	"https://xorg.freedesktop.org/archive/individual/proto/xorgproto-2022.2.tar.xz"
	"https://www.x.org/pub/individual/lib/libXau-1.0.10.tar.xz"
	"https://www.x.org/pub/individual/lib/libXdmcp-1.1.4.tar.xz"
	"https://xorg.freedesktop.org/archive/individual/proto/xcb-proto-1.15.2.tar.xz"
	"https://www.x.org/archive/individual/lib/libpthread-stubs-0.1.tar.bz2"
	"https://xorg.freedesktop.org/archive/individual/lib/libxcb-1.15.tar.xz"
	"https://www.x.org/pub/individual/lib/xtrans-1.4.0.tar.bz2"
	"https://www.x.org/pub/individual/lib/libX11-1.8.2.tar.xz"
	"https://www.x.org/pub/individual/lib/libXext-1.3.5.tar.xz"
	"https://www.x.org/pub/individual/lib/libICE-1.0.10.tar.bz2"
	"https://www.x.org/pub/individual/lib/libSM-1.2.3.tar.bz2"
	"https://www.x.org/pub/individual/lib/libXt-1.2.1.tar.bz2"
	"https://www.x.org/pub/individual/lib/libXmu-1.1.4.tar.xz"
	"https://www.x.org/pub/individual/lib/libXpm-3.5.14.tar.xz"
	"https://www.x.org/pub/individual/lib/libXaw-1.0.14.tar.bz2"
	"https://www.x.org/pub/individual/lib/libXfixes-6.0.0.tar.bz2"
	"https://www.x.org/pub/individual/lib/libXrender-0.9.11.tar.xz"
	"https://www.x.org/pub/individual/lib/libXcursor-1.2.1.tar.xz"
	"https://www.x.org/pub/individual/lib/libfontenc-1.1.6.tar.xz"
	"https://www.x.org/pub/individual/lib/libXfont-1.5.4.tar.bz2"
	"https://www.x.org/pub/individual/lib/libXft-2.3.7.tar.xz"
	"https://www.x.org/pub/individual/lib/libXi-1.8.tar.bz2"
	"https://www.x.org/pub/individual/lib/libXtst-1.2.4.tar.xz"
	"https://www.x.org/pub/individual/lib/libxkbfile-1.1.1.tar.xz"
	"https://www.x.org/pub/individual/data/xcursor-themes-1.0.6.tar.bz2"
	"https://www.x.org/pub/individual/font/font-util-1.3.3.tar.xz"
	"https://www.x.org/pub/individual/font/encodings-1.0.6.tar.xz"
	"https://www.x.org/pub/individual/font/font-alias-1.0.4.tar.bz2"
	"https://www.x.org/pub/individual/font/font-adobe-utopia-type1-1.0.4.tar.bz2"
	"https://www.x.org/pub/individual/font/font-bh-ttf-1.0.3.tar.bz2"
	"https://www.x.org/pub/individual/font/font-bh-type1-1.0.3.tar.bz2"
	"https://www.x.org/pub/individual/font/font-ibm-type1-1.0.3.tar.bz2"
	"https://www.x.org/pub/individual/font/font-misc-ethiopic-1.0.4.tar.bz2"
	"https://www.x.org/pub/individual/font/font-xfree86-type1-1.0.4.tar.bz2"
	"https://www.x.org/pub/individual/font/font-adobe-100dpi-1.0.3.tar.bz2"
	"https://www.x.org/pub/individual/font/font-adobe-75dpi-1.0.3.tar.bz2"
	"https://www.x.org/pub/individual/font/font-jis-misc-1.0.3.tar.bz2"
	"https://www.x.org/pub/individual/font/font-daewoo-misc-1.0.3.tar.bz2"
	"https://www.x.org/pub/individual/font/font-isas-misc-1.0.3.tar.bz2"
	"https://www.x.org/pub/individual/font/font-misc-misc-1.1.2.tar.bz2"
	"https://www.x.org/pub/individual/font/font-cursor-misc-1.0.3.tar.bz2"
	"https://www.x.org/pub/individual/app/xinit-1.4.1.tar.bz2"
	"https://www.x.org/pub/individual/app/xclock-1.1.1.tar.xz"
	"https://www.x.org/pub/individual/app/twm-1.0.12.tar.xz"
)

# build all packages
for PACKAGE_URL in ${PACKAGES_URLS[@]}; do
	cd $BASE_DIR

	# download package
	cd src
	wget -c $PACKAGE_URL
	cd ..

	# get package file
	PACKAGE_FILE=`echo $PACKAGE_URL | awk -F '/' '{ print $(NF) }'`

	# extract sources
	PACKAGE_EXTENSION=`echo $PACKAGE_FILE | awk -F '.' '{ print $(NF-1)"."$NF }'`
	PACKAGE_EXTENSION1=`echo $PACKAGE_FILE | awk -F '.' '{ print "."$NF }'`
	if [[ $PACKAGE_EXTENSION == "tar.gz" ]]; then
		SRC_DIR=`tar --list -zf "src/"$PACKAGE_FILE | head -1 | awk -F '/' '{ print $1 }'`
		rm -rf $SRC_DIR
		tar -xzvf "src/"$PACKAGE_FILE
	elif [[ $PACKAGE_EXTENSION == "tar.bz2" ]]; then
		SRC_DIR=`tar --list -jf "src/"$PACKAGE_FILE | head -1 | awk -F '/' '{ print $1 }'`
		rm -rf $SRC_DIR
		tar -xjvf "src/"$PACKAGE_FILE
	elif [[ $PACKAGE_EXTENSION == "tar.xz" ]]; then
		SRC_DIR=`tar --list -f "src/"$PACKAGE_FILE | head -1 | awk -F '/' '{ print $1 }'`
		rm -rf $SRC_DIR
		tar -xvf "src/"$PACKAGE_FILE
	elif [[ $PACKAGE_EXTENSION1 == ".tgz" ]]; then
		SRC_DIR=`tar --list -zf "src/"$PACKAGE_FILE | head -1 | awk -F '/' '{ print $1 }'`
		rm -rf $SRC_DIR
		tar -xzvf "src/"$PACKAGE_FILE
	else
		echo "Error : cannot extract file $PACKAGE_FILE"
		exit 1
	fi

	# go to source directory
	cd $SRC_DIR

	# build
	./configure					\
		--host=i386-linux			\
		--enable-malloc0returnsnull		\
		--prefix=$SYSROOT"/usr/X11"
	make -j$NJOBS LDFLAGS="-Wl,-rpath-link="$SYSROOT"/usr/X11/lib"
	make install

	# go back xorg build directory
	cd ..
done

# get tinyx
rm -rf tinyx
git clone https://github.com/tinycorelinux/tinyx.git
cd tinyx

# build tinyx
autoreconf -i
./configure									\
	--host=$HOST								\
	--with-fontdir="/usr/X11/share/fonts/X11"				\
	--prefix="/usr/X11"
make -j$NJOBS
make install DESTDIR=$SYSROOT

# Xfbdev = X
ln -svf "/usr/X11/bin/Xfbdev" $SYSROOT"/usr/X11/bin/X"