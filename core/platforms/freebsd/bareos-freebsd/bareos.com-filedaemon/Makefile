PORTNAME=	bareos.com
PORTREVISION=	0
PKGNAMESUFFIX=	-filedaemon
PKGMESSAGE=     ${PKGDIR}/pkg-message.file
COMMENT=	Backup archiving recovery open sourced (filedaemon)
PLIST=		${PKGDIR}/pkg-plist.filedaemon
MASTERDIR=	${.CURDIR}/../bareos.com-common

LIB_DEPENDS+=  libbareos.so:sysutils/bareos.com-common
LIB_DEPENDS+=  libbareoslmdb.so:sysutils/bareos.com-common
LIB_DEPENDS+=  libbareosfind.so:sysutils/bareos.com-common

# optional overrides, used by build system.
.-include "overrides.mk"

.include "${MASTERDIR}/BareosPackageOnlyMakefile"
.include "${MASTERDIR}/BareosCommonMakefile"
