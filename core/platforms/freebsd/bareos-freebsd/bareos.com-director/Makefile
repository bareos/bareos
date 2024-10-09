PORTNAME=	bareos.com
PORTREVISION=	0
PKGNAMESUFFIX=	-director
COMMENT=	Backup archiving recovery open sourced (director)
PLIST=		${PKGDIR}/pkg-plist.director
MASTERDIR=	${.CURDIR}/../bareos.com-common

PKGINSTALL=     ${PKGDIR}/pkg-install
PKGMESSAGE=     ${PKGDIR}/pkg-message.director

LIB_DEPENDS+= liblzo2.so:archivers/lzo2 \
              libjansson.so:devel/jansson \
              libreadline.so:devel/readline

LIB_DEPENDS+=  libbareos.so:sysutils/bareos.com-common
LIB_DEPENDS+=  libbareosndmp.so:sysutils/bareos.com-common
LIB_DEPENDS+=  libbareoslmdb.so:sysutils/bareos.com-common
LIB_DEPENDS+=  libbareosfind.so:sysutils/bareos.com-common

LIB_DEPENDS+=  libbareossql.so:sysutils/bareos.com-database-common

RUN_DEPENDS+=  bareos-dbcheck:sysutils/bareos.com-database-tools
RUN_DEPENDS+=  /usr/local/lib/bareos/scripts/ddl/creates/postgresql.sql:sysutils/bareos.com-database-postgresql

# optional overrides, used by build system.
.-include "overrides.mk"

.include "${MASTERDIR}/BareosPackageOnlyMakefile"
.include "${MASTERDIR}/BareosCommonMakefile"
