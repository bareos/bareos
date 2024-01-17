PORTNAME=	bareos.com
PORTREVISION=	0
PKGNAMESUFFIX=	-database-postgresql
COMMENT=	Backup archiving recovery open sourced (database-postgresql)
PLIST=		${PKGDIR}/pkg-plist.database-postgresql
MASTERDIR=	${.CURDIR}/../bareos.com-common

LIB_DEPENDS+=  libbareos.so:sysutils/bareos.com-common
LIB_DEPENDS+=  libbareossql.so:sysutils/bareos.com-database-common

# optional overrides, used by build system.
.-include "overrides.mk"

.include "${MASTERDIR}/BareosPackageOnlyMakefile"
.include "${MASTERDIR}/BareosCommonMakefile"
