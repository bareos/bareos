/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2009-2010 Free Software Foundation Europe e.V.

   This program is Free Software; you can redistribute it and/or
   modify it under the terms of version two of the GNU General Public
   License as published by the Free Software Foundation and included
   in the file LICENSE.

   This program is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
   Affero General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
   02110-1301, USA.
*/
/*
 * Bareos Catalog Database routines specific to Ingres
 *   These are Ingres specific routines
 *
 *    Stefan Reddig, June 2009 with help of Marco van Wieringen April 2010
 */

#include "bareos.h"

#ifdef HAVE_INGRES
EXEC SQL INCLUDE SQLCA;
EXEC SQL INCLUDE SQLDA;

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "myingres.h"

/*
 * ---Implementations---
 */
int INGgetCols(INGconn *dbconn, const char *query, bool explicit_commit)
{
   EXEC SQL BEGIN DECLARE SECTION;
   int sess_id;
   char *stmt;
   EXEC SQL END DECLARE SECTION;
   IISQLDA *sqlda;
   int number = -1;

   sqlda = (IISQLDA *)malloc(IISQDA_HEAD_SIZE + IISQDA_VAR_SIZE);
   memset(sqlda, 0, (IISQDA_HEAD_SIZE + IISQDA_VAR_SIZE));

   sqlda->sqln = number;

   stmt = bstrdup(query);

   EXEC SQL WHENEVER SQLERROR GOTO bail_out;

   /*
    * Switch to the correct default session for this thread.
    */
   sess_id = dbconn->session_id;
   EXEC SQL SET_SQL (SESSION = :sess_id);

   EXEC SQL PREPARE s1 INTO :sqlda FROM :stmt;

   EXEC SQL WHENEVER SQLERROR CONTINUE;

   number = sqlda->sqld;

bail_out:
   /*
    * If explicit_commit is set we commit our work now.
    */
   if (explicit_commit) {
      EXEC SQL COMMIT WORK;
   }

   /*
    * Switch to no default session for this thread.
    */
   EXEC SQL SET_SQL (SESSION = NONE);
   free(stmt);
   free(sqlda);
   return number;
}

static inline IISQLDA *INGgetDescriptor(int numCols, const char *query)
{
   EXEC SQL BEGIN DECLARE SECTION;
   char *stmt;
   EXEC SQL END DECLARE SECTION;
   int i;
   IISQLDA *sqlda;

   sqlda = (IISQLDA *)malloc(IISQDA_HEAD_SIZE + (numCols * IISQDA_VAR_SIZE));
   memset(sqlda, 0, (IISQDA_HEAD_SIZE + (numCols * IISQDA_VAR_SIZE)));

   sqlda->sqln = numCols;

   stmt = bstrdup(query);

   EXEC SQL PREPARE s2 INTO :sqlda FROM :stmt;

   for (i = 0; i < sqlda->sqld; ++i) {
      /*
       * Negative type indicates nullable columns, so an indicator
       * is allocated, otherwise it's null
       */
      if (sqlda->sqlvar[i].sqltype > 0) {
         sqlda->sqlvar[i].sqlind = NULL;
      } else {
         sqlda->sqlvar[i].sqlind = (short *)malloc(sizeof(short));
      }
      /*
       * Alloc space for variable like indicated in sqllen
       * for date types sqllen is always 0 -> allocate by type
       */
      switch (abs(sqlda->sqlvar[i].sqltype)) {
      case IISQ_TSW_TYPE:
         sqlda->sqlvar[i].sqldata = (char *)malloc(IISQ_TSW_LEN);
         break;
      case IISQ_TSWO_TYPE:
         sqlda->sqlvar[i].sqldata = (char *)malloc(IISQ_TSWO_LEN);
         break;
      case IISQ_TSTMP_TYPE:
         sqlda->sqlvar[i].sqldata = (char *)malloc(IISQ_TSTMP_LEN);
         break;
      default:
         /*
          * plus one to avoid zero mem allocs
          */
         sqlda->sqlvar[i].sqldata = (char *)malloc(sqlda->sqlvar[i].sqllen + 1);
         break;
      }
   }

   free(stmt);
   return sqlda;
}

static void INGfreeDescriptor(IISQLDA *sqlda)
{
   int i;

   if (!sqlda) {
      return;
   }

   for (i = 0; i < sqlda->sqld; ++i) {
      if (sqlda->sqlvar[i].sqldata) {
         free(sqlda->sqlvar[i].sqldata);
      }
      if (sqlda->sqlvar[i].sqlind) {
         free(sqlda->sqlvar[i].sqlind);
      }
   }
   free(sqlda);
}

