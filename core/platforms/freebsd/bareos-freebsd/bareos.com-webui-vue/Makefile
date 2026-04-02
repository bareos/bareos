PORTNAME=	bareos.com
PORTREVISION=	0
PKGNAMESUFFIX=	-webui-vue
COMMENT=	Backup archiving recovery open sourced (Vue/Quasar web interface)
PLIST=		${PKGDIR}/pkg-plist.webui-vue
MASTERDIR=	${.CURDIR}/../bareos.com-common

RUN_DEPENDS+=	${LOCALBASE}/sbin/bareos-webui-proxy:sysutils/bareos.com-webui-proxy

# optional overrides, used by build system.
.-include "overrides.mk"

.include "${MASTERDIR}/BareosPackageOnlyMakefile"
.include "${MASTERDIR}/BareosCommonMakefile"

# The assets/ directory contains content-hashed filenames that change with
# every build.  Regenerate the plist from the staged tree so pkg gets exact
# file paths rather than a directory entry it cannot checksum.
post-stage:
	@echo "etc/apache24/Includes/bareos-webui-vue.conf" \
		> ${PKGDIR}/pkg-plist.webui-vue
	@find ${STAGEDIR}${PREFIX}/share/bareos-webui-vue -type f \
		| sed "s|${STAGEDIR}${PREFIX}/||" \
		| sort \
		>> ${PKGDIR}/pkg-plist.webui-vue
