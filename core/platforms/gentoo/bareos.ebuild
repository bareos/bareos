# Copyright 2004 D. Scott Barninger
# Distributed under the terms of the GNU General Public License v2
#
# Modified from bareos-1.34.5.ebuild for 1.36.0 release
# 24 Oct 2004 D. Scott Barninger <barninger at fairfieldcomputers dot com>
#
# added cdrom rescue for 1.36.1
# init script now comes from source package not ${FILES} dir
# 26 Nov 2004 D. Scott Barninger <barninger at fairfieldcomputers dot com>
#
# fix symlink creation in rescue package in post script
# remove mask on x86 keyword
# fix post script so it doesn't talk about server config for client-only build
# bug #181 - unable to reproduce on 2.4 kernel system so add FEATURES="-sandbox"
# 04 Dec 2004 D. Scott Barninger <barninger at fairfieldcomputers dot com>
#
# more  on bug #181 - another user has reported a sandbox violation trying to
# write to /dev/sg0 - still can't reproduce this behavior
# add an 'addpredict /dev/sg0'
# 08 Dec 2004 D. Scott Barninger <barninger at fairfieldcomputers dot com>
#
# resolve bug #181 - problem is caused by configure calling cdrecord to scan
# the scsi bus. patch configure to remove this. add logrotate script.
# 06 Feb 2005 D. Scott Barninger <barninger at fairfieldcomputers dot com>
#
# fix documentation bug
# 07 Feb 2005 D. Scott Barninger <barninger at fairfieldcomputers dot com>
#
# new USE keywords bareos-clientonly bareos-split
# add new logwatch scripts
# 06 Mar 2005 D. Scott Barninger <barninger at fairfieldcomputers dot com>
#
# 1.36.3 doc changes
# 17 Apr 2005 D. Scott Barninger <barninger at fairfieldcomputers dot com>

DESCRIPTION="featureful client/server network backup suite"
HOMEPAGE="http://www.bareos.org/"
SRC_URI="mirror://sourceforge/bareos/${P}.tar.gz"

LICENSE="GPL-2"
SLOT="0"
KEYWORDS="x86 ~ppc ~sparc ~amd64"
IUSE="readline tcpd gnome mysql sqlite X static postgres wxwindows bareos-clientonly bareos-split"

inherit eutils

# there is a local sqlite use flag. use it -OR- mysql, not both.
# mysql is the recommended choice ...
# may need sys-libs/libtermcap-compat but try without first
DEPEND=">=sys-libs/zlib-1.1.4
	readline? ( >=sys-libs/readline-4.1 )
	tcpd? ( >=sys-apps/tcp-wrappers-7.6 )
	gnome? ( gnome-base/libgnome )
	gnome? ( app-admin/gnomesu )
	!bareos-clientonly? (
		sqlite? ( =dev-db/sqlite-2* )
		mysql? ( >=dev-db/mysql-3.23 )
		postgres? ( >=dev-db/postgresql-7.4.0 )
		sys-apps/mtx
	)
	X? ( virtual/x11 )
	wxwindows? ( >=x11-libs/wxGTK-2.4.2 )
	virtual/mta
	dev-libs/gmp
	app-text/tetex
	dev-tex/latex2html"
RDEPEND="${DEPEND}
	!bareos-clientonly? (
		sys-apps/mtx
		app-arch/mt-st
	)"