static inline int INGgetTypeSize(IISQLVAR *ingvar)
{
   int inglength = 0;

   switch (ingvar->sqltype) {
   case IISQ_TSWO_TYPE:
      inglength = 20;
      break;
   case IISQ_TSW_TYPE:
      inglength = 20;
      break;
   case IISQ_DTE_TYPE:
      inglength = 25;
      break;
   case IISQ_MNY_TYPE:
      inglength = 8;
      break;
   default:
      inglength = ingvar->sqllen;
      break;
   }

   return inglength;
}

static inline INGresult *INGgetINGresult(int numCols, const char *query)
{
   int i;
   INGresult *ing_res;

   ing_res = (INGresult *)malloc(sizeof(INGresult));
   memset(ing_res, 0, sizeof(INGresult));

   if ((ing_res->sqlda = INGgetDescriptor(numCols, query)) == NULL) {
      return NULL;
   }

   ing_res->num_fields = ing_res->sqlda->sqld;
   ing_res->num_rows = 0;
   ing_res->first_row = NULL;
   ing_res->status = ING_EMPTY_RESULT;
   ing_res->act_row = NULL;

   if (ing_res->num_fields) {
      ing_res->fields = (INGRES_FIELD *)malloc(sizeof(INGRES_FIELD) * ing_res->num_fields);
      memset(ing_res->fields, 0, sizeof(INGRES_FIELD) * ing_res->num_fields);

      for (i = 0; i < ing_res->num_fields; ++i) {
         ing_res->fields[i].name = (char *)malloc(ing_res->sqlda->sqlvar[i].sqlname.sqlnamel + 1);
         bstrncpy(ing_res->fields[i].name, ing_res->sqlda->sqlvar[i].sqlname.sqlnamec, ing_res->sqlda->sqlvar[i].sqlname.sqlnamel + 1);
         ing_res->fields[i].name[ing_res->sqlda->sqlvar[i].sqlname.sqlnamel] = '\0';
         ing_res->fields[i].max_length = INGgetTypeSize(&ing_res->sqlda->sqlvar[i]);
         ing_res->fields[i].type = abs(ing_res->sqlda->sqlvar[i].sqltype);
         ing_res->fields[i].flags = (ing_res->sqlda->sqlvar[i].sqltype < 0) ? 1 : 0;
      }
   }

   return ing_res;
}

static inline void INGfreeRowSpace(ING_ROW *row, IISQLDA *sqlda)
{
   int i;

   if (row == NULL || sqlda == NULL) {
      return;
   }

   for (i = 0; i < sqlda->sqld; ++i) {
      if (row->sqlvar[i].sqldata) {
         free(row->sqlvar[i].sqldata);
      }
      if (row->sqlvar[i].sqlind) {
         free(row->sqlvar[i].sqlind);
      }
   }
   free(row->sqlvar);
   free(row);
}

static void INGfreeINGresult(INGresult *ing_res)
{
   int i;
   int rows;
   ING_ROW *rowtemp;

   if (!ing_res) {
      return;
   }

   /*
    * Use of rows is a nasty workaround til I find the reason,
    * why aggregates like max() don't work
    */
   rows = ing_res->num_rows;
   ing_res->act_row = ing_res->first_row;
   while (ing_res->act_row != NULL && rows > 0) {
      rowtemp = ing_res->act_row->next;
      INGfreeRowSpace(ing_res->act_row, ing_res->sqlda);
      ing_res->act_row = rowtemp;
      --rows;
   }

   if (ing_res->fields) {
      for (i = 0; i < ing_res->num_fields; ++i) {
         free(ing_res->fields[i].name);
      }

      free(ing_res->fields);
   }

   INGfreeDescriptor(ing_res->sqlda);

   free(ing_res);
}

