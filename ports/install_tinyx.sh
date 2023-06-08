#!/bin/bash

if [[ `basename $PWD` != "nulix" ]]; then
	echo "This script must be run from main/root directory"
	exit 1
fi

# create tinyx build directory
mkdir -p ports/build/tinyx/src
cd ports/build/tinyx
BASE_DIR=`pwd`

# packages
declare -A PACKAGES
PACKAGES['util-macros']="https://www.x.org/pub/individual/util/util-macros-1.19.3.tar.bz2"
PACKAGES['xorgproto']="https://xorg.freedesktop.org/archive/individual/proto/xorgproto-2022.2.tar.xz"
PACKAGES['libXau']="https://www.x.org/pub/individual/lib/libXau-1.0.10.tar.xz"
PACKAGES['libXdmcp']="https://www.x.org/pub/individual/lib/libXdmcp-1.1.4.tar.xz"
PACKAGES['xcb-proto']="https://xorg.freedesktop.org/archive/individual/proto/xcb-proto-1.15.2.tar.xz"
PACKAGES['libpthread-stubs']="https://www.x.org/archive/individual/lib/libpthread-stubs-0.1.tar.bz2"
PACKAGES['libxcb']="https://xorg.freedesktop.org/archive/individual/lib/libxcb-1.15.tar.xz"
PACKAGES['xtrans']="https://www.x.org/pub/individual/lib/xtrans-1.4.0.tar.bz2"
PACKAGES['libX11']="https://www.x.org/pub/individual/lib/libX11-1.8.2.tar.xz"
PACKAGES['libXext']="https://www.x.org/pub/individual/lib/libXext-1.3.5.tar.xz"
PACKAGES['libICE']="https://www.x.org/pub/individual/lib/libICE-1.0.10.tar.bz2"
PACKAGES['libSM']="https://www.x.org/pub/individual/lib/libSM-1.2.3.tar.bz2"
PACKAGES['libXt']="https://www.x.org/pub/individual/lib/libXt-1.2.1.tar.bz2"
PACKAGES['libXmu']="https://www.x.org/pub/individual/lib/libXmu-1.1.4.tar.xz"
PACKAGES['libXpm']="https://www.x.org/pub/individual/lib/libXpm-3.5.14.tar.xz"
PACKAGES['libXaw']="https://www.x.org/pub/individual/lib/libXaw-1.0.14.tar.bz2"
PACKAGES['libXfixes']="https://www.x.org/pub/individual/lib/libXfixes-6.0.0.tar.bz2"
PACKAGES['libXrender']="https://www.x.org/pub/individual/lib/libXrender-0.9.11.tar.xz"
PACKAGES['libXcursor']="https://www.x.org/pub/individual/lib/libXcursor-1.2.1.tar.xz"
PACKAGES['libfontenc']="https://www.x.org/pub/individual/lib/libfontenc-1.1.6.tar.xz"
PACKAGES['libXfont']="https://www.x.org/pub/individual/lib/libXfont-1.5.4.tar.bz2"
PACKAGES['libXft']="https://www.x.org/pub/individual/lib/libXft-2.3.7.tar.xz"
PACKAGES['libXi']="https://www.x.org/pub/individual/lib/libXi-1.8.tar.bz2"
PACKAGES['libXtst']="https://www.x.org/pub/individual/lib/libXtst-1.2.4.tar.xz"
PACKAGES['libxkbfile']="https://www.x.org/pub/individual/lib/libxkbfile-1.1.1.tar.xz"
PACKAGES['xcursor-themes']="https://www.x.org/pub/individual/data/xcursor-themes-1.0.6.tar.bz2"
PACKAGES['font-util']="https://www.x.org/pub/individual/font/font-util-1.3.3.tar.xz"
PACKAGES['encodings']="https://www.x.org/pub/individual/font/encodings-1.0.6.tar.xz"
PACKAGES['font-alias']="https://www.x.org/pub/individual/font/font-alias-1.0.4.tar.bz2"
PACKAGES['font-adobe-utopia-type1']="https://www.x.org/pub/individual/font/font-adobe-utopia-type1-1.0.4.tar.bz2"
PACKAGES['font-bh-ttf']="https://www.x.org/pub/individual/font/font-bh-ttf-1.0.3.tar.bz2"
PACKAGES['font-bh-type1']="https://www.x.org/pub/individual/font/font-bh-type1-1.0.3.tar.bz2"
PACKAGES['font-ibm-type1']="https://www.x.org/pub/individual/font/font-ibm-type1-1.0.3.tar.bz2"
PACKAGES['font-misc-ethiopic']="https://www.x.org/pub/individual/font/font-misc-ethiopic-1.0.4.tar.bz2"
PACKAGES['font-xfree86-type1']="https://www.x.org/pub/individual/font/font-xfree86-type1-1.0.4.tar.bz2"
PACKAGES['font-adobe-100dpi']="https://www.x.org/pub/individual/font/font-adobe-100dpi-1.0.3.tar.bz2"
PACKAGES['font-adobe-75dpi']="https://www.x.org/pub/individual/font/font-adobe-75dpi-1.0.3.tar.bz2"
PACKAGES['font-jis-misc']="https://www.x.org/pub/individual/font/font-jis-misc-1.0.3.tar.bz2"
PACKAGES['font-daewoo-misc']="https://www.x.org/pub/individual/font/font-daewoo-misc-1.0.3.tar.bz2"
PACKAGES['font-isas-misc']="https://www.x.org/pub/individual/font/font-isas-misc-1.0.3.tar.bz2"
PACKAGES['font-misc-misc']="https://www.x.org/pub/individual/font/font-misc-misc-1.1.2.tar.bz2"
PACKAGES['font-cursor-misc']="https://www.x.org/pub/individual/font/font-cursor-misc-1.0.3.tar.bz2"

