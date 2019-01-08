AC_INIT([bareos-webui], [0.1])
AM_INIT_AUTOMAKE([foreign subdir-objects])

AC_PATH_PROGS( [APXS], [apxs apxs2], [apxs2], [$PATH$PATH_SEPARATOR/usr/sbin$PATH_SEPARATOR/sbin])
AC_SUBST( [APACHE2_SYSCONFDIR], [`$APXS -q SYSCONFDIR`] )

dnl Check for location of Apache conf.d directory
HTTP_CONF=no
AC_ARG_WITH(httpd_conf,AC_HELP_STRING([--with-httpd-conf=<path_to_conf>],[sets path to Apache conf.d directory]),HTTPD_CONF=$withval,HTTPD_CONF=no)
if test x$HTTPD_CONF = xno; then
        if test -d /etc/httpd/conf.d; then
                HTTPD_CONF="/etc/httpd/conf.d"
        elif test -d /etc/apache2/conf.d; then
                HTTPD_CONF="/etc/apache2/conf.d"
        elif test -d /etc/apache/conf.d; then
                HTTPD_CONF="/etc/apache/conf.d"
	elif test -d /etc/apache2/conf-available; then
		HTTPD_CONF="/etc/apache2/conf-available"
        else
                HTTPD_CONF="/etc/httpd/conf.d"
        fi
fi
AC_SUBST(HTTPD_CONF)

AC_CONFIG_SRCDIR([configure.ac])
AC_CONFIG_FILES(Makefile module/Application/language/Makefile)
AC_OUTPUT
