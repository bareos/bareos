/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2001-2011 Free Software Foundation Europe e.V.
   Copyright (C) 2011-2016 Planets Communications B.V.
   Copyright (C) 2013-2019 Bareos GmbH & Co. KG

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

#include "include/bareos.h"
#include "stored/stored.h"
#include "stored/stored_globals.h"
#include "lib/crypto_cache.h"
#include "findlib/find.h"
#include "cats/cats.h"
#include "cats/cats_backends.h"
#include "cats/sql.h"
#include "stored/acquire.h"
#include "stored/butil.h"
#include "stored/label.h"
#include "stored/mount.h"
#include "stored/read_record.h"
#include "lib/attribs.h"
#include "lib/edit.h"
#include "lib/parse_bsr.h"
#include "lib/bsignal.h"
#include "include/jcr.h"
#include "lib/bsock.h"
#include "lib/parse_conf.h"
#include "lib/util.h"

/* Dummy functions */
namespace storagedaemon {
extern bool ParseSdConfig(const char* configfile, int exit_code);
}

using namespace storagedaemon;

/* Forward referenced functions */
static void do_scan(void);
static bool RecordCb(DeviceControlRecord* dcr, DeviceRecord* rec);
static bool CreateFileAttributesRecord(BareosDb* db,
                                       JobControlRecord* mjcr,
                                       char* fname,
                                       char* lname,
                                       int type,
                                       char* ap,
                                       DeviceRecord* rec);
static bool CreateMediaRecord(BareosDb* db,
                              MediaDbRecord* mr,
                              VOLUME_LABEL* vl);
static bool UpdateMediaRecord(BareosDb* db, MediaDbRecord* mr);
static bool CreatePoolRecord(BareosDb* db, PoolDbRecord* pr);
static JobControlRecord* CreateJobRecord(BareosDb* db,
                                         JobDbRecord* mr,
                                         Session_Label* label,
                                         DeviceRecord* rec);
static bool UpdateJobRecord(BareosDb* db,
                            JobDbRecord* mr,
                            Session_Label* elabel,
                            DeviceRecord* rec);
static bool CreateClientRecord(BareosDb* db, ClientDbRecord* cr);
static bool CreateFilesetRecord(BareosDb* db, FileSetDbRecord* fsr);
static bool CreateJobmediaRecord(BareosDb* db, JobControlRecord* jcr);
static JobControlRecord* create_jcr(JobDbRecord* jr,
                                    DeviceRecord* rec,
                                    uint32_t JobId);
static bool UpdateDigestRecord(BareosDb* db,
                               char* digest,
                               DeviceRecord* rec,
                               int type);

/* Local variables */
static Device* dev = NULL;
static BareosDb* db;
static JobControlRecord* bjcr; /* jcr for bscan */
static BootStrapRecord* bsr = NULL;
static MediaDbRecord mr;
static PoolDbRecord pr;
static JobDbRecord jr;
static ClientDbRecord cr;
static FileSetDbRecord fsr;
static RestoreObjectDbRecord rop;
static AttributesDbRecord ar;
static FileDbRecord fr;
static Session_Label label;
static Session_Label elabel;
static Attributes* attr;

static time_t lasttime = 0;

static const char* backend_directory = _PATH_BAREOS_BACKENDDIR;
static const char* db_driver = "NULL";
static const char* db_name = "bareos";
static const char* db_user = "bareos";
static const char* db_password = "";
static const char* db_host = NULL;
static int db_port = 0;
static const char* wd = NULL;
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
  fprintf(
      stderr,
      _(PROG_COPYRIGHT
        "\nVersion: %s (%s)\n\n"
        "Usage: bscan [ options ] <device-name>\n"
        "       -B <drivername>   specify the database driver name (default "
        "NULL) <postgresql|mysql|sqlite3>\n"
        "       -b <bootstrap>    specify a bootstrap file\n"
        "       -c <path>         specify a Storage configuration file or "
        "directory\n"
        "       -d <nnn>          set debug level to <nnn>\n"
        "       -dt               print timestamp in debug output\n"
        "       -m                update media info in database\n"
        "       -D <director>     specify a director name specified in the "
        "storage daemon\n"
        "                         configuration file for the Key Encryption "
        "Key selection\n"
        "       -a <directory>    specify the database backend directory "
        "(default %s)\n"
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
        "       -w <directory>    specify working directory (default from "
        "configuration file)\n"
        "       -?                print this message\n\n"
        "example:\n"
        "bscan -B postgresql -V Full-0001 FileStorage\n"),
      2001, VERSION, BDATE, backend_directory);
  exit(1);
}

