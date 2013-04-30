#!/bin/bash
#
#

# make use of Studio 11 or 12 compiler
#
export CC=cc
export CXX=CC

INSTALL_BASE=/opt/bacula
SBIN_DIR=$INSTALL_BASE/sbin
MAN_DIR=$INSTALL_BASE/man
SYSCONF_DIR=$INSTALL_BASE/etc
SCRIPT_DIR=$INSTALL_BASE/etc
WORKING_DIR=/var/bacula

VERSION=2.2.5

CWD=`pwd`
# Try to guess the distribution base
DISTR_BASE=`dirname \`pwd\` | sed -e 's@/platforms$@@'`
echo "Distribution base:   $DISTR_BASE"

TMPINSTALLDIR=/tmp/`basename $DISTR_BASE`-build
echo "Temp install dir:  $TMPINSTALLDIR"
echo "Install directory: $INSTALL_BASE"

cd $DISTR_BASE

if [ "x$1" = "xbuild" ]; then
    ./configure --prefix=$INSTALL_BASE \
            --sbindir=$SBIN_DIR \
            --sysconfdir=$SYSCONF_DIR \
            --mandir=$MAN_DIR \
            --with-scriptdir=$SCRIPT_DIR \
            --with-working-dir=$WORKING_DIR \
            --with-subsys-dir=/var/lock/subsys \
            --with-pid-dir=/var/run \
            --enable-smartalloc \
            --enable-conio \
            --enable-readline \
            --enable-client-only \
            --disable-ipv6
    
    make
fi

if [ -d $TMPINSTALLDIR ]; then
    rm -rf $TMPINSTALLDIR
fi
mkdir $TMPINSTALLDIR

make DESTDIR=$TMPINSTALLDIR install

# copy additional files to install-dir
#


# change conf-files that they won't be overwritten by install
#
cd $TMPINSTALLDIR/$SYSCONF_DIR
for x in *.conf; do
    mv ${x} ${x}-dist
done


# cd back to my start-dir
#
cd $CWD

#cp prototype.master prototype
sed -e "s|__PKGSOURCE__|$CWD|" prototype.master > prototype

pkgproto $TMPINSTALLDIR/$INSTALL_BASE=. >> prototype

pkgmk -o -d /tmp -b $TMPINSTALLDIR/$INSTALL_BASE -f prototype

if [ $? = 0 ]; then
    pkgtrans /tmp bacula-$VERSION.pkg Bacula
    echo "Package has been created in /tmp"
fi 