static inline ING_ROW *INGgetRowSpace(INGresult *ing_res)
{
   int i;
   unsigned short len; /* used for VARCHAR type length */
   unsigned short th, tm, ts;
   IISQLDA *sqlda;
   ING_ROW *row = NULL;
   ING_TIMESTAMP *tsp;
   IISQLVAR *vars = NULL;

   row = (ING_ROW *)malloc(sizeof(ING_ROW));
   memset(row, 0, sizeof(ING_ROW));

   sqlda = ing_res->sqlda;
   vars = (IISQLVAR *)malloc(sizeof(IISQLVAR) * sqlda->sqld);
   memset(vars, 0, sizeof(IISQLVAR) * sqlda->sqld);

   row->sqlvar = vars;
   row->next = NULL;

   for (i = 0; i < sqlda->sqld; ++i) {
      /*
       * Make strings out of the data, then the space and assign
       * (why string? at least it seems that way, looking into the sources)
       */
      vars[i].sqlind = (short *)malloc(sizeof(short));
      if (sqlda->sqlvar[i].sqlind) {
         memcpy(vars[i].sqlind,sqlda->sqlvar[i].sqlind,sizeof(short));
      } else {
         *vars[i].sqlind = NULL;
      }
      /*
       * if sqlind pointer exists AND points to -1 -> column is 'null'
       */
      if ( *vars[i].sqlind && (*vars[i].sqlind == -1)) {
         vars[i].sqldata = NULL;
      } else {
         switch (ing_res->fields[i].type) {
         case IISQ_VCH_TYPE:
         case IISQ_LVCH_TYPE:
         case IISQ_VBYTE_TYPE:
         case IISQ_LBYTE_TYPE:
         case IISQ_NVCHR_TYPE:
         case IISQ_LNVCHR_TYPE:
            len = ((ING_VARCHAR *)sqlda->sqlvar[i].sqldata)->len;
            vars[i].sqldata = (char *)malloc(len + 1);
            memcpy(vars[i].sqldata,sqlda->sqlvar[i].sqldata + 2,len);
            vars[i].sqldata[len] = '\0';
            break;
         case IISQ_CHA_TYPE:
         case IISQ_BYTE_TYPE:
         case IISQ_NCHR_TYPE:
            vars[i].sqldata = (char *)malloc(ing_res->fields[i].max_length + 1);
            memcpy(vars[i].sqldata,sqlda->sqlvar[i].sqldata,sqlda->sqlvar[i].sqllen);
            vars[i].sqldata[ing_res->fields[i].max_length] = '\0';
            break;
         case IISQ_INT_TYPE:
            switch (sqlda->sqlvar[i].sqllen) {
            case 2:
               vars[i].sqldata = (char *)malloc(6);
               memset(vars[i].sqldata, 0, 6);
               bsnprintf(vars[i].sqldata, 6, "%d",*(int16_t *)sqlda->sqlvar[i].sqldata);
               break;
            case 4:
               vars[i].sqldata = (char *)malloc(11);
               memset(vars[i].sqldata, 0, 11);
               bsnprintf(vars[i].sqldata, 11, "%ld",*(int32_t *)sqlda->sqlvar[i].sqldata);
               break;
            case 8:
               vars[i].sqldata = (char *)malloc(20);
               memset(vars[i].sqldata, 0, 20);
               bsnprintf(vars[i].sqldata, 20, "%lld",*(int64_t *)sqlda->sqlvar[i].sqldata);
               break;
            }
            break;
         case IISQ_TSTMP_TYPE:
            vars[i].sqldata = (char *)malloc(IISQ_TSTMP_LEN + 1);
            vars[i].sqldata[IISQ_TSTMP_LEN] = '\0';
            break;
         case IISQ_TSWO_TYPE:
            tsp = (ING_TIMESTAMP *)sqlda->sqlvar[i].sqldata;
            th = tsp->secs / 3600; /* hours */
            tm = tsp->secs % 3600; /* remaining seconds */
            tm = tm / 60; /* minutes */
            ts = tsp->secs - (th * 3600) - (tm * 60); /* seconds */
            vars[i].sqldata = (char *)malloc(IISQ_TSTMP_LEN + 1);
            bsnprintf(vars[i].sqldata, IISQ_TSWO_LEN + 1,
                      "%04u-%02u-%02u %02u:%02u:%02u",
                      tsp->year, tsp->month, tsp->day, th, tm, ts);
            break;
         case IISQ_TSW_TYPE:
            tsp = (ING_TIMESTAMP *)sqlda->sqlvar[i].sqldata;
            th = tsp->secs / 3600; /* hours */
            tm = tsp->secs % 3600; /* remaining seconds */
            tm = tm / 60; /* minutes */
            ts = tsp->secs - (th * 3600) - (tm * 60); /* seconds */
            vars[i].sqldata = (char *)malloc(IISQ_TSW_LEN + 1);
            bsnprintf(vars[i].sqldata, IISQ_TSW_LEN + 1,
                      "%04u-%02u-%02u %02u:%02u:%02u",
                      tsp->year, tsp->month, tsp->day, th, tm, ts);
            break;
         default:
            Jmsg(NULL, M_FATAL, 0,
                 "INGgetRowSpace: encountered unhandled database datatype %d please report this as a bug\n",
                 ing_res->fields[i].type);
            break;
         }
      }
   }
   return row;
}

