#!/bin/bash

# shell script to build bacula rpm release
# copy this script into a working directory with the src rpm to build and execute
# 19 Aug 2006 D. Scott Barninger

# Copyright (C) 2006 Free Software Foundation Europe e.V.
# licensed under GPL-v2

# signing rpms
# Make sure you have a .rpmmacros file in your home directory containing the following:
#
# %_signature gpg
# %_gpg_name Your Name <your-email@site.org>
#
# the %_gpg_name information must match your key


# usage: ./build_rpm.sh

###########################################################################################
# script configuration section

VERSION=5.0.2
RELEASE=1

# build platform for spec
# set to one of rh7,rh8,rh9,fc1,fc3,fc4,fc5,fc6,fc7,fc8,fc9,wb3,rhel3,rhel4,rhel5,centos3,centos4,centos5,sl3, sl4,sl5,su9,su10,su102,su103,su110,su111,su112,mdk,mdv
PLATFORM=su111

# platform designator for file names
# for RedHat/Fedora set to one of rh7,rh8,rh9,fc1,fc3,fc4,fc5,fc6,fc7,fc8,fc9 OR
# for RHEL3/clones wb3, rhel3, sl3 & centos3 set to el3 OR
# for RHEL4/clones rhel4, sl4 & centos4 set to el4 OR
# for RHEL5/clones rhel5, sl5 & centos5 set to el5 OR
# for SuSE set to su90, su91, su92, su100 or su101 or su102 or su103 or su110 or su111 or su112 OR
# for Mandrake set to 101mdk or 20060mdk
FILENAME=su111

# MySQL version
# set to empty (for MySQL 3), 4 or 5
MYSQL=

# enter your name and email address here
PACKAGER="D. Scott Barninger <barninger@fairfieldcomputers.com>"

# enter the full path to your RPMS output directory
RPMDIR=/usr/src/packages/RPMS/i586
RPMDIR2=/usr/src/packages/RPMS/noarch

# enter the full path to your rpm BUILD directory
RPMBUILD=/usr/src/packages/BUILD

# enter your arch string here (i386, i586, i686, x86_64)
ARCH=i586

# if the src rpm is not in the current working directory enter the directory location
# with trailing slash where it is found.
SRPMDIR=

# to build the mtx package set to 1, else 0
BUILDMTX=0

# to build the bat package set to 1, else 0
BUILDBAT=1

# set to 1 to sign packages, 0 not to sign if you want to sign on another machine.
SIGN=0

# to save the bacula-updatedb package set to 1, else 0
# only one updatedb package is required per release so normally this should be 0
# for all contrib packagers
SAVEUPDATEDB=0

# to override your language shell variable uncomment and edit this
# export LANG=en_US.UTF-8

# this is now in the spec file but when building bat on older versions uncomment
#export QTDIR=$(pkg-config --variable=prefix QtCore)
#export QTINC=$(pkg-config --variable=includedir QtCore)
#export QTLIB=$(pkg-config --variable=libdir QtCore)
#export PATH=${QTDIR}/bin/:${PATH}

# Make no changes below this point without consensus

############################################################################################

SRPM=${SRPMDIR}bacula-$VERSION-$RELEASE.src.rpm
SRPM2=${SRPMDIR}bacula-bat-$VERSION-$RELEASE.src.rpm
SRPM3=${SRPMDIR}bacula-docs-$VERSION-$RELEASE.src.rpm
SRPM4=${SRPMDIR}bacula-mtx-$VERSION-$RELEASE.src.rpm

