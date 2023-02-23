PORTNAME=	bareos.com
PORTREVISION=	0
PKGNAMESUFFIX=	-common
COMMENT=	Backup archiving recovery open sourced (bconsole)
PLIST=		${PKGDIR}/pkg-plist.common
MASTERDIR=	${.CURDIR}/../bareos.com-common

LIB_DEPENDS+= liblzo2.so:archivers/lzo2 \
              libjansson.so:devel/jansson \
              libreadline.so:devel/readline

# optional overrides, used by build system.
.-include "overrides.mk"

post-patch:
	@echo post-patch ${PORTNAME}${PKGNAMESUFFIX} via master package in ${MASTERDIR}
	${REINPLACE_CMD} -e '1s|^#!.*perl|#!/usr/local/bin/perl|' -e 's|pg_dump|/usr/local/bin/pg_dump|g' ${WRKSRC}/core/src/cats/make_catalog_backup.pl.in

post-install:
	@echo post-install ${PORTNAME}${PKGNAMESUFFIX} via master package in ${MASTERDIR}
	find ${STAGEDIR} -name '*.conf' -exec mv -v {} {}.sample \;
	@${MKDIR} ${STAGEDIR}/var/lib/bareos/storage
	@${MKDIR} ${STAGEDIR}/var/log/bareos
	@${MKDIR} ${STAGEDIR}/var/run/bareos

pre-package:
	@echo pre-package ${PORTNAME}${PKGNAMESUFFIX} via master package in ${MASTERDIR}
	if [ -f ${WRKDIR}/.build/Testing/TAG ]; then echo "Testing/TAG exists, not running ctest a second time"; else cd ${WRKDIR}/.build && sudo REGRESS_DEBUG=1 ctest -V -S CTestScript.cmake || echo "ctest result:$?"; fi

.include "${MASTERDIR}/BareosCommonMakefile"
