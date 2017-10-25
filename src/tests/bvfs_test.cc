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
 * Program to test cache path
 *
 * Eric Bollengier, August 2009
 */

#include "bareos.h"
#include "jcr.h"
#include "cats/cats.h"
#include "cats/bvfs.h"
#include "findlib/find.h"

/* Local variables */
static B_DB *db;
static const char *file = "COPYRIGHT";
//static DBId_t fnid=0;

#if defined(HAVE_DYNAMIC_CATS_BACKENDS)
static const char *backend_directory = _PATH_BAREOS_BACKENDDIR;
#endif
static const char *db_name = "regress";
static const char *db_user = "regress";
static const char *db_password = "";
static const char *db_host = NULL;
static const char *db_driver = NULL;

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
"       -j <jobids>       specify jobids\n"
"       -p <path>         specify path\n"
"       -f <file>         specify file\n"
"       -l <limit>        maximum tuple to fetch\n"
"       -T                truncate cache table before starting\n"
"       -v                verbose\n"
"       -?                print this message\n\n"), 2001, VERSION, BDATE);
   exit(1);
}

static int result_handler(void *ctx, int fields, char **row)
{
   Bvfs *vfs = (Bvfs *)ctx;
   ATTR *attr = vfs->get_attr();
   char empty[] = "A A A A A A A A A A A A A A";

   memset(&attr->statp, 0, sizeof(struct stat));
   decode_stat((row[BVFS_LStat] && row[BVFS_LStat][0])?row[BVFS_LStat]:empty,
               &attr->statp, sizeof(attr->statp),  &attr->LinkFI);

   if (bvfs_is_dir(row) || bvfs_is_file(row)) {
      /* display clean stuffs */

      if (bvfs_is_dir(row)) {
         pm_strcpy(attr->ofname, bvfs_basename_dir(row[BVFS_Name]));
      } else {
         pm_strcpy(attr->ofname, row[BVFS_Name]);
      }
      print_ls_output(vfs->get_jcr(), attr);

   } else {
      Pmsg5(0, "JobId=%s FileId=%s\tMd5=%s\tVolName=%s\tVolInChanger=%s\n",
            row[BVFS_JobId], row[BVFS_FileId], row[BVFS_Md5], row[BVFS_VolName],
            row[BVFS_VolInchanger]);

      pm_strcpy(attr->ofname, file);
      print_ls_output(vfs->get_jcr(), attr);
   }
   return 0;
}


/* number of thread started */

int main (int argc, char *argv[])
{
   int ch;
#if defined(HAVE_DYNAMIC_CATS_BACKENDS)
   alist *backend_directories = NULL;
#endif
   char *jobids = (char *)"1";
   char *path=NULL, *client=NULL;
   uint64_t limit=0;
   bool clean=false;
   setlocale(LC_ALL, "");
   bindtextdomain("bareos", LOCALEDIR);
   textdomain("bareos");
   init_stack_dump();

   Dmsg0(0, "Starting bvfs_test tool\n");

   my_name_is(argc, argv, "bvfs_test");
   init_msg(NULL, NULL);

   OSDependentInit();

   while ((ch = getopt(argc, argv, "h:c:l:d:D:n:P:Su:vf:w:?j:p:f:T")) != -1) {
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
   JCR *bjcr = new_jcr(sizeof(JCR), NULL);
   bjcr->JobId = getpid();
   bjcr->setJobType(JT_CONSOLE);
   bjcr->setJobLevel(L_FULL);
   bjcr->JobStatus = JS_Running;
   bjcr->client_name = get_pool_memory(PM_FNAME);
   pm_strcpy(bjcr->client_name, "Dummy.Client.Name");
   bstrncpy(bjcr->Job, "bvfs_test", sizeof(bjcr->Job));

#if defined(HAVE_DYNAMIC_CATS_BACKENDS)
   backend_directories = New(alist(10, owned_by_alist));
   backend_directories->append((char *)backend_directory);

   db_set_backend_dirs(backend_directories);
#endif

   if ((db = db_init_database(NULL,
                              NULL,
                              db_name,
                              db_user,
                              db_password,
                              db_host,
                              0,
                              NULL,
                              false,
                              false,
                              false,
                              false)) == NULL) {
      Emsg0(M_ERROR_TERM, 0, _("Could not init Bareos database\n"));
   }
   Dmsg1(0, "db_type=%s\n", db->get_type());

   if (!db->open_database(NULL)) {
      Emsg0(M_ERROR_TERM, 0, db->strerror());
   }
   Dmsg0(200, "Database opened\n");
   if (verbose) {
      Pmsg2(000, _("Using Database: %s, User: %s\n"), db_name, db_user);
   }

   bjcr->db = db;

   if (clean) {
      Pmsg0(0, "Clean old table\n");
      db->sql_query("DELETE FROM PathHierarchy", NULL, NULL);
      db->sql_query("UPDATE Job SET HasCache=0", NULL, NULL);
      db->sql_query("DELETE FROM PathVisibility", NULL, NULL);
      db->bvfs_update_cache(bjcr);
   }

   Bvfs fs(bjcr, db);
   fs.set_handler(result_handler, &fs);

   fs.set_jobids(jobids);
   fs.update_cache();
   if (limit)
      fs.set_limit(limit);

   if (path) {
      fs.ch_dir(path);
      fs.ls_special_dirs();
      fs.ls_dirs();
      while (fs.ls_files()) {
         fs.next_offset();
      }

      //if (fnid && client) {
      if (client) {
         Pmsg0(0, "---------------------------------------------\n");
         Pmsg1(0, "Getting file version for %s\n", file);
         //fs.get_all_file_versions(fs.get_pwd(), fnid, client);
         fs.get_all_file_versions(fs.get_pwd(), file, client);
      }

      exit (0);
   }


   Pmsg0(0, "list /\n");
   fs.ch_dir("/");
   fs.ls_special_dirs();
   fs.ls_dirs();
   fs.ls_files();

   Pmsg0(0, "list /tmp/\n");
   fs.ch_dir("/tmp/");
   fs.ls_special_dirs();
   fs.ls_dirs();
   fs.ls_files();

   Pmsg0(0, "list /tmp/regress/\n");
   fs.ch_dir("/tmp/regress/");
   fs.ls_special_dirs();
   fs.ls_files();
   fs.ls_dirs();

   Pmsg0(0, "list /tmp/regress/build/\n");
   fs.ch_dir("/tmp/regress/build/");
   fs.ls_special_dirs();
   fs.ls_dirs();
   fs.ls_files();

   //fs.get_all_file_versions(1, 347, "zog4-fd");
   fs.get_all_file_versions(1, "testfile.txt", "zog4-fd");

   char p[200];
   strcpy(p, "/tmp/toto/rep/");
   bvfs_parent_dir(p);
   if(!bstrcmp(p, "/tmp/toto/")) {
      Pmsg0(000, "Error in bvfs_parent_dir\n");
   }
   bvfs_parent_dir(p);
   if(!bstrcmp(p, "/tmp/")) {
      Pmsg0(000, "Error in bvfs_parent_dir\n");
   }
   bvfs_parent_dir(p);
   if(!bstrcmp(p, "/")) {
      Pmsg0(000, "Error in bvfs_parent_dir\n");
   }
   bvfs_parent_dir(p);
   if(!bstrcmp(p, "")) {
      Pmsg0(000, "Error in bvfs_parent_dir\n");
   }
   bvfs_parent_dir(p);
   if(!bstrcmp(p, "")) {
      Pmsg0(000, "Error in bvfs_parent_dir\n");
   }

   return 0;
}
