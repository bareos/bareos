/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2001-2011 Free Software Foundation Europe e.V.
   Copyright (C) 2011-2016 Planets Communications B.V.
   Copyright (C) 2013-2016 Bareos GmbH & Co. KG

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
 * Kern E. Sibbald, December 2001
 */
/**
 * @file
 * Program to scan a Bareos Volume and compare it with
 * the catalog and optionally synchronize the catalog
 * with the tape.
 */

#include "bareos.h"
#include "stored.h"
#include "lib/crypto_cache.h"
#include "findlib/find.h"
#include "cats/cats.h"

/* Dummy functions */
extern bool parse_sd_config(CONFIG *config, const char *configfile, int exit_code);

/* Forward referenced functions */
static void do_scan(void);
static bool record_cb(DCR *dcr, DEV_RECORD *rec);
static bool create_file_attributes_record(B_DB *db, JCR *mjcr,
                                          char *fname, char *lname, int type,
                                          char *ap, DEV_RECORD *rec);
static bool create_media_record(B_DB *db, MEDIA_DBR *mr, VOLUME_LABEL *vl);
static bool update_media_record(B_DB *db, MEDIA_DBR *mr);
static bool create_pool_record(B_DB *db, POOL_DBR *pr);
static JCR *create_job_record(B_DB *db, JOB_DBR *mr, SESSION_LABEL *label, DEV_RECORD *rec);
static bool update_job_record(B_DB *db, JOB_DBR *mr, SESSION_LABEL *elabel, DEV_RECORD *rec);
static bool create_client_record(B_DB *db, CLIENT_DBR *cr);
static bool create_fileset_record(B_DB *db, FILESET_DBR *fsr);
static bool create_jobmedia_record(B_DB *db, JCR *jcr);
static JCR *create_jcr(JOB_DBR *jr, DEV_RECORD *rec, uint32_t JobId);
static bool update_digest_record(B_DB *db, char *digest, DEV_RECORD *rec, int type);

/* Local variables */
static DEVICE *dev = NULL;
static B_DB *db;
static JCR *bjcr;                     /* jcr for bscan */
static BSR *bsr = NULL;
static MEDIA_DBR mr;
static POOL_DBR pr;
static JOB_DBR jr;
static CLIENT_DBR cr;
static FILESET_DBR fsr;
static ROBJECT_DBR rop;
static ATTR_DBR ar;
static FILE_DBR fr;
static SESSION_LABEL label;
static SESSION_LABEL elabel;
static ATTR *attr;

static time_t lasttime = 0;

static const char *backend_directory = _PATH_BAREOS_BACKENDDIR;
static const char *backend_query_directory = _PATH_BAREOS_BACKENDQUERYDIR;
static const char *db_driver = "NULL";
static const char *db_name = "bareos";
static const char *db_user = "bareos";
static const char *db_password = "";
static const char *db_host = NULL;
static int db_port = 0;
static const char *wd = NULL;
static bool update_db = false;
static bool update_vol_info = false;
static bool list_records = false;
static int ignored_msgs = 0;

static uint64_t currentVolumeSize;
static int last_pct = -1;
static bool showProgress = false;
static int num_jobs = 0;
static int num_pools = 0;
static int num_media = 0;
static int num_files = 0;
static int num_restoreobjects = 0;

static void usage()
{
   fprintf(stderr, _(
PROG_COPYRIGHT
"\nVersion: %s (%s)\n\n"
"Usage: bscan [ options ] <device-name>\n"
"       -B <drivername>   specify the database driver name (default NULL) <postgresql|mysql|sqlite3>\n"
"       -b <bootstrap>    specify a bootstrap file\n"
"       -c <path>         specify a Storage configuration file or directory\n"
"       -d <nnn>          set debug level to <nnn>\n"
"       -dt               print timestamp in debug output\n"
"       -m                update media info in database\n"
"       -D <director>     specify a director name specified in the storage daemon\n"
"                         configuration file for the Key Encryption Key selection\n"
"       -a <directory>    specify the database backend directory (default %s)\n"
"       -q <directory>    specify the query backend directory (default %s)\n"
"       -n <name>         specify the database name (default bareos)\n"
"       -u <user>         specify database user name (default bareos)\n"
"       -P <password>     specify database password (default none)\n"
"       -h <host>         specify database host (default NULL)\n"
"       -t <port>         specify database port (default 0)\n"
"       -p                proceed inspite of I/O errors\n"
"       -r                list records\n"
"       -s                synchronize or store in database\n"
"       -S                show scan progress periodically\n"
"       -v                verbose\n"
"       -V <Volumes>      specify Volume names (separated by |)\n"
"       -w <directory>    specify working directory (default from configuration file)\n"
"       -?                print this message\n\n"
"example:\n"
"bscan -B postgresql -V Full-0001 FileStorage\n"),
            2001, VERSION, BDATE, backend_directory, backend_query_directory);
   exit(1);
}

