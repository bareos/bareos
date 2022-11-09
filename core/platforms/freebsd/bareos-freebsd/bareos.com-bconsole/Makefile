PORTNAME=	bareos.com
PORTREVISION=	0
PKGNAMESUFFIX=	-bconsole
COMMENT=	Backup archiving recovery open sourced (bconsole)
PLIST=		${PKGDIR}/pkg-plist.bconsole
MASTERDIR=	${.CURDIR}/../bareos.com-common

LIB_DEPENDS+=  libbareos.so:sysutils/baroes.com-common

# optional overrides, used by build system.
.-include "overrides.mk"

.include "${MASTERDIR}/BareosPackageOnlyMakefile"
.include "${MASTERDIR}/BareosCommonMakefile"
