PORTNAME=	bareos.com
PORTREVISION=	0
PKGNAMESUFFIX=	-common
COMMENT=	Backup archiving recovery open sourced (bconsole)
PLIST=		${PKGDIR}/pkg-plist.common
MASTERDIR=	${.CURDIR}/../bareos.com-common

LIB_DEPENDS+= liblzo2.so:archivers/lzo2 \
              libjansson.so:devel/jansson \
              libreadline.so:devel/readline

post-patch:
	@${REINPLACE_CMD} -e 's|pg_dump|/usr/local/bin/pg_dump|g' ${WRKSRC}/core/src/cats/make_catalog_backup.pl.in

pre-package:
	if [ -f work/.build/Testing/TAG ]; then echo "Testing/TAG exists, not running ctest a second time"; else cd work/.build && REGRESS_DEBUG=1  ctest -V -S CTestScript.cmake || echo "ctest result:$?"; fi


.include "${MASTERDIR}/BareosCommonMakefile"