int main (int argc, char *argv[])
{
   int ch;
   struct stat stat_buf;
   char *VolumeName = NULL;
   char *DirectorName = NULL;
   DIRRES *director = NULL;
   DCR *dcr;
#if defined(HAVE_DYNAMIC_CATS_BACKENDS)
   alist *backend_directories = NULL;
#endif
   alist *backend_query_directories = NULL;

   setlocale(LC_ALL, "");
   bindtextdomain("bareos", LOCALEDIR);
   textdomain("bareos");
   init_stack_dump();
   lmgr_init_thread();

   my_name_is(argc, argv, "bscan");
   init_msg(NULL, NULL);

   OSDependentInit();

   while ((ch = getopt(argc, argv, "a:B:b:c:d:D:h:p:mn:pP:q:rsSt:u:vV:w:?")) != -1) {
      switch (ch) {
      case 'a':
         backend_directory = optarg;
         break;

      case 'B':
         db_driver = optarg;
         break;

      case 'b':
         bsr = parse_bsr(NULL, optarg);
         break;

      case 'c':                    /* specify config file */
         if (configfile != NULL) {
            free(configfile);
         }
         configfile = bstrdup(optarg);
         break;

      case 'D':                    /* specify director name */
         if (DirectorName != NULL) {
            free(DirectorName);
         }
         DirectorName = bstrdup(optarg);
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

      case 'h':
         db_host = optarg;
         break;

      case 't':
         db_port = atoi(optarg);
         break;

      case 'm':
         update_vol_info = true;
         break;

      case 'n':
         db_name = optarg;
         break;

      case 'P':
         db_password = optarg;
         break;

      case 'p':
         forge_on = true;
         break;

      case 'q':
         backend_query_directory = optarg;
         break;

      case 'r':
         list_records = true;
         break;

      case 'S' :
         showProgress = true;
         break;

      case 's':
         update_db = true;
         break;

      case 'u':
         db_user = optarg;
         break;

      case 'V':                    /* Volume name */
         VolumeName = optarg;
         break;

      case 'v':
         verbose++;
         break;

      case 'w':
         wd = optarg;
         break;

      case '?':
      default:
         usage();

      }
   }
   argc -= optind;
   argv += optind;

   if (argc != 1) {
      Pmsg0(0, _("Wrong number of arguments: \n"));
      usage();
   }

   my_config = new_config_parser();
   parse_sd_config(my_config, configfile, M_ERROR_TERM);

   if (DirectorName) {
      foreach_res(director, R_DIRECTOR) {
         if (bstrcmp(director->name(), DirectorName)) {
            break;
         }
      }
      if (!director) {
         Emsg2(M_ERROR_TERM, 0, _("No Director resource named %s defined in %s. Cannot continue.\n"),
               DirectorName, configfile);
      }
   }

   load_sd_plugins(me->plugin_directory, me->plugin_names);

   read_crypto_cache(me->working_directory, "bareos-sd",
                     get_first_port_host_order(me->SDaddrs));

   /* Check if -w option given, otherwise use resource for working directory */
   if (wd) {
      working_directory = wd;
   } else if (!me->working_directory) {
      Emsg1(M_ERROR_TERM, 0, _("No Working Directory defined in %s. Cannot continue.\n"),
         configfile);
   } else {
      working_directory = me->working_directory;
   }

   /* Check that working directory is good */
   if (stat(working_directory, &stat_buf) != 0) {
      Emsg1(M_ERROR_TERM, 0, _("Working Directory: %s not found. Cannot continue.\n"),
            working_directory);
   }
   if (!S_ISDIR(stat_buf.st_mode)) {
      Emsg1(M_ERROR_TERM, 0, _("Working Directory: %s is not a directory. Cannot continue.\n"),
            working_directory);
   }

   dcr = New(DCR);
   bjcr = setup_jcr("bscan", argv[0], bsr, director, dcr, VolumeName, true);
   if (!bjcr) {
      exit(1);
   }
   dev = bjcr->read_dcr->dev;

   if (showProgress) {
      char ed1[50];
      struct stat sb;
      fstat(dev->fd(), &sb);
      currentVolumeSize = sb.st_size;
      Pmsg1(000, _("First Volume Size = %s\n"),
         edit_uint64(currentVolumeSize, ed1));
   }

#if defined(HAVE_DYNAMIC_CATS_BACKENDS)
   backend_directories = New(alist(10, owned_by_alist));
   backend_directories->append((char *)backend_directory);

   db_set_backend_dirs(backend_directories);
#endif
   backend_query_directories = New(alist(10, owned_by_alist));
   backend_query_directories->append((char *)backend_query_directory);

   db_set_query_dirs(backend_query_directories);

   db = db_init_database(NULL,
                         db_driver,
                         db_name,
                         db_user,
                         db_password,
                         db_host,
                         db_port,
                         NULL,
                         false,
                         false,
                         false,
                         false);
   if (db == NULL) {
      Emsg0(M_ERROR_TERM, 0, _("Could not init Bareos database\n"));
   }
   if (!db->open_database(NULL)) {
      Emsg0(M_ERROR_TERM, 0, db->strerror());
   }
   Dmsg0(200, "Database opened\n");
   if (verbose) {
      Pmsg2(000, _("Using Database: %s, User: %s\n"), db_name, db_user);
   }

   do_scan();
   if (update_db) {
      printf("Records added or updated in the catalog:\n%7d Media\n"
             "%7d Pool\n%7d Job\n%7d File\n%7d RestoreObject\n",
             num_media, num_pools, num_jobs, num_files, num_restoreobjects);
   } else {
      printf("Records would have been added or updated in the catalog:\n"
             "%7d Media\n%7d Pool\n%7d Job\n%7d File\n%7d RestoreObject\n",
             num_media, num_pools, num_jobs, num_files, num_restoreobjects);
   }
   db_flush_backends();

   clean_device(bjcr->dcr);
   dev->term();
   free_dcr(bjcr->dcr);
   free_jcr(bjcr);

   return 0;
}

/**
 * We are at the end of reading a tape. Now, we simulate handling
 * the end of writing a tape by wiffling through the attached
 * jcrs creating jobmedia records.
 */
static bool bscan_mount_next_read_volume(DCR *dcr)
{
   bool status;
   DEVICE *dev = dcr->dev;
   DCR *mdcr;

   Dmsg1(100, "Walk attached jcrs. Volume=%s\n", dev->getVolCatName());
   foreach_dlist(mdcr, dev->attached_dcrs) {
      JCR *mjcr = mdcr->jcr;
      Dmsg1(000, "========== JobId=%u ========\n", mjcr->JobId);
      if (mjcr->JobId == 0) {
         continue;
      }
      if (verbose) {
         Pmsg1(000, _("Create JobMedia for Job %s\n"), mjcr->Job);
      }
      mdcr->StartBlock = dcr->StartBlock;
      mdcr->StartFile = dcr->StartFile;
      mdcr->EndBlock = dcr->EndBlock;
      mdcr->EndFile = dcr->EndFile;
      mdcr->VolMediaId = dcr->VolMediaId;
      mjcr->read_dcr->VolLastIndex = dcr->VolLastIndex;
      if( mjcr->insert_jobmedia_records ) {
         if (!create_jobmedia_record(db, mjcr)) {
            Pmsg2(000, _("Could not create JobMedia record for Volume=%s Job=%s\n"),
               dev->getVolCatName(), mjcr->Job);
         }
      }
   }

   update_media_record(db, &mr);

   /* Now let common read routine get up next tape. Note,
    * we call mount_next... with bscan's jcr because that is where we
    * have the Volume list, but we get attached.
    */
   status = mount_next_read_volume(dcr);
   if (showProgress) {
      char ed1[50];
      struct stat sb;
      fstat(dev->fd(), &sb);
      currentVolumeSize = sb.st_size;
      Pmsg1(000, _("First Volume Size = %s\n"),
         edit_uint64(currentVolumeSize, ed1));
   }
   return status;
}

