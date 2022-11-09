PORTNAME=	bareos.com
PORTREVISION=	0
PKGNAMESUFFIX=	-storage-tape
COMMENT=	Backup archiving recovery open sourced (storage-tape)
PLIST=		${PKGDIR}/pkg-plist.storage-tape
MASTERDIR=	${.CURDIR}/../bareos.com-common

LIB_DEPENDS+=  libbareos.so:sysutils/bareos.com-common
LIB_DEPENDS+=  libbareossd.so:sysutils/bareos.com-common

# optional overrides, used by build system.
.-include "overrides.mk"

.include "${MASTERDIR}/BareosPackageOnlyMakefile"
.include "${MASTERDIR}/BareosCommonMakefile"
