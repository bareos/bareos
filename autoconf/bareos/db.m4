AC_DEFUN([BA_CHECK_DBI_DB],
[
AC_MSG_CHECKING(for DBI support)
AC_ARG_WITH(dbi,
AC_HELP_STRING([--with-dbi@<:@=DIR@:>@], [Include DBI support. DIR is the DBD base install directory, default is to search through a number of common places for the DBI files.]),
[
   if test "$withval" != "no"; then
      if test "$withval" = "yes"; then
         if test -f /usr/local/include/dbi/dbi.h; then
            DBI_INCDIR=/usr/local/dbi/include
            if test -d /usr/local/lib64; then
               DBI_LIBDIR=/usr/local/lib64
            else
               DBI_LIBDIR=/usr/local/lib
            fi
            DBI_BINDIR=/usr/local/bin
         elif test -f /usr/include/dbi/dbi.h; then
            DBI_INCDIR=/usr/include
            if test -d /usr/lib64; then
               DBI_LIBDIR=/usr/lib64
            else
               DBI_LIBDIR=/usr/lib
            fi
            DBI_BINDIR=/usr/bin
         elif test -f $prefix/include/dbi/dbi.h; then
            DBI_INCDIR=$prefix/include
            if test -d $prefix/lib64; then
               DBI_LIBDIR=$prefix/lib64
            else
               DBI_LIBDIR=$prefix/lib
            fi
            DBI_BINDIR=$prefix/bin
         else
            AC_MSG_RESULT(no)
            AC_MSG_ERROR(Unable to find dbi.h in standard locations)
         fi
         if test -d /usr/local/lib/dbd; then
            DRIVERDIR=/usr/local/lib/dbd
            if test -d /usr/local/lib64/dbd; then
               DRIVERDIR=/usr/local/lib64/dbd
            else
               DRIVERDIR=/usr/local/lib/dbd
            fi
         elif test -d /usr/lib/dbd; then
            DRIVERDIR=/usr/lib/dbd
            if test -d /usr/lib64/dbd; then
               DRIVERDIR=/usr/lib64/dbd
            else
               DRIVERDIR=/usr/lib/dbd
            fi
         elif test -d $prefix/lib/dbd; then
            if test -d $prefix/lib64/dbd; then
               DRIVERDIR=$prefix/lib64/dbd
            else
               DRIVERDIR=$prefix/lib/dbd
            fi
         elif test -d /usr/local/lib64/dbd; then
            DRIVERDIR=/usr/local/lib64/dbd
         elif test -d /usr/lib64/dbd; then
            DRIVERDIR=/usr/lib64/dbd
         elif test -d $prefix/lib64/dbd; then
            DRIVERDIR=$prefix/lib64/dbd
         else
            AC_MSG_RESULT(no)
            AC_MSG_ERROR(Unable to find DBD drivers in standard locations)
         fi
      else
         if test -f $withval/dbi.h; then
            DBI_INCDIR=$withval
            DBI_LIBDIR=$withval
            DBI_BINDIR=$withval
         elif test -f $withval/include/dbi/dbi.h; then
            DBI_INCDIR=$withval/include
            if test -d $withval/lib64; then
               DBI_LIBDIR=$withval/lib64
            else
               DBI_LIBDIR=$withval/lib
            fi
            DBI_BINDIR=$withval/bin
         else
            AC_MSG_RESULT(no)
            AC_MSG_ERROR(Invalid DBI directory $withval - unable to find dbi.h under $withval)
         fi
         if test -d $withval/dbd; then
            DRIVERDIR=$withval/dbd
         elif test -d $withval/lib/; then
            if test -d $withval/lib64/dbd; then
               DRIVERDIR=$withval/lib64/dbd
            else
               DRIVERDIR=$withval/lib/dbd
            fi
         elif test -d $withval/lib64/dbd; then
            DRIVERDIR=$withval/lib64/dbd
         else
            AC_MSG_RESULT(no)
            AC_MSG_ERROR(Invalid DBD driver directory $withval - unable to find DBD drivers under $withval)
         fi
      fi

      if test x$DBI_LIBDIR != x; then
         DBI_INCLUDE=-I$DBI_INCDIR
         if test x$use_libtool != xno; then
            DBI_LIBS="-R $DBI_LIBDIR -L$DBI_LIBDIR -ldbi"
         else
            DBI_LIBS="-L$DBI_LIBDIR -ldbi"
         fi
         DBI_LIB=$DBI_LIBDIR/libdbi.a
         DBI_DBD_DRIVERDIR="-D DBI_DRIVER_DIR=\\\"$DRIVERDIR\\\""
         DB_LIBS="${DB_LIBS} ${DBI_LIBS}"

         AC_DEFINE(HAVE_DBI, 1, [Set if you have the DBI driver])
         AC_MSG_RESULT(yes)

         if test -z "${db_backends}"; then
            db_backends="DBI"
         else
            db_backends="${db_backends} DBI"
         fi
         if test -z "${DB_BACKENDS}" ; then
            DB_BACKENDS="dbi"
         else
            DB_BACKENDS="${DB_BACKENDS} dbi"
         fi
         uncomment_dbi=" "

         dnl -------------------------------------------
         dnl Push the DB_PROG onto the stack of supported database backends for DBI
         dnl -------------------------------------------
         DB_BACKENDS="${DB_BACKENDS} ${DB_PROG}"

         dnl -------------------------------------------
         dnl Check if dbi supports batch mode
         dnl -------------------------------------------
         if test "x$support_batch_insert" = "xyes"; then
            if test $DB_PROG = postgresql; then
               AC_CHECK_LIB(pq, PQisthreadsafe, AC_DEFINE(HAVE_PQISTHREADSAFE, 1, [Define to 1 if you have the `PQisthreadsafe' function.]))
               AC_CHECK_LIB(pq, PQputCopyData, AC_DEFINE(HAVE_PQ_COPY, 1, [Define to 1 if you have the `PQputCopyData' function.]))
               test "x$ac_cv_lib_pq_PQputCopyData" = "xyes"
               pkg=$?
               if test $pkg = 0; then
                  AC_DEFINE(HAVE_POSTGRESQL_BATCH_FILE_INSERT, 1, [Define to 1 if PostgreSQL DB batch insert code enabled])
               fi
            fi
         fi
      fi
   else
      AC_MSG_RESULT(no)
   fi
],[
   AC_MSG_RESULT(no)
])

AC_SUBST(DBI_LIBS)
AC_SUBST(DBI_INCLUDE)
AC_SUBST(DBI_BINDIR)
AC_SUBST(DBI_DBD_DRIVERDIR)
])

AC_DEFUN([BA_CHECK_DBI_DRIVER],
[
db_prog=no
AC_MSG_CHECKING(for DBI drivers support)
AC_ARG_WITH(dbi-driver,
AC_HELP_STRING([--with-dbi-driver@<:@=DRIVER@:>@], [Suport for DBI driver. DRIVER is the one DBI driver like Mysql, Postgresql, others. Default is to not configure any driver.]),
[
   if test "$withval" != "no"; then
      case $withval in
         "mysql")
            db_prog="mysql"
            MYSQL_CONFIG=`which mysql_config 2>/dev/null`
            if test "x${MYSQL_CONFIG}" != x; then
               MYSQL_BINDIR="${MYSQL_CONFIG%/*}"
               ${MYSQL_CONFIG} --variable=pkglibdir > /dev/null 2>&1
               if test $? = 0 ; then
                  MYSQL_LIBDIR=`${MYSQL_CONFIG} --variable=pkglibdir`
                  MYSQL_INCDIR=`${MYSQL_CONFIG} --variable=pkgincludedir`
               else
                  MYSQL_LIBDIR=`${MYSQL_CONFIG} --libs | sed -e 's/.*-L//' -e 's/ .*//'`
                  MYSQL_INCDIR=`${MYSQL_CONFIG} --include | sed -e 's/-I//'`
               fi
            fi

            #
            # See if the mysql_config gave something that is worth anything.
            # Some OS-es just lie about this when you ask for the library to use
            # they just give you back the directory its not in.
            #
            # Fallback to the old way of finding the right settings to use and hope for the best.
            #
            if test x${MYSQL_LIBDIR} = x -o \
               ! \( -f ${MYSQL_LIBDIR}/libmysqlclient_r.so -o \
                    -f ${MYSQL_LIBDIR}/libmysqlclient_r.a \); then
               if test -f /usr/local/mysql/bin/mysql; then
                  MYSQL_BINDIR=/usr/local/mysql/bin
                  if test -f /usr/local/mysql/lib64/mysql/libmysqlclient_r.a \
                     -o -f /usr/local/mysql/lib64/mysql/libmysqlclient_r.so; then
                     MYSQL_LIBDIR=/usr/local/mysql/lib64/mysql
                  else
                     MYSQL_LIBDIR=/usr/local/mysql/lib/mysql
                  fi
               elif test -f /usr/bin/mysql; then
                  MYSQL_BINDIR=/usr/bin
                  if test -f /usr/lib64/mysql/libmysqlclient_r.a \
                      -o -f /usr/lib64/mysql/libmysqlclient_r.so; then
                      MYSQL_LIBDIR=/usr/lib64/mysql
                  elif test -f /usr/lib/mysql/libmysqlclient_r.a \
                      -o -f /usr/lib/mysql/libmysqlclient_r.so; then
                      MYSQL_LIBDIR=/usr/lib/mysql
                  else
                      MYSQL_LIBDIR=/usr/lib
                  fi
               elif test -f /usr/local/bin/mysql; then
                  MYSQL_BINDIR=/usr/local/bin
                  if test -f /usr/local/lib64/mysql/libmysqlclient_r.a \
                      -o -f /usr/local/lib64/mysql/libmysqlclient_r.so; then
                      MYSQL_LIBDIR=/usr/local/lib64/mysql
                  elif test -f /usr/local/lib/mysql/libmysqlclient_r.a \
                      -o -f /usr/local/lib/mysql/libmysqlclient_r.so; then
                      MYSQL_LIBDIR=/usr/local/lib/mysql
                  else
                      MYSQL_LIBDIR=/usr/local/lib
                  fi
               elif test -f $withval/bin/mysql; then
                  MYSQL_BINDIR=$withval/bin
                  if test -f $withval/lib64/mysql/libmysqlclient_r.a \
                      -o -f $withval/lib64/mysql/libmysqlclient_r.so; then
                      MYSQL_LIBDIR=$withval/lib64/mysql
                  elif test -f $withval/lib64/libmysqlclient_r.a \
                      -o -f $withval/lib64/libmysqlclient_r.so; then
                      MYSQL_LIBDIR=$withval/lib64
                  elif test -f $withval/lib/libmysqlclient_r.a \
                      -o -f $withval/lib/libmysqlclient_r.so; then
                      MYSQL_LIBDIR=$withval/lib/
                  else
                      MYSQL_LIBDIR=$withval/lib/mysql
                  fi
               else
                  AC_MSG_RESULT(no)
                  AC_MSG_ERROR(Unable to find mysql in standard locations)
               fi
            fi
            if test -f $MYSQL_LIBDIR/libmysqlclient_r.so; then
               DB_PROG_LIB=$MYSQL_LIBDIR/libmysqlclient_r.so
            else
               DB_PROG_LIB=$MYSQL_LIBDIR/libmysqlclient_r.a
            fi
            ;;
         "postgresql")
            db_prog="postgresql"
            PG_CONFIG=`which pg_config 2>/dev/null`
            if test -n "$PG_CONFIG"; then
               POSTGRESQL_BINDIR=`"$PG_CONFIG" --bindir`
               POSTGRESQL_LIBDIR=`"$PG_CONFIG" --libdir`
            fi

            #
            # See if the pg_config gave something that is worth anything.
            #
            if test x${POSTGRESQL_LIBDIR} = x -o \
               ! \( -f ${POSTGRESQL_LIBDIR}/libpq.so -o \
                   -f ${POSTGRESQL_LIBDIR}/libpq.a \); then
               if test -f /usr/local/bin/psql; then
                  POSTGRESQL_BINDIR=/usr/local/bin
                  if test -d /usr/local/lib64; then
                     POSTGRESQL_LIBDIR=/usr/local/lib64
                  else
                     POSTGRESQL_LIBDIR=/usr/local/lib
                  fi
               elif test -f /usr/bin/psql; then
                  POSTGRESQL_BINDIR=/usr/local/bin
                  if test -d /usr/lib64/postgresql; then
                     POSTGRESQL_LIBDIR=/usr/lib64/postgresql
                  elif test -d /usr/lib/postgresql; then
                     POSTGRESQL_LIBDIR=/usr/lib/postgresql
                  elif test -d /usr/lib64; then
                     POSTGRESQL_LIBDIR=/usr/lib64
                  else
                     POSTGRESQL_LIBDIR=/usr/lib
                  fi
               elif test -f $withval/bin/psql; then
                  POSTGRESQL_BINDIR=$withval/bin
                  if test -d $withval/lib64; then
                     POSTGRESQL_LIBDIR=$withval/lib64
                  else
                     POSTGRESQL_LIBDIR=$withval/lib
                  fi
               else
                  AC_MSG_RESULT(no)
                  AC_MSG_ERROR(Unable to find psql in standard locations)
               fi
            fi
            if test -f $POSTGRESQL_LIBDIR/libpq.so; then
               DB_PROG_LIB=$POSTGRESQL_LIBDIR/libpq.so
            else
               DB_PROG_LIB=$POSTGRESQL_LIBDIR/libpq.a
            fi
            ;;
         "sqlite3")
            db_prog="sqlite3"
            if test -f /usr/local/bin/sqlite3; then
               SQLITE_BINDIR=/usr/local/bin
               if test -d /usr/local/lib64; then
                  SQLITE_LIBDIR=/usr/local/lib64
               else
                  SQLITE_LIBDIR=/usr/local/lib
               fi
            elif test -f /usr/bin/sqlite3; then
               SQLITE_BINDIR=/usr/bin
               if test -d /usr/lib64; then
                  SQLITE_LIBDIR=/usr/lib64
               else
                  SQLITE_LIBDIR=/usr/lib
               fi
            elif test -f $withval/bin/sqlite3; then
               SQLITE_BINDIR=$withval/bin
               if test -d $withval/lib64; then
                  SQLITE_LIBDIR=$withval/lib64
               else
                  SQLITE_LIBDIR=$withval/lib
               fi
            else
               AC_MSG_RESULT(no)
               AC_MSG_ERROR(Unable to find sqlite in standard locations)
            fi
            if test -f $SQLITE_LIBDIR/libsqlite3.so; then
               DB_PROG_LIB=$SQLITE_LIBDIR/libsqlite3.so
            else
               DB_PROG_LIB=$SQLITE_LIBDIR/libsqlite3.a
            fi
            ;;
         *)
            AC_MSG_RESULT(no)
            AC_MSG_ERROR(Unable to set DBI driver. $withval is not supported)
            ;;
      esac

      AC_MSG_RESULT(yes)
      DB_PROG=$db_prog
   else
      AC_MSG_RESULT(no)
   fi
],[
   AC_MSG_RESULT(no)
])

AC_SUBST(MYSQL_BINDIR)
AC_SUBST(POSTGRESQL_BINDIR)
AC_SUBST(SQLITE_BINDIR)
AC_SUBST(DB_PROG)
AC_SUBST(DB_PROG_LIB)
])