static void do_scan()
{
   attr = new_attr(bjcr);

   memset(&ar, 0, sizeof(ar));
   memset(&pr, 0, sizeof(pr));
   memset(&jr, 0, sizeof(jr));
   memset(&cr, 0, sizeof(cr));
   memset(&fsr, 0, sizeof(fsr));
   memset(&fr, 0, sizeof(fr));

   /*
    * Detach bscan's jcr as we are not a real Job on the tape
    */
   read_records(bjcr->read_dcr, record_cb, bscan_mount_next_read_volume);

   if (update_db) {
      db->write_batch_file_records(bjcr); /* used by bulk batch file insert */
   }

   free_attr(attr);
}

/**
 * Returns: true  if OK
 *          false if error
 */
static inline bool unpack_restore_object(JCR *jcr, int32_t stream, char *rec, int32_t reclen, ROBJECT_DBR *rop)
{
   char *bp;
   uint32_t JobFiles;

   if (sscanf(rec, "%d %d %d %d %d %d ",
              &JobFiles, &rop->Stream, &rop->object_index, &rop->object_len,
              &rop->object_full_len, &rop->object_compression) != 6) {
      return false;
   }

   /*
    * Skip over the first 6 fields we scanned in the previous scan.
    */
   bp = rec;
   for (int i = 0; i < 6; i++) {
      bp = strchr(bp, ' ');
      if (bp) {
         bp++;
      } else {
         return false;
      }
   }

   rop->plugin_name = bp;
   rop->object_name = rop->plugin_name + strlen(rop->plugin_name) + 1;
   rop->object = rop->object_name + strlen(rop->object_name) + 1;

   return true;
}

/**
 * Returns: true  if OK
 *          false if error
 */
