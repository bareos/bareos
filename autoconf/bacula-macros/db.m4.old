AC_DEFUN([BA_CHECK_DBI_DB],
[
db_found=no
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
     SQL_INCLUDE=-I$DBI_INCDIR
     SQL_LFLAGS="-L$DBI_LIBDIR -ldbi"
     SQL_BINDIR=$DBI_BINDIR
     SQL_LIB=$DBI_LIBDIR/libdbi.a
     DBI_DBD_DRIVERDIR="-D DBI_DRIVER_DIR=\\\"$DRIVERDIR\\\""

     AC_DEFINE(HAVE_DBI, 1, [Set if you have the DBI driver])
     AC_MSG_RESULT(yes)
     db_found=yes
     support_dbi=yes
     db_type=DBI
     DB_TYPE=dbi
     uncomment_dbi=" "

  else
     AC_MSG_RESULT(no)
  fi
],[
  AC_MSG_RESULT(no)
])
AC_SUBST(SQL_LFLAGS)
AC_SUBST(SQL_INCLUDE)
AC_SUBST(SQL_BINDIR)
AC_SUBST(DBI_DBD_DRIVERDIR)  
AC_SUBST(uncomment_dbi)

])

AC_DEFUN([BA_CHECK_DBI_DRIVER],
[
db_found=no
db_prog=no
AC_MSG_CHECKING(for DBI drivers support)
AC_ARG_WITH(dbi-driver,
AC_HELP_STRING([--with-dbi-driver@<:@=DRIVER@:>@], [Suport for DBI driver. DRIVER is the one DBI driver like Mysql, Postgresql, others. Default is to not configure any driver.]),
[
  if test "$withval" != "no"; then
     case $withval in
        "mysql")
           db_prog="mysql"
           if test -f /usr/local/mysql/bin/mysql; then
              SQL_BINDIR=/usr/local/mysql/bin
              if test -f /usr/local/mysql/lib64/mysql/libmysqlclient_r.a \
                 -o -f /usr/local/mysql/lib64/mysql/libmysqlclient_r.so; then
                 SQL_LIBDIR=/usr/local/mysql/lib64/mysql
              else
                 SQL_LIBDIR=/usr/local/mysql/lib/mysql
              fi
           elif test -f /usr/bin/mysql; then
              SQL_BINDIR=/usr/bin
              if test -f /usr/lib64/mysql/libmysqlclient_r.a \
                  -o -f /usr/lib64/mysql/libmysqlclient_r.so; then  
                  SQL_LIBDIR=/usr/lib64/mysql
              elif test -f /usr/lib/mysql/libmysqlclient_r.a \
                  -o -f /usr/lib/mysql/libmysqlclient_r.so; then
                  SQL_LIBDIR=/usr/lib/mysql
              else
                  SQL_LIBDIR=/usr/lib
              fi
           elif test -f /usr/local/bin/mysql; then
              SQL_BINDIR=/usr/local/bin
              if test -f /usr/local/lib64/mysql/libmysqlclient_r.a \
                  -o -f /usr/local/lib64/mysql/libmysqlclient_r.so; then  
                  SQL_LIBDIR=/usr/local/lib64/mysql
              elif test -f /usr/local/lib/mysql/libmysqlclient_r.a \
                  -o -f /usr/local/lib/mysql/libmysqlclient_r.so; then
                  SQL_LIBDIR=/usr/local/lib/mysql
              else
                  SQL_LIBDIR=/usr/local/lib
              fi
           elif test -f $withval/bin/mysql; then
              SQL_BINDIR=$withval/bin
              if test -f $withval/lib64/mysql/libmysqlclient_r.a \
                  -o -f $withval/lib64/mysql/libmysqlclient_r.so; then
                  SQL_LIBDIR=$withval/lib64/mysql
              elif test -f $withval/lib64/libmysqlclient_r.a \
                  -o -f $withval/lib64/libmysqlclient_r.so; then
                  SQL_LIBDIR=$withval/lib64
              elif test -f $withval/lib/libmysqlclient_r.a \
                  -o -f $withval/lib/libmysqlclient_r.so; then
                  SQL_LIBDIR=$withval/lib/
              else
                  SQL_LIBDIR=$withval/lib/mysql
              fi
           else
              AC_MSG_RESULT(no)
              AC_MSG_ERROR(Unable to find mysql in standard locations)
           fi
           if test -f $SQL_LIBDIR/libmysqlclient_r.so; then
              DB_PROG_LIB=$SQL_LIBDIR/libmysqlclient_r.so
           else
              DB_PROG_LIB=$SQL_LIBDIR/libmysqlclient_r.a
           fi
        ;;
        "postgresql")
           db_prog="postgresql"
           PG_CONFIG=`which pg_config`
           if test -n "$PG_CONFIG"; then
              SQL_BINDIR=`"$PG_CONFIG" --bindir`
              SQL_LIBDIR=`"$PG_CONFIG" --libdir`
           elif test -f /usr/local/bin/psql; then
              SQL_BINDIR=/usr/local/bin
              if test -d /usr/local/lib64; then
                 SQL_LIBDIR=/usr/local/lib64
              else
                 SQL_LIBDIR=/usr/local/lib
              fi
           elif test -f /usr/bin/psql; then
              SQL_BINDIR=/usr/local/bin
              if test -d /usr/lib64/postgresql; then
                 SQL_LIBDIR=/usr/lib64/postgresql
              elif test -d /usr/lib/postgresql; then
                 SQL_LIBDIR=/usr/lib/postgresql
              elif test -d /usr/lib64; then
                 SQL_LIBDIR=/usr/lib64
              else
                 SQL_LIBDIR=/usr/lib
              fi
           elif test -f $withval/bin/psql; then
              SQL_BINDIR=$withval/bin
              if test -d $withval/lib64; then
                 SQL_LIBDIR=$withval/lib64
              else
                 SQL_LIBDIR=$withval/lib
              fi
           else
              AC_MSG_RESULT(no)
              AC_MSG_ERROR(Unable to find psql in standard locations)
           fi
           if test -f $SQL_LIBDIR/libpq.so; then
              DB_PROG_LIB=$SQL_LIBDIR/libpq.so
           else
              DB_PROG_LIB=$SQL_LIBDIR/libpq.a
           fi
        ;;
        "sqlite")
           db_prog="sqlite"
           if test -f /usr/local/bin/sqlite; then
              SQL_BINDIR=/usr/local/bin
              if test -d /usr/local/lib64; then
                 SQL_LIBDIR=/usr/local/lib64
              else
                 SQL_LIBDIR=/usr/local/lib
              fi
           elif test -f /usr/bin/sqlite; then
              SQL_BINDIR=/usr/bin
              if test -d /usr/lib64; then
                 SQL_LIBDIR=/usr/lib64
              else
                 SQL_LIBDIR=/usr/lib
              fi                 
           elif test -f $withval/bin/sqlite; then
              SQL_BINDIR=$withval/bin
              if test -d $withval/lib64; then
                 SQL_LIBDIR=$withval/lib64
              else
                 SQL_LIBDIR=$withval/lib
              fi                 
           else
              AC_MSG_RESULT(no)
              AC_MSG_ERROR(Unable to find sqlite in standard locations)
           fi
           if test -f $SQL_LIBDIR/libsqlite.so; then
              DB_PROG_LIB=$SQL_LIBDIR/libsqlite.so
           else
              DB_PROG_LIB=$SQL_LIBDIR/libsqlite.a
           fi
        ;;
        "sqlite3")
           db_prog="sqlite3"
           if test -f /usr/local/bin/sqlite3; then
              SQL_BINDIR=/usr/local/bin
              if test -d /usr/local/lib64; then
                 SQL_LIBDIR=/usr/local/lib64
              else
                 SQL_LIBDIR=/usr/local/lib
              fi
           elif test -f /usr/bin/sqlite3; then
              SQL_BINDIR=/usr/bin
              if test -d /usr/lib64; then
                 SQL_LIBDIR=/usr/lib64
              else
                 SQL_LIBDIR=/usr/lib
              fi                 
           elif test -f $withval/bin/sqlite3; then
              SQL_BINDIR=$withval/bin
              if test -d $withval/lib64; then
                 SQL_LIBDIR=$withval/lib64
              else
                 SQL_LIBDIR=$withval/lib
              fi                 
           else
              AC_MSG_RESULT(no)
              AC_MSG_ERROR(Unable to find sqlite in standard locations)
           fi
           if test -f $SQL_LIBDIR/libsqlite3.so; then
              DB_PROG_LIB=$SQL_LIBDIR/libsqlite3.so
           else
              DB_PROG_LIB=$SQL_LIBDIR/libsqlite3.a
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
AC_SUBST(SQL_BINDIR)
AC_SUBST(DB_PROG)
AC_SUBST(DB_PROG_LIB)

])