static inline int INGfetchAll(INGresult *ing_res)
{
   ING_ROW *row;
   IISQLDA *desc;
   int linecount = -1;

   desc = ing_res->sqlda;

   EXEC SQL WHENEVER SQLERROR GOTO bail_out;

   EXEC SQL DECLARE c2 CURSOR FOR s2;
   EXEC SQL OPEN c2;

   EXEC SQL WHENEVER SQLERROR CONTINUE;

   linecount = 0;
   do {
      EXEC SQL FETCH c2 USING DESCRIPTOR :desc;

      if (sqlca.sqlcode == 0 || sqlca.sqlcode == -40202) {
         /*
          * Allocate space for fetched row
          */
         row = INGgetRowSpace(ing_res);

         /*
          * Initialize list when encountered first time
          */
         if (ing_res->first_row == 0) {
            ing_res->first_row = row; /* head of the list */
            ing_res->first_row->next = NULL;
            ing_res->act_row = ing_res->first_row;
         }

         ing_res->act_row->next = row; /* append row to old act_row */
         ing_res->act_row = row; /* set row as act_row */
         row->row_number = linecount++;
      }
   } while ( (sqlca.sqlcode == 0) || (sqlca.sqlcode == -40202) );

   EXEC SQL CLOSE c2;

   ing_res->status = ING_COMMAND_OK;
   ing_res->num_rows = linecount;

bail_out:
   return linecount;
}

static inline ING_STATUS INGresultStatus(INGresult *ing_res)
{
   if (ing_res == NULL) {
      return ING_NO_RESULT;
   } else {
      return ing_res->status;
   }
}

static void INGrowSeek(INGresult *ing_res, int row_number)
{
   ING_ROW *trow = NULL;

   if (ing_res->act_row->row_number == row_number) {
      return;
   }

   /*
    * TODO: real error handling
    */
   if (row_number < 0 || row_number > ing_res->num_rows) {
      return;
   }

   for (trow = ing_res->first_row; trow->row_number != row_number; trow = trow->next) ;
   ing_res->act_row = trow;
   /*
    * Note - can be null - if row_number not found, right?
    */
}

char *INGgetvalue(INGresult *ing_res, int row_number, int column_number)
{
   if (row_number != ing_res->act_row->row_number) {
      INGrowSeek(ing_res, row_number);
   }

   return ing_res->act_row->sqlvar[column_number].sqldata;
}

bool INGgetisnull(INGresult *ing_res, int row_number, int column_number)
{
   if (row_number != ing_res->act_row->row_number) {
      INGrowSeek(ing_res, row_number);
   }

   return (*ing_res->act_row->sqlvar[column_number].sqlind == -1) ? true : false;
}

int INGntuples(const INGresult *ing_res)
{
   return ing_res->num_rows;
}

int INGnfields(const INGresult *ing_res)
{
   return ing_res->num_fields;
}

char *INGfname(const INGresult *ing_res, int column_number)
{
   if ((column_number > ing_res->num_fields) || (column_number < 0)) {
      return NULL;
   } else {
      return ing_res->fields[column_number].name;
   }
}

short INGftype(const INGresult *ing_res, int column_number)
{
   return ing_res->fields[column_number].type;
}

int INGexec(INGconn *dbconn, const char *query, bool explicit_commit)
{
   EXEC SQL BEGIN DECLARE SECTION;
   int sess_id;
   int rowcount;
   int errors;
   char *stmt;
   EXEC SQL END DECLARE SECTION;

   rowcount = -1;
   stmt = bstrdup(query);

   EXEC SQL WHENEVER SQLERROR GOTO bail_out;

   /*
    * Switch to the correct default session for this thread.
    */
   sess_id = dbconn->session_id;
   EXEC SQL SET_SQL (SESSION = :sess_id);

   EXEC SQL EXECUTE IMMEDIATE :stmt;
   EXEC SQL INQUIRE_INGRES(:rowcount = ROWCOUNT);

   /*
    * See if the negative rowcount is due to errors.
    */
   if (rowcount < 0) {
      EXEC SQL INQUIRE_INGRES(:errors = DBMSERROR);

      /*
       * If the number of errors is 0 we got a negative rowcount
       * because the statement we executed doesn't give a rowcount back.
       * Lets pretend we have a rowcount of 1 then.
       */
      if (errors == 0) {
         rowcount = 1;
      }
   }

   EXEC SQL WHENEVER SQLERROR CONTINUE;

bail_out:
   /*
    * If explicit_commit is set we commit our work now.
    */
   if (explicit_commit) {
      EXEC SQL COMMIT WORK;
   }

   /*
    * Switch to no default session for this thread.
    */
   EXEC SQL SET_SQL (SESSION = NONE);
   free(stmt);
   return rowcount;
}