static bool record_cb(DCR *dcr, DEV_RECORD *rec)
{
   JCR *mjcr;
   char ec1[30];
   DEVICE *dev = dcr->dev;
   JCR *bjcr = dcr->jcr;
   DEV_BLOCK *block = dcr->block;
   POOL_MEM sql_buffer;
   db_int64_ctx jmr_count;
   char digest[BASE64_SIZE(CRYPTO_DIGEST_MAX_SIZE)];

   if (rec->data_len > 0) {
      mr.VolBytes += rec->data_len + WRITE_RECHDR_LENGTH; /* Accumulate Volume bytes */
      if (showProgress && currentVolumeSize > 0) {
         int pct = (mr.VolBytes * 100) / currentVolumeSize;
         if (pct != last_pct) {
            fprintf(stdout, _("done: %d%%\n"), pct);
            fflush(stdout);
            last_pct = pct;
         }
      }
   }

   if (list_records) {
      Pmsg5(000, _("Record: SessId=%u SessTim=%u FileIndex=%d Stream=%d len=%u\n"),
            rec->VolSessionId, rec->VolSessionTime, rec->FileIndex,
            rec->Stream, rec->data_len);
   }

   /*
    * Check for Start or End of Session Record
    */
   if (rec->FileIndex < 0) {
      bool save_update_db = update_db;

      if (verbose > 1) {
         dump_label_record(dev, rec, true);
      }
      switch (rec->FileIndex) {
      case PRE_LABEL:
         Pmsg0(000, _("Volume is prelabeled. This tape cannot be scanned.\n"));
         return false;
         break;

      case VOL_LABEL:
         unser_volume_label(dev, rec);
         /*
          * Check Pool info
          */
         bstrncpy(pr.Name, dev->VolHdr.PoolName, sizeof(pr.Name));
         bstrncpy(pr.PoolType, dev->VolHdr.PoolType, sizeof(pr.PoolType));
         num_pools++;
         if (db->get_pool_record(bjcr, &pr)) {
            if (verbose) {
               Pmsg1(000, _("Pool record for %s found in DB.\n"), pr.Name);
            }
         } else {
            if (!update_db) {
               Pmsg1(000, _("VOL_LABEL: Pool record not found for Pool: %s\n"),
                  pr.Name);
            }
            create_pool_record(db, &pr);
         }
         if (!bstrcmp(pr.PoolType, dev->VolHdr.PoolType)) {
            Pmsg2(000, _("VOL_LABEL: PoolType mismatch. DB=%s Vol=%s\n"),
               pr.PoolType, dev->VolHdr.PoolType);
            return true;
         } else if (verbose) {
            Pmsg1(000, _("Pool type \"%s\" is OK.\n"), pr.PoolType);
         }

         /*
          * Check Media Info
          */
         memset(&mr, 0, sizeof(mr));
         bstrncpy(mr.VolumeName, dev->VolHdr.VolumeName, sizeof(mr.VolumeName));
         mr.PoolId = pr.PoolId;
         num_media++;
         if (db->get_media_record(bjcr, &mr)) {
            if (verbose) {
               Pmsg1(000, _("Media record for %s found in DB.\n"), mr.VolumeName);
            }
            /*
             * Clear out some volume statistics that will be updated
             */
            mr.VolJobs = mr.VolFiles = mr.VolBlocks = 0;
            mr.VolBytes = rec->data_len + 20;
         } else {
            if (!update_db) {
               Pmsg1(000, _("VOL_LABEL: Media record not found for Volume: %s\n"),
                  mr.VolumeName);
            }
            bstrncpy(mr.MediaType, dev->VolHdr.MediaType, sizeof(mr.MediaType));
            create_media_record(db, &mr, &dev->VolHdr);
         }
         if (!bstrcmp(mr.MediaType, dev->VolHdr.MediaType)) {
            Pmsg2(000, _("VOL_LABEL: MediaType mismatch. DB=%s Vol=%s\n"), mr.MediaType, dev->VolHdr.MediaType);
            return true;              /* ignore error */
         } else if (verbose) {
            Pmsg1(000, _("Media type \"%s\" is OK.\n"), mr.MediaType);
         }

         /*
          * Reset some DCR variables
          */
         foreach_dlist(dcr, dev->attached_dcrs) {
            dcr->VolFirstIndex = dcr->FileIndex = 0;
            dcr->StartBlock = dcr->EndBlock = 0;
            dcr->StartFile = dcr->EndFile = 0;
            dcr->VolMediaId = 0;
         }

         Pmsg1(000, _("VOL_LABEL: OK for Volume: %s\n"), mr.VolumeName);
         break;

      case SOS_LABEL:
         if (bsr && rec->match_stat < 1) {
            /*
             * Skipping record, because does not match BSR filter
             */
            Dmsg0(200, _("SOS_LABEL skipped. Record does not match BSR filter.\n"));
         } else {
            mr.VolJobs++;
            num_jobs++;
            if (ignored_msgs > 0) {
               Pmsg1(000, _("%d \"errors\" ignored before first Start of Session record.\n"), ignored_msgs);
               ignored_msgs = 0;
            }
            unser_session_label(&label, rec);
            memset(&jr, 0, sizeof(jr));
            bstrncpy(jr.Job, label.Job, sizeof(jr.Job));
            if (db->get_job_record(bjcr, &jr)) {
               /*
                * Job record already exists in DB
                */
               update_db = false;  /* don't change db in create_job_record */
               if (verbose) {
                  Pmsg1(000, _("SOS_LABEL: Found Job record for JobId: %d\n"), jr.JobId);
               }
            } else {
               /*
                * Must create a Job record in DB
                */
               if (!update_db) {
                  Pmsg1(000, _("SOS_LABEL: Job record not found for JobId: %d\n"), jr.JobId);
               }
            }

            /*
             * Create Client record if not already there
             */
            bstrncpy(cr.Name, label.ClientName, sizeof(cr.Name));
            create_client_record(db, &cr);
            jr.ClientId = cr.ClientId;

            /*
             * Process label, if Job record exists don't update db
             */
            mjcr = create_job_record(db, &jr, &label, rec);
            dcr = mjcr->read_dcr;
            update_db = save_update_db;

            jr.PoolId = pr.PoolId;
            mjcr->start_time = jr.StartTime;
            mjcr->setJobLevel(jr.JobLevel);

            mjcr->client_name = get_pool_memory(PM_FNAME);
            pm_strcpy(mjcr->client_name, label.ClientName);
            mjcr->fileset_name = get_pool_memory(PM_FNAME);
            pm_strcpy(mjcr->fileset_name, label.FileSetName);
            bstrncpy(dcr->pool_type, label.PoolType, sizeof(dcr->pool_type));
            bstrncpy(dcr->pool_name, label.PoolName, sizeof(dcr->pool_name));

            /*
             * Look for existing Job Media records for this job.  If there are
             * any, no new ones need be created.  This may occur if File
             * Retention has expired before Job Retention, or if the volume
             * has already been bscan'd
             */
            Mmsg(sql_buffer, "SELECT count(*) from JobMedia where JobId=%d", jr.JobId);
            db->sql_query(sql_buffer.c_str(), db_int64_handler, &jmr_count);
            if( jmr_count.value > 0 ) {
               mjcr->insert_jobmedia_records = false;
            } else {
               mjcr->insert_jobmedia_records = true;
            }

            if (rec->VolSessionId != jr.VolSessionId) {
               Pmsg3(000, _("SOS_LABEL: VolSessId mismatch for JobId=%u. DB=%d Vol=%d\n"), jr.JobId, jr.VolSessionId, rec->VolSessionId);
               return true;              /* ignore error */
            }
            if (rec->VolSessionTime != jr.VolSessionTime) {
               Pmsg3(000, _("SOS_LABEL: VolSessTime mismatch for JobId=%u. DB=%d Vol=%d\n"), jr.JobId, jr.VolSessionTime, rec->VolSessionTime);
               return true;              /* ignore error */
            }
            if (jr.PoolId != pr.PoolId) {
               Pmsg3(000, _("SOS_LABEL: PoolId mismatch for JobId=%u. DB=%d Vol=%d\n"), jr.JobId, jr.PoolId, pr.PoolId);
               return true;              /* ignore error */
            }
         }
         break;

      case EOS_LABEL:
         if (bsr && rec->match_stat < 1) {
            /*
             * Skipping record, because does not match BSR filter
             */
            Dmsg0(200, _("EOS_LABEL skipped. Record does not match BSR filter.\n"));
         } else {
            unser_session_label(&elabel, rec);

            /*
             * Create FileSet record
             */
            bstrncpy(fsr.FileSet, label.FileSetName, sizeof(fsr.FileSet));
            bstrncpy(fsr.MD5, label.FileSetMD5, sizeof(fsr.MD5));
            create_fileset_record(db, &fsr);
            jr.FileSetId = fsr.FileSetId;

            mjcr = get_jcr_by_session(rec->VolSessionId, rec->VolSessionTime);
            if (!mjcr) {
               Pmsg2(000, _("Could not find SessId=%d SessTime=%d for EOS record.\n"), rec->VolSessionId, rec->VolSessionTime);
               break;
            }

            /*
             * Do the final update to the Job record
             */
            update_job_record(db, &jr, &elabel, rec);

            mjcr->end_time = jr.EndTime;
            mjcr->setJobStatus(JS_Terminated);

            /*
             * Create JobMedia record
             */
            mjcr->read_dcr->VolLastIndex = dcr->VolLastIndex;
            if( mjcr->insert_jobmedia_records ) {
               create_jobmedia_record(db, mjcr);
            }
            free_dcr(mjcr->read_dcr);
            free_jcr(mjcr);
         }
         break;

      case EOM_LABEL:
         break;

      case EOT_LABEL:              /* end of all tapes */
         /*
          * Wiffle through all jobs still open and close them.
          */
         if (update_db) {
            DCR *mdcr;
            foreach_dlist(mdcr, dev->attached_dcrs) {
               JCR *mjcr = mdcr->jcr;
               if (!mjcr || mjcr->JobId == 0) {
                  continue;
               }
               jr.JobId = mjcr->JobId;
               jr.JobStatus = JS_ErrorTerminated; /* Mark Job as Error Terimined */
               jr.JobFiles = mjcr->JobFiles;
               jr.JobBytes = mjcr->JobBytes;
               jr.VolSessionId = mjcr->VolSessionId;
               jr.VolSessionTime = mjcr->VolSessionTime;
               jr.JobTDate = (utime_t)mjcr->start_time;
               jr.ClientId = mjcr->ClientId;
               if (!db->update_job_end_record(bjcr, &jr)) {
                  Pmsg1(0, _("Could not update job record. ERR=%s\n"), db->strerror());
               }
               mjcr->read_dcr = NULL;
               free_jcr(mjcr);
            }
         }
         mr.VolFiles = rec->File;
         mr.VolBlocks = rec->Block;
         mr.VolBytes += mr.VolBlocks * WRITE_BLKHDR_LENGTH; /* approx. */
         mr.VolMounts++;
         update_media_record(db, &mr);
         Pmsg3(0, _("End of all Volumes. VolFiles=%u VolBlocks=%u VolBytes=%s\n"), mr.VolFiles,
                    mr.VolBlocks, edit_uint64_with_commas(mr.VolBytes, ec1));
         break;
      default:
         break;
      } /* end switch */
      return true;
   }

   mjcr = get_jcr_by_session(rec->VolSessionId, rec->VolSessionTime);
   if (!mjcr) {
      if (mr.VolJobs > 0) {
         Pmsg2(000, _("Could not find Job for SessId=%d SessTime=%d record.\n"),
                      rec->VolSessionId, rec->VolSessionTime);
      } else {
         ignored_msgs++;
      }
      return true;
   }
   dcr = mjcr->read_dcr;
   if (dcr->VolFirstIndex == 0) {
      dcr->VolFirstIndex = block->FirstIndex;
   }

   /*
    * File Attributes stream
    */
   switch (rec->maskedStream) {
   case STREAM_UNIX_ATTRIBUTES:
   case STREAM_UNIX_ATTRIBUTES_EX:
      if (!unpack_attributes_record(bjcr, rec->Stream, rec->data, rec->data_len, attr)) {
         Emsg0(M_ERROR_TERM, 0, _("Cannot continue.\n"));
      }

      if (verbose > 1) {
         decode_stat(attr->attr, &attr->statp, sizeof(attr->statp), &attr->LinkFI);
         build_attr_output_fnames(bjcr, attr);
         print_ls_output(bjcr, attr);
      }
      fr.JobId = mjcr->JobId;
      fr.FileId = 0;
      num_files++;
      if (verbose && (num_files & 0x7FFF) == 0) {
         char ed1[30], ed2[30], ed3[30], ed4[30];
         Pmsg4(000, _("%s file records. At file:blk=%s:%s bytes=%s\n"),
                     edit_uint64_with_commas(num_files, ed1),
                     edit_uint64_with_commas(rec->File, ed2),
                     edit_uint64_with_commas(rec->Block, ed3),
                     edit_uint64_with_commas(mr.VolBytes, ed4));
      }
      create_file_attributes_record(db, mjcr, attr->fname, attr->lname, attr->type, attr->attr, rec);
      free_jcr(mjcr);
      break;

   case STREAM_RESTORE_OBJECT:
      if (!unpack_restore_object(bjcr, rec->Stream, rec->data, rec->data_len, &rop)) {
         Emsg0(M_ERROR_TERM, 0, _("Cannot continue.\n"));
      }
      rop.FileIndex = mjcr->FileId;
      rop.JobId = mjcr->JobId;


      if (update_db) {
         db->create_restore_object_record(mjcr, &rop);
      }

      num_restoreobjects++;

      free_jcr(mjcr);                 /* done using JCR */
      break;

   /*
    * Data streams
    */
   case STREAM_WIN32_DATA:
   case STREAM_FILE_DATA:
   case STREAM_SPARSE_DATA:
   case STREAM_MACOS_FORK_DATA:
   case STREAM_ENCRYPTED_FILE_DATA:
   case STREAM_ENCRYPTED_WIN32_DATA:
   case STREAM_ENCRYPTED_MACOS_FORK_DATA:
      /*
       * For encrypted stream, this is an approximation.
       * The data must be decrypted to know the correct length.
       */
      mjcr->JobBytes += rec->data_len;
      if (rec->maskedStream == STREAM_SPARSE_DATA) {
         mjcr->JobBytes -= sizeof(uint64_t);
      }

      free_jcr(mjcr);                 /* done using JCR */
      break;

   case STREAM_GZIP_DATA:
   case STREAM_COMPRESSED_DATA:
   case STREAM_ENCRYPTED_FILE_GZIP_DATA:
   case STREAM_ENCRYPTED_FILE_COMPRESSED_DATA:
   case STREAM_ENCRYPTED_WIN32_GZIP_DATA:
   case STREAM_ENCRYPTED_WIN32_COMPRESSED_DATA:
      /*
       * Not correct, we should (decrypt and) expand it.
       */
      mjcr->JobBytes += rec->data_len;
      free_jcr(mjcr);
      break;

   case STREAM_SPARSE_GZIP_DATA:
   case STREAM_SPARSE_COMPRESSED_DATA:
      mjcr->JobBytes += rec->data_len - sizeof(uint64_t); /* Not correct, we should expand it */
      free_jcr(mjcr);                 /* done using JCR */
      break;

   /*
    * Win32 GZIP stream
    */
   case STREAM_WIN32_GZIP_DATA:
   case STREAM_WIN32_COMPRESSED_DATA:
      mjcr->JobBytes += rec->data_len;
      free_jcr(mjcr);                 /* done using JCR */
      break;

   case STREAM_MD5_DIGEST:
      bin_to_base64(digest, sizeof(digest), (char *)rec->data, CRYPTO_DIGEST_MD5_SIZE, true);
      if (verbose > 1) {
         Pmsg1(000, _("Got MD5 record: %s\n"), digest);
      }
      update_digest_record(db, digest, rec, CRYPTO_DIGEST_MD5);
      break;

   case STREAM_SHA1_DIGEST:
      bin_to_base64(digest, sizeof(digest), (char *)rec->data, CRYPTO_DIGEST_SHA1_SIZE, true);
      if (verbose > 1) {
         Pmsg1(000, _("Got SHA1 record: %s\n"), digest);
      }
      update_digest_record(db, digest, rec, CRYPTO_DIGEST_SHA1);
      break;

   case STREAM_SHA256_DIGEST:
      bin_to_base64(digest, sizeof(digest), (char *)rec->data, CRYPTO_DIGEST_SHA256_SIZE, true);
      if (verbose > 1) {
         Pmsg1(000, _("Got SHA256 record: %s\n"), digest);
      }
      update_digest_record(db, digest, rec, CRYPTO_DIGEST_SHA256);
      break;

   case STREAM_SHA512_DIGEST:
      bin_to_base64(digest, sizeof(digest), (char *)rec->data, CRYPTO_DIGEST_SHA512_SIZE, true);
      if (verbose > 1) {
         Pmsg1(000, _("Got SHA512 record: %s\n"), digest);
      }
      update_digest_record(db, digest, rec, CRYPTO_DIGEST_SHA512);
      break;

   case STREAM_ENCRYPTED_SESSION_DATA:
      // TODO landonf: Investigate crypto support in bscan
      if (verbose > 1) {
         Pmsg0(000, _("Got signed digest record\n"));
      }
      break;

   case STREAM_SIGNED_DIGEST:
      // TODO landonf: Investigate crypto support in bscan
      if (verbose > 1) {
         Pmsg0(000, _("Got signed digest record\n"));
      }
      break;

   case STREAM_PROGRAM_NAMES:
      if (verbose) {
         Pmsg1(000, _("Got Prog Names Stream: %s\n"), rec->data);
      }
      break;

   case STREAM_PROGRAM_DATA:
      if (verbose > 1) {
         Pmsg0(000, _("Got Prog Data Stream record.\n"));
      }
      break;

   case STREAM_HFSPLUS_ATTRIBUTES:
      /* Ignore OSX attributes */
      break;

   case STREAM_PLUGIN_NAME:
   case STREAM_PLUGIN_DATA:
      /* Ignore plugin data */
      break;

   case STREAM_UNIX_ACCESS_ACL:          /* Deprecated Standard ACL attributes on UNIX */
   case STREAM_UNIX_DEFAULT_ACL:         /* Deprecated Default ACL attributes on UNIX */
   case STREAM_ACL_AIX_TEXT:
   case STREAM_ACL_DARWIN_ACCESS_ACL:
   case STREAM_ACL_FREEBSD_DEFAULT_ACL:
   case STREAM_ACL_FREEBSD_ACCESS_ACL:
   case STREAM_ACL_HPUX_ACL_ENTRY:
   case STREAM_ACL_IRIX_DEFAULT_ACL:
   case STREAM_ACL_IRIX_ACCESS_ACL:
   case STREAM_ACL_LINUX_DEFAULT_ACL:
   case STREAM_ACL_LINUX_ACCESS_ACL:
   case STREAM_ACL_TRU64_DEFAULT_ACL:
   case STREAM_ACL_TRU64_DEFAULT_DIR_ACL:
   case STREAM_ACL_TRU64_ACCESS_ACL:
   case STREAM_ACL_SOLARIS_ACLENT:
   case STREAM_ACL_SOLARIS_ACE:
   case STREAM_ACL_AFS_TEXT:
   case STREAM_ACL_AIX_AIXC:
   case STREAM_ACL_AIX_NFS4:
   case STREAM_ACL_FREEBSD_NFS4_ACL:
   case STREAM_ACL_HURD_DEFAULT_ACL:
   case STREAM_ACL_HURD_ACCESS_ACL:
   case STREAM_ACL_PLUGIN:
      /* Ignore Unix ACL attributes */
      break;

   case STREAM_XATTR_PLUGIN:
   case STREAM_XATTR_HURD:
   case STREAM_XATTR_IRIX:
   case STREAM_XATTR_TRU64:
   case STREAM_XATTR_AIX:
   case STREAM_XATTR_OPENBSD:
   case STREAM_XATTR_SOLARIS_SYS:
   case STREAM_XATTR_SOLARIS:
   case STREAM_XATTR_DARWIN:
   case STREAM_XATTR_FREEBSD:
   case STREAM_XATTR_LINUX:
   case STREAM_XATTR_NETBSD:
      /* Ignore Unix Extended attributes */
      break;

   case STREAM_NDMP_SEPARATOR:
      /* Ignore NDMP separators */
      break;

   default:
      Pmsg2(0, _("Unknown stream type!!! stream=%d len=%i\n"), rec->Stream, rec->data_len);
      break;
   }

   return true;
}

