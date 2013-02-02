#!/bin/sh

# Run tests using differents catalog databases

PWD=`pwd`

CATALOG_MODE="mysql-dbi mysql-dbi-batchinsert"
#"postgresql postgresql-batchinsert postgresql-dbi postgresql-dbi-batchinsert \
#              mysql mysql-batchinsert mysql-dbi mysql-dbi-batchinsert"

for CT in $CATALOG_MODE ; do

   CATALOG=`echo $CT | cut -d"-" -f1`
   DISABLE_BATCH_INSERT=`echo $CT | grep batchinsert`
   WITHOUT_DBI=`echo $CT | grep dbi`

   if test "$DISABLE_BATCH_INSERT" = "" ; then
      ENABLE_BATCH_INSERT="--disable-batch-insert"
   else
      ENABLE_BATCH_INSERT="--enable-batch-insert"
   fi

   if test "$WITHOUT_DBI" = "" ; then

     _WHICHDB="WHICHDB=\"--with-${CATALOG}\""
     _OPENSSL="OPENSSL=\"--with-openssll --disable-nls ${ENABLE_BATCH_INSERT}\""
     _LIBDBI="#LIBDBI"
   else
      if test "$CATALOG" = "mysql" ; then
         DBPORT=3306
      elif test "$CATALOG" = "postgresql" ; then
         DBPORT=5432
      elif test "$CATALOG" = "sqlite" ; then
         DBPORT=0
      elif test "$CATALOG" = "sqlite3" ; then
         DBPORT=0
      fi

      _WHICHDB="WHICHDB=\"--with-dbi\""
      _OPENSSL="OPENSSL=\"--with-openssl --disable-nls ${ENABLE_BATCH_INSERT} --with-dbi-driver=${CATALOG} --with-db-port=${DBPORT}\""
      _LIBDBI="LIBDBI=\"dbdriver = dbi:${CATALOG}; dbaddress = 127.0.0.1; dbport = ${DBPORT}\""
   fi

   _SITE_NAME="SITE_NAME=joaohf-bareos-${CT}"

   # substitute config values
   cp -a ${PWD}/config ${PWD}/config.tmp

   mkdir -p tmp

   echo "/^SITE_NAME/c $_SITE_NAME" >> tmp/config_sed
   echo "/^WHICHDB/c $_WHICHDB"  >> tmp/config_sed
   echo "/^OPENSSL/c $_OPENSSL" >> tmp/config_sed
   echo "/^#LIBDBI/c $_LIBDBI" >> tmp/config_sed
   echo "/^LIBDBI/c $_LIBDBI" >> tmp/config_sed

   sed -f tmp/config_sed ${PWD}/config.tmp > ${PWD}/config
   rm tmp/config_sed

   make setup
   echo " ==== Starting ${_SITE_NAME} ====" >> test.out

   if test x"$1" = "xctest" ; then
      ./experimental-disk
   else
      ./all-disk-tests
   fi
   echo " ==== Submiting ${_SITE_NAME} ====" >> test.out
done
