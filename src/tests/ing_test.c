/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2009-2011 Free Software Foundation Europe e.V.

   This program is Free Software; you can redistribute it and/or
   modify it under the terms of version three of the GNU Affero General Public
   License as published by the Free Software Foundation and included
   in the file LICENSE.

   This program is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
   Affero General Public License for more details.

   You should have received a copy of the GNU Affero General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
   02110-1301, USA.
*/
/*
 * Program to test Ingres DB routines
 *
 * Stefan Reddig, February 2010
 *
 * reusing code by
 * Eric Bollengier, August 2009
 */
#ifdef needed
#include "bareos.h"
#include "cats/cats.h"

/* Local variables */
static B_DB *db;
static const char *file = "COPYRIGHT";
#if defined(HAVE_DYNAMIC_CATS_BACKENDS)
static const char *backend_directory = _PATH_BAREOS_BACKENDDIR;
#endif
static const char *db_name = "bareos";
static const char *db_user = "bareos";
static const char *db_password = "";
static const char *db_host = NULL;

static void usage()
{
   fprintf(stderr, _(
PROG_COPYRIGHT
"\nVersion: %s (%s)\n"
"       -d <nn>           set debug level to <nn>\n"
"       -dt               print timestamp in debug output\n"
"       -n <name>         specify the database name (default bareos)\n"
"       -u <user>         specify database user name (default bareos)\n"
"       -P <password      specify database password (default none)\n"
"       -h <host>         specify database host (default NULL)\n"
"       -w <working>      specify working directory\n"
"       -j <jobids>       specify jobids\n"
"       -p <path>         specify path\n"
"       -f <file>         specify file\n"
"       -l <limit>        maximum tuple to fetch\n"
"       -T                truncate cache table before starting\n"
"       -v                verbose\n"
"       -?                print this message\n\n"), 2001, VERSION, BDATE);
   exit(1);
}

/*
 * simple handler for debug output of CRUD example
 */
static int test_handler(void *ctx, int num_fields, char **row)
{
   Pmsg2(0, "   Values are %d, %s\n", str_to_int64(row[0]), row[1]);
   return 0;
}

/*
 * string handler for debug output of simple SELECT tests
 */
static int string_handler(void *ctx, int num_fields, char **row)
{
   Pmsg1(0, "   Value is >>%s<<\n", row[0]);
   return 0;
}


/* number of thread started */