/**
 * Free the Job Control Record if no one is still using it.
 *  Called from main free_jcr() routine in src/lib/jcr.c so
 *  that we can do our Director specific cleanup of the jcr.
 */
static void bscan_free_jcr(JCR *jcr)
{
   Dmsg0(200, "Start bscan free_jcr\n");

   if (jcr->file_bsock) {
      Dmsg0(200, "Close File bsock\n");
      jcr->file_bsock->close();
      delete jcr->file_bsock;
      jcr->file_bsock = NULL;
   }

   if (jcr->store_bsock) {
      Dmsg0(200, "Close Store bsock\n");
      jcr->store_bsock->close();
      delete jcr->store_bsock;
      jcr->store_bsock = NULL;
   }

   if (jcr->RestoreBootstrap) {
      free(jcr->RestoreBootstrap);
   }

   if (jcr->dcr) {
      free_dcr(jcr->dcr);
      jcr->dcr = NULL;
   }

   if (jcr->read_dcr) {
      free_dcr(jcr->read_dcr);
      jcr->read_dcr = NULL;
   }

   Dmsg0(200, "End bscan free_jcr\n");
}

/**
 * We got a File Attributes record on the tape.  Now, lookup the Job
 * record, and then create the attributes record.
 */
static bool create_file_attributes_record(B_DB *db, JCR *mjcr,
                                          char *fname, char *lname, int type,
                                          char *ap, DEV_RECORD *rec)
{
   DCR *dcr = mjcr->read_dcr;
   ar.fname = fname;
   ar.link = lname;
   ar.ClientId = mjcr->ClientId;
   ar.JobId = mjcr->JobId;
   ar.Stream = rec->Stream;
   if (type == FT_DELETED) {
      ar.FileIndex = 0;
   } else {
      ar.FileIndex = rec->FileIndex;
   }
   ar.attr = ap;
   if (dcr->VolFirstIndex == 0) {
      dcr->VolFirstIndex = rec->FileIndex;
   }
   dcr->FileIndex = rec->FileIndex;
   mjcr->JobFiles++;

   if (!update_db) {
      return true;
   }

   if (!db->create_file_attributes_record(bjcr, &ar)) {
      Pmsg1(0, _("Could not create File Attributes record. ERR=%s\n"), db->strerror());
      return false;
   }
   mjcr->FileId = ar.FileId;

   if (verbose > 1) {
      Pmsg1(000, _("Created File record: %s\n"), fname);
   }

   return true;
}