echo Building MySQL packages for "$PLATFORM"...
sleep 2
rpmbuild --rebuild --define "build_${PLATFORM} 1" \
--define "build_mysql${MYSQL} 1" \
--define "build_python 1" \
--define "contrib_packager ${PACKAGER}" ${SRPM}
rm -rf ${RPMBUILD}/*

echo Building PostgreSQL packages for "$PLATFORM"...
sleep 2
rpmbuild --rebuild --define "build_${PLATFORM} 1" \
--define "build_postgresql 1" \
--define "contrib_packager ${PACKAGER}" \
--define "build_python 1" ${SRPM}
rm -rf ${RPMBUILD}/*

echo Building SQLite packages for "$PLATFORM"...
sleep 2
rpmbuild --rebuild --define "build_${PLATFORM} 1" \
--define "build_sqlite 1" \
--define "contrib_packager ${PACKAGER}" \
--define "build_python 1" ${SRPM}
rm -rf ${RPMBUILD}/*

if [ "$BUILDBAT" = "1" ]; then
	echo Building Bat package for "$PLATFORM"...
	sleep 2
	rpmbuild --rebuild ${SRPM2}
	rm -rf ${RPMBUILD}/*
fi

echo Building Docs package for "$PLATFORM"...
sleep 2
rpmbuild --rebuild ${SRPM3}
rm -rf ${RPMBUILD}/*

if [ "$BUILDMTX" = "1" ]; then
	echo Building mtx package for "$PLATFORM"...
	sleep 2
	rpmbuild --rebuild ${SRPM4}
	rm -rf ${RPMBUILD}/*
fi

# delete the updatedb package and any debuginfo packages built
rm -f ${RPMDIR}/bacula*debug*
if [ "$SAVEUPDATEDB" = "1" ]; then
        mv -f ${RPMDIR}/bacula-updatedb* ./;
else
        rm -f ${RPMDIR}/bacula-updatedb*;
fi

# copy files to cwd and rename files to final upload names

mv -f ${RPMDIR}/bacula-mysql-${VERSION}-${RELEASE}.${ARCH}.rpm \
./bacula-mysql-${VERSION}-${RELEASE}.${FILENAME}.${ARCH}.rpm

mv -f ${RPMDIR}/bacula-postgresql-${VERSION}-${RELEASE}.${ARCH}.rpm \
./bacula-postgresql-${VERSION}-${RELEASE}.${FILENAME}.${ARCH}.rpm

mv -f ${RPMDIR}/bacula-sqlite-${VERSION}-${RELEASE}.${ARCH}.rpm \
./bacula-sqlite-${VERSION}-${RELEASE}.${FILENAME}.${ARCH}.rpm

if [ "$BUILDMTX" = "1" ]; then
	mv -f ${RPMDIR}/bacula-mtx-${VERSION}-${RELEASE}.${ARCH}.rpm \
	./bacula-mtx-${VERSION}-${RELEASE}.${FILENAME}.${ARCH}.rpm
fi

mv -f ${RPMDIR}/bacula-client-${VERSION}-${RELEASE}.${ARCH}.rpm \
./bacula-client-${VERSION}-${RELEASE}.${FILENAME}.${ARCH}.rpm

mv -f ${RPMDIR}/bacula-libs-${VERSION}-${RELEASE}.${ARCH}.rpm \
./bacula-libs-${VERSION}-${RELEASE}.${FILENAME}.${ARCH}.rpm

if [ "$BUILDBAT" = "1" ]; then
	mv -f ${RPMDIR}/bacula-bat-${VERSION}-${RELEASE}.${ARCH}.rpm \
	./bacula-bat-${VERSION}-${RELEASE}.${FILENAME}.${ARCH}.rpm
fi

mv -f ${RPMDIR2}/bacula-docs-${VERSION}-${RELEASE}.noarch.rpm .

# now sign the packages
if [ "$SIGN" = "1" ]; then
        echo Ready to sign packages...;
        sleep 2;
        rpm --addsign ./*.rpm;
fi

echo
echo Finished.
echo
ls

# changelog
# 16 Jul 2006 initial release
# 05 Aug 2006 add python support
# 06 Aug 2006 add remote source directory, add switch for signing, refine file names
# 19 Aug 2006 add $LANG override to config section per request Felix Schwartz
# 27 Jan 2007 add fc6 target
# 29 Apr 2007 add sl3 & sl4 target and bat package
# 06 May 2007 add fc7 target
# 15 Sep 2007 add rhel5 and clones
# 10 Nov 2007 add su103
# 12 Jan 2008 add fc8
# 23 May 2008 add fc9
# 28 Jun 2008 add su110
# 08 Nov 2008 add use of pkgconfig to obtain QT4 paths
# 31 Dec 2008 add su111
# 05 Apr 2009 deprecate gconsole and wxconsole, bat built by default
# 30 Jan 2010 adjust for mtx, bat and docs in separate srpm
# 02 May 2010 add bacula-libs package
