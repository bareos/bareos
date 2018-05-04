/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2011-2011 Free Software Foundation Europe e.V.

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
 * Program to test CATS DB routines
 */
#define _BDB_PRIV_INTERFACE_

#include "include/bareos.h"
#include "jcr.h"
#include "cats/cats.h"
#include "cats/bdb_priv.h"

/* Local variables */
static BareosDb *db;
static const char *file = "COPYRIGHT";
#if defined(HAVE_DYNAMIC_CATS_BACKENDS)
static const char *backend_directory = _PATH_BAREOS_BACKENDDIR;
#endif
static const char *db_name = "bareos";
static const char *db_user = "bareos";
static const char *db_password = "";
static const char *db_host = NULL;
static const char *db_address = NULL;
static const char *db_driver = NULL;
static int db_port = 0;
static int64_t pid = 0;
static JobControlRecord *jcr=NULL;

#define PLINE "\n============================================================\n"
static void usage()
{
   fprintf(stderr, _(
PROG_COPYRIGHT
"\nVersion: %s (%s)\n"
"       -d <nn>           set debug level to <nn>\n"
"       -dt               print timestamp in debug output\n"
"       -D <driver name>  specify the driver database name (default NULL)\n"
"       -n <name>         specify the database name (default bareos)\n"
"       -u <user>         specify database user name (default bareos)\n"
"       -P <password      specify database password (default none)\n"
"       -h <host>         specify database host (default NULL)\n"
"       -w <working>      specify working directory\n"
"       -p <path>         specify path\n"
"       -f <file>         specify file\n"
"       -l <limit>        maximum tuple to fetch\n"
"       -q                print only errors\n"
"       -v                verbose\n"
"       -?                print this message\n\n"), 2011, VERSION, BDATE);
   exit(1);
}

bool print_ok=true;
int _err=0;
int _wrn=0;
int _nb=0;

bool _warn(const char *file, int l, const char *op, int value, const char *label)
{
   bool ret=false;
   _nb++;
   if (!value) {
      _wrn++;
      printf("WRN %.30s %s:%i on %s\n", label, file, l, op);
   } else {
      ret=true;
      printf("OK  %.30s\n", label);
   }
   return ret;
}

#define warn(x, label) _warn(__FILE__, __LINE__, #x, (x), label)

bool _ok(const char *file, int l, const char *op, int value, const char *label)
{
   bool ret=false;
   _nb++;
   if (!value) {
      _err++;
      printf("ERR %.30s %s:%i on %s\n", label, file, l, op);
   } else {
      ret=true;
      if (print_ok) {
         printf("OK  %.30s\n", label);
      }
   }
   return ret;
}

#define ok(x, label) _ok(__FILE__, __LINE__, #x, (x), label)

bool _nok(const char *file, int l, const char *op, int value, const char *label)
{
   bool ret=false;
   _nb++;
   if (value) {
      _err++;
      printf("ERR %.30s %s:%i on !%s\n", label, file, l, op);
   } else {
      ret = true;
      if (print_ok) {
         printf("OK  %.30s\n", label);
      }
   }
   return ret;
}

#define nok(x, label) _nok(__FILE__, __LINE__, #x, (x), label)

int report()
{
   printf("Result %i/%i OK\n", _nb - _err, _nb);
   return _err>0;
}

