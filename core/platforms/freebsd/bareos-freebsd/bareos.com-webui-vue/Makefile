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
