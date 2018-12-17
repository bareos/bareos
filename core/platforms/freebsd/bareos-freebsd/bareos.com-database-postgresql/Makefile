PORTNAME=	bareos.com
PORTREVISION=	0
PKGNAMESUFFIX=	-database-postgresql
COMMENT=	Backup archiving recovery open sourced (database-postgresql)
PLIST=		${PKGDIR}/pkg-plist.database-postgresql
MASTERDIR=	${.CURDIR}/../bareos.com-common

LIB_DEPENDS+=  libbareos.so:sysutils/bareos.com-common
LIB_DEPENDS+=  libbareosql.so:sysutils/bareos.com-database-common
LIB_DEPENDS+=  libbareoscats.so:sysutils/bareos.com-database-common

USES+=pgsql

.include "${MASTERDIR}/BareosPackageOnlyMakefile"
.include "${MASTERDIR}/BareosCommonMakefile"