AC_DEFUN([BA_CHECK_MYSQL_DB],
[
db_found=no
AC_MSG_CHECKING(for MySQL support)
AC_ARG_WITH(mysql,
AC_HELP_STRING([--with-mysql@<:@=DIR@:>@], [Include MySQL support. DIR is the MySQL base install directory, default is to search through a number of common places for the MySQL files.]),
[
  if test "$withval" != "no"; then
        if test "$withval" = "yes"; then
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
    SQL_INCLUDE=-I$MYSQL_INCDIR
    if test -f $MYSQL_LIBDIR/libmysqlclient_r.a \
         -o -f $MYSQL_LIBDIR/libmysqlclient_r.so; then
       SQL_LFLAGS="-L$MYSQL_LIBDIR -lmysqlclient_r -lz"
       AC_DEFINE(HAVE_THREAD_SAFE_MYSQL)
    fi
    SQL_BINDIR=$MYSQL_BINDIR
    SQL_LIB=$MYSQL_LIBDIR/libmysqlclient_r.a

    AC_DEFINE(HAVE_MYSQL, 1, [Set if you have an MySQL Database])
    AC_MSG_RESULT(yes)
    db_found=yes
    support_mysql=yes
    db_type=MySQL
    DB_TYPE=mysql

  else
        AC_MSG_RESULT(no)
  fi
]
)

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
    SQL_INCLUDE=-I$MYSQL_INCDIR
    SQL_LFLAGS="-L$MYSQL_LIBDIR -lmysqld -lz -lm -lcrypt"
    SQL_BINDIR=$MYSQL_BINDIR
    SQL_LIB=$MYSQL_LIBDIR/libmysqld.a

    AC_DEFINE(HAVE_MYSQL)
    AC_DEFINE(HAVE_EMBEDDED_MYSQL)
    AC_MSG_RESULT(yes)
    db_found=yes
    support_mysql=yes
    db_type=MySQL
    DB_TYPE=mysql

  else
        AC_MSG_RESULT(no)
  fi
]
)


