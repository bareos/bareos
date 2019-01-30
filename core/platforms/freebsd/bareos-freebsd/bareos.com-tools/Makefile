PORTNAME=	bareos.com
PORTREVISION=	0
PKGNAMESUFFIX=	-tools
COMMENT=	Backup archiving recovery open sourced (tools)
PLIST=		${PKGDIR}/pkg-plist.tools
MASTERDIR=	${.CURDIR}/../bareos.com-common

LIB_DEPENDS+=  libbareos.so:sysutils/bareos.com-common
LIB_DEPENDS+=  libbareosds.so:sysutils/bareos.com-common
LIB_DEPENDS+=  libbareosfind.so:sysutils/bareos.com-common

.include "${MASTERDIR}/BareosPackageOnlyMakefile"
.include "${MASTERDIR}/BareosCommonMakefile"