/**
 * For each Volume we see, we create a Medium record
 */
static bool create_media_record(B_DB *db, MEDIA_DBR *mr, VOLUME_LABEL *vl)
{
   struct date_time dt;
   struct tm tm;

   /*
    * We mark Vols as Archive to keep them from being re-written
    */
   bstrncpy(mr->VolStatus, "Archive", sizeof(mr->VolStatus));
   mr->VolRetention = 365 * 3600 * 24; /* 1 year */
   mr->Enabled = 1;
   if (vl->VerNum >= 11) {
      mr->set_first_written = true; /* Save FirstWritten during update_media */
      mr->FirstWritten = btime_to_utime(vl->write_btime);
      mr->LabelDate    = btime_to_utime(vl->label_btime);
   } else {
      /*
       * DEPRECATED DO NOT USE
       */
      dt.julian_day_number = vl->write_date;
      dt.julian_day_fraction = vl->write_time;
      tm_decode(&dt, &tm);
      mr->FirstWritten = mktime(&tm);
      dt.julian_day_number = vl->label_date;
      dt.julian_day_fraction = vl->label_time;
      tm_decode(&dt, &tm);
      mr->LabelDate = mktime(&tm);
   }
   lasttime = mr->LabelDate;

   if (mr->VolJobs == 0) {
      mr->VolJobs = 1;
   }

   if (mr->VolMounts == 0) {
      mr->VolMounts = 1;
   }

   if (!update_db) {
      return true;
   }

   if (!db->create_media_record(bjcr, mr)) {
      Pmsg1(000, _("Could not create media record. ERR=%s\n"), db->strerror());
      return false;
   }
   if (!db->update_media_record(bjcr, mr)) {
      Pmsg1(000, _("Could not update media record. ERR=%s\n"), db->strerror());
      return false;
   }
   if (verbose) {
      Pmsg1(000, _("Created Media record for Volume: %s\n"), mr->VolumeName);
   }

   return true;
}

