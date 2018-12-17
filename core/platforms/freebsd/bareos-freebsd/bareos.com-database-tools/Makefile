PORTNAME=	bareos.com
PORTREVISION=	0
PKGNAMESUFFIX=	-database-tools
COMMENT=	Backup archiving recovery open sourced (database-tools)
PLIST=		${PKGDIR}/pkg-plist.database-tools
MASTERDIR=	${.CURDIR}/../bareos.com-common

LIB_DEPENDS+=  libbareos.so:sysutils/bareos.com-common
LIB_DEPENDS+=  libbareosfind.so:sysutils/bareos.com-common
LIB_DEPENDS+=  libbareossd.so:sysutils/bareos.com-common

LIB_DEPENDS+=  libbareosql.so:sysutils/bareos.com-database-common
LIB_DEPENDS+=  libbareoscats.so:sysutils/bareos.com-database-common

.include "${MASTERDIR}/BareosPackageOnlyMakefile"
.include "${MASTERDIR}/BareosCommonMakefile"