static void cmp_pool(PoolDbRecord &pr, PoolDbRecord &pr2)
{
   ok(pr.MaxVols == pr2.MaxVols,                "  Check Pool MaxVols");
   ok(pr.UseOnce == pr2.UseOnce,                "  Check Pool UseOnce");
   ok(pr.UseCatalog == pr2.UseCatalog,          "  Check Pool UseCatalog");
   ok(pr.AcceptAnyVolume == pr2.AcceptAnyVolume,"  Check Pool AcceptAnyVolume");
   ok(pr.AutoPrune == pr2.AutoPrune,            "  Check Pool AutoPrune");
   ok(pr.Recycle == pr2.Recycle,                "  Check Pool Recycle");
   ok(pr.VolRetention == pr2.VolRetention ,     "  Check Pool VolRetention");
   ok(pr.VolUseDuration == pr2.VolUseDuration,  "  Check Pool VolUseDuration");
   ok(pr.MaxVolJobs == pr2.MaxVolJobs,          "  Check Pool MaxVolJobs");
   ok(pr.MaxVolFiles == pr2.MaxVolFiles,        "  Check Pool MaxVolFiles");
   ok(pr.MaxVolBytes == pr2.MaxVolBytes,        "  Check Pool MaxVolBytes");
   ok(bstrcmp(pr.PoolType, pr2.PoolType),       "  Check Pool PoolType");
   ok(pr.LabelType == pr2.LabelType,            "  Check Pool LabelType");
   ok(bstrcmp(pr.LabelFormat, pr2.LabelFormat), "  Check Pool LabelFormat");
   ok(pr.RecyclePoolId == pr2.RecyclePoolId,    "  Check Pool RecyclePoolId");
   ok(pr.ScratchPoolId == pr2.ScratchPoolId,    "  Check Pool ScratchPoolId");
   ok(pr.ActionOnPurge == pr2.ActionOnPurge,    "  Check Pool ActionOnPurge");
}

static void cmp_client(ClientDbRecord &cr, ClientDbRecord &cr2)
{
   ok(bstrcmp(cr2.Name, cr.Name),           "  Check Client Name");
   ok(bstrcmp(cr2.Uname, cr.Uname),         "  Check Client Uname");
   ok(cr.AutoPrune == cr2.AutoPrune,        "  Check Client Autoprune");
   ok(cr.JobRetention == cr2.JobRetention,  "  Check Client JobRetention");
   ok(cr.FileRetention == cr2.FileRetention,"  Check Client FileRetention");
}

static void cmp_job(JobDbRecord &jr, JobDbRecord &jr2)
{
   ok(jr.VolSessionId == jr2.VolSessionId,     "  Check VolSessionId");
   ok(jr.VolSessionTime == jr2.VolSessionTime, "  Check VolSessionTime");
   ok(jr.PoolId == jr2.PoolId,                 "  Check PoolId");
   ok(jr.StartTime == jr2.StartTime,           "  Check StartTime");
   ok(jr.EndTime == jr2.EndTime,               "  Check EndTime");
   ok(jr.JobFiles == jr2.JobFiles,             "  Check JobFiles");
   ok(jr.JobBytes == jr2.JobBytes,             "  Check JobBytes");
   ok(jr.JobTDate == jr2.JobTDate,             "  Check JobTDate");
   ok(bstrcmp(jr.Job, jr2.Job),                "  Check Job");
   ok(jr.JobStatus == jr2.JobStatus,           "  Check JobStatus");
   ok(jr.JobType == jr2.JobType,               "  Check Type");
   ok(jr.JobLevel == jr2.JobLevel,             "  Check Level");
   ok(jr.ClientId == jr2.ClientId,             "  Check ClientId");
   ok(bstrcmp(jr.Name, jr2.Name),              "  Check Name");
   ok(jr.PriorJobId == jr2.PriorJobId,         "  Check PriorJobId");
   ok(jr.RealEndTime == jr2.RealEndTime,       "  Check RealEndTime");
   ok(jr.JobId == jr2.JobId,                   "  Check JobId");
   ok(jr.FileSetId == jr2.FileSetId,           "  Check FileSetId");
   ok(jr.SchedTime == jr2.SchedTime,           "  Check SchedTime");
   ok(jr.RealEndTime == jr2.RealEndTime,       "  Check RealEndTime");
   ok(jr.ReadBytes == jr2.ReadBytes,           "  Check ReadBytes");
   ok(jr.HasBase == jr2.HasBase,               "  Check HasBase");
   ok(jr.PurgedFiles == jr2.PurgedFiles,       "  Check PurgedFiles");
}


#define aPATH "/aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
#define aFILE "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"