int main(int argc, char* argv[])
{
  int ch;
  struct stat stat_buf;
  char* VolumeName = NULL;
  char* DirectorName = NULL;
  DirectorResource* director = NULL;
  DeviceControlRecord* dcr;
#if defined(HAVE_DYNAMIC_CATS_BACKENDS)
  alist* backend_directories = NULL;
#endif

  setlocale(LC_ALL, "");
  bindtextdomain("bareos", LOCALEDIR);
  textdomain("bareos");
  InitStackDump();

  MyNameIs(argc, argv, "bscan");
  InitMsg(NULL, NULL);

  OSDependentInit();

  while ((ch = getopt(argc, argv, "a:B:b:c:d:D:h:p:mn:pP:q:rsSt:u:vV:w:?")) !=
         -1) {
    switch (ch) {
      case 'a':
        backend_directory = optarg;
        break;

      case 'B':
        db_driver = optarg;
        break;

      case 'b':
        bsr = libbareos::parse_bsr(NULL, optarg);
        break;

      case 'c': /* specify config file */
        if (configfile != NULL) { free(configfile); }
        configfile = strdup(optarg);
        break;

      case 'D': /* specify director name */
        if (DirectorName != NULL) { free(DirectorName); }
        DirectorName = strdup(optarg);
        break;

      case 'd': /* debug level */
        if (*optarg == 't') {
          dbg_timestamp = true;
        } else {
          debug_level = atoi(optarg);
          if (debug_level <= 0) { debug_level = 1; }
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

      case 'r':
        list_records = true;
        break;

      case 'S':
        showProgress = true;
        break;

      case 's':
        update_db = true;
        break;

      case 'u':
        db_user = optarg;
        break;

      case 'V': /* Volume name */
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

  my_config = InitSdConfig(configfile, M_ERROR_TERM);
  ParseSdConfig(configfile, M_ERROR_TERM);

  if (DirectorName) {
    foreach_res (director, R_DIRECTOR) {
      if (bstrcmp(director->resource_name_, DirectorName)) { break; }
    }
    if (!director) {
      Emsg2(
          M_ERROR_TERM, 0,
          _("No Director resource named %s defined in %s. Cannot continue.\n"),
          DirectorName, configfile);
    }
  }

  LoadSdPlugins(me->plugin_directory, me->plugin_names);

  ReadCryptoCache(me->working_directory, "bareos-sd",
                  GetFirstPortHostOrder(me->SDaddrs));

  /* Check if -w option given, otherwise use resource for working directory */
  if (wd) {
    working_directory = wd;
  } else if (!me->working_directory) {
    Emsg1(M_ERROR_TERM, 0,
          _("No Working Directory defined in %s. Cannot continue.\n"),
          configfile);
  } else {
    working_directory = me->working_directory;
  }

  /* Check that working directory is good */
  if (stat(working_directory, &stat_buf) != 0) {
    Emsg1(M_ERROR_TERM, 0,
          _("Working Directory: %s not found. Cannot continue.\n"),
          working_directory);
  }
  if (!S_ISDIR(stat_buf.st_mode)) {
    Emsg1(M_ERROR_TERM, 0,
          _("Working Directory: %s is not a directory. Cannot continue.\n"),
          working_directory);
  }

  dcr = new DeviceControlRecord;
  bjcr = SetupJcr("bscan", argv[0], bsr, director, dcr, VolumeName, true);
  if (!bjcr) { exit(1); }
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
  backend_directories = new alist(10, owned_by_alist);
  backend_directories->append((char*)backend_directory);

  DbSetBackendDirs(backend_directories);
#endif

  db = db_init_database(NULL, db_driver, db_name, db_user, db_password, db_host,
                        db_port, NULL, false, false, false, false);
  if (db == NULL) {
    Emsg0(M_ERROR_TERM, 0, _("Could not init Bareos database\n"));
  }
  if (!db->OpenDatabase(NULL)) { Emsg0(M_ERROR_TERM, 0, db->strerror()); }
  Dmsg0(200, "Database opened\n");
  if (verbose) {
    Pmsg2(000, _("Using Database: %s, User: %s\n"), db_name, db_user);
  }

  do_scan();
  if (update_db) {
    printf(
        "Records added or updated in the catalog:\n%7d Media\n"
        "%7d Pool\n%7d Job\n%7d File\n%7d RestoreObject\n",
        num_media, num_pools, num_jobs, num_files, num_restoreobjects);
  } else {
    printf(
        "Records would have been added or updated in the catalog:\n"
        "%7d Media\n%7d Pool\n%7d Job\n%7d File\n%7d RestoreObject\n",
        num_media, num_pools, num_jobs, num_files, num_restoreobjects);
  }
  DbFlushBackends();

  CleanDevice(bjcr->dcr);
  dev->term();
  FreeDeviceControlRecord(bjcr->dcr);
  FreeJcr(bjcr);

  return 0;
}

/**
 * We are at the end of reading a tape. Now, we simulate handling
 * the end of writing a tape by wiffling through the attached
 * jcrs creating jobmedia records.
 */
static bool BscanMountNextReadVolume(DeviceControlRecord* dcr)
{
  bool status;
  Device* dev = dcr->dev;
  DeviceControlRecord* mdcr;

  Dmsg1(100, "Walk attached jcrs. Volume=%s\n", dev->getVolCatName());
  foreach_dlist (mdcr, dev->attached_dcrs) {
    JobControlRecord* mjcr = mdcr->jcr;
    Dmsg1(000, "========== JobId=%u ========\n", mjcr->JobId);
    if (mjcr->JobId == 0) { continue; }
    if (verbose) { Pmsg1(000, _("Create JobMedia for Job %s\n"), mjcr->Job); }
    mdcr->StartBlock = dcr->StartBlock;
    mdcr->StartFile = dcr->StartFile;
    mdcr->EndBlock = dcr->EndBlock;
    mdcr->EndFile = dcr->EndFile;
    mdcr->VolMediaId = dcr->VolMediaId;
    mjcr->read_dcr->VolLastIndex = dcr->VolLastIndex;
    if (mjcr->insert_jobmedia_records) {
      if (!CreateJobmediaRecord(db, mjcr)) {
        Pmsg2(000, _("Could not create JobMedia record for Volume=%s Job=%s\n"),
              dev->getVolCatName(), mjcr->Job);
      }
    }
  }

  UpdateMediaRecord(db, &mr);

  /* Now let common read routine get up next tape. Note,
   * we call mount_next... with bscan's jcr because that is where we
   * have the Volume list, but we get attached.
   */
  status = MountNextReadVolume(dcr);
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

  AttributesDbRecord ar_emtpy;
  PoolDbRecord pr_empty;
  JobDbRecord jr_empty;
  ClientDbRecord cr_empty;
  FileSetDbRecord fsr_empty;
  FileDbRecord fr_empty;

  ar = ar_emtpy;
  pr = pr_empty;
  jr = jr_empty;
  cr = cr_empty;
  fsr = fsr_empty;
  fr = fr_empty;

  /*
   * Detach bscan's jcr as we are not a real Job on the tape
   */
  ReadRecords(bjcr->read_dcr, RecordCb, BscanMountNextReadVolume);

  if (update_db) {
    db->WriteBatchFileRecords(bjcr); /* used by bulk batch file insert */
  }

  FreeAttr(attr);
}

/**
 * Returns: true  if OK
 *          false if error
 */
static inline bool UnpackRestoreObject(JobControlRecord* jcr,
                                       int32_t stream,
                                       char* rec,
                                       int32_t reclen,
                                       RestoreObjectDbRecord* rop)
{
  char* bp;
  uint32_t JobFiles;

  if (sscanf(rec, "%d %d %d %d %d %d ", &JobFiles, &rop->Stream,
             &rop->object_index, &rop->object_len, &rop->object_full_len,
             &rop->object_compression) != 6) {
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
static bool RecordCb(DeviceControlRecord* dcr, DeviceRecord* rec)
{
  JobControlRecord* mjcr;
  char ec1[30];
  Device* dev = dcr->dev;
  JobControlRecord* bjcr = dcr->jcr;
  DeviceBlock* block = dcr->block;
  PoolMem sql_buffer;
  db_int64_ctx jmr_count;
  char digest[BASE64_SIZE(CRYPTO_DIGEST_MAX_SIZE)];

  if (rec->data_len > 0) {
    mr.VolBytes +=
        rec->data_len + WRITE_RECHDR_LENGTH; /* Accumulate Volume bytes */
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
    Pmsg5(000,
          _("Record: SessId=%u SessTim=%u FileIndex=%d Stream=%d len=%u\n"),
          rec->VolSessionId, rec->VolSessionTime, rec->FileIndex, rec->Stream,
          rec->data_len);
  }

  /*
   * Check for Start or End of Session Record
   */
  if (rec->FileIndex < 0) {
    bool save_update_db = update_db;

    if (verbose > 1) { DumpLabelRecord(dev, rec, true); }
    switch (rec->FileIndex) {
      case PRE_LABEL:
        Pmsg0(000, _("Volume is prelabeled. This tape cannot be scanned.\n"));
        return false;
        break;

      case VOL_LABEL:
        UnserVolumeLabel(dev, rec);
        /*
         * Check Pool info
         */
        bstrncpy(pr.Name, dev->VolHdr.PoolName, sizeof(pr.Name));
        bstrncpy(pr.PoolType, dev->VolHdr.PoolType, sizeof(pr.PoolType));
        num_pools++;
        if (db->GetPoolRecord(bjcr, &pr)) {
          if (verbose) {
            Pmsg1(000, _("Pool record for %s found in DB.\n"), pr.Name);
          }
        } else {
          if (!update_db) {
            Pmsg1(000, _("VOL_LABEL: Pool record not found for Pool: %s\n"),
                  pr.Name);
          }
          CreatePoolRecord(db, &pr);
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
        {
          MediaDbRecord emptyMediaDbRecord;
          mr = emptyMediaDbRecord;
        }
        bstrncpy(mr.VolumeName, dev->VolHdr.VolumeName, sizeof(mr.VolumeName));
        mr.PoolId = pr.PoolId;
        num_media++;
        if (db->GetMediaRecord(bjcr, &mr)) {
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
          CreateMediaRecord(db, &mr, &dev->VolHdr);
        }
        if (!bstrcmp(mr.MediaType, dev->VolHdr.MediaType)) {
          Pmsg2(000, _("VOL_LABEL: MediaType mismatch. DB=%s Vol=%s\n"),
                mr.MediaType, dev->VolHdr.MediaType);
          return true; /* ignore error */
        } else if (verbose) {
          Pmsg1(000, _("Media type \"%s\" is OK.\n"), mr.MediaType);
        }

        /*
         * Reset some DeviceControlRecord variables
         */
        foreach_dlist (dcr, dev->attached_dcrs) {
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
           * Skipping record, because does not match BootStrapRecord filter
           */
          Dmsg0(200, _("SOS_LABEL skipped. Record does not match "
                       "BootStrapRecord filter.\n"));
        } else {
          mr.VolJobs++;
          num_jobs++;
          if (ignored_msgs > 0) {
            Pmsg1(000,
                  _("%d \"errors\" ignored before first Start of Session "
                    "record.\n"),
                  ignored_msgs);
            ignored_msgs = 0;
          }
          UnserSessionLabel(&label, rec);
          {
            JobDbRecord emptyJobDbRecord;
            jr = emptyJobDbRecord;
          }
          bstrncpy(jr.Job, label.Job, sizeof(jr.Job));
          if (db->GetJobRecord(bjcr, &jr)) {
            /*
             * Job record already exists in DB
             */
            update_db = false; /* don't change db in CreateJobRecord */
            if (verbose) {
              Pmsg1(000, _("SOS_LABEL: Found Job record for JobId: %d\n"),
                    jr.JobId);
            }
          } else {
            /*
             * Must create a Job record in DB
             */
            if (!update_db) {
              Pmsg1(000, _("SOS_LABEL: Job record not found for JobId: %d\n"),
                    jr.JobId);
            }
          }

          /*
           * Create Client record if not already there
           */
          bstrncpy(cr.Name, label.ClientName, sizeof(cr.Name));
          CreateClientRecord(db, &cr);
          jr.ClientId = cr.ClientId;

          /*
           * Process label, if Job record exists don't update db
           */
          mjcr = CreateJobRecord(db, &jr, &label, rec);
          dcr = mjcr->read_dcr;
          update_db = save_update_db;

          jr.PoolId = pr.PoolId;
          mjcr->start_time = jr.StartTime;
          mjcr->setJobLevel(jr.JobLevel);

          mjcr->client_name = GetPoolMemory(PM_FNAME);
          PmStrcpy(mjcr->client_name, label.ClientName);
          mjcr->fileset_name = GetPoolMemory(PM_FNAME);
          PmStrcpy(mjcr->fileset_name, label.FileSetName);
          bstrncpy(dcr->pool_type, label.PoolType, sizeof(dcr->pool_type));
          bstrncpy(dcr->pool_name, label.PoolName, sizeof(dcr->pool_name));

          /*
           * Look for existing Job Media records for this job.  If there are
           * any, no new ones need be created.  This may occur if File
           * Retention has expired before Job Retention, or if the volume
           * has already been bscan'd
           */
          Mmsg(sql_buffer, "SELECT count(*) from JobMedia where JobId=%d",
               jr.JobId);
          db->SqlQuery(sql_buffer.c_str(), db_int64_handler, &jmr_count);
          if (jmr_count.value > 0) {
            mjcr->insert_jobmedia_records = false;
          } else {
            mjcr->insert_jobmedia_records = true;
          }

          if (rec->VolSessionId != jr.VolSessionId) {
            Pmsg3(
                000,
                _("SOS_LABEL: VolSessId mismatch for JobId=%u. DB=%d Vol=%d\n"),
                jr.JobId, jr.VolSessionId, rec->VolSessionId);
            return true; /* ignore error */
          }
          if (rec->VolSessionTime != jr.VolSessionTime) {
            Pmsg3(000,
                  _("SOS_LABEL: VolSessTime mismatch for JobId=%u. DB=%d "
                    "Vol=%d\n"),
                  jr.JobId, jr.VolSessionTime, rec->VolSessionTime);
            return true; /* ignore error */
          }
          if (jr.PoolId != pr.PoolId) {
            Pmsg3(000,
                  _("SOS_LABEL: PoolId mismatch for JobId=%u. DB=%d Vol=%d\n"),
                  jr.JobId, jr.PoolId, pr.PoolId);
            return true; /* ignore error */
          }
        }
        break;

      case EOS_LABEL:
        if (bsr && rec->match_stat < 1) {
          /*
           * Skipping record, because does not match BootStrapRecord filter
           */
          Dmsg0(200, _("EOS_LABEL skipped. Record does not match "
                       "BootStrapRecord filter.\n"));
        } else {
          UnserSessionLabel(&elabel, rec);

          /*
           * Create FileSet record
           */
          bstrncpy(fsr.FileSet, label.FileSetName, sizeof(fsr.FileSet));
          bstrncpy(fsr.MD5, label.FileSetMD5, sizeof(fsr.MD5));
          CreateFilesetRecord(db, &fsr);
          jr.FileSetId = fsr.FileSetId;

          mjcr = get_jcr_by_session(rec->VolSessionId, rec->VolSessionTime);
          if (!mjcr) {
            Pmsg2(000,
                  _("Could not find SessId=%d SessTime=%d for EOS record.\n"),
                  rec->VolSessionId, rec->VolSessionTime);
            break;
          }

          /*
           * Do the final update to the Job record
           */
          UpdateJobRecord(db, &jr, &elabel, rec);

          mjcr->end_time = jr.EndTime;
          mjcr->setJobStatus(JS_Terminated);

          /*
           * Create JobMedia record
           */
          mjcr->read_dcr->VolLastIndex = dcr->VolLastIndex;
          if (mjcr->insert_jobmedia_records) { CreateJobmediaRecord(db, mjcr); }
          FreeDeviceControlRecord(mjcr->read_dcr);
          FreeJcr(mjcr);
        }
        break;

      case EOM_LABEL:
        break;

      case EOT_LABEL: /* end of all tapes */
        /*
         * Wiffle through all jobs still open and close them.
         */
        if (update_db) {
          DeviceControlRecord* mdcr;
          foreach_dlist (mdcr, dev->attached_dcrs) {
            JobControlRecord* mjcr = mdcr->jcr;
            if (!mjcr || mjcr->JobId == 0) { continue; }
            jr.JobId = mjcr->JobId;
            jr.JobStatus = JS_ErrorTerminated; /* Mark Job as Error Terimined */
            jr.JobFiles = mjcr->JobFiles;
            jr.JobBytes = mjcr->JobBytes;
            jr.VolSessionId = mjcr->VolSessionId;
            jr.VolSessionTime = mjcr->VolSessionTime;
            jr.JobTDate = (utime_t)mjcr->start_time;
            jr.ClientId = mjcr->ClientId;
            if (!db->UpdateJobEndRecord(bjcr, &jr)) {
              Pmsg1(0, _("Could not update job record. ERR=%s\n"),
                    db->strerror());
            }
          }
        }
        mr.VolFiles = rec->File;
        mr.VolBlocks = rec->Block;
        mr.VolBytes += mr.VolBlocks * WRITE_BLKHDR_LENGTH; /* approx. */
        mr.VolMounts++;
        UpdateMediaRecord(db, &mr);
        Pmsg3(0,
              _("End of all Volumes. VolFiles=%u VolBlocks=%u VolBytes=%s\n"),
              mr.VolFiles, mr.VolBlocks,
              edit_uint64_with_commas(mr.VolBytes, ec1));
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
  if (dcr->VolFirstIndex == 0) { dcr->VolFirstIndex = block->FirstIndex; }

  /*
   * File Attributes stream
   */
  switch (rec->maskedStream) {
    case STREAM_UNIX_ATTRIBUTES:
    case STREAM_UNIX_ATTRIBUTES_EX:
      if (!UnpackAttributesRecord(bjcr, rec->Stream, rec->data, rec->data_len,
                                  attr)) {
        Emsg0(M_ERROR_TERM, 0, _("Cannot continue.\n"));
      }

      if (verbose > 1) {
        DecodeStat(attr->attr, &attr->statp, sizeof(attr->statp),
                   &attr->LinkFI);
        BuildAttrOutputFnames(bjcr, attr);
        PrintLsOutput(bjcr, attr);
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
      CreateFileAttributesRecord(db, mjcr, attr->fname, attr->lname, attr->type,
                                 attr->attr, rec);
      FreeJcr(mjcr);
      break;

    case STREAM_RESTORE_OBJECT:
      if (!UnpackRestoreObject(bjcr, rec->Stream, rec->data, rec->data_len,
                               &rop)) {
        Emsg0(M_ERROR_TERM, 0, _("Cannot continue.\n"));
      }
      rop.FileIndex = mjcr->FileId;
      rop.JobId = mjcr->JobId;


      if (update_db) { db->CreateRestoreObjectRecord(mjcr, &rop); }

      num_restoreobjects++;

      FreeJcr(mjcr); /* done using JobControlRecord */
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

      FreeJcr(mjcr); /* done using JobControlRecord */
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
      FreeJcr(mjcr);
      break;

    case STREAM_SPARSE_GZIP_DATA:
    case STREAM_SPARSE_COMPRESSED_DATA:
      mjcr->JobBytes += rec->data_len -
                        sizeof(uint64_t); /* Not correct, we should expand it */
      FreeJcr(mjcr);                      /* done using JobControlRecord */
      break;

    /*
     * Win32 GZIP stream
     */
    case STREAM_WIN32_GZIP_DATA:
    case STREAM_WIN32_COMPRESSED_DATA:
      mjcr->JobBytes += rec->data_len;
      FreeJcr(mjcr); /* done using JobControlRecord */
      break;

    case STREAM_MD5_DIGEST:
      BinToBase64(digest, sizeof(digest), (char*)rec->data,
                  CRYPTO_DIGEST_MD5_SIZE, true);
      if (verbose > 1) { Pmsg1(000, _("Got MD5 record: %s\n"), digest); }
      UpdateDigestRecord(db, digest, rec, CRYPTO_DIGEST_MD5);
      break;

    case STREAM_SHA1_DIGEST:
      BinToBase64(digest, sizeof(digest), (char*)rec->data,
                  CRYPTO_DIGEST_SHA1_SIZE, true);
      if (verbose > 1) { Pmsg1(000, _("Got SHA1 record: %s\n"), digest); }
      UpdateDigestRecord(db, digest, rec, CRYPTO_DIGEST_SHA1);
      break;

    case STREAM_SHA256_DIGEST:
      BinToBase64(digest, sizeof(digest), (char*)rec->data,
                  CRYPTO_DIGEST_SHA256_SIZE, true);
      if (verbose > 1) { Pmsg1(000, _("Got SHA256 record: %s\n"), digest); }
      UpdateDigestRecord(db, digest, rec, CRYPTO_DIGEST_SHA256);
      break;

    case STREAM_SHA512_DIGEST:
      BinToBase64(digest, sizeof(digest), (char*)rec->data,
                  CRYPTO_DIGEST_SHA512_SIZE, true);
      if (verbose > 1) { Pmsg1(000, _("Got SHA512 record: %s\n"), digest); }
      UpdateDigestRecord(db, digest, rec, CRYPTO_DIGEST_SHA512);
      break;

    case STREAM_ENCRYPTED_SESSION_DATA:
      // TODO landonf: Investigate crypto support in bscan
      if (verbose > 1) { Pmsg0(000, _("Got signed digest record\n")); }
      break;

    case STREAM_SIGNED_DIGEST:
      // TODO landonf: Investigate crypto support in bscan
      if (verbose > 1) { Pmsg0(000, _("Got signed digest record\n")); }
      break;

    case STREAM_PROGRAM_NAMES:
      if (verbose) { Pmsg1(000, _("Got Prog Names Stream: %s\n"), rec->data); }
      break;

    case STREAM_PROGRAM_DATA:
      if (verbose > 1) { Pmsg0(000, _("Got Prog Data Stream record.\n")); }
      break;

    case STREAM_HFSPLUS_ATTRIBUTES:
      /* Ignore OSX attributes */
      break;

    case STREAM_PLUGIN_NAME:
    case STREAM_PLUGIN_DATA:
      /* Ignore plugin data */
      break;

    case STREAM_UNIX_ACCESS_ACL:  /* Deprecated Standard ACL attributes on UNIX
                                   */
    case STREAM_UNIX_DEFAULT_ACL: /* Deprecated Default ACL attributes on UNIX
                                   */
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
      Pmsg2(0, _("Unknown stream type!!! stream=%d len=%i\n"), rec->Stream,
            rec->data_len);
      break;
  }

  return true;
}

/**
 * Free the Job Control Record if no one is still using it.
 *  Called from main FreeJcr() routine in src/lib/jcr.c so
 *  that we can do our Director specific cleanup of the jcr.
 */
static void BscanFreeJcr(JobControlRecord* jcr)
{
  Dmsg0(200, "Start bscan FreeJcr\n");

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

  if (jcr->RestoreBootstrap) { free(jcr->RestoreBootstrap); }

  if (jcr->dcr) {
    FreeDeviceControlRecord(jcr->dcr);
    jcr->dcr = NULL;
  }

  if (jcr->read_dcr) {
    FreeDeviceControlRecord(jcr->read_dcr);
    jcr->read_dcr = NULL;
  }

  Dmsg0(200, "End bscan FreeJcr\n");
}

/**
 * We got a File Attributes record on the tape.  Now, lookup the Job
 * record, and then create the attributes record.
 */
static bool CreateFileAttributesRecord(BareosDb* db,
                                       JobControlRecord* mjcr,
                                       char* fname,
                                       char* lname,
                                       int type,
                                       char* ap,
                                       DeviceRecord* rec)
{
  DeviceControlRecord* dcr = mjcr->read_dcr;
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
  if (dcr->VolFirstIndex == 0) { dcr->VolFirstIndex = rec->FileIndex; }
  dcr->FileIndex = rec->FileIndex;
  mjcr->JobFiles++;

  if (!update_db) { return true; }

  if (!db->CreateFileAttributesRecord(bjcr, &ar)) {
    Pmsg1(0, _("Could not create File Attributes record. ERR=%s\n"),
          db->strerror());
    return false;
  }
  mjcr->FileId = ar.FileId;

  if (verbose > 1) { Pmsg1(000, _("Created File record: %s\n"), fname); }

  return true;
}

/**
 * For each Volume we see, we create a Medium record
 */
static bool CreateMediaRecord(BareosDb* db, MediaDbRecord* mr, VOLUME_LABEL* vl)
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
    mr->FirstWritten = BtimeToUtime(vl->write_btime);
    mr->LabelDate = BtimeToUtime(vl->label_btime);
  } else {
    /*
     * DEPRECATED DO NOT USE
     */
    dt.julian_day_number = vl->write_date;
    dt.julian_day_fraction = vl->write_time;
    TmDecode(&dt, &tm);
    mr->FirstWritten = mktime(&tm);
    dt.julian_day_number = vl->label_date;
    dt.julian_day_fraction = vl->label_time;
    TmDecode(&dt, &tm);
    mr->LabelDate = mktime(&tm);
  }
  lasttime = mr->LabelDate;

  if (mr->VolJobs == 0) { mr->VolJobs = 1; }

  if (mr->VolMounts == 0) { mr->VolMounts = 1; }

  if (!update_db) { return true; }

  if (!db->CreateMediaRecord(bjcr, mr)) {
    Pmsg1(000, _("Could not create media record. ERR=%s\n"), db->strerror());
    return false;
  }
  if (!db->UpdateMediaRecord(bjcr, mr)) {
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
static bool UpdateMediaRecord(BareosDb* db, MediaDbRecord* mr)
{
  if (!update_db && !update_vol_info) { return true; }

  mr->LastWritten = lasttime;
  if (!db->UpdateMediaRecord(bjcr, mr)) {
    Pmsg1(000, _("Could not update media record. ERR=%s\n"), db->strerror());
    return false;
  }

  if (verbose) {
    Pmsg1(000, _("Updated Media record at end of Volume: %s\n"),
          mr->VolumeName);
  }

  return true;
}

static bool CreatePoolRecord(BareosDb* db, PoolDbRecord* pr)
{
  pr->NumVols++;
  pr->UseCatalog = 1;
  pr->VolRetention = 355 * 3600 * 24; /* 1 year */

  if (!update_db) { return true; }

  if (!db->CreatePoolRecord(bjcr, pr)) {
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
static bool CreateClientRecord(BareosDb* db, ClientDbRecord* cr)
{
  /*
   * Note, update_db can temporarily be set false while
   * updating the database, so we must ensure that ClientId is non-zero.
   */
  if (!update_db) {
    cr->ClientId = 0;
    if (!db->GetClientRecord(bjcr, cr)) {
      Pmsg1(0, _("Could not get Client record. ERR=%s\n"), db->strerror());
      return false;
    }

    return true;
  }

  if (!db->CreateClientRecord(bjcr, cr)) {
    Pmsg1(000, _("Could not create Client record. ERR=%s\n"), db->strerror());
    return false;
  }

  if (verbose) {
    Pmsg1(000, _("Created Client record for Client: %s\n"), cr->Name);
  }

  return true;
}

static bool CreateFilesetRecord(BareosDb* db, FileSetDbRecord* fsr)
{
  if (!update_db) { return true; }

  fsr->FileSetId = 0;
  if (fsr->MD5[0] == 0) {
    fsr->MD5[0] = ' '; /* Equivalent to nothing */
    fsr->MD5[1] = 0;
  }

  if (db->GetFilesetRecord(bjcr, fsr)) {
    if (verbose) {
      Pmsg1(000, _("Fileset \"%s\" already exists.\n"), fsr->FileSet);
    }
  } else {
    if (!db->CreateFilesetRecord(bjcr, fsr)) {
      Pmsg2(000, _("Could not create FileSet record \"%s\". ERR=%s\n"),
            fsr->FileSet, db->strerror());
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
static JobControlRecord* CreateJobRecord(BareosDb* db,
                                         JobDbRecord* jr,
                                         Session_Label* label,
                                         DeviceRecord* rec)
{
  JobControlRecord* mjcr;
  struct date_time dt;
  struct tm tm;

  jr->JobId = label->JobId;
  jr->JobType = label->JobType;
  jr->JobLevel = label->JobLevel;
  jr->JobStatus = JS_Created;
  bstrncpy(jr->Name, label->JobName, sizeof(jr->Name));
  bstrncpy(jr->Job, label->Job, sizeof(jr->Job));
  if (label->VerNum >= 11) {
    jr->SchedTime = BtimeToUnix(label->write_btime);
  } else {
    dt.julian_day_number = label->write_date;
    dt.julian_day_fraction = label->write_time;
    TmDecode(&dt, &tm);
    jr->SchedTime = mktime(&tm);
  }

  jr->StartTime = jr->SchedTime;
  jr->JobTDate = (utime_t)jr->SchedTime;
  jr->VolSessionId = rec->VolSessionId;
  jr->VolSessionTime = rec->VolSessionTime;

  /* Now create a JobControlRecord as if starting the Job */
  mjcr = create_jcr(jr, rec, label->JobId);

  if (!update_db) { return mjcr; }

  /*
   * This creates the bare essentials
   */
  if (!db->CreateJobRecord(bjcr, jr)) {
    Pmsg1(0, _("Could not create JobId record. ERR=%s\n"), db->strerror());
    return mjcr;
  }

  /*
   * This adds the client, StartTime, JobTDate, ...
   */
  if (!db->UpdateJobStartRecord(bjcr, jr)) {
    Pmsg1(0, _("Could not update job start record. ERR=%s\n"), db->strerror());
    return mjcr;
  }

  Pmsg2(000, _("Created new JobId=%u record for original JobId=%u\n"),
        jr->JobId, label->JobId);
  mjcr->JobId = jr->JobId; /* set new JobId */

  return mjcr;
}

/**
 * Simulate the database call that updates the Job at Job termination time.
 */
static bool UpdateJobRecord(BareosDb* db,
                            JobDbRecord* jr,
                            Session_Label* elabel,
                            DeviceRecord* rec)
{
  struct date_time dt;
  struct tm tm;
  JobControlRecord* mjcr;

  mjcr = get_jcr_by_session(rec->VolSessionId, rec->VolSessionTime);
  if (!mjcr) {
    Pmsg2(000, _("Could not find SessId=%d SessTime=%d for EOS record.\n"),
          rec->VolSessionId, rec->VolSessionTime);
    return false;
  }

  if (elabel->VerNum >= 11) {
    jr->EndTime = BtimeToUnix(elabel->write_btime);
  } else {
    dt.julian_day_number = elabel->write_date;
    dt.julian_day_fraction = elabel->write_time;
    TmDecode(&dt, &tm);
    jr->EndTime = mktime(&tm);
  }

  lasttime = jr->EndTime;
  mjcr->end_time = jr->EndTime;

  jr->JobId = mjcr->JobId;
  jr->JobStatus = elabel->JobStatus;
  mjcr->JobStatus = elabel->JobStatus;
  jr->JobFiles = elabel->JobFiles;
  if (jr->JobFiles > 0) { /* If we found files, force PurgedFiles */
    jr->PurgedFiles = 0;
  }
  jr->JobBytes = elabel->JobBytes;
  jr->VolSessionId = rec->VolSessionId;
  jr->VolSessionTime = rec->VolSessionTime;
  jr->JobTDate = (utime_t)mjcr->start_time;
  jr->ClientId = mjcr->ClientId;

  if (!update_db) {
    FreeJcr(mjcr);
    return true;
  }

  if (!db->UpdateJobEndRecord(bjcr, jr)) {
    Pmsg2(0, _("Could not update JobId=%u record. ERR=%s\n"), jr->JobId,
          db->strerror());
    FreeJcr(mjcr);
    return false;
  }

  if (verbose) {
    Pmsg3(
        000,
        _("Updated Job termination record for JobId=%u Level=%s TermStat=%c\n"),
        jr->JobId, job_level_to_str(mjcr->getJobLevel()), jr->JobStatus);
  }

  if (verbose > 1) {
    const char* TermMsg;
    static char term_code[70];
    char sdt[50], edt[50];
    char ec1[30], ec2[30], ec3[30];

    switch (mjcr->JobStatus) {
      case JS_Terminated:
        TermMsg = _("Backup OK");
        break;
      case JS_Warnings:
        TermMsg = _("Backup OK -- with warnings");
        break;
      case JS_FatalError:
      case JS_ErrorTerminated:
        TermMsg = _("*** Backup Error ***");
        break;
      case JS_Canceled:
        TermMsg = _("Backup Canceled");
        break;
      default:
        TermMsg = term_code;
        sprintf(term_code, _("Job Termination code: %d"), mjcr->JobStatus);
        break;
    }
    bstrftime(sdt, sizeof(sdt), mjcr->start_time);
    bstrftime(edt, sizeof(edt), mjcr->end_time);
    Pmsg15(000,
           _("%s\n"
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
             "Bareos binary info:     %s\n"
             "Termination:            %s\n\n"),
           edt, mjcr->JobId, mjcr->Job, mjcr->fileset_name,
           job_level_to_str(mjcr->getJobLevel()), mjcr->client_name, sdt, edt,
           edit_uint64_with_commas(mjcr->JobFiles, ec1),
           edit_uint64_with_commas(mjcr->JobBytes, ec2), mjcr->VolSessionId,
           mjcr->VolSessionTime, edit_uint64_with_commas(mr.VolBytes, ec3),
           BAREOS_BINARY_INFO, TermMsg);
  }
  FreeJcr(mjcr);

  return true;
}

static bool CreateJobmediaRecord(BareosDb* db, JobControlRecord* mjcr)
{
  JobMediaDbRecord jmr;
  DeviceControlRecord* dcr = mjcr->read_dcr;

  dcr->EndBlock = dev->EndBlock;
  dcr->EndFile = dev->EndFile;
  dcr->VolMediaId = dev->VolCatInfo.VolMediaId;

  jmr.JobId = mjcr->JobId;
  jmr.MediaId = mr.MediaId;
  jmr.FirstIndex = dcr->VolFirstIndex;
  jmr.LastIndex = dcr->VolLastIndex;
  jmr.StartFile = dcr->StartFile;
  jmr.EndFile = dcr->EndFile;
  jmr.StartBlock = dcr->StartBlock;
  jmr.EndBlock = dcr->EndBlock;

  if (!update_db) { return true; }

  if (!db->CreateJobmediaRecord(bjcr, &jmr)) {
    Pmsg1(0, _("Could not create JobMedia record. ERR=%s\n"), db->strerror());
    return false;
  }
  if (verbose) {
    Pmsg2(000, _("Created JobMedia record JobId %d, MediaId %d\n"), jmr.JobId,
          jmr.MediaId);
  }

  return true;
}

/**
 * Simulate the database call that updates the MD5/SHA1 record
 */
static bool UpdateDigestRecord(BareosDb* db,
                               char* digest,
                               DeviceRecord* rec,
                               int type)
{
  JobControlRecord* mjcr;

  mjcr = get_jcr_by_session(rec->VolSessionId, rec->VolSessionTime);
  if (!mjcr) {
    if (mr.VolJobs > 0) {
      Pmsg2(000,
            _("Could not find SessId=%d SessTime=%d for MD5/SHA1 record.\n"),
            rec->VolSessionId, rec->VolSessionTime);
    } else {
      ignored_msgs++;
    }
    return false;
  }

  if (!update_db || mjcr->FileId == 0) {
    FreeJcr(mjcr);
    return true;
  }

  if (!db->AddDigestToFileRecord(bjcr, mjcr->FileId, digest, type)) {
    Pmsg1(0, _("Could not add MD5/SHA1 to File record. ERR=%s\n"),
          db->strerror());
    FreeJcr(mjcr);
    return false;
  }

  if (verbose > 1) { Pmsg0(000, _("Updated MD5/SHA1 record\n")); }
  FreeJcr(mjcr);

  return true;
}

/**
 * Create a JobControlRecord as if we are really starting the job
 */
static JobControlRecord* create_jcr(JobDbRecord* jr,
                                    DeviceRecord* rec,
                                    uint32_t JobId)
{
  JobControlRecord* jobjcr;
  /*
   * Transfer as much as possible to the Job JobControlRecord. Most important is
   *   the JobId and the ClientId.
   */
  jobjcr = new_jcr(sizeof(JobControlRecord), BscanFreeJcr);
  jobjcr->setJobType(jr->JobType);
  jobjcr->setJobLevel(jr->JobLevel);
  jobjcr->JobStatus = jr->JobStatus;
  bstrncpy(jobjcr->Job, jr->Job, sizeof(jobjcr->Job));
  jobjcr->JobId = JobId; /* this is JobId on tape */
  jobjcr->sched_time = jr->SchedTime;
  jobjcr->start_time = jr->StartTime;
  jobjcr->VolSessionId = rec->VolSessionId;
  jobjcr->VolSessionTime = rec->VolSessionTime;
  jobjcr->ClientId = jr->ClientId;
  jobjcr->dcr = jobjcr->read_dcr = new DeviceControlRecord;
  SetupNewDcrDevice(jobjcr, jobjcr->dcr, dev, NULL);

  return jobjcr;
}