AC_SUBST(SQL_LFLAGS)
AC_SUBST(SQL_INCLUDE)
AC_SUBST(SQL_BINDIR)
  
])


AC_DEFUN([BA_CHECK_INGRES_DB],
[
db_found=no
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
     SQL_INCLUDE=-I$INGRES_INCDIR
     SQL_LFLAGS="-L$INGRES_LIBDIR -lq.1 -lcompat.1 -lframe.1"
     SQL_BINDIR=$INGRES_BINDIR
     SQL_LIB=$INGRES_LIBDIR/libingres.a
     AC_DEFINE(HAVE_INGRES, 1, [Set if have Ingres Database])
     AC_MSG_RESULT(yes)
     db_found=yes
     support_ingres=yes
     db_type=Ingres
     DB_TYPE=ingres

  else
     AC_MSG_RESULT(no)
  fi
],[
  AC_MSG_RESULT(no)
])

AC_SUBST(SQL_LFLAGS)
AC_SUBST(SQL_INCLUDE)
AC_SUBST(SQL_BINDIR)
])

AC_DEFUN([BA_CHECK_SQLITE3_DB],
[
db_found=no
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
     SQL_INCLUDE=-I$SQLITE_INCDIR
     SQL_LFLAGS="-L$SQLITE_LIBDIR -lsqlite3"
     SQL_BINDIR=$SQLITE_BINDIR
     SQL_LIB=$SQLITE_LIBDIR/libsqlite3.a

     AC_DEFINE(HAVE_SQLITE3)
     AC_MSG_RESULT(yes)
     db_found=yes
     support_sqlite3=yes
     db_type=SQLite3
     DB_TYPE=sqlite3

  else
     AC_MSG_RESULT(no)
  fi
],[
  AC_MSG_RESULT(no)
])
AC_SUBST(SQL_LFLAGS)
AC_SUBST(SQL_INCLUDE)
AC_SUBST(SQL_BINDIR)
  
])


