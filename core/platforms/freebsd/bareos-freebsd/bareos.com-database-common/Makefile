PORTNAME=	bareos.com
PORTREVISION=	0
PKGNAMESUFFIX=	-database-common
COMMENT=	Backup archiving recovery open sourced (database-common)
PLIST=		${PKGDIR}/pkg-plist.database-common
MASTERDIR=	${.CURDIR}/../bareos.com-common

LIB_DEPENDS+=  libbareos.so:sysutils/bareos.com-common


# optional overrides, used by build system.
.-include "overrides.mk"

.include "${MASTERDIR}/BareosPackageOnlyMakefile"
.include "${MASTERDIR}/BareosCommonMakefile"