src_compile() {

	# this resolves bug #181
	epatch ${FILESDIR}/1.36.2-cdrecord-configure.patch

	local myconf=""

	myconf="
		`use_enable readline`
		`use_enable gnome`
		`use_enable tcpd tcp-wrappers`
		`use_enable X x`"

	# define this to skip building the other daemons ...
	if use bareos-clientonly
	then
		myconf="${myconf} --enable-client-only"
	fi

	# select database support
	if ! use bareos-clientonly
	then
		# mysql is the recomended choice ...
		if use mysql
		then
			myconf="${myconf} --with-mysql=/usr"
		elif use postgres
		then
			myconf="${myconf} --with-postgresql=/usr"
		elif use sqlite
		then
			myconf="${myconf} --with-sqlite=/usr"
		elif  use sqlite && use mysql
		then
			myconf="${myconf/--with-sqlite/}"
		fi
	fi

	if use wxwindows
	then
	   myconf="${myconf} --enable-wx-console"
	fi

	if use readline
	then
	   myconf="${myconf} --enable-readline"
	fi

	if use gnome
	then
	myconf="${myconf} --enable-traymonitor"
	fi

	./configure \
		--enable-smartalloc \
		--prefix=/usr \
		--mandir=/usr/share/man \
		--with-pid-dir=/var/run \
		--sysconfdir=/etc \
		--with-confdir=/etc/bareos \
		--infodir=/usr/share/info \
		--with-subsys-dir=/var/lock/subsys \
		--with-working-dir=/var/bareos \
		--with-scriptdir=/etc/bareos \
		--with-dir-user=root \
		--with-dir-group=bareos \
		--with-sd-user=root \
		--with-sd-group=bareos \
		--with-fd-user=root \
		--with-fd-group=bareos \
		--host=${CHOST} ${myconf} || die "bad ./configure"

	emake || die "compile problem"

	# for the rescue package regardless of use static
	cd ${S}/src/filed
	make static-bareos-fd
	cd ${S}

	# make the docs
	cd ${S}/doc/latex
	make
	cd ${S}

	if use static
	then
		cd ${S}/src/console
		make static-console
		cd ${S}/src/dird
		make static-bareos-dir
		if use gnome
		then
		  cd ${S}/src/gnome-console
		  make static-gnome-console
		fi
		if use wxwindows
		then
		  cd ${S}/src/wx-console
		  make static-wx-console
		fi
		cd ${S}/src/stored
		make static-bareos-sd
	fi
}