AC_DEFUN([BA_CHECK_POSTGRESQL_DB],
[
db_found=no
AC_MSG_CHECKING(for PostgreSQL support)
AC_ARG_WITH(postgresql,
AC_HELP_STRING([--with-postgresql@<:@=DIR@:>@], [Include PostgreSQL support. DIR is the PostgreSQL base install directory, @<:@default=/usr/local/pgsql@:>@]),
[
  if test "$withval" != "no"; then
      if test "$db_found" = "yes"; then
          AC_MSG_RESULT(error)
          AC_MSG_ERROR("You can configure for only one database.");
      fi
      if test "$withval" = "yes"; then
          PG_CONFIG=`which pg_config`
          if test -n "$PG_CONFIG";then
              POSTGRESQL_INCDIR=`"$PG_CONFIG" --includedir`
              POSTGRESQL_LIBDIR=`"$PG_CONFIG" --libdir`
              POSTGRESQL_BINDIR=`"$PG_CONFIG" --bindir`
          elif test -f /usr/local/include/libpq-fe.h; then
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
      elif test -f $withval/include/libpq-fe.h; then
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
      AC_DEFINE(HAVE_POSTGRESQL)
      AC_MSG_RESULT(yes)
      POSTGRESQL_LFLAGS="-L$POSTGRESQL_LIBDIR -lpq"
      AC_CHECK_FUNC(crypt, , AC_CHECK_LIB(crypt, crypt, [POSTGRESQL_LFLAGS="$POSTGRESQL_LFLAGS -lcrypt"]))
      SQL_INCLUDE=-I$POSTGRESQL_INCDIR
      SQL_LFLAGS=$POSTGRESQL_LFLAGS
      SQL_BINDIR=$POSTGRESQL_BINDIR
      SQL_LIB=$POSTGRESQL_LIBDIR/libpq.a

      db_found=yes
      support_postgresql=yes
      db_type=PostgreSQL
      DB_TYPE=postgresql
  else
      AC_MSG_RESULT(no)
  fi
],[
  AC_MSG_RESULT(no)
])
AC_SUBST(SQL_LFLAGS)
AC_SUBST(SQL_INCLUDE)
AC_SUBST(SQL_BINDIR)

])



