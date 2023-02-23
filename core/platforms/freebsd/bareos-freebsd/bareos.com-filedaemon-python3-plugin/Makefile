PORTNAME=	bareos.com
PORTREVISION=	0
PKGNAMESUFFIX=	-filedaemon-python3-plugin
COMMENT=	Backup archiving recovery open sourced (filedaemon-python3-plugin)
PLIST=		${PKGDIR}/pkg-plist.filedaemon-python3-plugin
MASTERDIR=	${.CURDIR}/../bareos.com-common

LIB_DEPENDS+=  libbareos.so:sysutils/bareos.com-common

USES+=python:3.7

# optional overrides, used by build system.
.-include "overrides.mk"

.include "${MASTERDIR}/BareosPackageOnlyMakefile"
.include "${MASTERDIR}/BareosCommonMakefile"