src_install() {
	make DESTDIR=${D} install || die

	if use static
	then
		cd ${S}/src/filed
		cp static-bareos-fd ${D}/usr/sbin/bareos-fd
		cd ${S}/src/console
		cp static-console ${D}/usr/sbin/console
		cd ${S}/src/dird
		cp static-bareos-dir ${D}/usr/sbin/bareos-dir
		if use gnome
		then
			cd ${S}/src/gnome-console
			cp static-gnome-console ${D}/usr/sbin/gnome-console
		fi
		if use wxwindows
		then
			cd ${S}/src/wx-console
			cp static-wx-console ${D}/usr/sbin/wx-console
		fi
		cd ${S}/src/stored
		cp static-bareos-sd ${D}/usr/sbin/bareos-sd
	fi

	# the menu stuff
	if use gnome
	then
	mkdir -p ${D}/usr/share/pixmaps
	mkdir -p ${D}/usr/share/applications
	cp ${S}/scripts/bareos.png ${D}/usr/share/pixmaps/bareos.png
	cp ${S}/scripts/bareos.desktop.gnome2.xsu ${D}/usr/share/applications/bareos.desktop
	cp ${S}/src/tray-monitor/generic.xpm ${D}/usr/share/pixmaps/bareos-tray-monitor.xpm
	cp ${S}/scripts/bareos-tray-monitor.desktop \
		${D}/usr/share/applications/bareos-tray-monitor.desktop
	chmod 755 ${D}/usr/sbin/bareos-tray-monitor
	chmod 644 ${D}/etc/bareos/tray-monitor.conf
	fi

	if ! use bareos-clientonly
	then
		# the database update scripts
		mkdir -p ${D}/etc/bareos/updatedb
		cp ${S}/updatedb/* ${D}/etc/bareos/updatedb/
		chmod 754 ${D}/etc/bareos/updatedb/*

		# the logrotate configuration
		mkdir -p ${D}/etc/logrotate.d
		cp ${S}/scripts/logrotate ${D}/etc/logrotate.d/bareos
		chmod 644 ${D}/etc/logrotate.d/bareos

		# the logwatch scripts
		mkdir -p ${D}/etc/log.d/conf/logfiles
		mkdir -p ${D}/etc/log.d/conf/services
		mkdir -p ${D}/etc/log.d/scripts/services
		cp ${S}/scripts/logwatch/bareos ${D}/etc/log.d/scripts/services/bareos
		cp ${S}/scripts/logwatch/logfile.bareos.conf ${D}/etc/log.d/conf/logfiles/bareos.conf
		cp ${S}/scripts/logwatch/services.bareos.conf ${D}/etc/log.d/conf/services/bareos.conf
		chmod 755 ${D}/etc/log.d/scripts/services/bareos
		chmod 644 ${D}/etc/log.d/conf/logfiles/bareos.conf
		chmod 644 ${D}/etc/log.d/conf/services/bareos.conf

	fi

	# the cdrom rescue package
	mkdir -p ${D}/etc/bareos/rescue/cdrom
	cp -R ${S}/rescue/linux/cdrom/* ${D}/etc/bareos/rescue/cdrom/
	mkdir ${D}/etc/bareos/rescue/cdrom/bin
	cp ${S}/src/filed/static-bareos-fd ${D}/etc/bareos/rescue/cdrom/bin/bareos-fd
	chmod 754 ${D}/etc/bareos/rescue/cdrom/bin/bareos-fd

	# documentation
	for a in ${S}/{ChangeLog,README,ReleaseNotes,kernstodo,LICENSE,doc/latex/bareos.pdf}
	do
		dodoc $a
	done

	dohtml -r ${S}/doc/latex/bareos

	# clean up permissions left broken by install
	chmod o-r ${D}/etc/bareos/query.sql

	# remove the working dir so we can add it postinst with group
	rmdir ${D}/var/bareos

	# init scripts
	exeinto /etc/init.d
	if use bareos-clientonly
	then
		newexe ${S}/platforms/gentoo/bareos-fd bareos-fd
	else
		if use bareos-split
		then
			newexe ${S}/platforms/gentoo/bareos-fd bareos-fd
			newexe ${S}/platforms/gentoo/bareos-sd bareos-sd
			newexe ${S}/platforms/gentoo/bareos-dir bareos-dir
		else
			newexe ${S}/platforms/gentoo/bareos-init bareos
		fi
	fi
}

pkg_postinst() {
	# create the daemon group
	HAVE_BAREOS=`cat /etc/group | grep bareos 2>/dev/null`
	if [ -z $HAVE_BAREOS ]; then
	enewgroup bareos
	einfo
	einfo "The group bareos has been created. Any users you add to"
	einfo "this group have access to files created by the daemons."
	fi

	# the working directory
	install -m0750 -o root -g bareos -d ${ROOT}/var/bareos

	# link installed bareos-fd.conf into rescue directory
	#ln -s /etc/bareos/bareos-fd.conf /etc/bareos/rescue/cdrom/bareos-fd.conf
	# no longer necessary after 1.36.2

	einfo
	einfo "The CDRom rescue disk package has been installed into the"
	einfo "/etc/bareos/rescue/cdrom/ directory. Please examine the manual"
	einfo "for information on creating a rescue CD. CDR device detection"
	einfo "during build has been disabled to prevent sandbox violations."
	einfo "You need to examine /etc/bareos/rescue/cdrom/Makefile and adjust"
	einfo "the device information for your CD recorder."
	einfo

	if ! use bareos-clientonly; then
	einfo
	einfo "Please note either/or nature of database USE flags for"
	einfo "Bareos.  If mysql is set, it will be used, else postgres"
	einfo "else finally SQLite.  If you wish to have multiple DBs on"
	einfo "one system, you may wish to unset auxillary DBs for this"
	einfo "build."
	einfo

	if use mysql
	then
	# test for an existing database
	# note: this ASSUMES no password has been set for bareos database
	DB_VER=`mysql 2>/dev/null bareos -e 'select * from Version;'|tail -n 1`
		if [ -z "$DB_VER" ]; then
		einfo "This appears to be a new install and you plan to use mysql"
		einfo "for your catalog database. You should now create it by doing"
		einfo "these commands:"
		einfo " sh /etc/bareos/grant_mysql_privileges"
		einfo " sh /etc/bareos/create_mysql_database"
		einfo " sh /etc/bareos/make_mysql_tables"
		elif [ "$DB_VER" -lt "8" ]; then
		elinfo "This release requires an upgrade to your bareos database"
		einfo "as the database format has changed.  Please read the"
		einfo "manual chapter for how to upgrade your database!!!"
		einfo
		einfo "Backup your database with the command:"
		einfo " mysqldump -f --opt bareos | bzip2 > /var/bareos/bareos_backup.sql.bz"
		einfo
		einfo "Then update your database using the scripts found in"
		einfo "/etc/bareos/updatedb/ from your current version $DB_VER to"
		einfo "version 8. Note that scripts must be run in order from your"
		einfo "version to the current version."
		fi
	fi

	if use postgres
	then
	# test for an existing database
	# note: this ASSUMES no password has been set for bareos database
	DB_VER=`echo 'select * from Version;' | psql bareos 2>/dev/null | tail -3 | head -1`
		if [ -z "$DB_VER" ]; then
		einfo "This appears to be a new install and you plan to use postgresql"
		einfo "for your catalog database. You should now create it by doing"
		einfo "these commands:"
		einfo " sh /etc/bareos/create_postgresql_database"
		einfo " sh /etc/bareos/make_postgresql_tables"
		einfo " sh /etc/bareos/grant_postgresql_privileges"
		elif [ "$DB_VER" -lt "8" ]; then
		elinfo "This release requires an upgrade to your bareos database"
		einfo "as the database format has changed.  Please read the"
		einfo "manual chapter for how to upgrade your database!!!"
		einfo
		einfo "Backup your database with the command:"
		einfo " pg_dump bareos | bzip2 > /var/bareos/bareos_backup.sql.bz2"
		einfo
		einfo "Then update your database using the scripts found in"
		einfo "/etc/bareos/updatedb/ from your current version $DB_VER to"
		einfo "version 8. Note that scripts must be run in order from your"
		einfo "version to the current version."
		fi
	fi

	if use sqlite
	then
	# test for an existing database
	# note: this ASSUMES no password has been set for bareos database
	DB_VER=`echo "select * from Version;" | sqlite 2>/dev/null /var/bareos/bareos.db | tail -n 1`
		if [ -z "$DB_VER" ]; then
		einfo "This appears to be a new install and you plan to use sqlite"
		einfo "for your catalog database. You should now create it by doing"
		einfo "these commands:"
		einfo " sh /etc/bareos/grant_sqlite_privileges"
		einfo " sh /etc/bareos/create_sqlite_database"
		einfo " sh /etc/bareos/make_sqlite_tables"
		elif [ "$DB_VER" -lt "8" ]; then
		elinfo "This release requires an upgrade to your bareos database"
		einfo "as the database format has changed.  Please read the"
		einfo "manual chapter for how to upgrade your database!!!"
		einfo
		einfo "Backup your database with the command:"
		einfo " echo .dump | sqlite /var/bareos/bareos.db | bzip2 > \\"
		einfo "   /var/bareos/bareos_backup.sql.bz2"
		einfo
		einfo "Then update your database using the scripts found in"
		einfo "/etc/bareos/updatedb/ from your current version $DB_VER to"
		einfo "version 8. Note that scripts must be run in order from your"
		einfo "version to the current version."
		fi
	fi
	fi

	einfo
	einfo "Review your configuration files in /etc/bareos and"
	einfo "start the daemons:"
	if use bareos-clientonly; then
		einfo " /etc/init.d/bareos-fd start"
	else
		if use bareos-split; then
		einfo " /etc/init.d/bareos-sd start"
		einfo " /etc/init.d/bareos-dir start"
		einfo " /etc/init.d/bareos-fd start"
		einfo " or /etc/bareos/bareos will start all three."
		else
		einfo " /etc/init.d/bareos start"
		fi
	fi
	einfo
	einfo "You may also wish to:"
	if use bareos-clientonly; then
		einfo " rc-update add bareos-fd default"
	else
		if use bareos-split; then
			einfo " rc-update add bareos-sd default"
			einfo " rc-update add bareos-dir default"
			einfo " rc-update add bareos-fd default"
		else
			einfo " rc-update add bareos default"
		fi
	fi
	einfo
}
