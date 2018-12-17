PORTNAME=	bareos.com
PORTREVISION=	0
PKGNAMESUFFIX=	-filedaemon-python-plugin
COMMENT=	Backup archiving recovery open sourced (filedaemon-python-plugin)
PLIST=		${PKGDIR}/pkg-plist.filedaemon-python-plugin
MASTERDIR=	${.CURDIR}/../bareos.com-common

LIB_DEPENDS+=  libbareos.so:sysutils/bareos.com-common

USES=python

.include "${MASTERDIR}/BareosPackageOnlyMakefile"
.include "${MASTERDIR}/BareosCommonMakefile"
