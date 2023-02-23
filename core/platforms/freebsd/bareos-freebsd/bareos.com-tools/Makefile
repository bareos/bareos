PORTNAME=	bareos.com
PORTREVISION=	0
PKGNAMESUFFIX=	-tools
COMMENT=	Backup archiving recovery open sourced (tools)
PLIST=		${PKGDIR}/pkg-plist.tools
MASTERDIR=	${.CURDIR}/../bareos.com-common

LIB_DEPENDS+=  libbareos.so:sysutils/bareos.com-common
LIB_DEPENDS+=  libbareossd.so:sysutils/bareos.com-common
LIB_DEPENDS+=  libbareosfind.so:sysutils/bareos.com-common

# optional overrides, used by build system.
.-include "overrides.mk"

.include "${MASTERDIR}/BareosPackageOnlyMakefile"
.include "${MASTERDIR}/BareosCommonMakefile"
