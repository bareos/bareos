/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2001-2011 Free Software Foundation Europe e.V.
   Copyright (C) 2011-2016 Planets Communications B.V.
   Copyright (C) 2013-2024 Bareos GmbH & Co. KG

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
// Kern E. Sibbald, December 2001
/**
 * @file
 * Program to scan a Bareos Volume and compare it with
 * the catalog and optionally synchronize the catalog
 * with the tape.
 */

#include "include/bareos.h"
#include "include/exit_codes.h"
#include "include/filetypes.h"
#include "include/streams.h"
#include "stored/stored.h"
#include "stored/stored_globals.h"
#include "stored/stored_jcr_impl.h"
#include "lib/crypto_cache.h"
#include "findlib/find.h"
#include "cats/cats.h"
#include "cats/sql.h"
#include "stored/acquire.h"
#include "stored/butil.h"
#include "stored/device_control_record.h"
#include "stored/label.h"
#include "stored/mount.h"
#include "stored/read_record.h"
#include "lib/attribs.h"
#include "lib/cli.h"
#include "lib/edit.h"
#include "lib/parse_bsr.h"
#include "lib/bsignal.h"
#include "include/jcr.h"
#include "lib/bsock.h"
#include "lib/parse_conf.h"
#include "lib/util.h"
#include "lib/version.h"
#include "lib/compression.h"

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
                              Volume_Label* vl);
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
static Device* dev = nullptr;
static BareosDb* db;
static JobControlRecord* bjcr; /* jcr for bscan */
static BootStrapRecord* bsr = nullptr;
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

