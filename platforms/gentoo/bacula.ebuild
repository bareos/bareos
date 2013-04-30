# Copyright 2004 D. Scott Barninger
# Distributed under the terms of the GNU General Public License v2
#
# Modified from bacula-1.34.5.ebuild for 1.36.0 release
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
# new USE keywords bacula-clientonly bacula-split
# add new logwatch scripts
# 06 Mar 2005 D. Scott Barninger <barninger at fairfieldcomputers dot com>
#
# 1.36.3 doc changes
# 17 Apr 2005 D. Scott Barninger <barninger at fairfieldcomputers dot com>

DESCRIPTION="featureful client/server network backup suite"
HOMEPAGE="http://www.bacula.org/"
SRC_URI="mirror://sourceforge/bacula/${P}.tar.gz"

LICENSE="GPL-2"
SLOT="0"
KEYWORDS="x86 ~ppc ~sparc ~amd64"
IUSE="readline tcpd gnome mysql sqlite X static postgres wxwindows bacula-clientonly bacula-split"

inherit eutils

# there is a local sqlite use flag. use it -OR- mysql, not both.
# mysql is the recommended choice ...
# may need sys-libs/libtermcap-compat but try without first
DEPEND=">=sys-libs/zlib-1.1.4
	readline? ( >=sys-libs/readline-4.1 )
	tcpd? ( >=sys-apps/tcp-wrappers-7.6 )
	gnome? ( gnome-base/libgnome )
	gnome? ( app-admin/gnomesu )
	!bacula-clientonly? (
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
	!bacula-clientonly? (
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
	if use bacula-clientonly
	then
		myconf="${myconf} --enable-client-only"
	fi

	# select database support
	if ! use bacula-clientonly
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
	myconf="${myconf} --enable-tray-monitor"
	fi

	./configure \
		--enable-smartalloc \
		--prefix=/usr \
		--mandir=/usr/share/man \
		--with-pid-dir=/var/run \
		--sysconfdir=/etc/bacula \
		--infodir=/usr/share/info \
		--with-subsys-dir=/var/lock/subsys \
		--with-working-dir=/var/bacula \
		--with-scriptdir=/etc/bacula \
		--with-dir-user=root \
		--with-dir-group=bacula \
		--with-sd-user=root \
		--with-sd-group=bacula \
		--with-fd-user=root \
		--with-fd-group=bacula \
		--host=${CHOST} ${myconf} || die "bad ./configure"

	emake || die "compile problem"

	# for the rescue package regardless of use static
	cd ${S}/src/filed
	make static-bacula-fd
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
		make static-bacula-dir
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
		make static-bacula-sd
	fi
}

src_install() {
	make DESTDIR=${D} install || die

	if use static
	then
		cd ${S}/src/filed
		cp static-bacula-fd ${D}/usr/sbin/bacula-fd
		cd ${S}/src/console
		cp static-console ${D}/usr/sbin/console
		cd ${S}/src/dird
		cp static-bacula-dir ${D}/usr/sbin/bacula-dir
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
		cp static-bacula-sd ${D}/usr/sbin/bacula-sd
	fi

	# the menu stuff
	if use gnome
	then
	mkdir -p ${D}/usr/share/pixmaps
	mkdir -p ${D}/usr/share/applications
	cp ${S}/scripts/bacula.png ${D}/usr/share/pixmaps/bacula.png
	cp ${S}/scripts/bacula.desktop.gnome2.xsu ${D}/usr/share/applications/bacula.desktop
	cp ${S}/src/tray-monitor/generic.xpm ${D}/usr/share/pixmaps/bacula-tray-monitor.xpm
	cp ${S}/scripts/bacula-tray-monitor.desktop \
		${D}/usr/share/applications/bacula-tray-monitor.desktop
	chmod 755 ${D}/usr/sbin/bacula-tray-monitor
	chmod 644 ${D}/etc/bacula/tray-monitor.conf
	fi

	if ! use bacula-clientonly
	then
		# the database update scripts
		mkdir -p ${D}/etc/bacula/updatedb
		cp ${S}/updatedb/* ${D}/etc/bacula/updatedb/
		chmod 754 ${D}/etc/bacula/updatedb/*

		# the logrotate configuration
		mkdir -p ${D}/etc/logrotate.d
		cp ${S}/scripts/logrotate ${D}/etc/logrotate.d/bacula
		chmod 644 ${D}/etc/logrotate.d/bacula

		# the logwatch scripts
		mkdir -p ${D}/etc/log.d/conf/logfiles
		mkdir -p ${D}/etc/log.d/conf/services
		mkdir -p ${D}/etc/log.d/scripts/services
		cp ${S}/scripts/logwatch/bacula ${D}/etc/log.d/scripts/services/bacula
		cp ${S}/scripts/logwatch/logfile.bacula.conf ${D}/etc/log.d/conf/logfiles/bacula.conf
		cp ${S}/scripts/logwatch/services.bacula.conf ${D}/etc/log.d/conf/services/bacula.conf
		chmod 755 ${D}/etc/log.d/scripts/services/bacula
		chmod 644 ${D}/etc/log.d/conf/logfiles/bacula.conf
		chmod 644 ${D}/etc/log.d/conf/services/bacula.conf

	fi

	# the cdrom rescue package
	mkdir -p ${D}/etc/bacula/rescue/cdrom
	cp -R ${S}/rescue/linux/cdrom/* ${D}/etc/bacula/rescue/cdrom/
	mkdir ${D}/etc/bacula/rescue/cdrom/bin
	cp ${S}/src/filed/static-bacula-fd ${D}/etc/bacula/rescue/cdrom/bin/bacula-fd
	chmod 754 ${D}/etc/bacula/rescue/cdrom/bin/bacula-fd

	# documentation
	for a in ${S}/{ChangeLog,README,ReleaseNotes,kernstodo,LICENSE,doc/latex/bacula.pdf}
	do
		dodoc $a
	done

	dohtml -r ${S}/doc/latex/bacula
	
	# clean up permissions left broken by install
	chmod o-r ${D}/etc/bacula/query.sql

	# remove the working dir so we can add it postinst with group
	rmdir ${D}/var/bacula

	# init scripts
	exeinto /etc/init.d
	if use bacula-clientonly
	then
		newexe ${S}/platforms/gentoo/bacula-fd bacula-fd
	else
		if use bacula-split
		then
			newexe ${S}/platforms/gentoo/bacula-fd bacula-fd
			newexe ${S}/platforms/gentoo/bacula-sd bacula-sd
			newexe ${S}/platforms/gentoo/bacula-dir bacula-dir
		else
			newexe ${S}/platforms/gentoo/bacula-init bacula
		fi
	fi
}

pkg_postinst() {
	# create the daemon group
	HAVE_BACULA=`cat /etc/group | grep bacula 2>/dev/null`
	if [ -z $HAVE_BACULA ]; then
	enewgroup bacula
	einfo
	einfo "The group bacula has been created. Any users you add to"
	einfo "this group have access to files created by the daemons."
	fi

	# the working directory
	install -m0750 -o root -g bacula -d ${ROOT}/var/bacula

	# link installed bacula-fd.conf into rescue directory
	#ln -s /etc/bacula/bacula-fd.conf /etc/bacula/rescue/cdrom/bacula-fd.conf
	# no longer necessary after 1.36.2

	einfo
	einfo "The CDRom rescue disk package has been installed into the"
	einfo "/etc/bacula/rescue/cdrom/ directory. Please examine the manual"
	einfo "for information on creating a rescue CD. CDR device detection"
	einfo "during build has been disabled to prevent sandbox violations."
	einfo "You need to examine /etc/bacula/rescue/cdrom/Makefile and adjust"
	einfo "the device information for your CD recorder."
	einfo

	if ! use bacula-clientonly; then
	einfo
	einfo "Please note either/or nature of database USE flags for"
	einfo "Bacula.  If mysql is set, it will be used, else postgres"
	einfo "else finally SQLite.  If you wish to have multiple DBs on"
	einfo "one system, you may wish to unset auxillary DBs for this"
	einfo "build."
	einfo

	if use mysql
	then
	# test for an existing database
	# note: this ASSUMES no password has been set for bacula database
	DB_VER=`mysql 2>/dev/null bacula -e 'select * from Version;'|tail -n 1`
		if [ -z "$DB_VER" ]; then
		einfo "This appears to be a new install and you plan to use mysql"
		einfo "for your catalog database. You should now create it by doing"
		einfo "these commands:"
		einfo " sh /etc/bacula/grant_mysql_privileges"
		einfo " sh /etc/bacula/create_mysql_database"
		einfo " sh /etc/bacula/make_mysql_tables"
		elif [ "$DB_VER" -lt "8" ]; then
		elinfo "This release requires an upgrade to your bacula database"
		einfo "as the database format has changed.  Please read the"
		einfo "manual chapter for how to upgrade your database!!!"
		einfo
		einfo "Backup your database with the command:"
		einfo " mysqldump -f --opt bacula | bzip2 > /var/bacula/bacula_backup.sql.bz"
		einfo
		einfo "Then update your database using the scripts found in"
		einfo "/etc/bacula/updatedb/ from your current version $DB_VER to"
		einfo "version 8. Note that scripts must be run in order from your"
		einfo "version to the current version."
		fi
	fi

	if use postgres
	then
	# test for an existing database
	# note: this ASSUMES no password has been set for bacula database
	DB_VER=`echo 'select * from Version;' | psql bacula 2>/dev/null | tail -3 | head -1`
		if [ -z "$DB_VER" ]; then
		einfo "This appears to be a new install and you plan to use postgresql"
		einfo "for your catalog database. You should now create it by doing"
		einfo "these commands:"
		einfo " sh /etc/bacula/create_postgresql_database"
		einfo " sh /etc/bacula/make_postgresql_tables"
		einfo " sh /etc/bacula/grant_postgresql_privileges"
		elif [ "$DB_VER" -lt "8" ]; then
		elinfo "This release requires an upgrade to your bacula database"
		einfo "as the database format has changed.  Please read the"
		einfo "manual chapter for how to upgrade your database!!!"
		einfo
		einfo "Backup your database with the command:"
		einfo " pg_dump bacula | bzip2 > /var/bacula/bacula_backup.sql.bz2"
		einfo
		einfo "Then update your database using the scripts found in"
		einfo "/etc/bacula/updatedb/ from your current version $DB_VER to"
		einfo "version 8. Note that scripts must be run in order from your"
		einfo "version to the current version."
		fi
	fi

	if use sqlite
	then
	# test for an existing database
	# note: this ASSUMES no password has been set for bacula database
	DB_VER=`echo "select * from Version;" | sqlite 2>/dev/null /var/bacula/bacula.db | tail -n 1`
		if [ -z "$DB_VER" ]; then
		einfo "This appears to be a new install and you plan to use sqlite"
		einfo "for your catalog database. You should now create it by doing"
		einfo "these commands:"
		einfo " sh /etc/bacula/grant_sqlite_privileges"
		einfo " sh /etc/bacula/create_sqlite_database"
		einfo " sh /etc/bacula/make_sqlite_tables"
		elif [ "$DB_VER" -lt "8" ]; then
		elinfo "This release requires an upgrade to your bacula database"
		einfo "as the database format has changed.  Please read the"
		einfo "manual chapter for how to upgrade your database!!!"
		einfo
		einfo "Backup your database with the command:"
		einfo " echo .dump | sqlite /var/bacula/bacula.db | bzip2 > \\"
		einfo "   /var/bacula/bacula_backup.sql.bz2"
		einfo
		einfo "Then update your database using the scripts found in"
		einfo "/etc/bacula/updatedb/ from your current version $DB_VER to"
		einfo "version 8. Note that scripts must be run in order from your"
		einfo "version to the current version."
		fi
	fi
	fi

	einfo
	einfo "Review your configuration files in /etc/bacula and"
	einfo "start the daemons:"
	if use bacula-clientonly; then
		einfo " /etc/init.d/bacula-fd start"
	else
		if use bacula-split; then
		einfo " /etc/init.d/bacula-sd start"
		einfo " /etc/init.d/bacula-dir start"
		einfo " /etc/init.d/bacula-fd start"
		einfo " or /etc/bacula/bacula will start all three."
		else
		einfo " /etc/init.d/bacula start"
		fi
	fi
	einfo
	einfo "You may also wish to:"
	if use bacula-clientonly; then
		einfo " rc-update add bacula-fd default"
	else
		if use bacula-split; then
			einfo " rc-update add bacula-sd default"
			einfo " rc-update add bacula-dir default"
			einfo " rc-update add bacula-fd default"
		else
			einfo " rc-update add bacula default"
		fi
	fi
	einfo
}
