#!/bin/csh

# setup environement
setenv TARGET		i386
setenv SYSROOT		`pwd`/sysroot
setenv CC		$SYSROOT"/bin/musl-gcc"
setenv LD		$SYSROOT"/bin/musl-gcc"
setenv INSTALL_DIR	`pwd`/root/
setenv CFLAGS		"-static"
setenv LDFLAGS		"-static"
setenv NJOBS		8

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
make install CC=$CC LD=$LD LDFLAGS=$LDFLAGS DESTDIR=$INSTALL_DIR