/**
 * Called at end of media to update it
 */
static bool update_media_record(B_DB *db, MEDIA_DBR *mr)
{
   if (!update_db && !update_vol_info) {
      return true;
   }

   mr->LastWritten = lasttime;
   if (!db->update_media_record(bjcr, mr)) {
      Pmsg1(000, _("Could not update media record. ERR=%s\n"), db->strerror());
      return false;
   }

   if (verbose) {
      Pmsg1(000, _("Updated Media record at end of Volume: %s\n"), mr->VolumeName);
   }

   return true;
}

static bool create_pool_record(B_DB *db, POOL_DBR *pr)
{
   pr->NumVols++;
   pr->UseCatalog = 1;
   pr->VolRetention = 355 * 3600 * 24; /* 1 year */

   if (!update_db) {
      return true;
   }

   if (!db->create_pool_record(bjcr, pr)) {
      Pmsg1(000, _("Could not create pool record. ERR=%s\n"), db->strerror());
      return false;
   }

   if (verbose) {
      Pmsg1(000, _("Created Pool record for Pool: %s\n"), pr->Name);
   }

   return true;
}

/**
 * Called from SOS to create a client for the current Job
 */
static bool create_client_record(B_DB *db, CLIENT_DBR *cr)
{
   /*
    * Note, update_db can temporarily be set false while
    * updating the database, so we must ensure that ClientId is non-zero.
    */
   if (!update_db) {
      cr->ClientId = 0;
      if (!db->get_client_record(bjcr, cr)) {
        Pmsg1(0, _("Could not get Client record. ERR=%s\n"), db->strerror());
        return false;
      }

      return true;
   }

   if (!db->create_client_record(bjcr, cr)) {
      Pmsg1(000, _("Could not create Client record. ERR=%s\n"), db->strerror());
      return false;
   }

   if (verbose) {
      Pmsg1(000, _("Created Client record for Client: %s\n"), cr->Name);
   }

   return true;
}

static bool create_fileset_record(B_DB *db, FILESET_DBR *fsr)
{
   if (!update_db) {
      return true;
   }

   fsr->FileSetId = 0;
   if (fsr->MD5[0] == 0) {
      fsr->MD5[0] = ' ';              /* Equivalent to nothing */
      fsr->MD5[1] = 0;
   }

   if (db->get_fileset_record(bjcr, fsr)) {
      if (verbose) {
         Pmsg1(000, _("Fileset \"%s\" already exists.\n"), fsr->FileSet);
      }
   } else {
      if (!db->create_fileset_record(bjcr, fsr)) {
         Pmsg2(000, _("Could not create FileSet record \"%s\". ERR=%s\n"), fsr->FileSet, db->strerror());
         return false;
      }

      if (verbose) {
         Pmsg1(000, _("Created FileSet record \"%s\"\n"), fsr->FileSet);
      }
   }

   return true;
}

/**
 * Simulate the two calls on the database to create the Job record and
 * to update it when the Job actually begins running.
 */
static JCR *create_job_record(B_DB *db, JOB_DBR *jr, SESSION_LABEL *label, DEV_RECORD *rec)
{
   JCR *mjcr;
   struct date_time dt;
   struct tm tm;

   jr->JobId = label->JobId;
   jr->JobType = label->JobType;
   jr->JobLevel = label->JobLevel;
   jr->JobStatus = JS_Created;
   bstrncpy(jr->Name, label->JobName, sizeof(jr->Name));
   bstrncpy(jr->Job, label->Job, sizeof(jr->Job));
   if (label->VerNum >= 11) {
      jr->SchedTime = btime_to_unix(label->write_btime);
   } else {
      dt.julian_day_number = label->write_date;
      dt.julian_day_fraction = label->write_time;
      tm_decode(&dt, &tm);
      jr->SchedTime = mktime(&tm);
   }

   jr->StartTime = jr->SchedTime;
   jr->JobTDate = (utime_t)jr->SchedTime;
   jr->VolSessionId = rec->VolSessionId;
   jr->VolSessionTime = rec->VolSessionTime;

   /* Now create a JCR as if starting the Job */
   mjcr = create_jcr(jr, rec, label->JobId);

   if (!update_db) {
      return mjcr;
   }

   /*
    * This creates the bare essentials
    */
   if (!db->create_job_record(bjcr, jr)) {
      Pmsg1(0, _("Could not create JobId record. ERR=%s\n"), db->strerror());
      return mjcr;
   }

   /*
    * This adds the client, StartTime, JobTDate, ...
    */
   if (!db->update_job_start_record(bjcr, jr)) {
      Pmsg1(0, _("Could not update job start record. ERR=%s\n"), db->strerror());
      return mjcr;
   }

   Pmsg2(000, _("Created new JobId=%u record for original JobId=%u\n"), jr->JobId, label->JobId);
   mjcr->JobId = jr->JobId;           /* set new JobId */

   return mjcr;
}

/**
 * Simulate the database call that updates the Job at Job termination time.
 */