int main(int argc, char* argv[])
{
  setlocale(LC_ALL, "");
  tzset();
  bindtextdomain("bareos", LOCALEDIR);
  textdomain("bareos");
  InitStackDump();

  MyNameIs(argc, argv, "bscan");
  InitMsg(nullptr, nullptr);

  OSDependentInit();

  CLI::App bscan_app;
  InitCLIApp(bscan_app, "The Bareos Database Scan tool.", 2001);

  bscan_app
      .add_option(
          "-b,--parse-bootstrap",
          [](std::vector<std::string> vals) {
            bsr = libbareos::parse_bsr(nullptr, vals.front().data());
            return true;
          },
          "Specify a bootstrap file")
      ->check(CLI::ExistingFile)
      ->type_name("<bootstrap>");

  bscan_app
      .add_option(
          "-c,--config",
          [](std::vector<std::string> val) {
            if (configfile != nullptr) { free(configfile); }
            configfile = strdup(val.front().c_str());
            return true;
          },
          "Use <path> as configuration file or directory")
      ->check(CLI::ExistingPath)
      ->type_name("<path>");

  std::string DirectorName;
  bscan_app
      .add_option("-D,--director", DirectorName,
                  "Specify a director name specified in the storage.\n"
                  "Configuration file for the Key Encryption Key selection.")
      ->type_name("<director>");

  AddDebugOptions(bscan_app);

  std::string db_name = "bareos";
  bscan_app.add_option("-n,--dbname", db_name, "Specify database name.")
      ->type_name("<name>")
      ->capture_default_str();

  std::string db_host;
  bscan_app.add_option("-o,--dbhost", db_host, "Specify database host.")
      ->type_name("<host>");

  std::string db_password = "";
  bscan_app
      .add_option("-P,--dbpassword", db_password, "Specify database password.")
      ->type_name("<password>");

  int db_port = 0;
  bscan_app.add_option("-t,--dbport", db_port, "Specify database port.")
      ->type_name("<port>");

  std::string db_user = "bareos";
  bscan_app.add_option("-u,--dbuser", db_user, "Specify database user name.")
      ->type_name("<user>")
      ->capture_default_str();

  bscan_app.add_flag("-m,--update-volume-info", update_vol_info,
                     "Update media info in database.");

  bscan_app.add_flag("-p,--proceed-io", forge_on,
                     "Proceed inspite of IO errors");

  bscan_app.add_flag("-r,--list-records", list_records, "List records.");

  bscan_app.add_flag("-S,--show-progress", showProgress,
                     "Show scan progress periodically.");

  bscan_app.add_flag("-s,--update-db", update_db,
                     "Synchronize or store in database.");

  std::string volumes;
  bscan_app
      .add_option("-V,--volumes", volumes,
                  "Specify volume names (separated by |).")
      ->type_name("<vol1|vol2|...>");

  AddVerboseOption(bscan_app);

  std::string work_dir;
  bscan_app
      .add_option("-w,--working-directory", work_dir,
                  "Specify working directory.")
      ->type_name("<directory>");

  std::string device_name;
  bscan_app
      .add_option("device_name", device_name,
                  "Specify the input device name (either as name of a Bareos "
                  "Storage Daemon Device resource or identical to the Archive "
                  "Device in a Bareos Storage Daemon Device resource).")
      ->type_name(" ");

  ParseBareosApp(bscan_app, argc, argv);

  my_config = InitSdConfig(configfile, M_CONFIG_ERROR);
  ParseSdConfig(configfile, M_CONFIG_ERROR);

  if (device_name.empty()) {
    printf(T_("%sNothing done."), AvailableDevicesListing().c_str());
    return BEXIT_SUCCESS;
  }

  DirectorResource* director = nullptr;
  if (!DirectorName.empty()) {
    foreach_res (director, R_DIRECTOR) {
      if (bstrcmp(director->resource_name_, DirectorName.c_str())) { break; }
    }
    if (!director) {
      Emsg2(M_ERROR_TERM, 0,
            T_("No Director resource named %s defined in %s. Cannot "
               "continue.\n"),
            DirectorName.c_str(), configfile);
    }
  }

  LoadSdPlugins(me->plugin_directory, me->plugin_names);

  ReadCryptoCache(me->working_directory, "bareos-sd",
                  GetFirstPortHostOrder(me->SDaddrs));

  /* Check if -w option given, otherwise use resource for working directory
   */
  if (!work_dir.empty()) {
    working_directory = work_dir.c_str();
  } else if (!me->working_directory) {
    Emsg1(M_ERROR_TERM, 0,
          T_("No Working Directory defined in %s. Cannot continue.\n"),
          configfile);
  } else {
    working_directory = me->working_directory;
  }

  /* Check that working directory is good */
  struct stat stat_buf;
  if (stat(working_directory, &stat_buf) != 0) {
    Emsg1(M_ERROR_TERM, 0,
          T_("Working Directory: %s not found. Cannot continue.\n"),
          working_directory);
  }
  if (!S_ISDIR(stat_buf.st_mode)) {
    Emsg1(M_ERROR_TERM, 0,
          T_("Working Directory: %s is not a directory. Cannot continue.\n"),
          working_directory);
  }

  DeviceControlRecord* dcr = new DeviceControlRecord;
  bjcr = SetupJcr("bscan", device_name.data(), bsr, director, dcr, volumes,
                  true);
  if (!bjcr) { exit(BEXIT_FAILURE); }
  dev = bjcr->sd_impl->read_dcr->dev;

  // Let SD plugins setup the record translation
  if (GeneratePluginEvent(bjcr, bSdEventSetupRecordTranslation, dcr)
      != bRC_OK) {
    Jmsg(bjcr, M_FATAL, 0, T_("bSdEventSetupRecordTranslation call failed!\n"));
  }


  if (showProgress) {
    char ed1[50];
    struct stat sb;
    fstat(dev->fd, &sb);
    currentVolumeSize = sb.st_size;
    Pmsg1(000, T_("First Volume Size = %s\n"),
          edit_uint64(currentVolumeSize, ed1));
  }

  std::string db_driver = "postgresql";
  db = db_init_database(nullptr, db_driver.c_str(), db_name.c_str(),
                        db_user.c_str(), db_password.c_str(), db_host.c_str(),
                        db_port, nullptr, false, false, false, false);
  if (db == nullptr) {
    Emsg0(M_ERROR_TERM, 0, T_("Could not init Bareos database\n"));
  }
  if (!db->OpenDatabase(nullptr)) { Emsg0(M_ERROR_TERM, 0, db->strerror()); }
  Dmsg0(200, "Database opened\n");
  if (g_verbose) {
    Pmsg2(000, T_("Using Database: %s, User: %s\n"), db_name.c_str(),
          db_user.c_str());
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
  db->CloseDatabase(bjcr);
  CleanDevice(bjcr->sd_impl->dcr);
  delete dev;
  FreeDeviceControlRecord(bjcr->sd_impl->dcr);
  FreePlugins(bjcr);
  CleanupCompression(bjcr);
  FreeJcr(bjcr);
  UnloadSdPlugins();

  return BEXIT_SUCCESS;
}

/**
 * We are at the end of reading a tape. Now, we simulate handling
 * the end of writing a tape by wiffling through the attached
 * jcrs creating jobmedia records.
 */
static bool BscanMountNextReadVolume(DeviceControlRecord* dcr)
{
  bool status;
  Device* my_dev = dcr->dev;

  Dmsg1(100, "Walk attached jcrs. Volume=%s\n", my_dev->getVolCatName());
  for (auto mdcr : my_dev->attached_dcrs) {
    JobControlRecord* mjcr = mdcr->jcr;
    Dmsg1(000, "========== JobId=%u ========\n", mjcr->JobId);
    if (mjcr->JobId == 0) { continue; }
    if (g_verbose) {
      Pmsg1(000, T_("Create JobMedia for Job %s\n"), mjcr->Job);
    }
    mdcr->StartBlock = dcr->StartBlock;
    mdcr->StartFile = dcr->StartFile;
    mdcr->EndBlock = dcr->EndBlock;
    mdcr->EndFile = dcr->EndFile;
    mdcr->VolMediaId = dcr->VolMediaId;
    mjcr->sd_impl->read_dcr->VolLastIndex = dcr->VolLastIndex;
    if (mjcr->sd_impl->insert_jobmedia_records) {
      if (!CreateJobmediaRecord(db, mjcr)) {
        Pmsg2(000,
              T_("Could not create JobMedia record for Volume=%s Job=%s\n"),
              my_dev->getVolCatName(), mjcr->Job);
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
    fstat(my_dev->fd, &sb);
    currentVolumeSize = sb.st_size;
    Pmsg1(000, T_("First Volume Size = %s\n"),
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

  // Detach bscan's jcr as we are not a real Job on the tape
  ReadRecords(bjcr->sd_impl->read_dcr, RecordCb, BscanMountNextReadVolume);

  if (update_db) {
    db->WriteBatchFileRecords(bjcr); /* used by bulk batch file insert */
  }

  FreeAttr(attr);
}

/**
 * Returns: true  if OK
 *          false if error
 */
static inline bool UnpackRestoreObject(JobControlRecord*,
                                       int32_t /* stream */,
                                       char* rec,
                                       int32_t /* reclen */,
                                       RestoreObjectDbRecord* t_rop)
{
  char* bp;
  uint32_t JobFiles;

  if (sscanf(rec, "%d %d %d %d %d %d ", &JobFiles, &t_rop->Stream,
             &t_rop->object_index, &t_rop->object_len, &t_rop->object_full_len,
             &t_rop->object_compression)
      != 6) {
    return false;
  }

  // Skip over the first 6 fields we scanned in the previous scan.
  bp = rec;
  for (int i = 0; i < 6; i++) {
    bp = strchr(bp, ' ');
    if (bp) {
      bp++;
    } else {
      return false;
    }
  }

  t_rop->plugin_name = bp;
  t_rop->object_name = t_rop->plugin_name + strlen(t_rop->plugin_name) + 1;
  t_rop->object = t_rop->object_name + strlen(t_rop->object_name) + 1;

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
  Device* my_dev = dcr->dev;
  JobControlRecord* my_bjcr = dcr->jcr;
  DeviceBlock* block = dcr->block;
  PoolMem sql_buffer;
  db_int64_ctx jmr_count;
  char digest[BASE64_SIZE(CRYPTO_DIGEST_MAX_SIZE)];

  if (rec->data_len > 0) {
    mr.VolBytes
        += rec->data_len + WRITE_RECHDR_LENGTH; /* Accumulate Volume bytes */
    if (showProgress && currentVolumeSize > 0) {
      int pct = (mr.VolBytes * 100) / currentVolumeSize;
      if (pct != last_pct) {
        fprintf(stdout, T_("done: %d%%\n"), pct);
        fflush(stdout);
        last_pct = pct;
      }
    }
  }

  if (list_records) {
    Pmsg5(000,
          T_("Record: SessId=%u SessTim=%u FileIndex=%d Stream=%d len=%u\n"),
          rec->VolSessionId, rec->VolSessionTime, rec->FileIndex, rec->Stream,
          rec->data_len);
  }

  // Check for Start or End of Session Record
  if (rec->FileIndex < 0) {
    bool save_update_db = update_db;

    if (g_verbose > 1) { DumpLabelRecord(my_dev, rec, true); }
    switch (rec->FileIndex) {
      case PRE_LABEL:
        Pmsg0(000, T_("Volume is prelabeled. This tape cannot be scanned.\n"));
        return false;
        break;

      case VOL_LABEL:
        UnserVolumeLabel(my_dev, rec);
        // Check Pool info
        bstrncpy(pr.Name, my_dev->VolHdr.PoolName, sizeof(pr.Name));
        bstrncpy(pr.PoolType, my_dev->VolHdr.PoolType, sizeof(pr.PoolType));
        num_pools++;
        if (db->GetPoolRecord(my_bjcr, &pr)) {
          if (g_verbose) {
            Pmsg1(000, T_("Pool record for %s found in DB.\n"), pr.Name);
          }
        } else {
          if (!update_db) {
            Pmsg1(000, T_("VOL_LABEL: Pool record not found for Pool: %s\n"),
                  pr.Name);
          }
          CreatePoolRecord(db, &pr);
        }
        if (!bstrcmp(pr.PoolType, my_dev->VolHdr.PoolType)) {
          Pmsg2(000, T_("VOL_LABEL: PoolType mismatch. DB=%s Vol=%s\n"),
                pr.PoolType, my_dev->VolHdr.PoolType);
          return true;
        } else if (g_verbose) {
          Pmsg1(000, T_("Pool type \"%s\" is OK.\n"), pr.PoolType);
        }

        // Check Media Info
        mr = MediaDbRecord{};
        bstrncpy(mr.VolumeName, my_dev->VolHdr.VolumeName,
                 sizeof(mr.VolumeName));
        mr.PoolId = pr.PoolId;
        num_media++;
        if (db->GetMediaRecord(my_bjcr, &mr)) {
          if (g_verbose) {
            Pmsg1(000, T_("Media record for %s found in DB.\n"), mr.VolumeName);
          }
          // Clear out some volume statistics that will be updated
          mr.VolJobs = mr.VolFiles = mr.VolBlocks = 0;
          mr.VolBytes = rec->data_len + 20;
        } else {
          if (!update_db) {
            Pmsg1(000, T_("VOL_LABEL: Media record not found for Volume: %s\n"),
                  mr.VolumeName);
          }
          bstrncpy(mr.MediaType, my_dev->VolHdr.MediaType,
                   sizeof(mr.MediaType));
          CreateMediaRecord(db, &mr, &my_dev->VolHdr);
        }
        if (!bstrcmp(mr.MediaType, my_dev->VolHdr.MediaType)) {
          Pmsg2(000, T_("VOL_LABEL: MediaType mismatch. DB=%s Vol=%s\n"),
                mr.MediaType, my_dev->VolHdr.MediaType);
          return true; /* ignore error */
        } else if (g_verbose) {
          Pmsg1(000, T_("Media type \"%s\" is OK.\n"), mr.MediaType);
        }

        // Reset some DeviceControlRecord variables
        for (auto d : my_dev->attached_dcrs) {
          d->VolFirstIndex = d->FileIndex = 0;
          d->StartBlock = d->EndBlock = 0;
          d->StartFile = d->EndFile = 0;
          d->VolMediaId = 0;
        }

        Pmsg1(000, T_("VOL_LABEL: OK for Volume: %s\n"), mr.VolumeName);
        break;

      case SOS_LABEL:
        if (bsr && rec->match_stat < 1) {
          // Skipping record, because does not match BootStrapRecord filter
          Dmsg0(200, T_("SOS_LABEL skipped. Record does not match "
                        "BootStrapRecord filter.\n"));
        } else {
          mr.VolJobs++;
          num_jobs++;
          if (ignored_msgs > 0) {
            Pmsg1(000,
                  T_("%d \"errors\" ignored before first Start of Session "
                     "record.\n"),
                  ignored_msgs);
            ignored_msgs = 0;
          }
          UnserSessionLabel(&label, rec);
          jr = JobDbRecord{};
          bstrncpy(jr.Job, label.Job, sizeof(jr.Job));
          if (db->GetJobRecord(my_bjcr, &jr)) {
            // Job record already exists in DB
            update_db = false; /* don't change db in CreateJobRecord */
            if (g_verbose) {
              Pmsg1(000, T_("SOS_LABEL: Found Job record for JobId: %d\n"),
                    jr.JobId);
            }
          } else {
            // Must create a Job record in DB
            if (!update_db) {
              Pmsg1(000, T_("SOS_LABEL: Job record not found for JobId: %d\n"),
                    jr.JobId);
            }
          }

          // Create Client record if not already there
          bstrncpy(cr.Name, label.ClientName, sizeof(cr.Name));
          CreateClientRecord(db, &cr);
          jr.ClientId = cr.ClientId;

          // Process label, if Job record exists don't update db
          mjcr = CreateJobRecord(db, &jr, &label, rec);
          dcr = mjcr->sd_impl->read_dcr;
          update_db = save_update_db;

          jr.PoolId = pr.PoolId;
          mjcr->start_time = jr.StartTime;
          mjcr->setJobLevel(jr.JobLevel);

          mjcr->client_name = GetPoolMemory(PM_FNAME);
          PmStrcpy(mjcr->client_name, label.ClientName);
          mjcr->sd_impl->fileset_name = GetPoolMemory(PM_FNAME);
          PmStrcpy(mjcr->sd_impl->fileset_name, label.FileSetName);
          bstrncpy(dcr->pool_type, label.PoolType, sizeof(dcr->pool_type));
          bstrncpy(dcr->pool_name, label.PoolName, sizeof(dcr->pool_name));

          /* Look for existing Job Media records for this job.  If there are
           * any, no new ones need be created.  This may occur if File
           * Retention has expired before Job Retention, or if the volume
           * has already been bscan'd */
          Mmsg(sql_buffer, "SELECT count(*) from JobMedia where JobId=%d",
               jr.JobId);
          db->SqlQuery(sql_buffer.c_str(), db_int64_handler, &jmr_count);
          if (jmr_count.value > 0) {
            mjcr->sd_impl->insert_jobmedia_records = false;
          } else {
            mjcr->sd_impl->insert_jobmedia_records = true;
          }

          if (rec->VolSessionId != jr.VolSessionId) {
            Pmsg3(000,
                  T_("SOS_LABEL: VolSessId mismatch for JobId=%u. DB=%d "
                     "Vol=%d\n"),
                  jr.JobId, jr.VolSessionId, rec->VolSessionId);
            return true; /* ignore error */
          }
          if (rec->VolSessionTime != jr.VolSessionTime) {
            Pmsg3(000,
                  T_("SOS_LABEL: VolSessTime mismatch for JobId=%u. DB=%d "
                     "Vol=%d\n"),
                  jr.JobId, jr.VolSessionTime, rec->VolSessionTime);
            return true; /* ignore error */
          }
          if (jr.PoolId != pr.PoolId) {
            Pmsg3(000,
                  T_("SOS_LABEL: PoolId mismatch for JobId=%u. DB=%d Vol=%d\n"),
                  jr.JobId, jr.PoolId, pr.PoolId);
            return true; /* ignore error */
          }
        }
        break;

      case EOS_LABEL:
        if (bsr && rec->match_stat < 1) {
          // Skipping record, because does not match BootStrapRecord filter
          Dmsg0(200, T_("EOS_LABEL skipped. Record does not match "
                        "BootStrapRecord filter.\n"));
        } else {
          UnserSessionLabel(&elabel, rec);

          // Create FileSet record
          bstrncpy(fsr.FileSet, label.FileSetName, sizeof(fsr.FileSet));
          bstrncpy(fsr.MD5, label.FileSetMD5, sizeof(fsr.MD5));
          CreateFilesetRecord(db, &fsr);
          jr.FileSetId = fsr.FileSetId;

          mjcr = get_jcr_by_session(rec->VolSessionId, rec->VolSessionTime);
          if (!mjcr) {
            Pmsg2(000,
                  T_("Could not find SessId=%d SessTime=%d for EOS record.\n"),
                  rec->VolSessionId, rec->VolSessionTime);
            break;
          }

          // Do the final update to the Job record
          UpdateJobRecord(db, &jr, &elabel, rec);

          mjcr->end_time = jr.EndTime;
          mjcr->setJobStatusWithPriorityCheck(JS_Terminated);

          // Create JobMedia record
          mjcr->sd_impl->read_dcr->VolLastIndex = dcr->VolLastIndex;
          if (mjcr->sd_impl->insert_jobmedia_records) {
            CreateJobmediaRecord(db, mjcr);
          }
          FreeDeviceControlRecord(mjcr->sd_impl->read_dcr);
          FreeJcr(mjcr);
        }
        break;

      case EOM_LABEL:
        break;

      case EOT_LABEL: /* end of all tapes */
        // Wiffle through all jobs still open and close them.
        if (update_db) {
          for (auto mdcr : my_dev->attached_dcrs) {
            JobControlRecord* mjcr2 = mdcr->jcr;
            if (!mjcr2 || mjcr2->JobId == 0) { continue; }
            jr.JobId = mjcr2->JobId;
            jr.JobStatus = JS_ErrorTerminated; /* Mark Job as Error Terimined */
            jr.JobFiles = mjcr2->JobFiles;
            jr.JobBytes = mjcr2->JobBytes;
            jr.VolSessionId = mjcr2->VolSessionId;
            jr.VolSessionTime = mjcr2->VolSessionTime;
            jr.JobTDate = (utime_t)mjcr2->start_time;
            jr.ClientId = mjcr2->ClientId;
            if (DbLocker _{db}; !db->UpdateJobEndRecord(my_bjcr, &jr)) {
              Pmsg1(0, T_("Could not update job record. ERR=%s\n"),
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
              T_("End of all Volumes. VolFiles=%u VolBlocks=%u VolBytes=%s\n"),
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
      Pmsg2(000, T_("Could not find Job for SessId=%d SessTime=%d record.\n"),
            rec->VolSessionId, rec->VolSessionTime);
    } else {
      ignored_msgs++;
    }
    return true;
  }
  dcr = mjcr->sd_impl->read_dcr;
  if (dcr->VolFirstIndex == 0) { dcr->VolFirstIndex = block->FirstIndex; }

  // File Attributes stream
  switch (rec->maskedStream) {
    case STREAM_UNIX_ATTRIBUTES:
    case STREAM_UNIX_ATTRIBUTES_EX:
      if (!UnpackAttributesRecord(my_bjcr, rec->Stream, rec->data,
                                  rec->data_len, attr)) {
        Emsg0(M_ERROR_TERM, 0, T_("Cannot continue.\n"));
      }

      if (g_verbose > 1) {
        DecodeStat(attr->attr, &attr->statp, sizeof(attr->statp),
                   &attr->LinkFI);
        BuildAttrOutputFnames(my_bjcr, attr);
        PrintLsOutput(my_bjcr, attr);
      }
      fr.JobId = mjcr->JobId;
      fr.FileId = 0;
      num_files++;
      if (g_verbose && (num_files & 0x7FFF) == 0) {
        char ed1[30], ed2[30], ed3[30], ed4[30];
        Pmsg4(000, T_("%s file records. At file:blk=%s:%s bytes=%s\n"),
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
      if (!UnpackRestoreObject(my_bjcr, rec->Stream, rec->data, rec->data_len,
                               &rop)) {
        Emsg0(M_ERROR_TERM, 0, T_("Cannot continue.\n"));
      }
      rop.FileIndex = mjcr->FileId;
      rop.JobId = mjcr->JobId;
      rop.FileType = FT_RESTORE_FIRST;


      if (update_db) { db->CreateRestoreObjectRecord(mjcr, &rop); }

      num_restoreobjects++;

      FreeJcr(mjcr); /* done using JobControlRecord */
      break;

    // Data streams
    case STREAM_WIN32_DATA:
    case STREAM_FILE_DATA:
    case STREAM_SPARSE_DATA:
    case STREAM_MACOS_FORK_DATA:
    case STREAM_ENCRYPTED_FILE_DATA:
    case STREAM_ENCRYPTED_WIN32_DATA:
    case STREAM_ENCRYPTED_MACOS_FORK_DATA:
      /* For encrypted stream, this is an approximation.
       * The data must be decrypted to know the correct length. */
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
      // Not correct, we should (decrypt and) expand it.
      mjcr->JobBytes += rec->data_len;
      FreeJcr(mjcr);
      break;

    case STREAM_SPARSE_GZIP_DATA:
    case STREAM_SPARSE_COMPRESSED_DATA:
      mjcr->JobBytes
          += rec->data_len
             - sizeof(uint64_t); /* Not correct, we should expand it */
      FreeJcr(mjcr);             /* done using JobControlRecord */
      break;

    // Win32 GZIP stream
    case STREAM_WIN32_GZIP_DATA:
    case STREAM_WIN32_COMPRESSED_DATA:
      mjcr->JobBytes += rec->data_len;
      FreeJcr(mjcr); /* done using JobControlRecord */
      break;

    case STREAM_MD5_DIGEST:
      BinToBase64(digest, sizeof(digest), (char*)rec->data,
                  CRYPTO_DIGEST_MD5_SIZE, true);
      if (g_verbose > 1) { Pmsg1(000, T_("Got MD5 record: %s\n"), digest); }
      UpdateDigestRecord(db, digest, rec, CRYPTO_DIGEST_MD5);
      break;

    case STREAM_SHA1_DIGEST:
      BinToBase64(digest, sizeof(digest), (char*)rec->data,
                  CRYPTO_DIGEST_SHA1_SIZE, true);
      if (g_verbose > 1) { Pmsg1(000, T_("Got SHA1 record: %s\n"), digest); }
      UpdateDigestRecord(db, digest, rec, CRYPTO_DIGEST_SHA1);
      break;

    case STREAM_SHA256_DIGEST:
      BinToBase64(digest, sizeof(digest), (char*)rec->data,
                  CRYPTO_DIGEST_SHA256_SIZE, true);
      if (g_verbose > 1) { Pmsg1(000, T_("Got SHA256 record: %s\n"), digest); }
      UpdateDigestRecord(db, digest, rec, CRYPTO_DIGEST_SHA256);
      break;

    case STREAM_SHA512_DIGEST:
      BinToBase64(digest, sizeof(digest), (char*)rec->data,
                  CRYPTO_DIGEST_SHA512_SIZE, true);
      if (g_verbose > 1) { Pmsg1(000, T_("Got SHA512 record: %s\n"), digest); }
      UpdateDigestRecord(db, digest, rec, CRYPTO_DIGEST_SHA512);
      break;

    case STREAM_XXH128_DIGEST:
      BinToBase64(digest, sizeof(digest), (char*)rec->data,
                  CRYPTO_DIGEST_XXH128_SIZE, true);
      if (g_verbose > 1) { Pmsg1(000, T_("Got XXH128 record: %s\n"), digest); }
      UpdateDigestRecord(db, digest, rec, CRYPTO_DIGEST_XXH128);
      break;

    case STREAM_ENCRYPTED_SESSION_DATA:
      // TODO landonf: Investigate crypto support in bscan
      if (g_verbose > 1) { Pmsg0(000, T_("Got signed digest record\n")); }
      break;

    case STREAM_SIGNED_DIGEST:
      // TODO landonf: Investigate crypto support in bscan
      if (g_verbose > 1) { Pmsg0(000, T_("Got signed digest record\n")); }
      break;

    case STREAM_PROGRAM_NAMES:
      if (g_verbose) {
        Pmsg1(000, T_("Got Prog Names Stream: %s\n"), rec->data);
      }
      break;

    case STREAM_PROGRAM_DATA:
      if (g_verbose > 1) { Pmsg0(000, T_("Got Prog Data Stream record.\n")); }
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
      Pmsg2(0, T_("Unknown stream type!!! stream=%d len=%i\n"), rec->Stream,
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
    jcr->file_bsock = nullptr;
  }

  if (jcr->store_bsock) {
    Dmsg0(200, "Close Store bsock\n");
    jcr->store_bsock->close();
    delete jcr->store_bsock;
    jcr->store_bsock = nullptr;
  }

  if (jcr->RestoreBootstrap) { free(jcr->RestoreBootstrap); }

  if (jcr->sd_impl->dcr) {
    FreeDeviceControlRecord(jcr->sd_impl->dcr);
    jcr->sd_impl->dcr = nullptr;
  }

  if (jcr->sd_impl->read_dcr) {
    FreeDeviceControlRecord(jcr->sd_impl->read_dcr);
    jcr->sd_impl->read_dcr = nullptr;
  }

  if (jcr->sd_impl) {
    delete jcr->sd_impl;
    jcr->sd_impl = nullptr;
  }

  Dmsg0(200, "End bscan FreeJcr\n");
}

/**
 * We got a File Attributes record on the tape.  Now, lookup the Job
 * record, and then create the attributes record.
 */
static bool CreateFileAttributesRecord(BareosDb* t_db,
                                       JobControlRecord* mjcr,
                                       char* fname,
                                       char* lname,
                                       int type,
                                       char* ap,
                                       DeviceRecord* rec)
{
  DeviceControlRecord* dcr = mjcr->sd_impl->read_dcr;
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

  if (DbLocker _{t_db}; !t_db->CreateFileAttributesRecord(bjcr, &ar)) {
    Pmsg1(0, T_("Could not create File Attributes record. ERR=%s\n"),
          t_db->strerror());
    return false;
  }
  mjcr->FileId = ar.FileId;

  if (g_verbose > 1) { Pmsg1(000, T_("Created File record: %s\n"), fname); }

  return true;
}

// For each Volume we see, we create a Medium record
static bool CreateMediaRecord(BareosDb* t_db,
                              MediaDbRecord* t_mr,
                              Volume_Label* vl)
{
  // We mark Vols as Archive to keep them from being re-written
  bstrncpy(t_mr->VolStatus, "Archive", sizeof(t_mr->VolStatus));
  t_mr->VolRetention = 365 * 3600 * 24; /* 1 year */
  t_mr->Enabled = 1;
  // make sure this volume wasn't written by bacula 1.26 or earlier
  ASSERT(vl->VerNum >= 11);
  t_mr->set_first_written = true; /* Save FirstWritten during update_media */
  t_mr->FirstWritten = BtimeToUtime(vl->write_btime);
  t_mr->LabelDate = BtimeToUtime(vl->label_btime);
  lasttime = t_mr->LabelDate;

  if (t_mr->VolJobs == 0) { t_mr->VolJobs = 1; }

  if (t_mr->VolMounts == 0) { t_mr->VolMounts = 1; }

  if (!update_db) { return true; }

  DbLocker _{t_db};
  if (!t_db->CreateMediaRecord(bjcr, t_mr)) {
    Pmsg1(000, T_("Could not create media record. ERR=%s\n"), t_db->strerror());
    return false;
  }
  if (!t_db->UpdateMediaRecord(bjcr, t_mr)) {
    Pmsg1(000, T_("Could not update media record. ERR=%s\n"), t_db->strerror());
    return false;
  }
  if (g_verbose) {
    Pmsg1(000, T_("Created Media record for Volume: %s\n"), t_mr->VolumeName);
  }

  return true;
}

// Called at end of media to update it
static bool UpdateMediaRecord(BareosDb* t_db, MediaDbRecord* t_mr)
{
  if (!update_db && !update_vol_info) { return true; }

  t_mr->LastWritten = lasttime;
  if (DbLocker _{t_db}; !t_db->UpdateMediaRecord(bjcr, t_mr)) {
    Pmsg1(000, T_("Could not update media record. ERR=%s\n"), t_db->strerror());
    return false;
  }

  if (g_verbose) {
    Pmsg1(000, T_("Updated Media record at end of Volume: %s\n"),
          t_mr->VolumeName);
  }

  return true;
}

static bool CreatePoolRecord(BareosDb* t_db, PoolDbRecord* t_pr)
{
  t_pr->NumVols++;
  t_pr->UseCatalog = 1;
  t_pr->VolRetention = 355 * 3600 * 24; /* 1 year */

  if (!update_db) { return true; }

  if (DbLocker _{t_db}; !t_db->CreatePoolRecord(bjcr, t_pr)) {
    Pmsg1(000, T_("Could not create pool record. ERR=%s\n"), t_db->strerror());
    return false;
  }

  if (g_verbose) {
    Pmsg1(000, T_("Created Pool record for Pool: %s\n"), t_pr->Name);
  }

  return true;
}

// Called from SOS to create a client for the current Job
static bool CreateClientRecord(BareosDb* t_db, ClientDbRecord* t_cr)
{
  /* Note, update_db can temporarily be set false while
   * updating the database, so we must ensure that ClientId is non-zero. */
  if (!update_db) {
    t_cr->ClientId = 0;
    if (DbLocker _{t_db}; !t_db->GetClientRecord(bjcr, t_cr)) {
      Pmsg1(0, T_("Could not get Client record. ERR=%s\n"), t_db->strerror());
      return false;
    }

    return true;
  }

  if (DbLocker _{t_db}; !t_db->CreateClientRecord(bjcr, t_cr)) {
    Pmsg1(000, T_("Could not create Client record. ERR=%s\n"),
          t_db->strerror());
    return false;
  }

  if (g_verbose) {
    Pmsg1(000, T_("Created Client record for Client: %s\n"), t_cr->Name);
  }

  return true;
}

static bool CreateFilesetRecord(BareosDb* t_db, FileSetDbRecord* t_fsr)
{
  if (!update_db) { return true; }

  t_fsr->FileSetId = 0;
  if (t_fsr->MD5[0] == 0) {
    t_fsr->MD5[0] = ' '; /* Equivalent to nothing */
    t_fsr->MD5[1] = 0;
  }

  if (t_db->GetFilesetRecord(bjcr, t_fsr)) {
    if (g_verbose) {
      Pmsg1(000, T_("Fileset \"%s\" already exists.\n"), t_fsr->FileSet);
    }
  } else {
    if (DbLocker _{t_db}; !t_db->CreateFilesetRecord(bjcr, t_fsr)) {
      Pmsg2(000, T_("Could not create FileSet record \"%s\". ERR=%s\n"),
            t_fsr->FileSet, t_db->strerror());
      return false;
    }

    if (g_verbose) {
      Pmsg1(000, T_("Created FileSet record \"%s\"\n"), t_fsr->FileSet);
    }
  }

  return true;
}

/**
 * Simulate the two calls on the database to create the Job record and
 * to update it when the Job actually begins running.
 */
static JobControlRecord* CreateJobRecord(BareosDb* t_db,
                                         JobDbRecord* t_jr,
                                         Session_Label* t_label,
                                         DeviceRecord* t_rec)
{
  JobControlRecord* mjcr;

  t_jr->JobId = t_label->JobId;
  t_jr->JobType = t_label->JobType;
  t_jr->JobLevel = t_label->JobLevel;
  t_jr->JobStatus = JS_Created;
  bstrncpy(t_jr->Name, t_label->JobName, sizeof(t_jr->Name));
  bstrncpy(t_jr->Job, t_label->Job, sizeof(t_jr->Job));
  // make sure this volume wasn't written by bacula 1.26 or earlier
  ASSERT(t_label->VerNum >= 11);
  t_jr->SchedTime = BtimeToUnix(t_label->write_btime);
  t_jr->StartTime = t_jr->SchedTime;
  t_jr->JobTDate = (utime_t)t_jr->SchedTime;
  t_jr->VolSessionId = t_rec->VolSessionId;
  t_jr->VolSessionTime = t_rec->VolSessionTime;

  /* Now create a JobControlRecord as if starting the Job */
  mjcr = create_jcr(t_jr, t_rec, t_label->JobId);

  if (!update_db) { return mjcr; }

  DbLocker _{t_db};
  // This creates the bare essentials
  if (!t_db->CreateJobRecord(bjcr, t_jr)) {
    Pmsg1(0, T_("Could not create JobId record. ERR=%s\n"), t_db->strerror());
    return mjcr;
  }

  // This adds the client, StartTime, JobTDate, ...
  if (!t_db->UpdateJobStartRecord(bjcr, t_jr)) {
    Pmsg1(0, T_("Could not update job start record. ERR=%s\n"),
          t_db->strerror());
    return mjcr;
  }

  Pmsg2(000, T_("Created new JobId=%u record for original JobId=%u\n"),
        t_jr->JobId, t_label->JobId);
  mjcr->JobId = t_jr->JobId; /* set new JobId */

  return mjcr;
}

// Simulate the database call that updates the Job at Job termination time.
static bool UpdateJobRecord(BareosDb* t_db,
                            JobDbRecord* t_jr,
                            Session_Label* t_elabel,
                            DeviceRecord* rec)
{
  JobControlRecord* mjcr;

  mjcr = get_jcr_by_session(rec->VolSessionId, rec->VolSessionTime);
  if (!mjcr) {
    Pmsg2(000, T_("Could not find SessId=%d SessTime=%d for EOS record.\n"),
          rec->VolSessionId, rec->VolSessionTime);
    return false;
  }

  // make sure this volume wasn't written by bacula 1.26 or earlier
  ASSERT(t_elabel->VerNum >= 11);
  t_jr->EndTime = BtimeToUnix(t_elabel->write_btime);

  lasttime = t_jr->EndTime;
  mjcr->end_time = t_jr->EndTime;

  t_jr->JobId = mjcr->JobId;
  t_jr->JobStatus = t_elabel->JobStatus;
  mjcr->setJobStatus(t_elabel->JobStatus);
  t_jr->JobFiles = t_elabel->JobFiles;
  if (t_jr->JobFiles > 0) { /* If we found files, force PurgedFiles */
    t_jr->PurgedFiles = 0;
  }
  t_jr->JobBytes = t_elabel->JobBytes;
  t_jr->VolSessionId = rec->VolSessionId;
  t_jr->VolSessionTime = rec->VolSessionTime;
  t_jr->JobTDate = (utime_t)mjcr->start_time;
  t_jr->ClientId = mjcr->ClientId;

  if (!update_db) {
    FreeJcr(mjcr);
    return true;
  }

  if (DbLocker _{t_db}; !t_db->UpdateJobEndRecord(bjcr, t_jr)) {
    Pmsg2(0, T_("Could not update JobId=%u record. ERR=%s\n"), t_jr->JobId,
          t_db->strerror());
    FreeJcr(mjcr);
    return false;
  }

  if (g_verbose) {
    Pmsg3(000,
          T_("Updated Job termination record for JobId=%u Level=%s "
             "TermStat=%c\n"),
          t_jr->JobId, job_level_to_str(mjcr->getJobLevel()), t_jr->JobStatus);
  }

  if (g_verbose > 1) {
    const char* TermMsg;
    static char term_code[70];
    char sdt[50], edt[50];
    char ec1[30], ec2[30], ec3[30];

    switch (mjcr->getJobStatus()) {
      case JS_Terminated:
        TermMsg = T_("Backup OK");
        break;
      case JS_Warnings:
        TermMsg = T_("Backup OK -- with warnings");
        break;
      case JS_FatalError:
      case JS_ErrorTerminated:
        TermMsg = T_("*** Backup Error ***");
        break;
      case JS_Canceled:
        TermMsg = T_("Backup Canceled");
        break;
      default:
        TermMsg = term_code;
        sprintf(term_code, T_("Job Termination code: %d"),
                mjcr->getJobStatus());
        break;
    }
    bstrftime(sdt, sizeof(sdt), mjcr->start_time);
    bstrftime(edt, sizeof(edt), mjcr->end_time);
    Pmsg15(000,
           T_("%s\n"
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
           edt, mjcr->JobId, mjcr->Job, mjcr->sd_impl->fileset_name,
           job_level_to_str(mjcr->getJobLevel()), mjcr->client_name, sdt, edt,
           edit_uint64_with_commas(mjcr->JobFiles, ec1),
           edit_uint64_with_commas(mjcr->JobBytes, ec2), mjcr->VolSessionId,
           mjcr->VolSessionTime, edit_uint64_with_commas(mr.VolBytes, ec3),
           kBareosVersionStrings.BinaryInfo, TermMsg);
  }
  FreeJcr(mjcr);

  return true;
}

static bool CreateJobmediaRecord(BareosDb* t_db, JobControlRecord* mjcr)
{
  JobMediaDbRecord jmr;
  DeviceControlRecord* dcr = mjcr->sd_impl->read_dcr;

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

  if (DbLocker _{t_db}; !t_db->CreateJobmediaRecord(bjcr, &jmr)) {
    Pmsg1(0, T_("Could not create JobMedia record. ERR=%s\n"),
          t_db->strerror());
    return false;
  }
  if (g_verbose) {
    Pmsg2(000, T_("Created JobMedia record JobId %d, MediaId %d\n"), jmr.JobId,
          jmr.MediaId);
  }

  return true;
}

// Simulate the database call that updates the MD5/SHA1 record
static bool UpdateDigestRecord(BareosDb* t_db,
                               char* digest,
                               DeviceRecord* rec,
                               int type)
{
  JobControlRecord* mjcr;

  mjcr = get_jcr_by_session(rec->VolSessionId, rec->VolSessionTime);
  if (!mjcr) {
    if (mr.VolJobs > 0) {
      Pmsg2(000,
            T_("Could not find SessId=%d SessTime=%d for MD5/SHA1 record.\n"),
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

  if (DbLocker _{t_db};
      !t_db->AddDigestToFileRecord(bjcr, mjcr->FileId, digest, type)) {
    Pmsg1(0, T_("Could not add MD5/SHA1 to File record. ERR=%s\n"),
          t_db->strerror());
    FreeJcr(mjcr);
    return false;
  }

  if (g_verbose > 1) { Pmsg0(000, T_("Updated MD5/SHA1 record\n")); }
  FreeJcr(mjcr);

  return true;
}

// Create a JobControlRecord as if we are really starting the job
static JobControlRecord* create_jcr(JobDbRecord* t_jr,
                                    DeviceRecord* rec,
                                    uint32_t JobId)
{
  JobControlRecord* jobjcr;
  /* Transfer as much as possible to the Job JobControlRecord. Most important is
   *   the JobId and the ClientId. */
  jobjcr = new_jcr(BscanFreeJcr);
  jobjcr->sd_impl = new StoredJcrImpl;
  register_jcr(jobjcr);
  jobjcr->setJobType(t_jr->JobType);
  jobjcr->setJobLevel(t_jr->JobLevel);
  jobjcr->setJobStatus(t_jr->JobStatus);
  bstrncpy(jobjcr->Job, t_jr->Job, sizeof(jobjcr->Job));
  jobjcr->JobId = JobId; /* this is JobId on tape */
  jobjcr->sched_time = t_jr->SchedTime;
  jobjcr->start_time = t_jr->StartTime;
  jobjcr->VolSessionId = rec->VolSessionId;
  jobjcr->VolSessionTime = rec->VolSessionTime;
  jobjcr->ClientId = t_jr->ClientId;
  jobjcr->sd_impl->dcr = jobjcr->sd_impl->read_dcr = new DeviceControlRecord;
  SetupNewDcrDevice(jobjcr, jobjcr->sd_impl->dcr, dev, nullptr);

  return jobjcr;
}
