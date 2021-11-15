PORTNAME=	bareos.com
PORTREVISION=	0
PKGNAMESUFFIX=	-storage
PKGMESSAGE=     ${PKGDIR}/pkg-message.storage
COMMENT=	Backup archiving recovery open sourced (storage)
PLIST=		${PKGDIR}/pkg-plist.storage
MASTERDIR=	${.CURDIR}/../bareos.com-common

LIB_DEPENDS+=  libbareos.so:sysutils/bareos.com-common
LIB_DEPENDS+=  libbareosndmp.so:sysutils/bareos.com-common
LIB_DEPENDS+=  libbareosfind.so:sysutils/bareos.com-common
LIB_DEPENDS+=  libbareossd.so:sysutils/bareos.com-common

.include "${MASTERDIR}/BareosPackageOnlyMakefile"
.include "${MASTERDIR}/BareosCommonMakefile"