static int list_files(void *ctx, int nb_col, char **row)
{
   uint32_t *k = (uint32_t*) ctx;
   (*k)++;
   ok(nb_col > 4, "Check result columns");
   ok(bstrcmp(row[0], aPATH aPATH aPATH aPATH "/"), "Check path");
   ok(bstrcmp(row[1], aFILE aFILE ".txt"), "Check filename");
   ok(str_to_int64(row[2]) == 10, "Check FileIndex");
   ok(str_to_int64(row[3]) == jcr->JobId, "Check JobId");
   return 1;
}

static int count_col(void *ctx, int nb_col, char **row)
{
   *((int32_t*) ctx) = nb_col;
   return 1;
}

/* number of thread started */

int main (int argc, char *argv[])
{
   int ch;
   alist* backend_directories = NULL;
   char *path=NULL, *client=NULL;
   uint64_t limit=0;
   bool clean=false;
   bool full_test=false;
   int dbtype;
   uint32_t j;
   char temp[20];
   POOLMEM *buf = GetPoolMemory(PM_FNAME);
   POOLMEM *buf2 = GetPoolMemory(PM_FNAME);
   POOLMEM *buf3 = GetPoolMemory(PM_FNAME);

   setlocale(LC_ALL, "");
   bindtextdomain("bareos", LOCALEDIR);
   textdomain("bareos");
   InitStackDump();
   pid = getpid();

   Pmsg0(0, "Starting cats_test tool" PLINE);

   MyNameIs(argc, argv, "");
   InitMsg(NULL, NULL);

   OSDependentInit();

   while ((ch = getopt(argc, argv, "qh:c:l:d:D:n:P:Su:vFw:?p:f:T")) != -1) {
      switch (ch) {
      case 'q':
         print_ok = false;
         break;

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

      case 'D':
         db_driver = optarg;
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

      case 'F':
         full_test = true;
         break;

      case 'f':
         file = optarg;
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

   /* TODO:
    *  - Open DB
    *    - With errors
    *    - With good info
    *    - With multiple thread in //
    *  - Test cats.h
    *  - Test all sql_cmds.c
    *  - Test all sql.c (db_)
    *  - Test all sql_create.c
    *  - Test db_handler
    */

   jcr = new_jcr(sizeof(JobControlRecord), NULL);
   jcr->setJobType(JT_CONSOLE);
   jcr->setJobLevel(L_NONE);
   jcr->JobStatus = JS_Running;
   bstrncpy(jcr->Job, "**dummy**", sizeof(jcr->Job));
   jcr->JobId = pid;      /* this is JobId on tape */
   jcr->start_time = jcr->sched_time = time(NULL);

   /* Test DB connexion */
   Pmsg1(0, PLINE "Test DB connection \"%s\"" PLINE, db_name);

#if defined(HAVE_DYNAMIC_CATS_BACKENDS)
   backend_directories = New(alist(10, owned_by_alist));
   backend_directories->append((char *)backend_directory);

   DbSetBackendDirs(backend_directories);
#endif

   if (full_test) {
      db = db_init_database(jcr, db_driver, db_name, db_user, db_password, db_address, db_port + 100, NULL);
      ok(db != NULL, "Test bad connection");
      if (!db) {
         report();
         exit (1);
      }
      nok(db_open_database(jcr, db), "Open bad Database");
      db_close_database(jcr, db);
   }

   db = db_init_database(jcr, db_driver, db_name, db_user, db_password, db_address, db_port, NULL);

   ok(db != NULL, "Test db connection");
   if (!db) {
      report();
      exit (1);
   }
   if (!ok(db_open_database(jcr, db), "Open Database")) {
      Pmsg1(000, _("Could not open database \"%s\".\n"), db_name);
      Jmsg(jcr, M_FATAL, 0, _("Could not open, database \"%s\".\n"), db_name);
      Jmsg(jcr, M_FATAL, 0, _("%s"), db_strerror(db));
      Pmsg1(000, "%s", db_strerror(db));
      db_close_database(jcr, db);
      report();
      exit (1);
   }
   dbtype = db_get_type_index(db);


   /* Check if the SQL library is thread-safe */
   //db_check_backend_thread_safe();
   ok(CheckTablesVersion(jcr, db), "Check table version");
   ok(db_sql_query(db, "SELECT VersionId FROM Version",
                   db_int_handler, &j), "SELECT VersionId");

   ok(UPDATE_DB(jcr, db, (char*)"UPDATE Version SET VersionId = 1"),
      "Update VersionId");
   nok(CheckTablesVersion(jcr, db), "Check table version");
   Mmsg(buf, "UPDATE Version SET VersionId = %d", j);
   ok(UPDATE_DB(jcr, db, buf), "Restore VersionId");

   if (dbtype != SQL_TYPE_SQLITE3) {
      ok(db_check_max_connections(jcr, db, 1), "Test min Max Connexion");
      nok(db_check_max_connections(jcr, db, 10000), "Test max Max Connexion");
   }

   ok(db_open_batch_connexion(jcr, db), "Opening batch connection");
   db_close_database(jcr, jcr->db_batch);
   jcr->db_batch = NULL;

   /* ---------------------------------------------------------------- */

   uint32_t storageid=0;
   ok(db_sql_query(db, "SELECT MIN(StorageId) FROM Storage",
                   db_int_handler, &storageid), "Get StorageId");
   ok(storageid > 0, "Check StorageId");
   if (!storageid) {
      Pmsg0(0, "Please, run REGRESS_DEBUG=1 tests/bareos-backup-test before this test");
      exit (1);
   }

   /* ---------------------------------------------------------------- */
   Pmsg0(0, PLINE "Doing Basic SQL tests" PLINE);
   ok(db_sql_query(db, "SELECT 1,2,3,4,5", count_col, &j), "Count 5 rows");
   ok(j == 5, "Check number of columns");
   ok(db_sql_query(db, "SELECT 1,2,3,4,5,'a','b','c','d','e'",
                   count_col, &j), "Count 10 rows");
   ok(j == 10, "Check number of columns");

   bsnprintf(temp, sizeof(temp), "t%lld", pid);
   ok(db_sql_query(db, "SELECT 2", db_int_handler, &j), "Good SELECT query");
   ok(db_sql_query(db, "SELECT 1 FROM Media WHERE VolumeName='missing'",
                   db_int_handler, &j), "Good empty SELECT query");

   db_int64_ctx i64;
   i64.value = 0; i64.count = 0;
   ok(db_sql_query(db, "SELECT 1",db_int64_handler, &i64),"db_int64_handler");
   ok(i64.value == 1, "Check db_int64_handler return");

   db_list_ctx lctx;
   ok(db_sql_query(db, "SELECT FileId FROM File ORDER By FileId LIMIT 10",
                   db_list_handler, &lctx), "db_list_ctx");
   ok(lctx.count == 10, "Check db_list_ctx count ");
   ok(bstrcmp(lctx.list, "1,2,3,4,5,6,7,8,9,10"), "Check db_list_ctx list");

   nok(db_sql_query(db, "blabla", db_int_handler, &j), "Bad query");

   Mmsg(buf, "CREATE Table %s (a int)", temp);
   ok(db_sql_query(db, buf, NULL, NULL), "CREATE query");

   Mmsg(buf, "INSERT INTO %s (a) VALUES (1)", temp);
   ok(INSERT_DB(jcr, db, buf), "INSERT query");
   ok(INSERT_DB(jcr, db, buf), "INSERT query");
   ok(SqlAffectedRows(db) == 1, "Check SqlAffectedRows");

   Mmsg(buf, "INSERT INTO aaa%s (a) VALUES (1)", temp);
   nok(INSERT_DB(jcr, db, buf), "Bad INSERT query");
   ok(SqlAffectedRows(db) == 0, "Check SqlAffectedRows");

   Mmsg(buf, "UPDATE %s SET a = 2", temp);
   ok(UPDATE_DB(jcr, db, buf), "UPDATE query");
   ok(SqlAffectedRows(db) == 2, "Check SqlAffectedRows");

   Mmsg(buf, "UPDATE %s SET a = 2 WHERE a = 1", temp);
   nok(UPDATE_DB(jcr, db, buf), "Empty UPDATE query");

   Mmsg(buf, "UPDATE aaa%s SET a = 2", temp);
   nok(UPDATE_DB(jcr, db, buf), "Bad UPDATE query");

   Mmsg(buf, "DELETE FROM %s", temp);
   ok(DELETE_DB(jcr, db, buf), "DELETE query");
   nok(DELETE_DB(jcr, db, buf), "Empty DELETE query"); /* TODO bug ? */

   Mmsg(buf, "DELETE FROM aaa%s", temp);
   ok(DELETE_DB(jcr, db, buf), "Bad DELETE query"); /* TODO bug ? */

   Mmsg(buf, "DROP TABLE %s", temp);
   ok(QUERY_DB(jcr, db, buf), "DROP query");
   nok(QUERY_DB(jcr, db, buf), "Empty DROP query");

   /* ---------------------------------------------------------------- */

   strcpy(buf, "This string should be 'escaped'");
   db_escape_string(jcr, db, buf2, buf, strlen(buf));
   ok((strlen(buf) + 2) == strlen(buf2),"Quoted string should be longer");
   Mmsg(buf, "INSERT INTO Path (Path) VALUES ('%lld-%s')", pid, buf2);
   ok(db_sql_query(db, buf, NULL, NULL), "Inserting quoted string");

   /* ---------------------------------------------------------------- */
   Pmsg0(0, PLINE "Doing Job tests" PLINE);

   JobDbRecord jr, jr2;
   memset(&jr, 0, sizeof(jr));
   memset(&jr2, 0, sizeof(jr2));
   jr.JobId = 1;
   ok(db_get_job_record(jcr, db, &jr), "Get Job record for JobId=1");
   ok(jr.JobFiles > 10, "Check number of files");

   jr.JobId = (JobId_t)pid;
   Mmsg(buf, "%s-%lld", jr.Job, pid);
   strcpy(jr.Job, buf);
   ok(db_create_job_record(jcr, db, &jr), "Create Job record");
   ok(db_update_job_start_record(jcr, db, &jr), "Update Start Record");
   ok(db_update_job_end_record(jcr, db, &jr), "Update End Record");
   jr2.JobId = jr.JobId;
   ok(db_get_job_record(jcr, db, &jr2), "Get Job record by JobId");
   cmp_job(jr, jr2);

   memset(&jr2, 0, sizeof(jr2));
   strcpy(jr2.Job, jr.Job);
   ok(db_get_job_record(jcr, db, &jr2), "Get Job record by Job name");
   cmp_job(jr, jr2);

   memset(&jr2, 0, sizeof(jr2));
   jr2.JobId = 99999;
   nok(db_get_job_record(jcr, db, &jr2), "Get non existing Job record (JobId)");

   memset(&jr2, 0, sizeof(jr2));
   strcpy(jr2.Job, "test");
   nok(db_get_job_record(jcr, db, &jr2), "Get non existing Job record (Job)");

   /* ---------------------------------------------------------------- */

   AttributesDbRecord ar;
   memset(&ar, 0, sizeof(ar));
   Mmsg(buf2, aPATH aPATH aPATH aPATH "/" aFILE aFILE ".txt");
   ar.fname = buf2;
   Mmsg(buf3, "gD ImIZ IGk B Po Po A A A JY BNNvf5 BNKzS7 BNNuwC A A C");
   ar.attr = buf3;
   ar.FileIndex = 10;
   ar.Stream = STREAM_UNIX_ATTRIBUTES;
   ar.FileType = FT_REG;
   jcr->JobId = ar.JobId = jr.JobId;
   jcr->JobStatus = JS_Running;
   ok(db_create_attributes_record(jcr, db, &ar), "Inserting Filename");
   ok(db_write_batch_file_records(jcr), "Commit batch session");
   Mmsg(buf, "SELECT FileIndex FROM File WHERE JobId=%lld",(int64_t)jcr->JobId);
   ok(db_sql_query(db, buf, db_int_handler, &j), "Get Inserted record");
   ok(j == ar.FileIndex, "Check FileIndex");
   Mmsg(buf, "SELECT COUNT(1) FROM File WHERE JobId=%lld",(int64_t)jcr->JobId);
   ok(db_sql_query(db, buf, db_int_handler, &j), "List records");
   ok(j == 1, "Check batch session records");
   j = 0;
   Mmsg(buf, "%lld", (uint64_t)jcr->JobId);
   ok(db_get_file_list(jcr, jcr->db_batch, buf, false, false, list_files, &j),
      "List files with db_get_file_list()");
   ok(j == 1, "Check db_get_file_list results");
   /* ---------------------------------------------------------------- */

   Pmsg0(0, PLINE "Doing Client tests" PLINE);
   ClientDbRecord cr, cr2;
   memset(&cr, 0, sizeof(cr));
   memset(&cr2, 0, sizeof(cr2));

   cr.AutoPrune = 1;
   cr.FileRetention = 10;
   cr.JobRetention = 15;
   bsnprintf(cr.Name, sizeof(cr.Name), "client-%lld-fd", pid);
   bsnprintf(cr.Uname, sizeof(cr.Uname), "uname-%lld", pid);

   ok(db_create_client_record(jcr, db, &cr), "db_create_client_record()");
   ok(cr.ClientId > 0, "Check ClientId");

   cr2.ClientId = cr.ClientId; /* Save it */
   cr.ClientId = 0;

   Pmsg0(0, "Search client by ClientId\n");
   ok(db_create_client_record(jcr, db, &cr),"Should get the client record");
   ok(cr.ClientId == cr2.ClientId,           "Check if ClientId is the same");

   ok(db_get_client_record(jcr, db, &cr2), "Search client by ClientId");
   cmp_client(cr, cr2);

   Pmsg0(0, "Search client by Name\n");
   memset(&cr2, 0, sizeof(cr2));
   strcpy(cr2.Name, cr.Name);
   ok(db_get_client_record(jcr, db, &cr2),"Search client by Name");
   cmp_client(cr, cr2);

   Pmsg0(0, "Search non existing client by Name\n");
   memset(&cr2, 0, sizeof(cr2));
   bsnprintf(cr2.Name, sizeof(cr2.Name), "hollow-client-%lld-fd", pid);
   nok(db_get_client_record(jcr, db, &cr2), "Search non existing client");
   ok(cr2.ClientId == 0, "Check ClientId after failed search");

   cr.AutoPrune = 0;
   strcpy(cr.Uname, "NewUname");
   ok(db_update_client_record(jcr, db, &cr), "Update Client record");
   memset(&cr2, 0, sizeof(cr2));
   cr2.ClientId = cr.ClientId;
   ok(db_get_client_record(jcr, db, &cr2),"Search client by ClientId");
   cmp_client(cr, cr2);

   int nb, i;
   uint32_t *ret_ids;
   ok(db_get_client_ids(jcr, db, &nb, &ret_ids), "Get Client Ids");
   ok(nb > 0, "Should find at least 1 Id");
   for (i = 0; i < nb; i++) {
      if (ret_ids[i] == cr2.ClientId) {
         break;
      }
   }
   ok(i < nb, "Check if ClientId was found");

   /* ---------------------------------------------------------------- */
   Pmsg0(0, PLINE "Doing Pool tests" PLINE);
   PoolDbRecord pr, pr2;
   memset(&pr, 0, sizeof(pr));
   memset(&pr2, 0, sizeof(pr2));

   bsnprintf(pr.Name, sizeof(pr.Name), "pool-%lld", pid);
   pr.MaxVols = 10;
   pr.UseOnce = 0;
   pr.UseCatalog = true;
   pr.AcceptAnyVolume = true;
   pr.AutoPrune = true;
   pr.Recycle = true;
   pr.VolRetention = 1000;
   pr.VolUseDuration = 1000;
   pr.MaxVolJobs = 100;
   pr.MaxVolFiles = 1000;
   pr.MaxVolBytes = 1000000;
   strcpy(pr.PoolType, "Backup");
   pr.LabelType = 0;
   pr.LabelFormat[0] = 0;
   pr.RecyclePoolId = 0;
   pr.ScratchPoolId = 0;
   pr.ActionOnPurge = 1;

   ok(db_create_pool_record(jcr, db, &pr), "db_create_pool_record()");
   ok(pr.PoolId > 0, "Check PoolId");

   pr2.PoolId = pr.PoolId;
   pr.PoolId = 0;

   Pmsg0(0, "Search pool by PoolId\n");
   nok(db_create_pool_record(jcr, db, &pr),"Can't create pool twice");
   ok(db_get_pool_record(jcr, db, &pr2), "Search pool by PoolId");
   cmp_pool(pr, pr2);

   pr2.MaxVols++;
   pr2.AutoPrune = false;
   pr2.Recycle = false;
   pr2.VolRetention++;
   pr2.VolUseDuration++;
   pr2.MaxVolJobs++;
   pr2.MaxVolFiles++;
   pr2.MaxVolBytes++;
   strcpy(pr2.PoolType, "Restore");
   strcpy(pr2.LabelFormat, "VolFormat");
   pr2.RecyclePoolId = 0;
   pr2.ScratchPoolId = 0;
   pr2.ActionOnPurge = 2;

   ok(db_update_pool_record(jcr, db, &pr2), "Update Pool record");
   memset(&pr, 0, sizeof(pr));
   pr.PoolId = pr2.PoolId;
   ok(db_get_pool_record(jcr, db, &pr), "Search pool by PoolId");
   cmp_pool(pr, pr2);

   ok(db_delete_pool_record(jcr, db, &pr), "Delete Pool");
   nok(db_delete_pool_record(jcr, db, &pr), "Delete non existing Pool");
   nok(db_update_pool_record(jcr, db, &pr), "Update non existing Pool");
   ok(db_create_pool_record(jcr, db, &pr), "Recreate Pool");

   /* ---------------------------------------------------------------- */
   Pmsg0(0, PLINE "Doing Media tests" PLINE);

   MediaDbRecord mr, mr2;
   memset(&mr, 0, sizeof(mr));
   memset(&mr2, 0, sizeof(mr2));

   bsnprintf(mr.VolumeName, sizeof(mr.VolumeName), "media-%lld", pid);
   bsnprintf(mr.MediaType, sizeof(mr.MediaType), "type-%lld", pid);

   /* from SetPoolDbrDefaultsInMediaDbr(&mr, &pr);  */
   mr.PoolId = pr.PoolId;
   bstrncpy(mr.VolStatus, NT_("Append"), sizeof(mr.VolStatus));
   mr.Recycle = pr.Recycle;
   mr.VolRetention = pr.VolRetention;
   mr.VolUseDuration = pr.VolUseDuration;
   mr.ActionOnPurge = pr.ActionOnPurge;
   mr.RecyclePoolId = pr.RecyclePoolId;
   mr.MaxVolJobs = pr.MaxVolJobs;
   mr.MaxVolFiles = pr.MaxVolFiles;
   mr.MaxVolBytes = pr.MaxVolBytes;
   mr.LabelType = pr.LabelType;
   mr.Enabled = 1;

   mr.VolCapacityBytes = 1000;
   mr.Slot = 1;
   mr.VolBytes = 1000;
   mr.InChanger = 1;
   mr.VolReadTime = 10000;
   mr.VolWriteTime = 99999;
   mr.StorageId = 0;
   mr.DeviceId = 0;
   mr.LocationId = 0;
   mr.ScratchPoolId = 0;
   mr.RecyclePoolId = 0;

   ok(db_create_media_record(jcr, db, &mr), "Create Media");
   nok(db_create_media_record(jcr, db, &mr), "Create Media twice");

   /* ---------------------------------------------------------------- */
   Pmsg0(0, PLINE "Doing ... tests" PLINE);

   db_close_database(jcr, db);
   report();
   FreePoolMemory(buf);
   FreePoolMemory(buf2);
   FreePoolMemory(buf3);
   return 0;
}
