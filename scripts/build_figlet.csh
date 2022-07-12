#!/bin/csh

# setup environement
setenv SYSROOT		`pwd`/sysroot
setenv MUSL_CC		$SYSROOT"/bin/musl-gcc"
setenv INSTALL_DIR	`pwd`/root/

# create port directory if needed
mkdir ports >& /dev/null
cd ports

# create directory if needed
mkdir figlet >& /dev/null
cd figlet

# cleanup directories
rm -rf *

# download figlet sources
wget http://ftp.figlet.org/pub/figlet/program/unix/figlet-2.2.5.tar.gz
tar -xzvf figlet-2.2.5.tar.gz

# patch figlet
patch -p0 < ../../patches/figlet-2.2.5.patch

# build figlet
cd figlet-2.2.5
make install CC=$MUSL_CC LD=$MUSL_CC DESTDIR=$INSTALL_DIR LDFLAGS="-static"