INGresult *INGquery(INGconn *dbconn, const char *query, bool explicit_commit)
{
   /*
    * TODO: error handling
    */
   INGresult *ing_res = NULL;
   int rows;
   int cols;
   EXEC SQL BEGIN DECLARE SECTION;
   int sess_id;
   EXEC SQL END DECLARE SECTION;

   cols = INGgetCols(dbconn, query, explicit_commit);

   /*
    * Switch to the correct default session for this thread.
    */
   sess_id = dbconn->session_id;
   EXEC SQL SET_SQL (SESSION = :sess_id);

   ing_res = INGgetINGresult(cols, query);
   if (!ing_res) {
      goto bail_out;
   }

   rows = INGfetchAll(ing_res);

   if (rows < 0) {
      INGfreeINGresult(ing_res);
      ing_res = NULL;
      goto bail_out;
   }

bail_out:
   /*
    * If explicit_commit is set we commit our work now.
    */
   if (explicit_commit) {
      EXEC SQL COMMIT WORK;
   }

   /*
    * Switch to no default session for this thread.
    */
   EXEC SQL SET_SQL (SESSION = NONE);
   return ing_res;
}

void INGclear(INGresult *ing_res)
{
   if (ing_res == NULL) {
      return;
   }

   INGfreeINGresult(ing_res);
}

void INGcommit(const INGconn *dbconn)
{
   EXEC SQL BEGIN DECLARE SECTION;
   int sess_id;
   EXEC SQL END DECLARE SECTION;

   if (dbconn != NULL) {
      /*
       * Switch to the correct default session for this thread.
       */
      sess_id = dbconn->session_id;
      EXEC SQL SET_SQL (SESSION = :sess_id);

      /*
       * Commit our work.
       */
      EXEC SQL COMMIT WORK;

      /*
       * Switch to no default session for this thread.
       */
      EXEC SQL SET_SQL (SESSION = NONE);
   }
}

INGconn *INGconnectDB(char *dbname, char *user, char *passwd, int session_id)
{
   EXEC SQL BEGIN DECLARE SECTION;
   char *ingdbname;
   char *ingdbuser = NULL;
   char *ingdbpasswd = NULL;
   int sess_id;
   EXEC SQL END DECLARE SECTION;
   INGconn *dbconn = NULL;

   if (dbname == NULL || strlen(dbname) == 0) {
      return NULL;
   }

   sess_id = session_id;
   ingdbname = dbname;

   EXEC SQL WHENEVER SQLERROR GOTO bail_out;

   if (user != NULL) {
      ingdbuser = user;
      if (passwd != NULL) {
         ingdbpasswd = passwd;
         EXEC SQL CONNECT
            :ingdbname
            SESSION :sess_id
            IDENTIFIED BY :ingdbuser
            DBMS_PASSWORD = :ingdbpasswd;
      } else {
         EXEC SQL CONNECT
            :ingdbname
            SESSION :sess_id
            IDENTIFIED BY :ingdbuser;
      }
   } else {
      EXEC SQL CONNECT
         :ingdbname
         SESSION :sess_id;
   }

   EXEC SQL WHENEVER SQLERROR CONTINUE;

   dbconn = (INGconn *)malloc(sizeof(INGconn));
   memset(dbconn, 0, sizeof(INGconn));

   dbconn->dbname = bstrdup(ingdbname);
   if (user != NULL) {
      dbconn->user = bstrdup(ingdbuser);
      dbconn->password = bstrdup(ingdbpasswd);
   }
   dbconn->session_id = sess_id;
   dbconn->msg = (char *)malloc(257);
   memset(dbconn->msg, 0, 257);

   /*
    * Switch to no default session for this thread undo default settings from SQL CONNECT.
    */
   EXEC SQL SET_SQL (SESSION = NONE);

bail_out:
   return dbconn;
}