int main (int argc, char *argv[])
{
   int ch;
   alist* backend_directories = NULL;
   char *jobids = (char *)"1";
   char *path=NULL, *client=NULL;
   uint64_t limit=0;
   bool clean=false;
   setlocale(LC_ALL, "");
   bindtextdomain("bareos", LOCALEDIR);
   textdomain("bareos");
   init_stack_dump();

   Dmsg0(0, "Starting ing_test tool\n");

   my_name_is(argc, argv, "ing_test");
   init_msg(NULL, NULL);

   OSDependentInit();

   while ((ch = getopt(argc, argv, "h:c:l:d:n:P:Su:vf:w:?j:p:f:T")) != -1) {
      switch (ch) {
      case 'd':                    /* debug level */
         if (*optarg == 't') {
            dbg_timestamp = true;
         } else {
            debug_level = atoi(optarg);
            if (debug_level <= 0) {
               debug_level = 1;
            }
         }
         break;
      case 'l':
         limit = str_to_int64(optarg);
         break;

      case 'c':
         client = optarg;
         break;

      case 'h':
         db_host = optarg;
         break;

      case 'n':
         db_name = optarg;
         break;

      case 'w':
         working_directory = optarg;
         break;

      case 'u':
         db_user = optarg;
         break;

      case 'P':
         db_password = optarg;
         break;

      case 'v':
         verbose++;
         break;

      case 'p':
         path = optarg;
         break;

      case 'f':
         file = optarg;
         break;

      case 'j':
         jobids = optarg;
         break;

      case 'T':
         clean = true;
         break;

      case '?':
      default:
         usage();

      }
   }
   argc -= optind;
   argv += optind;

   if (argc != 0) {
      Pmsg0(0, _("Wrong number of arguments: \n"));
      usage();
   }

#if defined(HAVE_DYNAMIC_CATS_BACKENDS)
   backend_directories = New(alist(10, owned_by_alist));
   backend_directories->append((char *)backend_directory);

   db_set_backend_dirs(backend_directories);
#endif
   if ((db = db_init_database(NULL, "ingres", db_name, db_user, db_password, db_host, 0, NULL)) == NULL) {
      Emsg0(M_ERROR_TERM, 0, _("Could not init Bareos database\n"));
   }
   Dmsg1(0, "db_type=%s\n", db_get_type(db));

   if (!db_open_database(NULL, db)) {
      Emsg0(M_ERROR_TERM, 0, db_strerror(db));
   }
   Dmsg0(200, "Database opened\n");
   if (verbose) {
      Pmsg2(000, _("Using Database: %s, User: %s\n"), db_name, db_user);
   }

   /*
    * simple CRUD test including create/drop table
    */
   Pmsg0(0, "\nsimple CRUD test...\n\n");
   const char *stmt1[8] = {
      "CREATE TABLE t1 ( c1 integer, c2 varchar(29))",
      "INSERT INTO t1 VALUES (1, 'foo')",
      "SELECT c1,c2 FROM t1",
      "UPDATE t1 SET c2='bar' WHERE c1=1",
      "SELECT * FROM t1",
      "DELETE FROM t1 WHERE c2 LIKE '\%r'",
      "SELECT * FROM t1",
      "DROP TABLE t1"
   };
   int (*hndl1[8])(void*,int,char**) = {
      NULL,
      NULL,
      test_handler,
      NULL,
      test_handler,
      NULL,
      test_handler,
      NULL
   };

   for (int i=0; i<8; ++i) {
      Pmsg1(0, "DB-Statement: %s\n",stmt1[i]);
      if (!db_sql_query(db, stmt1[i], hndl1[i], NULL)) {
         Emsg0(M_ERROR_TERM, 0, _("Stmt went wrong\n"));
      }
   }


   /*
    * simple SELECT tests without tables
    */
   Pmsg0(0, "\nsimple SELECT tests without tables...\n\n");
   const char *stmt2[8] = {
      "SELECT 'Test of simple SELECT!'",
      "SELECT 'Test of simple SELECT!' as Text",
      "SELECT VARCHAR(LENGTH('Test of simple SELECT!'))",
      "SELECT DBMSINFO('_version')",
      "SELECT 'This is a ''quoting'' test with single quotes'",
      "SELECT 'This is a \"quoting\" test with double quotes'",
      "SELECT null",
      "SELECT ''"
   };
   int (*hndl2[8])(void*,int,char**) = {
      string_handler,
      string_handler,
      string_handler,
      string_handler,
      string_handler,
      string_handler,
      string_handler,
      string_handler
   };

   for (int i=0; i<8; ++i) {
      Pmsg1(0, "DB-Statement: %s\n",stmt2[i]);
      if (!db_sql_query(db, stmt2[i], hndl2[i], NULL)) {
         Emsg0(M_ERROR_TERM, 0, _("Stmt went wrong\n"));
      }
   }

   /*
    * testing aggregates like avg, max, sum
    */
   Pmsg0(0, "\ntesting aggregates...\n\n");
   const char *stmt[11] = {
      "CREATE TABLE t1 (c1 integer, c2 varchar(29))",
      "INSERT INTO t1 VALUES (1,'foo')",
      "INSERT INTO t1 VALUES (2,'bar')",
      "INSERT INTO t1 VALUES (3,'fun')",
      "INSERT INTO t1 VALUES (4,'egg')",
      "SELECT max(c1) from t1",
      "SELECT sum(c1) from t1",
      "INSERT INTO t1 VALUES (5,NULL)",
      "SELECT count(*) from t1",
      "SELECT count(c2) from t1",
      "DROP TABLE t1"
   };
   int (*hndl[11])(void*,int,char**) = {
      NULL,
      NULL,
      NULL,
      NULL,
      NULL,
      string_handler,
      string_handler,
      NULL,
      string_handler,
      string_handler,
      NULL
   };

   for (int i=0; i<11; ++i) {
      Pmsg1(0, "DB-Statement: %s\n",stmt[i]);
      if (!db_sql_query(db, stmt[i], hndl[i], NULL)) {
         Emsg0(M_ERROR_TERM, 0, _("Stmt went wrong\n"));
      }
   }


   /*
    * datatypes test
    */
   Pmsg0(0, "\ndatatypes test... (TODO)\n\n");


   Dmsg0(200, "DB-Statement: CREATE TABLE for datatypes\n");
   if (!db_sql_query(db, "CREATE TABLE t2 ("
     "c1        integer,"
     "c2        varchar(255),"
     "c3        char(255)"
     /* some more datatypes... "c4      ," */
     ")" , NULL, NULL)) {
      Emsg0(M_ERROR_TERM, 0, _("CREATE-Stmt went wrong\n"));
   }

   Dmsg0(200, "DB-Statement: DROP TABLE for datatypes\n");
   if (!db_sql_query(db, "DROP TABLE t2", NULL, NULL)) {
      Emsg0(M_ERROR_TERM, 0, _("DROP-Stmt went wrong\n"));
   }


   db_close_database(NULL, db);
   db_flush_backends();
   Dmsg0(200, "Database closed\n");

   return 0;
}
#else  /* needed */
int main (int argc, char *argv[])
{
   return 1;
}
#endif /* needed */