AC_DEFUN([BA_CHECK_SQL_DB], 
[AC_MSG_CHECKING(Checking for various databases)
dnl# --------------------------------------------------------------------------
dnl# CHECKING FOR VARIOUS DATABASES (thanks to UdmSearch team)
dnl# --------------------------------------------------------------------------
dnl Check for some DBMS backend
dnl NOTE: we can use only one backend at a time
db_found=no
DB_TYPE=none

if test x$support_mysql = xyes; then
   cats=cats
fi

AC_MSG_CHECKING(for Berkeley DB support)
AC_ARG_WITH(berkeleydb,
AC_HELP_STRING([--with-berkeleydb@<:@=DIR@:>@], [Include Berkeley DB support. DIR is the Berkeley DB base install directory, default is to search through a number of common places for the DB files.]),
[
  if test "$withval" != "no"; then
        if test "$withval" = "yes"; then
                if test -f /usr/include/db.h; then
                        BERKELEYDB_INCDIR=/usr/include
                        if test -d /usr/lib64; then
                                BERKELEYDB_LIBDIR=/usr/lib64
                        else
                                BERKELEYDB_LIBDIR=/usr/lib
                        fi
                else
                AC_MSG_RESULT(no)
                AC_MSG_ERROR(Invalid Berkeley DB directory - unable to find db.h)
                fi
        else
                if test -f $withval/include/db.h; then
                        BERKELEYDB_INCDIR=$withval/include
                        if test -d $withval/lib64; then
                                BERKELEYDB_LIBDIR=$withval/lib64
                        else
                                BERKELEYDB_LIBDIR=$withval/lib
                        fi
                else
                AC_MSG_RESULT(no)
                AC_MSG_ERROR(Invalid Berkeley DB directory - unable to find db.h under $withval)
                fi
        fi
    SQL_INCLUDE=-I$BERKELEYDB_INCDIR
    SQL_LFLAGS="-L$BERKELEYDB_LIBDIR -ldb"

    AC_DEFINE(HAVE_BERKELEY_DB)
    AC_MSG_RESULT(yes)
    have_db=yes
    support_mysql=yes
    DB_TYPE=BerkelyDB

  else
        AC_MSG_RESULT(no)
  fi
],[
  AC_MSG_RESULT(no)
])
AC_SUBST(SQL_LFLAGS)
AC_SUBST(SQL_INCLUDE)

if test x$support_berkleydb = xyes; then
   cats=cats
fi




AC_MSG_CHECKING(for mSQL support)
AC_ARG_WITH(msql,
AC_HELP_STRING([--with-msql@<:@=DIR@:>@], [Include mSQL support. DIR is the mSQL base install directory @<:@default=/usr/local/Hughes@:>@]),
[
  if test "$withval" != "no"; then
    if test "$have_db" = "yes"; then
        AC_MSG_RESULT(error)
        AC_MSG_ERROR("You can configure for only one database.");
    fi

    if test "$withval" = "yes"; then
        MSQL_INCDIR=/usr/local/Hughes/include
        if test -d /usr/local/Hughes/lib64; then
            MSQL_LIBDIR=/usr/local/Hughes/lib64
        else
            MSQL_LIBDIR=/usr/local/Hughes/lib
        fi
    else
        MSQL_INCDIR=$withval/include
        if test -d $withval/lib64; then
            MSQL_LIBDIR=$withval/lib64
        else
            MSQL_LIBDIR=$withval/lib
        fi
    fi
    MSQL_INCLUDE=-I$MSQL_INCDIR
    MSQL_LFLAGS="-L$MSQL_LIBDIR -lmsql"

    AC_DEFINE(HAVE_MSQL)
    AC_MSG_RESULT(yes)
    have_db=yes
  else
        AC_MSG_RESULT(no)
  fi
],[
  AC_MSG_RESULT(no)
])
AC_SUBST(MSQL_LFLAGS)
AC_SUBST(MSQL_INCLUDE)


AC_MSG_CHECKING(for iODBC support)
AC_ARG_WITH(iodbc,
AC_HELP_STRING([--with-iodbc@<:@=DIR@:>], [Include iODBC support. DIR is the iODBC base install directory @<:@default=/usr/local@:>@]),
[
        if test "$withval" != "no"; then
            if test "$have_db" = "yes"; then
                AC_MSG_RESULT(error)
                AC_MSG_ERROR("You can configure for only one database.");
            fi
        fi
        
        if test "$withval" = "yes"; then
                withval=/usr/local
        fi

        if test "$withval" != "no"; then
                if test -f $withval/include/isql.h; then
                        IODBC_INCDIR=$withval/include
                        if test -d $withval/lib64; then
                                IODBC_LIBDIR=$withval/lib64
                        else
                                IODBC_LIBDIR=$withval/lib
                        fi
                else
                        AC_MSG_RESULT(no)
                        AC_MSG_ERROR(Invalid iODBC directory - unable to find isql.h)
                fi
                IODBC_LFLAGS="-L$IODBC_LIBDIR -liodbc"
                IODBC_INCLUDE=-I$IODBC_INCDIR
                AC_DEFINE(HAVE_IODBC)
                AC_MSG_RESULT(yes)
                have_db=yes
        fi
],[
        AC_MSG_RESULT(no)
])
AC_SUBST(IODBC_LFLAGS)
AC_SUBST(IODBC_INCLUDE)


AC_MSG_CHECKING(for unixODBC support)
AC_ARG_WITH(unixODBC,
AC_HELP_STRING([--with-unixODBC@<:@=DIR@:>], [Include unixODBC support. DIR is the unixODBC base install directory @<:@default=/usr/local@:>@]),
[
        if test "$withval" != "no"; then
            if test "$have_db" = "yes"; then
                AC_MSG_RESULT(error)
                AC_MSG_ERROR("You can configure for only one database.");
            fi
        fi
        
        if test "$withval" = "yes"; then
                withval=/usr/local
        fi

        if test "$withval" != "no"; then
                if test -f $withval/include/sql.h; then
                        UNIXODBC_INCDIR=$withval/include
                        if test -d $withval/lib64; then
                                UNIXODBC_LIBDIR=$withval/lib64
                        else
                                UNIXODBC_LIBDIR=$withval/lib
                        fi
                else
                        AC_MSG_RESULT(no)
                        AC_MSG_ERROR(Invalid unixODBC directory - unable to find sql.h)
                fi
                UNIXODBC_LFLAGS="-L$UNIXODBC_LIBDIR -lodbc"
                UNIXODBC_INCLUDE=-I$UNIXODBC_INCDIR
                AC_DEFINE(HAVE_UNIXODBC)
                AC_MSG_RESULT(yes)
                have_db=yes
        fi
],[
        AC_MSG_RESULT(no)
])
AC_SUBST(UNIXODBC_LFLAGS)
AC_SUBST(UNIXODBC_INCLUDE)


AC_MSG_CHECKING(for Solid support)
AC_ARG_WITH(solid,
AC_HELP_STRING([--with-solid@<:@=DIR@:>], [Include Solid support. DIR is the Solid base install directory @<:@default=/usr/local@:>@]),
[
        if test "$withval" != "no"; then
            if test "$have_db" = "yes"; then
                AC_MSG_RESULT(error)
                AC_MSG_ERROR("You can configure for only one database.");
            fi
        fi

        if test "$withval" = "yes"; then
                withval=/usr/local
        fi

        if test "$withval" != "no"; then
                if test -f $withval/include/cli0cli.h; then
                        SOLID_INCDIR=$withval/include
                        if test -d $withval/lib64; then
                                SOLID_LIBDIR=$withval/lib64
                        else
                                SOLID_LIBDIR=$withval/lib
                        fi
                else
                        AC_MSG_RESULT(no)
                        AC_MSG_ERROR(Invalid Solid directory - unable to find cli0cli.h)
                fi
                SOLID_LFLAGS="-L$SOLID_LIBDIR -lsolcli"
                SOLID_INCLUDE="-I$SOLID_INCDIR"
                AC_DEFINE(HAVE_SOLID)
                AC_MSG_RESULT(yes)
                have_db=yes
        fi
],[
        AC_MSG_RESULT(no)
])
AC_SUBST(SOLID_LFLAGS)
AC_SUBST(SOLID_INCLUDE)

AC_MSG_CHECKING(for OpenLink ODBC support)
AC_ARG_WITH(openlink,
AC_HELP_STRING([--with-openlink@<:@=DIR@:>], [Include OpenLink ODBC support. DIR is the base OpenLink ODBC install directory]),
[
  if test "$withval" != "no"; then
        if test "$withval" = "yes"; then

                if test "$have_db" = "yes"; then
                        AC_MSG_RESULT(error)
                        AC_MSG_ERROR("You can configure for only one database.");
                fi

                if test -f /usr/local/virtuoso-ent/odbcsdk/include/isql.h; then
                        VIRT_INCDIR=/usr/local/virtuoso-ent/odbcsdk/include/
                        if test -d /usr/local/virtuoso-ent/odbcsdk/lib64/; then
                                VIRT_LIBDIR=/usr/local/virtuoso-ent/odbcsdk/lib64/
                        else
                                VIRT_LIBDIR=/usr/local/virtuoso-ent/odbcsdk/lib/
                        fi
                elif test -f /usr/local/virtuoso-lite/odbcsdk/include/isql.h; then
                        VIRT_INCDIR=/usr/local/virtuoso-lite/odbcsdk/include/
                        if test -d /usr/local/virtuoso-lite/odbcsdk/lib64/; then
                                VIRT_LIBDIR=/usr/local/virtuoso-lite/odbcsdk/lib64/
                        else
                                VIRT_LIBDIR=/usr/local/virtuoso-lite/odbcsdk/lib/
                        fi
                elif test -f /usr/local/virtuoso/odbcsdk/include/isql.h; then
                        VIRT_INCDIR=/usr/local/virtuoso/odbcsdk/include/
                        if test -d /usr/local/virtuoso/odbcsdk/lib64/; then
                                VIRT_LIBDIR=/usr/local/virtuoso/odbcsdk/lib64/
                        else
                                VIRT_LIBDIR=/usr/local/virtuoso/odbcsdk/lib/
                        fi
                else
                AC_MSG_RESULT(no)
                AC_MSG_ERROR(Invalid OpenLink ODBC directory - unable to find isql.h)
                fi
        else
                if test -f $withval/odbcsdk/include/isql.h; then
                        VIRT_INCDIR=$withval/odbcsdk/include/
                        if test -d $withval/odbcsdk/lib64/; then
                                VIRT_LIBDIR=$withval/odbcsdk/lib64/
                        else
                                VIRT_LIBDIR=$withval/odbcsdk/lib/
                        fi
                elif test -f $withval/include/isql.h; then
                        VIRT_INCDIR=$withval/include/
                        if test -d $withval/lib64/; then
                                VIRT_LIBDIR=$withval/lib64/
                        else
                                VIRT_LIBDIR=$withval/lib/
                        fi
                else
                AC_MSG_RESULT(no)
                AC_MSG_ERROR(Invalid OpenLink ODBC directory - unable to find isql.h under $withval)
                fi
        fi
    VIRT_INCLUDE=-I$VIRT_INCDIR
    VIRT_LFLAGS="-L$VIRT_LIBDIR -liodbc"

    AC_DEFINE(HAVE_VIRT)
    AC_MSG_RESULT(yes)
    have_db=yes

  else
        AC_MSG_RESULT(no)
  fi
],[
  AC_MSG_RESULT(no)
])
AC_SUBST(VIRT_LFLAGS)
AC_SUBST(VIRT_INCLUDE)


AC_MSG_CHECKING(for EasySoft ODBC support)
AC_ARG_WITH(easysoft,
AC_HELP_STRING([--with-easysoft@<:@=DIR@:>], [Include EasySoft ODBC support. DIR is the base EasySoft ODBC install directory]),
[
  if test "$withval" != "no"; then
        if test "$withval" = "yes"; then

                if test "$have_db" = "yes"; then
                        AC_MSG_RESULT(error)
                        AC_MSG_ERROR("You can configure for only one database.");
                fi

                if test -f /usr/local/easysoft/oob/client/include/sql.h; then
                        EASYSOFT_INCDIR=/usr/local/easysoft/oob/client/include/
                        if test -d /usr/local/easysoft/oob/client/lib64/; then
                                EASYSOFT_LFLAGS="-L/usr/local/easysoft/oob/client/lib64/ -L/usr/local/easysoft/lib64"
                        else
                                EASYSOFT_LFLAGS="-L/usr/local/easysoft/oob/client/lib/ -L/usr/local/easysoft/lib"
                        fi
                else
                AC_MSG_RESULT(no)
                AC_MSG_ERROR(Invalid EasySoft ODBC directory - unable to find sql.h)
                fi
        else
                if test -f $withval/easysoft/oob/client/include/sql.h; then
                        EASYSOFT_INCDIR=$withval/easysoft/oob/client/include/
                        if test -d $withval/easysoft/oob/client/lib64/; then
                                EASYSOFT_LFLAGS="-L$withval/easysoft/oob/client/lib64/ -L$withval/easysoft/lib64"
                        else
                                EASYSOFT_LFLAGS="-L$withval/easysoft/oob/client/lib/ -L$withval/easysoft/lib"
                        fi
                else
                AC_MSG_RESULT(no)
                AC_MSG_ERROR(Invalid EasySoft ODBC directory - unable to find sql.h under $withval)
                fi
        fi
    EASYSOFT_INCLUDE=-I$EASYSOFT_INCDIR
    EASYSOFT_LFLAGS="$EASYSOFT_LFLAGS -lesoobclient -lesrpc -lsupport -lextra"

    AC_DEFINE(HAVE_EASYSOFT)
    AC_MSG_RESULT(yes)
    have_db=yes

  else
        AC_MSG_RESULT(no)
  fi
],[
  AC_MSG_RESULT(no)
])
AC_SUBST(EASYSOFT_LFLAGS)
AC_SUBST(EASYSOFT_INCLUDE)



AC_MSG_CHECKING(for InterBase support)
AC_ARG_WITH(ibase,
AC_HELP_STRING([--with-ibase@<:@=DIR@:>@], [Include InterBase support. DIR is the InterBase install directory @<:@default=/usr/interbase@:>@]),
[
        if test "$withval" != "no"; then
            if test "$have_db" = "yes"; then
                AC_MSG_RESULT(error)
                AC_MSG_ERROR("You can configure for only one database.");
            fi
        fi
        
        if test "$withval" = "yes"; then
                withval=/usr/interbase
        fi

        if test "$withval" != "no"; then
                if test -f $withval/include/ibase.h; then
                        IBASE_INCDIR=$withval/include
                        if test -d $withval/lib64; then
                                IBASE_LIBDIR=$withval/lib64
                        else
                                IBASE_LIBDIR=$withval/lib
                        fi
                else
                        AC_MSG_RESULT(no)
                        AC_MSG_ERROR(Invalid InterBase directory - unable to find ibase.h)
                fi
                IBASE_LFLAGS="-L$IBASE_LIBDIR -lgds"
                IBASE_INCLUDE=-I$IBASE_INCDIR
                AC_DEFINE(HAVE_IBASE)
                AC_MSG_RESULT(yes)
                have_db=yes
        fi
],[
        AC_MSG_RESULT(no)
])
AC_SUBST(IBASE_LFLAGS)
AC_SUBST(IBASE_INCLUDE)

AC_MSG_CHECKING(for Oracle8 support)
AC_ARG_WITH(oracle8,
AC_HELP_STRING([--with-oracle8@<:@=DIR@:>@], [Include Oracle8 support. DIR is the Oracle home directory @<:@default=$ORACLE_HOME or /oracle8/app/oracle/product/8.0.5@:>@]),
[
        if test "$withval" != "no"; then
            if test "$have_db" = "yes"; then
                AC_MSG_RESULT(error)
                AC_MSG_ERROR("You can configure for only one database.");
            fi
        fi

        if test "$withval" = "yes"; then
                withval="$ORACLE_HOME"
                if test "$withval" = ""; then
                        withval=/oracle8/app/oracle/product/8.0.5
                fi
        fi

        if test "$withval" != "no"; then
                if test -f $withval/rdbms/demo/oci.h; then
                        ORACLE8_INCDIR1=$withval/rdbms/demo/
                        ORACLE8_INCDIR2=$withval/rdbms/public/:
                        ORACLE8_INCDIR3=$withval/network/public/
                        ORACLE8_INCDIR4=$withval/plsql/public/
                        if test -d $withval/lib64; then
                                ORACLE8_LIBDIR1=$withval/lib64
                        else
                                ORACLE8_LIBDIR1=$withval/lib
                        fi
                        if test -d $withval/rdbms/lib64; then
                               ORACLE8_LIBDIR2=$withval/rdbms/lib64
                        else
                                ORACLE8_LIBDIR2=$withval/rdbms/lib
                        fi
                else
                        AC_MSG_RESULT(no)
                        AC_MSG_ERROR(Invalid ORACLE directory - unable to find oci.h)
                fi
                if test -f $withval/lib64/libclntsh.so; then
                        ORACLE8_LFLAGS="-L$ORACLE8_LIBDIR1 -L$ORACLE8_LIBDIR2 $withval/lib64/libclntsh.so -lmm -lepc -lclient -lvsn -lcommon -lgeneric -lcore4 -lnlsrtl3 -lnsl -lm -ldl -lnetv2 -lnttcp -lnetwork -lncr -lsql"
                else
                        ORACLE8_LFLAGS="-L$ORACLE8_LIBDIR1 -L$ORACLE8_LIBDIR2 $withval/lib/libclntsh.so -lmm -lepc -lclient -lvsn -lcommon -lgeneric -lcore4 -lnlsrtl3 -lnsl -lm -ldl -lnetv2 -lnttcp -lnetwork -lncr -lsql"
                fi
                ORACLE8_INCLUDE="-I$ORACLE8_INCDIR1 -I$ORACLE8_INCDIR2 -I$ORACLE8_INCDIR3 -I$ORACLE8_INCDIR4"
                AC_DEFINE(HAVE_ORACLE8)
                AC_MSG_RESULT(yes)
                have_db=yes
        fi
],[
        AC_MSG_RESULT(no)
])
AC_SUBST(ORACLE8_LFLAGS)
AC_SUBST(ORACLE8_INCLUDE)


AC_MSG_CHECKING(for Oracle7 support)
AC_ARG_WITH(oracle7,
AC_HELP_STRING([--with-oracle7@<:@=DIR@:>@], [Include Oracle 7.3 support. DIR is the Oracle home directory @<:@default=$ORACLE_HOME@:>@]),
[
        if test "$withval" != "no"; then
            if test "$have_db" = "yes"; then
                AC_MSG_RESULT(error)
                AC_MSG_ERROR("You can configure for only one database.");
            fi
        fi

        if test "$withval" = "yes"; then
                withval="$ORACLE_HOME"
        fi

        if test "$withval" != "no"; then
                if test -f $withval/rdbms/demo/ocidfn.h; then
                        ORACLE7_INCDIR=$withval/rdbms/demo/
                        if test -d $withval/lib64; then
                                ORACLE7_LIBDIR1=$withval/lib64
                        else
                                ORACLE7_LIBDIR1=$withval/lib
                        fi
                        if test -d $withval/rdbms/lib64; then
                                ORACLE7_LIBDIR2=$withval/rdbms/lib64
                        else
                                ORACLE7_LIBDIR2=$withval/rdbms/lib
                        fi
                else
                        AC_MSG_RESULT(no)
                        AC_MSG_ERROR(Invalid ORACLE directory - unable to find ocidfn.h)
                fi

        ORACLEINST_TOP=$withval
        if test -f "$ORACLEINST_TOP/rdbms/lib/sysliblist"
        then
          ORA_SYSLIB="`cat $ORACLEINST_TOP/rdbms/lib/sysliblist`"
        elif test -f "$ORACLEINST_TOP/rdbms/lib64/sysliblist"
        then
          ORA_SYSLIB="`cat $ORACLEINST_TOP/rdbms/lib64/sysliblist`"
        elif test -f "$ORACLEINST_TOP/lib/sysliblist"
            then
          ORA_SYSLIB="`cat $ORACLEINST_TOP/lib/sysliblist`"
        elif test -f "$ORACLEINST_TOP/lib64/sysliblist"
            then
          ORA_SYSLIB="`cat $ORACLEINST_TOP/lib64/sysliblist`"
        else
          ORA_SYSLIB="-lm"
        fi
          
                ORACLE7_LFLAGS="-L$ORACLE7_LIBDIR1 -L$ORACLE7_LIBDIR2 \
            -lclient -lsqlnet -lncr -lsqlnet -lclient -lcommon \
            -lgeneric -lsqlnet -lncr -lsqlnet -lclient -lcommon -lgeneric \
            -lepc -lnlsrtl3 -lc3v6 -lcore3 -lnlsrtl3 -lcore3 -lnlsrtl3 \
            $ORA_SYSLIB -lcore3 $ORA_SYSLIB"
                ORACLE7_INCLUDE="-I$ORACLE7_INCDIR "
                AC_DEFINE(HAVE_ORACLE7)
                AC_MSG_RESULT(yes)
                have_db=yes
        fi
],[
        AC_MSG_RESULT(no)
])
AC_SUBST(ORACLE7_LFLAGS)
AC_SUBST(ORACLE7_INCLUDE)
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