void INGsetDefaultLockingMode(INGconn *dbconn)
{
   /*
    * Set the default Ingres session locking mode:
    *
    * SET LOCKMODE provides four different parameters to govern
    * the nature of locking in an INGRES session:
    *
    * Level: This refers to the level of granularity desired when
    * the table is accessed. You can specify any of the following
    * locking levels:
    *
    * row     Specifies locking at the level of the row (subject to
    *         escalation criteria; see below)
    * page    Specifies locking at the level of the data page (subject to
    *         escalation criteria; see below)
    * table   Specifies table-level locking in the database
    * session Specifies the current default for your INGRES session
    * system  Specifies that INGRES will start with page-level locking,
    *         unless it estimates that more than Maxlocks pages will be
    *         referenced, in which case table-level locking will be used.
    *
    * Readlock: This refers to locking in situations where table access
    *           is required for reading data only (as opposed to updating
    *           data). You can specify any of the following Readlock modes:
    *
    *    nolock     Specifies no locking when reading data
    *    shared     Specifies the default mode of locking when reading data
    *    exclusive  Specifies exclusive locking when reading data (useful in
    *               "select-for-update" processing within a multi-statement
    *               transaction)
    *    system     Specifies the general Readlock default for the INGRES system
    *
    * Maxlocks: This refers to an escalation factor, or number of locks on
    *           data pages, at which locking escalates from page-level
    *           to table-level. The number of locks available to you is
    *           dependent upon your system configuration. You can specify the
    *           following Maxlocks escalation factors:
    *
    *    n       A specific (integer) number of page locks to allow before
    *            escalating to table-level locking. The default "n" is 10,
    *            and "n" must be greater than 0.
    *    session Specifies the current Maxlocks default for your INGRES
    *            session
    *    system  Specifies the general Maxlocks default for the INGRES system
    *
    * Note: If you specify page-level locking, and the number of locks granted
    * during a query exceeds the system-wide lock limit, or if the operating
    * system's locking resources are depleted, locking escalates to table-level.
    * This escalation occurs automatically and is independent of the user.
    *
    * Timeout: This refers to a time limit, expressed in seconds, for which
    * a lock request should remain pending. If INGRES cannot grant the lock
    * request within the specified time, then the query that requested the
    * lock aborts. You can specify the following timeout characteristics:
    *
    *    n       A specific (integer) number of seconds to wait for a lock
    *            (setting "n" to 0 requires INGRES to wait indefinitely for
    *            the lock)
    *    session Specifies the current timeout default for your INGRES
    *            session (which is also the INGRES default)
    *    system  Specifies the general timeout default for the INGRES system
    *
    */
   EXEC SQL BEGIN DECLARE SECTION;
   int sess_id;
   EXEC SQL END DECLARE SECTION;

   if (dbconn != NULL) {
      /*
       * Switch to the correct default session for this thread.
       */
      sess_id = dbconn->session_id;
      EXEC SQL SET_SQL (SESSION = :sess_id);

      EXEC SQL SET LOCKMODE SESSION WHERE level = row, readlock = nolock;

      /*
       * Switch to no default session for this thread.
       */
      EXEC SQL SET_SQL (SESSION = NONE);
   }
}

void INGdisconnectDB(INGconn *dbconn)
{
   EXEC SQL BEGIN DECLARE SECTION;
   int sess_id;
   EXEC SQL END DECLARE SECTION;

   if (dbconn != NULL) {
      sess_id = dbconn->session_id;
      EXEC SQL DISCONNECT SESSION :sess_id;

      free(dbconn->dbname);
      if (dbconn->user) {
         free(dbconn->user);
      }
      if (dbconn->password) {
         free(dbconn->password);
      }
      free(dbconn->msg);
      free(dbconn);
   }
}

char *INGerrorMessage(const INGconn *dbconn)
{
   EXEC SQL BEGIN DECLARE SECTION;
   int sess_id;
   char errbuf[256];
   EXEC SQL END DECLARE SECTION;

   if (dbconn != NULL) {
      /*
       * Switch to the correct default session for this thread.
       */
      sess_id = dbconn->session_id;
      EXEC SQL SET_SQL (SESSION = :sess_id);

      EXEC SQL INQUIRE_INGRES (:errbuf = ERRORTEXT);
      strncpy(dbconn->msg, errbuf, sizeof(dbconn->msg));

      /*
       * Switch to no default session for this thread.
       */
      EXEC SQL SET_SQL (SESSION = NONE);
   }

   return dbconn->msg;
}

#endif