static bool update_job_record(B_DB *db, JOB_DBR *jr, SESSION_LABEL *elabel,
                              DEV_RECORD *rec)
{
   struct date_time dt;
   struct tm tm;
   JCR *mjcr;

   mjcr = get_jcr_by_session(rec->VolSessionId, rec->VolSessionTime);
   if (!mjcr) {
      Pmsg2(000, _("Could not find SessId=%d SessTime=%d for EOS record.\n"),
            rec->VolSessionId, rec->VolSessionTime);
      return false;
   }

   if (elabel->VerNum >= 11) {
      jr->EndTime = btime_to_unix(elabel->write_btime);
   } else {
      dt.julian_day_number = elabel->write_date;
      dt.julian_day_fraction = elabel->write_time;
      tm_decode(&dt, &tm);
      jr->EndTime = mktime(&tm);
   }

   lasttime = jr->EndTime;
   mjcr->end_time = jr->EndTime;

   jr->JobId = mjcr->JobId;
   jr->JobStatus = elabel->JobStatus;
   mjcr->JobStatus = elabel->JobStatus;
   jr->JobFiles = elabel->JobFiles;
   if (jr->JobFiles > 0) {  /* If we found files, force PurgedFiles */
      jr->PurgedFiles = 0;
   }
   jr->JobBytes = elabel->JobBytes;
   jr->VolSessionId = rec->VolSessionId;
   jr->VolSessionTime = rec->VolSessionTime;
   jr->JobTDate = (utime_t)mjcr->start_time;
   jr->ClientId = mjcr->ClientId;

   if (!update_db) {
      free_jcr(mjcr);
      return true;
   }

   if (!db->update_job_end_record(bjcr, jr)) {
      Pmsg2(0, _("Could not update JobId=%u record. ERR=%s\n"), jr->JobId,  db->strerror());
      free_jcr(mjcr);
      return false;
   }

   if (verbose) {
      Pmsg3(000, _("Updated Job termination record for JobId=%u Level=%s TermStat=%c\n"),
         jr->JobId, job_level_to_str(mjcr->getJobLevel()), jr->JobStatus);
   }

   if (verbose > 1) {
      const char *term_msg;
      static char term_code[70];
      char sdt[50], edt[50];
      char ec1[30], ec2[30], ec3[30];

      switch (mjcr->JobStatus) {
      case JS_Terminated:
         term_msg = _("Backup OK");
         break;
      case JS_Warnings:
         term_msg = _("Backup OK -- with warnings");
         break;
      case JS_FatalError:
      case JS_ErrorTerminated:
         term_msg = _("*** Backup Error ***");
         break;
      case JS_Canceled:
         term_msg = _("Backup Canceled");
         break;
      default:
         term_msg = term_code;
         sprintf(term_code, _("Job Termination code: %d"), mjcr->JobStatus);
         break;
      }
      bstrftime(sdt, sizeof(sdt), mjcr->start_time);
      bstrftime(edt, sizeof(edt), mjcr->end_time);
      Pmsg14(000,  _("%s\n"
"JobId:                  %d\n"
"Job:                    %s\n"
"FileSet:                %s\n"
"Backup Level:           %s\n"
"Client:                 %s\n"
"Start time:             %s\n"
"End time:               %s\n"
"Files Written:          %s\n"
"Bytes Written:          %s\n"
"Volume Session Id:      %d\n"
"Volume Session Time:    %d\n"
"Last Volume Bytes:      %s\n"
"Termination:            %s\n\n"),
        edt,
        mjcr->JobId,
        mjcr->Job,
        mjcr->fileset_name,
        job_level_to_str(mjcr->getJobLevel()),
        mjcr->client_name,
        sdt,
        edt,
        edit_uint64_with_commas(mjcr->JobFiles, ec1),
        edit_uint64_with_commas(mjcr->JobBytes, ec2),
        mjcr->VolSessionId,
        mjcr->VolSessionTime,
        edit_uint64_with_commas(mr.VolBytes, ec3),
        term_msg);
   }
   free_jcr(mjcr);

   return true;
}

static bool create_jobmedia_record(B_DB *db, JCR *mjcr)
{
   JOBMEDIA_DBR jmr;
   DCR *dcr = mjcr->read_dcr;

   dcr->EndBlock = dev->EndBlock;
   dcr->EndFile  = dev->EndFile;
   dcr->VolMediaId = dev->VolCatInfo.VolMediaId;

   memset(&jmr, 0, sizeof(jmr));
   jmr.JobId = mjcr->JobId;
   jmr.MediaId = mr.MediaId;
   jmr.FirstIndex = dcr->VolFirstIndex;
   jmr.LastIndex = dcr->VolLastIndex;
   jmr.StartFile = dcr->StartFile;
   jmr.EndFile = dcr->EndFile;
   jmr.StartBlock = dcr->StartBlock;
   jmr.EndBlock = dcr->EndBlock;

   if (!update_db) {
      return true;
   }

   if (!db->create_jobmedia_record(bjcr, &jmr)) {
      Pmsg1(0, _("Could not create JobMedia record. ERR=%s\n"), db->strerror());
      return false;
   }
   if (verbose) {
      Pmsg2(000, _("Created JobMedia record JobId %d, MediaId %d\n"), jmr.JobId, jmr.MediaId);
   }

   return true;
}

/**
 * Simulate the database call that updates the MD5/SHA1 record
 */
static bool update_digest_record(B_DB *db, char *digest, DEV_RECORD *rec, int type)
{
   JCR *mjcr;

   mjcr = get_jcr_by_session(rec->VolSessionId, rec->VolSessionTime);
   if (!mjcr) {
      if (mr.VolJobs > 0) {
         Pmsg2(000, _("Could not find SessId=%d SessTime=%d for MD5/SHA1 record.\n"),
               rec->VolSessionId, rec->VolSessionTime);
      } else {
         ignored_msgs++;
      }
      return false;
   }

   if (!update_db || mjcr->FileId == 0) {
      free_jcr(mjcr);
      return true;
   }

   if (!db->add_digest_to_file_record(bjcr, mjcr->FileId, digest, type)) {
      Pmsg1(0, _("Could not add MD5/SHA1 to File record. ERR=%s\n"), db->strerror());
      free_jcr(mjcr);
      return false;
   }

   if (verbose > 1) {
      Pmsg0(000, _("Updated MD5/SHA1 record\n"));
   }
   free_jcr(mjcr);

   return true;
}

/**
 * Create a JCR as if we are really starting the job
 */
static JCR *create_jcr(JOB_DBR *jr, DEV_RECORD *rec, uint32_t JobId)
{
   JCR *jobjcr;
   /*
    * Transfer as much as possible to the Job JCR. Most important is
    *   the JobId and the ClientId.
    */
   jobjcr = new_jcr(sizeof(JCR), bscan_free_jcr);
   jobjcr->setJobType(jr->JobType);
   jobjcr->setJobLevel(jr->JobLevel);
   jobjcr->JobStatus = jr->JobStatus;
   bstrncpy(jobjcr->Job, jr->Job, sizeof(jobjcr->Job));
   jobjcr->JobId = JobId;      /* this is JobId on tape */
   jobjcr->sched_time = jr->SchedTime;
   jobjcr->start_time = jr->StartTime;
   jobjcr->VolSessionId = rec->VolSessionId;
   jobjcr->VolSessionTime = rec->VolSessionTime;
   jobjcr->ClientId = jr->ClientId;
   jobjcr->dcr = jobjcr->read_dcr = New(DCR);
   setup_new_dcr_device(jobjcr, jobjcr->dcr, dev, NULL);

   return jobjcr;
}
