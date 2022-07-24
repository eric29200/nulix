#!/bin/csh

# go to ports directory
cd `dirname $0`
set BASE_DIR = `pwd`

# setup env
setenv TARGET			i386
setenv SYSROOT			`realpath ../sysroot`
setenv CC			$SYSROOT"/bin/musl-gcc"
setenv LD			$SYSROOT"/bin/musl-gcc"
setenv PKG_CONFIG		$SYSROOT"/bin/pkgconf"
setenv PKG_CONFIG_PATH		$SYSROOT"/lib/pkgconfig"
setenv PKG_CONFIG_LIBDIR	$SYSROOT"/lib/pkgconfig"
setenv ROOT_DIR			`realpath ../root`
setenv CFLAGS			"-static"
setenv LDFLAGS			"-static"
setenv NJOBS			`nproc`

# check number of arguments
if ($#argv == 0) then
	echo "Usage : ports/install.sh port_name (or all)"
	exit 1
endif

# get ports list
set PORTS = ""
set BUILD_ALL = 0
foreach PORT ($argv)
	if ($PORT == "all") then
		set BUILD_ALL = 1
	endif

	set PORTS = "$PORTS $PORT"
end

# get ports list
if ($BUILD_ALL == 1) then
	set PORTS = " "

	foreach INSTALL_FILE (`ls */install`)
		set PORT = `echo $INSTALL_FILE | awk -F '/' '{ print $1 }'`
		if ($PORT != "musl" && $PORT != "pkgconf" && $PORT != "linux-headers") then
			set PORTS = "$PORTS $PORT"
		endif
	end
endif

# for each port
foreach PORT ($PORTS)
	cd $BASE_DIR

	# do not allow to build musl or pkgconf
	if ($PORT == "musl" || $PORT == "pkgconf" || $PORT == "linux-headers") then
		echo "Error : to build musl/pkg config use 'make musl'"
		exit 1
	endif

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

	# configure
	cd $SRC_DIR
	if ( -e "../../../$PORT/config" ) then
		cp ../../../$PORT/config .config
	else if ( -e "./configure") then
		./configure --host=$TARGET --prefix=$INSTALL_DIR $CONFIG_OPTIONS
	endif

	# build
	make -j$NJOBS CC=$CC LD=$LD CFLAGS=$CFLAGS LDFLAGS=$LDFLAGS
	if ( -e "./configure") then
		make install CC=$CC LD=$LD
	else
		make install CC=$CC LD=$LD INSTALL_TOP=$INSTALL_DIR DESTDIR=$INSTALL_DIR
	endif
end