AC_DEFUN([BA_CHECK_MYSQL_DB],
[
AC_MSG_CHECKING(for MySQL support)
AC_ARG_WITH(mysql,
AC_HELP_STRING([--with-mysql@<:@=DIR@:>@], [Include MySQL support. DIR is the MySQL base install directory, default is to search through a number of common places for the MySQL files.]),
[
   if test "$withval" != "no"; then
      if test "$withval" = "yes"; then
         MYSQL_CONFIG=`which mysql_config 2>/dev/null`
         if test "x${MYSQL_CONFIG}" != x; then
            MYSQL_BINDIR="${MYSQL_CONFIG%/*}"
            ${MYSQL_CONFIG} --variable=pkglibdir > /dev/null 2>&1
            if test $? = 0 ; then
               MYSQL_LIBDIR=`${MYSQL_CONFIG} --variable=pkglibdir`
               MYSQL_INCDIR=`${MYSQL_CONFIG} --variable=pkgincludedir`
            else
               MYSQL_LIBDIR=`${MYSQL_CONFIG} --libs | sed -e 's/.*-L//' -e 's/ .*//'`
               MYSQL_INCDIR=`${MYSQL_CONFIG} --include | sed -e 's/-I//'`
            fi
         fi

         #
         # See if the mysql_config gave something that is worth anything.
         # Some OS-es just lie about this when you ask for the library to use
         # they just give you back the directory its not in.
         #
         # Fallback to the old way of finding the right settings to use and hope for the best.
         #
         if test x${MYSQL_LIBDIR} = x -o \
            ! \( -f ${MYSQL_LIBDIR}/libmysqlclient_r.so -o \
                 -f ${MYSQL_LIBDIR}/libmysqlclient.so -o \
                 -f ${MYSQL_LIBDIR}/libmysqlclient.a -o \
                 -f ${MYSQL_LIBDIR}/libmysqlclient_r.a \); then
            if test -f /usr/local/mysql/include/mysql/mysql.h; then
               MYSQL_INCDIR=/usr/local/mysql/include/mysql
               if test -f /usr/local/mysql/lib64/mysql/libmysqlclient_r.a \
                       -o -f /usr/local/mysql/lib64/mysql/libmysqlclient_r.so; then
                  MYSQL_LIBDIR=/usr/local/mysql/lib64/mysql
               else
                  MYSQL_LIBDIR=/usr/local/mysql/lib/mysql
               fi
               MYSQL_BINDIR=/usr/local/mysql/bin
            elif test -f /usr/include/mysql/mysql.h; then
               MYSQL_INCDIR=/usr/include/mysql
               if test -f /usr/lib64/mysql/libmysqlclient_r.a \
                       -o -f /usr/lib64/mysql/libmysqlclient_r.so; then
                  MYSQL_LIBDIR=/usr/lib64/mysql
               elif test -f /usr/lib64/libmysqlclient_r.a \
                         -o -f /usr/lib64/libmysqlclient_r.so; then
                  MYSQL_LIBDIR=/usr/lib64
               elif test -f /usr/lib/x86_64-linux-gnu/libmysqlclient_r.a \
                         -o -f /usr/lib/x86_64-linux-gnu/libmysqlclient_r.so; then
                  MYSQL_LIBDIR=/usr/lib/x86_64-linux-gnu
               elif test -f /usr/lib/mysql/libmysqlclient_r.a \
                         -o -f /usr/lib/mysql/libmysqlclient_r.so; then
                  MYSQL_LIBDIR=/usr/lib/mysql
               else
                  MYSQL_LIBDIR=/usr/lib
               fi
               MYSQL_BINDIR=/usr/bin
            elif test -f /usr/include/mysql.h; then
               MYSQL_INCDIR=/usr/include
               if test -f /usr/lib64/libmysqlclient_r.a \
                       -o -f /usr/lib64/libmysqlclient_r.so; then
                  MYSQL_LIBDIR=/usr/lib64
               else
                  MYSQL_LIBDIR=/usr/lib
               fi
               MYSQL_BINDIR=/usr/bin
            elif test -f /usr/local/include/mysql/mysql.h; then
               MYSQL_INCDIR=/usr/local/include/mysql
               if test -f /usr/local/lib64/mysql/libmysqlclient_r.a \
                       -o -f /usr/local/lib64/mysql/libmysqlclient_r.so; then
                  MYSQL_LIBDIR=/usr/local/lib64/mysql
               else
                  MYSQL_LIBDIR=/usr/local/lib/mysql
               fi
               MYSQL_BINDIR=/usr/local/bin
            elif test -f /usr/local/include/mysql.h; then
               MYSQL_INCDIR=/usr/local/include
               if test -f /usr/local/lib64/libmysqlclient_r.a \
                       -o -f /usr/local/lib64/libmysqlclient_r.so; then
                  MYSQL_LIBDIR=/usr/local/lib64
               else
                  MYSQL_LIBDIR=/usr/local/lib
               fi
               MYSQL_BINDIR=/usr/local/bin
            else
               AC_MSG_RESULT(no)
               AC_MSG_ERROR(Unable to find mysql.h in standard locations)
            fi
         fi
      else
         if test -f $withval/include/mysql/mysql.h; then
            MYSQL_INCDIR=$withval/include/mysql
            if test -f $withval/lib64/mysql/libmysqlclient_r.a \
                 -o -f $withval/lib64/mysql/libmysqlclient_r.so; then
               MYSQL_LIBDIR=$withval/lib64/mysql
            elif test -f $withval/lib64/libmysqlclient_r.a \
                 -o -f $withval/lib64/libmysqlclient_r.so; then
               MYSQL_LIBDIR=$withval/lib64
            elif test -f $withval/lib/libmysqlclient_r.a \
                 -o -f $withval/lib/libmysqlclient_r.so; then
               MYSQL_LIBDIR=$withval/lib
            else
               MYSQL_LIBDIR=$withval/lib/mysql
            fi
            MYSQL_BINDIR=$withval/bin
         elif test -f $withval/include/mysql.h; then
            MYSQL_INCDIR=$withval/include
            if test -f $withval/lib64/libmysqlclient_r.a \
                 -o -f $withval/lib64/libmysqlclient_r.so; then
               MYSQL_LIBDIR=$withval/lib64
            else
               MYSQL_LIBDIR=$withval/lib
            fi
            MYSQL_BINDIR=$withval/bin
         else
            AC_MSG_RESULT(no)
            AC_MSG_ERROR(Invalid MySQL directory $withval - unable to find mysql.h under $withval)
         fi
      fi

      if test x${MYSQL_LIBDIR} != x; then
         MYSQL_INCLUDE=-I$MYSQL_INCDIR

         if test -f $MYSQL_LIBDIR/libmysqlclient_r.a \
              -o -f $MYSQL_LIBDIR/libmysqlclient_r.so; then
            if test x$use_libtool != xno; then
               MYSQL_LIBS="-R $MYSQL_LIBDIR -L$MYSQL_LIBDIR -lmysqlclient_r -lz"
            else
               MYSQL_LIBS="-L$MYSQL_LIBDIR -lmysqlclient_r -lz"
            fi
            DB_LIBS="${DB_LIBS} ${MYSQL_LIBS}"
            MYSQL_LIB=$MYSQL_LIBDIR/libmysqlclient_r.a
         else
            if test -f $MYSQL_LIBDIR/libmysqlclient.a \
                 -o -f $MYSQL_LIBDIR/libmysqlclient.so; then
               if test x$use_libtool != xno; then
                  MYSQL_LIBS="-R $MYSQL_LIBDIR -L$MYSQL_LIBDIR -lmysqlclient -lz"
               else
                  MYSQL_LIBS="-L$MYSQL_LIBDIR -lmysqlclient -lz"
               fi
               DB_LIBS="${DB_LIBS} ${MYSQL_LIBS}"
            fi
            MYSQL_LIB=$MYSQL_LIBDIR/libmysqlclient.a
         fi

         AC_DEFINE(HAVE_MYSQL, 1, [Set if you have an MySQL Database])
         AC_MSG_RESULT(yes)

         if test -z "${db_backends}" ; then
             db_backends="MySQL"
         else
            db_backends="${db_backends} MySQL"
         fi
         if test -z "${DB_BACKENDS}" ; then
             DB_BACKENDS="mysql"
         else
             DB_BACKENDS="${DB_BACKENDS} mysql"
         fi

         dnl -------------------------------------------
         dnl Check if mysql supports batch mode
         dnl -------------------------------------------
         if test "x$support_batch_insert" = "xyes"; then
             dnl For mysql checking
             saved_LDFLAGS="${LDFLAGS}"
             LDFLAGS="${saved_LDFLAGS} -L$MYSQL_LIBDIR"
             saved_LIBS="${LIBS}"
             LIBS="${saved_LIBS} -lz"

             if test -f $MYSQL_LIBDIR/libmysqlclient_r.so; then
                 AC_CHECK_LIB(mysqlclient_r, mysql_thread_safe, AC_DEFINE(HAVE_MYSQL_THREAD_SAFE, 1, [Define to 1 if you have the `mysql_thread_safe' function.]))

                 if test "x$ac_cv_lib_mysqlclient_r_mysql_thread_safe" = "xyes"; then
                     if test -z "${batch_insert_db_backends}"; then
                         batch_insert_db_backends="MySQL"
                     else
                         batch_insert_db_backends="${batch_insert_db_backends} MySQL"
                     fi
                 fi
             else
                 AC_CHECK_LIB(mysqlclient, mysql_thread_safe, AC_DEFINE(HAVE_MYSQL_THREAD_SAFE, 1, [Define to 1 if you have the `mysql_thread_safe' function.]))

                 if test "x$ac_cv_lib_mysqlclient_mysql_thread_safe" = "xyes"; then
                     if test -z "${batch_insert_db_backends}"; then
                         batch_insert_db_backends="MySQL"
                     else
                         batch_insert_db_backends="${batch_insert_db_backends} MySQL"
                     fi
                 fi
             fi

             dnl Revert after mysql checks
             LDFLAGS="${saved_LDFLAGS}"
             LIBS="${saved_LIBS}"
         fi
      else
         AC_MSG_RESULT(no)
      fi
   fi
],[
   AC_MSG_RESULT(no)
])

AC_MSG_CHECKING(for MySQL embedded support)
AC_ARG_WITH(embedded-mysql,
AC_HELP_STRING([--with-embedded-mysql@<:@=DIR@:>@], [Include MySQL support. DIR is the MySQL base install directory, default is to search through a number of common places for the MySQL files.]),
[
   if test "$withval" != "no"; then
      if test "$withval" = "yes"; then
         if test -f /usr/local/mysql/include/mysql/mysql.h; then
            MYSQL_INCDIR=/usr/local/mysql/include/mysql
            if test -d /usr/local/mysql/lib64/mysql; then
               MYSQL_LIBDIR=/usr/local/mysql/lib64/mysql
            else
               MYSQL_LIBDIR=/usr/local/mysql/lib/mysql
            fi
            MYSQL_BINDIR=/usr/local/mysql/bin
         elif test -f /usr/include/mysql/mysql.h; then
            MYSQL_INCDIR=/usr/include/mysql
            if test -d /usr/lib64/mysql; then
               MYSQL_LIBDIR=/usr/lib64/mysql
            else
               MYSQL_LIBDIR=/usr/lib/mysql
            fi
            MYSQL_BINDIR=/usr/bin
         elif test -f /usr/include/mysql.h; then
            MYSQL_INCDIR=/usr/include
            if test -d /usr/lib64; then
               MYSQL_LIBDIR=/usr/lib64
            else
               MYSQL_LIBDIR=/usr/lib
            fi
            MYSQL_BINDIR=/usr/bin
         elif test -f /usr/local/include/mysql/mysql.h; then
            MYSQL_INCDIR=/usr/local/include/mysql
            if test -d /usr/local/lib64/mysql; then
               MYSQL_LIBDIR=/usr/local/lib64/mysql
            else
               MYSQL_LIBDIR=/usr/local/lib/mysql
            fi
            MYSQL_BINDIR=/usr/local/bin
         elif test -f /usr/local/include/mysql.h; then
            MYSQL_INCDIR=/usr/local/include
            if test -d /usr/local/lib64; then
               MYSQL_LIBDIR=/usr/local/lib64
            else
               MYSQL_LIBDIR=/usr/local/lib
            fi
            MYSQL_BINDIR=/usr/local/bin
         else
            AC_MSG_RESULT(no)
            AC_MSG_ERROR(Unable to find mysql.h in standard locations)
         fi
      else
         if test -f $withval/include/mysql/mysql.h; then
            MYSQL_INCDIR=$withval/include/mysql
            if test -d $withval/lib64/mysql; then
               MYSQL_LIBDIR=$withval/lib64/mysql
            else
               MYSQL_LIBDIR=$withval/lib/mysql
            fi
            MYSQL_BINDIR=$withval/bin
         elif test -f $withval/include/mysql.h; then
            MYSQL_INCDIR=$withval/include
            if test -d $withval/lib64; then
               MYSQL_LIBDIR=$withval/lib64
            else
               MYSQL_LIBDIR=$withval/lib
            fi
            MYSQL_BINDIR=$withval/bin
         else
            AC_MSG_RESULT(no)
            AC_MSG_ERROR(Invalid MySQL directory $withval - unable to find mysql.h under $withval)
         fi
      fi

      if test x$MYSQL_LIBDIR != x; then
         MYSQL_INCLUDE=-I$MYSQL_INCDIR
         if test x$use_libtool != xno; then
            MYSQL_LIBS="-R $MYSQL_LIBDIR -L$MYSQL_LIBDIR -lmysqld -lz -lm -lcrypt"
         else
            MYSQL_LIBS="-L$MYSQL_LIBDIR -lmysqld -lz -lm -lcrypt"
         fi
         MYSQL_LIB=$MYSQL_LIBDIR/libmysqld.a
         DB_LIBS="${DB_LIBS} ${MYSQL_LIBS}"

         AC_DEFINE(HAVE_MYSQL, 1, [Set if you have an MySQL Database])
         AC_DEFINE(HAVE_EMBEDDED_MYSQL, 1, [Set if you have an Embedded MySQL Database])
         AC_MSG_RESULT(yes)

         if test -z "${db_backends}"; then
            db_backends="MySQL"
         else
            db_backends="${db_backends} MySQL"
         fi
         if test -z "${DB_BACKENDS}"; then
            DB_BACKENDS="mysql"
         else
            DB_BACKENDS="${DB_BACKENDS} mysql"
         fi

         dnl -------------------------------------------
         dnl Check if mysql supports batch mode
         dnl -------------------------------------------
         if test "x$support_batch_insert" = "xyes"; then
            dnl For mysql checking
            saved_LDFLAGS="${LDFLAGS}"
            LDFLAGS="${saved_LDFLAGS} -L$MYSQL_LIBDIR"
            saved_LIBS="${LIBS}"
            LIBS="${saved_LIBS} -lz -lm -lcrypt"

            AC_CHECK_LIB(mysqlclient_r, mysql_thread_safe, AC_DEFINE(HAVE_MYSQL_THREAD_SAFE, 1, [Set if have mysql_thread_safe]))
            if test "x$ac_cv_lib_mysqlclient_r_mysql_thread_safe" = "xyes"; then
               if test -z "${batch_insert_db_backends}"; then
                  batch_insert_db_backends="MySQL"
               else
                  batch_insert_db_backends="${batch_insert_db_backends} MySQL"
               fi
            fi

            dnl Revert after mysql checks
            LDFLAGS="${saved_LDFLAGS}"
            LIBS="${saved_LIBS}"
         fi
      fi
  else
     AC_MSG_RESULT(no)
  fi
],[
   AC_MSG_RESULT(no)
])

AC_SUBST(MYSQL_LIBS)
AC_SUBST(MYSQL_INCLUDE)
AC_SUBST(MYSQL_BINDIR)
])

AC_DEFUN([BA_CHECK_INGRES_DB],
[
AC_MSG_CHECKING(for Ingres support)
AC_ARG_WITH(ingres,
AC_HELP_STRING([--with-ingres@<:@=DIR@:>@], [Include Ingres support. DIR is the Ingres base install directory, default is to search through a number of common places for the Ingres files.]),
[
   if test "$withval" != "no"; then
      if test "$withval" = "yes"; then
         if test -f ${II_SYSTEM}/files/eqdefc.h; then
            INGRES_INCDIR=${II_SYSTEM}/files
            INGRES_LIBDIR=${II_SYSTEM}/lib
            INGRES_BINDIR=${II_SYSTEM}/bin
         elif test -f ${II_SYSTEM}/ingres/files/eqdefc.h; then
            INGRES_INCDIR=${II_SYSTEM}/ingres/files
            INGRES_LIBDIR=${II_SYSTEM}/ingres/lib
            INGRES_BINDIR=${II_SYSTEM}/ingres/bin
         else
            AC_MSG_RESULT(no)
            AC_MSG_ERROR(Unable to find eqdefc.h in standard locations)
         fi
      else
         if test -f $withval/files/eqdefc.h; then
            INGRES_INCDIR=$withval/files
            INGRES_LIBDIR=$withval/lib
            INGRES_BINDIR=$withval/bin
         else
            AC_MSG_RESULT(no)
            AC_MSG_ERROR(Invalid Ingres directory $withval - unable to find Ingres headers under $withval)
         fi
      fi

      if test x$INGRES_LIBDIR != x; then
         INGRES_INCLUDE=-I$INGRES_INCDIR
         if test x$use_libtool != xno; then
            INGRES_LIBS="-R $INGRES_LIBDIR -L$INGRES_LIBDIR -lq.1 -lcompat.1 -lframe.1"
         else
            INGRES_LIBS="-L$INGRES_LIBDIR -lq.1 -lcompat.1 -lframe.1"
         fi
         DB_LIBS="${DB_LIBS} ${INGRES_LIBS}"
         AC_DEFINE(HAVE_INGRES, 1, [Set if have Ingres Database])
         AC_MSG_RESULT(yes)

         if test -z "${db_backends}"; then
             db_backends="Ingres"
         else
            db_backends="${db_backends} Ingres"
         fi
         if test -z "${DB_BACKENDS}"; then
            DB_BACKENDS="ingres"
         else
            DB_BACKENDS="${DB_BACKENDS} ingres"
         fi
      fi
   else
      AC_MSG_RESULT(no)
   fi
],[
   AC_MSG_RESULT(no)
])

AC_SUBST(INGRES_LIBS)
AC_SUBST(INGRES_INCLUDE)
AC_SUBST(INGRES_BINDIR)
])

AC_DEFUN([BA_CHECK_SQLITE3_DB],
[
AC_MSG_CHECKING(for SQLite3 support)
AC_ARG_WITH(sqlite3,
AC_HELP_STRING([--with-sqlite3@<:@=DIR@:>@], [Include SQLite3 support. DIR is the SQLite3 base install directory, default is to search through a number of common places for the SQLite3 files.]),
[
   if test "$withval" != "no"; then
      if test "$withval" = "yes"; then
         if test -f /usr/local/include/sqlite3.h; then
            SQLITE_INCDIR=/usr/local/include
            if test -d /usr/local/lib64; then
               SQLITE_LIBDIR=/usr/local/lib64
            else
               SQLITE_LIBDIR=/usr/local/lib
            fi
            SQLITE_BINDIR=/usr/local/bin
         elif test -f /usr/include/sqlite3.h; then
            SQLITE_INCDIR=/usr/include
            if test -d /usr/lib64; then
               SQLITE_LIBDIR=/usr/lib64
            else
               SQLITE_LIBDIR=/usr/lib
            fi
            SQLITE_BINDIR=/usr/bin
         elif test -f $prefix/include/sqlite3.h; then
            SQLITE_INCDIR=$prefix/include
            if test -d $prefix/lib64; then
               SQLITE_LIBDIR=$prefix/lib64
            else
               SQLITE_LIBDIR=$prefix/lib
            fi
            SQLITE_BINDIR=$prefix/bin
         else
            AC_MSG_RESULT(no)
            AC_MSG_ERROR(Unable to find sqlite3.h in standard locations)
         fi
      else
         if test -f $withval/sqlite3.h; then
            SQLITE_INCDIR=$withval
            SQLITE_LIBDIR=$withval
            SQLITE_BINDIR=$withval
         elif test -f $withval/include/sqlite3.h; then
            SQLITE_INCDIR=$withval/include
            if test -d $withval/lib64; then
               SQLITE_LIBDIR=$withval/lib64
            else
               SQLITE_LIBDIR=$withval/lib
            fi
            SQLITE_BINDIR=$withval/bin
         else
            AC_MSG_RESULT(no)
            AC_MSG_ERROR(Invalid SQLite3 directory $withval - unable to find sqlite3.h under $withval)
         fi
      fi

      if test x$SQLITE_LIBDIR != x; then
         SQLITE_INCLUDE=-I$SQLITE_INCDIR
         if test x$use_libtool != xno; then
            SQLITE_LIBS="-R $SQLITE_LIBDIR -L$SQLITE_LIBDIR -lsqlite3"
         else
            SQLITE_LIBS="-L$SQLITE_LIBDIR -lsqlite3"
         fi
         SQLITE_LIB=$SQLITE_LIBDIR/libsqlite3.a
         DB_LIBS="${DB_LIBS} ${SQLITE_LIBS}"

         AC_DEFINE(HAVE_SQLITE3, 1, [Set if you have an SQLite3 Database])
         AC_MSG_RESULT(yes)

         if test -z "${db_backends}"; then
            db_backends="SQLite3"
         else
            db_backends="${db_backends} SQLite3"
         fi
         if test -z "${DB_BACKENDS}"; then
            DB_BACKENDS="sqlite3"
         else
            DB_BACKENDS="${DB_BACKENDS} sqlite3"
         fi

         dnl -------------------------------------------
         dnl Check if sqlite supports batch mode
         dnl -------------------------------------------
         if test "x$support_batch_insert" = "xyes"; then
            dnl For sqlite checking
            saved_LDFLAGS="${LDFLAGS}"
            LDFLAGS="${saved_LDFLAGS} -L$SQLITE_LIBDIR"

            AC_CHECK_LIB(sqlite3, sqlite3_threadsafe, AC_DEFINE(HAVE_SQLITE3_THREADSAFE, 1, [Define to 1 if you have the `sqlite3_threadsafe' function.]))
            if test "x$ac_cv_lib_sqlite3_sqlite3_threadsafe" = "xyes"; then
               if test -z "${batch_insert_db_backends}"; then
                  batch_insert_db_backends="SQLite3"
               else
                  batch_insert_db_backends="${batch_insert_db_backends} SQLite3"
               fi
            fi

            dnl Revert after sqlite checks
            LDFLAGS="${saved_LDFLAGS}"
         fi
      fi
   else
     AC_MSG_RESULT(no)
   fi
],[
   AC_MSG_RESULT(no)
])
AC_SUBST(SQLITE_LIBS)
AC_SUBST(SQLITE_INCLUDE)
AC_SUBST(SQLITE_BINDIR)
])

AC_DEFUN([BA_CHECK_POSTGRESQL_DB],
[
AC_MSG_CHECKING(for PostgreSQL support)
AC_ARG_WITH(postgresql,
AC_HELP_STRING([--with-postgresql@<:@=DIR@:>@], [Include PostgreSQL support. DIR is the PostgreSQL base install directory, @<:@default=/usr/local/pgsql@:>@]),
[
   if test "$withval" != "no"; then
      if test "$withval" = "yes"; then
         PG_CONFIG=`which pg_config 2>/dev/null`
         if test -n "$PG_CONFIG"; then
            POSTGRESQL_INCDIR=`"$PG_CONFIG" --includedir`
            POSTGRESQL_LIBDIR=`"$PG_CONFIG" --libdir`
            POSTGRESQL_BINDIR=`"$PG_CONFIG" --bindir`
         fi

         #
         # See if the pg_config gave something that is worth anything.
         #
         if test x${POSTGRESQL_LIBDIR} = x -o \
            ! \( -f ${POSTGRESQL_LIBDIR}/libpq.so -o \
                -f ${POSTGRESQL_LIBDIR}/libpq.a \); then
            if test -f /usr/local/include/libpq-fe.h; then
               POSTGRESQL_INCDIR=/usr/local/include
               if test -d /usr/local/lib64; then
                  POSTGRESQL_LIBDIR=/usr/local/lib64
               else
                  POSTGRESQL_LIBDIR=/usr/local/lib
               fi
               POSTGRESQL_BINDIR=/usr/local/bin
            elif test -f /usr/include/libpq-fe.h; then
               POSTGRESQL_INCDIR=/usr/include
               if test -d /usr/lib64; then
                  POSTGRESQL_LIBDIR=/usr/lib64
               else
                  POSTGRESQL_LIBDIR=/usr/lib
               fi
               POSTGRESQL_BINDIR=/usr/bin
            elif test -f /usr/include/pgsql/libpq-fe.h; then
               POSTGRESQL_INCDIR=/usr/include/pgsql
               if test -d /usr/lib64/pgsql; then
                  POSTGRESQL_LIBDIR=/usr/lib64/pgsql
               else
                  POSTGRESQL_LIBDIR=/usr/lib/pgsql
               fi
               POSTGRESQL_BINDIR=/usr/bin
            elif test -f /usr/include/postgresql/libpq-fe.h; then
               POSTGRESQL_INCDIR=/usr/include/postgresql
               if test -d /usr/lib64/postgresql; then
                  POSTGRESQL_LIBDIR=/usr/lib64/postgresql
               else
                  POSTGRESQL_LIBDIR=/usr/lib/postgresql
               fi
               POSTGRESQL_BINDIR=/usr/bin
            else
               AC_MSG_RESULT(no)
               AC_MSG_ERROR(Unable to find libpq-fe.h in standard locations)
            fi
         fi
      else
         if test -f $withval/include/libpq-fe.h; then
            POSTGRESQL_INCDIR=$withval/include
            POSTGRESQL_LIBDIR=$withval/lib
            POSTGRESQL_BINDIR=$withval/bin
         elif test -f $withval/include/postgresql/libpq-fe.h; then
            POSTGRESQL_INCDIR=$withval/include/postgresql
            if test -d $withval/lib64; then
               POSTGRESQL_LIBDIR=$withval/lib64
            else
               POSTGRESQL_LIBDIR=$withval/lib
            fi
            POSTGRESQL_BINDIR=$withval/bin
         else
            AC_MSG_RESULT(no)
            AC_MSG_ERROR(Invalid PostgreSQL directory $withval - unable to find libpq-fe.h under $withval)
         fi
      fi

      if test x$POSTGRESQL_LIBDIR != x; then
         AC_DEFINE(HAVE_POSTGRESQL, 1, [Set if you have an PostgreSQL Database])
         AC_MSG_RESULT(yes)

         POSTGRESQL_INCLUDE=-I$POSTGRESQL_INCDIR
         if test x$use_libtool != xno; then
            POSTGRESQL_LIBS="-R $POSTGRESQL_LIBDIR -L$POSTGRESQL_LIBDIR -lpq"
         else
            POSTGRESQL_LIBS="-L$POSTGRESQL_LIBDIR -lpq"
         fi
         AC_CHECK_FUNC(crypt, , AC_CHECK_LIB(crypt, crypt, [POSTGRESQL_LIBS="$POSTGRESQL_LIBS -lcrypt"]))
         POSTGRESQL_LIB=$POSTGRESQL_LIBDIR/libpq.a
         DB_LIBS="${DB_LIBS} ${POSTGRESQL_LIBS}"

         if test -z "${db_backends}"; then
            db_backends="PostgreSQL"
         else
            db_backends="${db_backends} PostgreSQL"
         fi
         if test -z "${DB_BACKENDS}"; then
            DB_BACKENDS="postgresql"
         else
            DB_BACKENDS="${DB_BACKENDS} postgresql"
         fi

         dnl -------------------------------------------
         dnl Check if postgresql supports batch mode
         dnl -------------------------------------------
         if test "x$support_batch_insert" = "xyes"; then
            dnl For postgresql checking
            saved_LDFLAGS="${LDFLAGS}"
            LDFLAGS="${saved_LDFLAGS} -L$POSTGRESQL_LIBDIR"
            saved_LIBS="${LIBS}"
            if test "x$ac_cv_lib_crypt_crypt" = "xyes" ; then
               LIBS="${saved_LIBS} -lcrypt"
            fi

            AC_CHECK_LIB(pq, PQisthreadsafe, AC_DEFINE(HAVE_PQISTHREADSAFE, 1, [Set if have PQisthreadsafe]))
            AC_CHECK_LIB(pq, PQputCopyData, AC_DEFINE(HAVE_PQ_COPY, 1, [Set if have PQputCopyData]))
            if test "x$ac_cv_lib_pq_PQputCopyData" = "xyes"; then
               if test $support_batch_insert = yes ; then
                  AC_DEFINE(HAVE_POSTGRESQL_BATCH_FILE_INSERT, 1, [Set if PostgreSQL DB batch insert code enabled])
                  if test -z "${batch_insert_db_backends}"; then
                     batch_insert_db_backends="PostgreSQL"
                  else
                     batch_insert_db_backends="${batch_insert_db_backends} PostgreSQL"
                  fi
               fi
            fi

            if test x$ac_cv_lib_pq_PQisthreadsafe != xyes -a x$support_batch_insert = xyes; then
                 echo "WARNING: Your PostgreSQL client library is too old to detect "
                 echo "if it was compiled with --enable-thread-safety, consider to "
                 echo "upgrade it in order to avoid problems with Batch insert mode"
            fi

            dnl Revert after postgresql checks
            LDFLAGS="${saved_LDFLAGS}"
            LIBS="${saved_LIBS}"
         fi
      fi
   else
      AC_MSG_RESULT(no)
   fi
],[
   AC_MSG_RESULT(no)
])
AC_SUBST(POSTGRESQL_LIBS)
AC_SUBST(POSTGRESQL_INCLUDE)
AC_SUBST(POSTGRESQL_BINDIR)
])

AC_DEFUN([AM_CONDITIONAL],
[AC_SUBST($1_TRUE)
AC_SUBST($1_FALSE)
if $2; then
  $1_TRUE=
  $1_FALSE='#'
else
  $1_TRUE='#'
  $1_FALSE=
fi])