#################################
## download and extract a port ##
#################################
function download_port() {
	cd $BASE_DIR

	# download package
	cd src
	PACKAGE_URL=${PACKAGES[$1]}
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
}

######################
## fix libtool path ##
######################
function fix_libtool() {
	SYSROOT_ESC=$(echo $SYSROOT | sed 's/\//\\\//g')
	for FILE in $(find . -name "*.pc" -o -name "*.la" -o -name "*.lai"); do
		sed -i "s/=\/usr\/X11/=$SYSROOT_ESC\/usr\/X11/g" $FILE
		sed -i "s/ \/usr\/X11/ $SYSROOT_ESC\/usr\/X11/g" $FILE
		sed -i "s/='\/usr\/X11/='$SYSROOT_ESC\/usr\/X11/g" $FILE
	done
}

#######################
## build util-macros ##
#######################
function build_util_macros() {
	download_port "util-macros"
	./configure --host=i386-linux --prefix=$SYSROOT"/usr/X11"
	make -j$NJOBS
	fix_libtool
	make install
}

#################
## build xport ##
#################
function build_xport() {
	download_port $1
	./configure --host=i386-linux --enable-malloc0returnsnull --prefix="/usr/X11"
	make -j$NJOBS
	fix_libtool
	make install DESTDIR=$SYSROOT
}

#####################
## build xresource ##
#####################
function build_xresource() {
	download_port $1
	./configure --host=i386-linux --enable-malloc0returnsnull --prefix="/usr/X11"
	make -j$NJOBS
	fix_libtool
	make install
}

#################
## build tinyx ##
#################
function build_tinyx() {
	# download tinyx
	cd $BASE_DIR
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
}

# build dependencies and tinyx
build_util_macros
build_xport "xorgproto"
build_xport "libXau"
build_xport "libXdmcp"
build_xport "xcb-proto"
build_xport "libpthread-stubs"
build_xport "libxcb"
build_xport "xtrans"
build_xport "libX11"
build_xport "libXext"
build_xport "libICE"
build_xport "libSM"
build_xport "libXt"
build_xport "libXmu"
build_xport "libXpm"
build_xport "libXaw"
build_xport "libXfixes"
build_xport "libXrender"
build_xport "libXcursor"
build_xport "libfontenc"
build_xport "libXfont"
build_xport "libXft"
build_xport "libXi"
build_xport "libXtst"
build_xport "libxkbfile"
build_xport "font-util"
build_xresource "xcursor-themes"
build_xresource "encodings"
build_xresource "font-alias"
build_xresource "font-adobe-utopia-type1"
build_xresource "font-bh-ttf"
build_xresource "font-bh-type1"
build_xresource "font-ibm-type1"
build_xresource "font-misc-ethiopic"
build_xresource "font-xfree86-type1"
build_xresource "font-adobe-100dpi"
build_xresource "font-adobe-75dpi"
build_xresource "font-jis-misc"
build_xresource "font-daewoo-misc"
build_xresource "font-isas-misc"
build_xresource "font-misc-misc"
build_xresource "font-cursor-misc"
build_tinyx
