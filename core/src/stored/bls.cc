/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2000-2012 Free Software Foundation Europe e.V.
   Copyright (C) 2011-2012 Planets Communications B.V.
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
// Kern Sibbald, MM
/**
 * @file
 * Dumb program to do an "ls" of a Bareos 2.0 mortal file.
 */

#if !defined(HAVE_MSVC)
#  include <unistd.h>
#endif
#include "include/bareos.h"
#include "include/exit_codes.h"
#include "include/streams.h"
#include "stored/stored.h"
#include "stored/stored_globals.h"
#include "lib/berrno.h"
#include "lib/crypto_cache.h"
#include "findlib/find.h"
#include "stored/acquire.h"
#include "stored/butil.h"
#include "stored/device_control_record.h"
#include "stored/stored_jcr_impl.h"
#include "stored/label.h"
#include "stored/match_bsr.h"
#include "stored/mount.h"
#include "stored/read_record.h"
#include "findlib/match.h"
#include "lib/address_conf.h"
#include "lib/attribs.h"
#include "lib/bsignal.h"
#include "lib/cli.h"
#include "lib/parse_bsr.h"
#include "include/jcr.h"
#include "lib/parse_conf.h"
#include "lib/compression.h"
#include "lib/bareos_universal_initialiser.h"

namespace storagedaemon {
extern bool ParseSdConfig(const char* configfile, int exit_code);
}

using namespace storagedaemon;

static void DoBlocks(char* infname);
static void do_jobs(char* infname);
static void do_ls(char* fname);
static void do_close(JobControlRecord* jcr);
static void GetSessionRecord(Device* dev,
                             DeviceRecord* rec,
                             Session_Label* sessrec);
static bool RecordCb(DeviceControlRecord* dcr, DeviceRecord* rec);

static Device* dev;
static DeviceControlRecord* dcr;
static bool dump_label = false;
static DeviceRecord* rec;
static JobControlRecord* jcr;
static Session_Label sessrec;
static uint32_t num_files = 0;
static Attributes* attr;

static FindFilesPacket* ff;
static BootStrapRecord* bsr = nullptr;

int main(int argc, char* argv[])
{
  DirectorResource* director = nullptr;
  setlocale(LC_ALL, "");
  tzset();
  bindtextdomain("bareos", LOCALEDIR);
  textdomain("bareos");
  InitStackDump();

  working_directory = "/tmp";
  MyNameIs(argc, argv, "bls");
  InitMsg(nullptr, nullptr); /* initialize message handler */

  OSDependentInit();

  (void)WSA_Init(); /* Initialize Windows sockets */

  ff = init_find_files();

  CLI::App bls_app;
  InitCLIApp(bls_app, "The Bareos ls tool.", 2000);

  std::string bsrName;
  bls_app
      .add_option("-b,--parse-bootstrap", bsrName, "Specify a bootstrap file.")
      ->check(CLI::ExistingFile)
      ->type_name("<file>");

  bls_app
      .add_option(
          "-c,--config",
          [](std::vector<std::string> val) {
            if (configfile != nullptr) { free(configfile); }
            configfile = strdup(val.front().c_str());
            return true;
          },
          "Use <path> as configuration file or directory.")
      ->check(CLI::ExistingPath)
      ->type_name("<path>");

  std::string DirectorName{};
  bls_app
      .add_option("-D,--director", DirectorName,
                  "Specify a director name found in the storage.\n"
                  "Configuration file for the Key Encryption Key selection.")
      ->type_name("<director>");

  AddDebugOptions(bls_app);

  FILE* fd;
  char line[1000];
  bls_app
      .add_option(
          "-e,--exclude",
          [&line, &fd](std::vector<std::string> val) {
            if ((fd = fopen(val.front().c_str(), "rb")) == nullptr) {
              BErrNo be;
              Pmsg2(0, T_("Could not open exclude file: %s, ERR=%s\n"),
                    val.front().c_str(), be.bstrerror());
              exit(BEXIT_FAILURE);
            }
            while (fgets(line, sizeof(line), fd) != nullptr) {
              StripTrailingJunk(line);
              Dmsg1(100, "add_exclude %s\n", line);
              AddFnameToExcludeList(ff, line);
            }
            fclose(fd);
            return true;
          },
          "Exclude list.")
      ->type_name("<file>");

  bls_app
      .add_option(
          "-i,--include-list",
          [&line, &fd](std::vector<std::string> val) {
            if ((fd = fopen(val.front().c_str(), "rb")) == nullptr) {
              BErrNo be;
              Pmsg2(0, T_("Could not open include file: %s, ERR=%s\n"),
                    val.front().c_str(), be.bstrerror());
              exit(BEXIT_FAILURE);
            }
            while (fgets(line, sizeof(line), fd) != nullptr) {
              StripTrailingJunk(line);
              Dmsg1(100, "add_include %s\n", line);
              AddFnameToIncludeList(ff, 0, line);
            }
            fclose(fd);
            return true;
          },
          "Include list.")
      ->type_name("<file>");

  bool list_jobs = false;
  bls_app.add_flag("-j,--list-jobs", list_jobs, "List jobs.");

  bool list_blocks = false;
  bls_app.add_flag("-k,--list-blocks", list_blocks,
                   "List blocks.\n"
                   "If neither -j or -k specified, list saved files.");

  bls_app.add_flag("-L,--dump-labels", dump_label, "Dump labels.");


  bool ignore_label_errors = false;
  bls_app.add_flag(
      "-p,--ignore-errors",
      [&ignore_label_errors](bool) {
        ignore_label_errors = true;
        forge_on = true;
      },
      "Proceed inspite of IO errors.");

  AddVerboseOption(bls_app);

  std::string VolumeNames;
  bls_app
      .add_option("-V,--volumes", VolumeNames, "Volume names (separated by |)")
      ->type_name("<vol1|vol2|...>");

  std::vector<std::string> device_names;
  bls_app
      .add_option("device-names", device_names,
                  "Specify the input device name "
                  "(either as name of a Bareos Storage Daemon Device resource "
                  "or identical to the "
                  "Archive Device in a Bareos Storage Daemon Device resource).")
      ->type_name(" ");

  ParseBareosApp(bls_app, argc, argv);

  my_config = InitSdConfig(configfile, M_CONFIG_ERROR);
  ParseSdConfig(configfile, M_CONFIG_ERROR);

  if (device_names.size() == 0) {
    printf(T_("%sNothing done."), AvailableDevicesListing().c_str());
    return BEXIT_SUCCESS;
  }

  if (!DirectorName.empty()) {
    foreach_res (director, R_DIRECTOR) {
      if (bstrcmp(director->resource_name_, DirectorName.c_str())) { break; }
    }
    if (!director) {
      Emsg2(
          M_ERROR_TERM, 0,
          T_("No Director resource named %s defined in %s. Cannot continue.\n"),
          DirectorName.c_str(), configfile);
    }
  }

  LoadSdPlugins(me->plugin_directory, me->plugin_names);

  ReadCryptoCache(me->working_directory, "bareos-sd",
                  GetFirstPortHostOrder(me->SDaddrs));

  if (ff->included_files_list == nullptr) { AddFnameToIncludeList(ff, 0, "/"); }


  for (std::string device : device_names) {
    if (!bsrName.empty()) {
      bsr = libbareos::parse_bsr(nullptr, bsrName.data());
    }
    dcr = new DeviceControlRecord;
    jcr = SetupJcr("bls", device.data(), bsr, director, dcr, VolumeNames,
                   true); /* read device */
    if (!jcr) { exit(BEXIT_FAILURE); }


    // Let SD plugins setup the record translation
    if (GeneratePluginEvent(jcr, bSdEventSetupRecordTranslation, dcr)
        != bRC_OK) {
      Jmsg(jcr, M_FATAL, 0,
           T_("bSdEventSetupRecordTranslation call failed!\n"));
    }

    jcr->sd_impl->ignore_label_errors = ignore_label_errors;
    dev = jcr->sd_impl->dcr->dev;
    if (!dev) { exit(BEXIT_FAILURE); }
    dcr = jcr->sd_impl->dcr;
    rec = new_record();
    attr = new_attr(jcr);
    /* Assume that we have already read the volume label.
     * If on second or subsequent volume, adjust buffer pointer */
    if (dev->VolHdr.PrevVolumeName[0] != 0) { /* second volume */
      Pmsg1(0,
            T_("\n"
               "Warning, this Volume is a continuation of Volume %s\n"),
            dev->VolHdr.PrevVolumeName);
    }

    if (list_blocks) {
      DoBlocks(device.data());
    } else if (list_jobs) {
      do_jobs(device.data());
    } else {
      do_ls(device.data());
    }
    do_close(jcr);
  }
  if (bsr) { libbareos::FreeBsr(bsr); }
  TermIncludeExcludeFiles(ff);
  TermFindFiles(ff);
  return BEXIT_SUCCESS;
}

static void do_close(JobControlRecord* t_jcr)
{
  FreeAttr(attr);
  FreeRecord(rec);
  CleanDevice(t_jcr->sd_impl->dcr);
  delete dev;
  FreeDeviceControlRecord(t_jcr->sd_impl->dcr);

  CleanupCompression(t_jcr);
  FreePlugins(t_jcr);
  FreeJcr(t_jcr);
  UnloadSdPlugins();
}

/* List just block information */
static void DoBlocks(char*)
{
  DeviceBlock* block = dcr->block;
  char buf1[100], buf2[100];
  for (;;) {
    switch (dcr->ReadBlockFromDevice(NO_BLOCK_NUMBER_CHECK)) {
      case DeviceControlRecord::ReadStatus::Ok:
        // no special handling required
        break;
      case DeviceControlRecord::ReadStatus::EndOfTape:
        if (!MountNextReadVolume(dcr)) {
          Jmsg(jcr, M_INFO, 0,
               T_("Got EOM at file %u on device %s, Volume \"%s\"\n"),
               dev->file, dev->print_name(), dcr->VolumeName);
          return;
        }
        /* Read and discard Volume label */
        DeviceRecord* record;
        record = new_record();
        dcr->ReadBlockFromDevice(NO_BLOCK_NUMBER_CHECK);
        ReadRecordFromBlock(dcr, record);
        GetSessionRecord(dev, record, &sessrec);
        FreeRecord(record);
        Jmsg(jcr, M_INFO, 0, T_("Mounted Volume \"%s\".\n"), dcr->VolumeName);
        break;
      case DeviceControlRecord::ReadStatus::EndOfFile:
        Jmsg(jcr, M_INFO, 0, T_("End of file %u on device %s, Volume \"%s\"\n"),
             dev->file, dev->print_name(), dcr->VolumeName);
        Dmsg0(20, "read_record got eof. try again\n");
        continue;
      default:
        Dmsg1(100, "!read_block(): ERR=%s\n", dev->bstrerror());
        if (dev->IsShortBlock()) {
          Jmsg(jcr, M_INFO, 0, "%s", dev->errmsg);
          continue;
        } else {
          /* I/O error */
          DisplayTapeErrorStatus(jcr, dev);
          return;
        }
    }
    if (!MatchBsrBlock(bsr, block)) {
      Dmsg5(100, "reject Blk=%u blen=%u bVer=%d SessId=%u SessTim=%u\n",
            block->BlockNumber, block->block_len, block->BlockVer,
            block->VolSessionId, block->VolSessionTime);
      continue;
    }
    Dmsg5(100, "Blk=%u blen=%u bVer=%d SessId=%u SessTim=%u\n",
          block->BlockNumber, block->block_len, block->BlockVer,
          block->VolSessionId, block->VolSessionTime);
    if (g_verbose == 1) {
      ReadRecordFromBlock(dcr, rec);
      Pmsg9(-1,
            T_("File:blk=%u:%u blk_num=%u blen=%u First rec FI=%s SessId=%u "
               "SessTim=%u Strm=%s rlen=%d\n"),
            dev->file, dev->block_num, block->BlockNumber, block->block_len,
            FI_to_ascii(buf1, rec->FileIndex), rec->VolSessionId,
            rec->VolSessionTime,
            stream_to_ascii(buf2, rec->Stream, rec->FileIndex), rec->data_len);
      rec->remainder = 0;
    } else if (g_verbose > 1) {
      DumpBlock(block, "");
    } else {
      printf(T_("Block: %d size=%d\n"), block->BlockNumber, block->block_len);
    }
  }
  return;
}

// We are only looking for labels or in particular Job Session records
static bool jobs_cb(DeviceControlRecord* t_dcr, DeviceRecord* t_rec)
{
  if (t_rec->FileIndex < 0) { DumpLabelRecord(t_dcr->dev, t_rec, g_verbose); }
  t_rec->remainder = 0;
  return true;
}

/* Do list job records */
static void do_jobs(char*) { ReadRecords(dcr, jobs_cb, MountNextReadVolume); }

/* Do an ls type listing of an archive */
static void do_ls(char*)
{
  if (dump_label) {
    DumpVolumeLabel(dev);
    return;
  }
  ReadRecords(dcr, RecordCb, MountNextReadVolume);
  printf("%u files and directories found.\n", num_files);
}

// Called here for each record from ReadRecords()
static bool RecordCb(DeviceControlRecord*, DeviceRecord* t_rec)
{
  PoolMem record_str(PM_MESSAGE);

  if (t_rec->FileIndex < 0) {
    GetSessionRecord(dev, t_rec, &sessrec);
    return true;
  }

  // File Attributes stream
  switch (t_rec->maskedStream) {
    case STREAM_UNIX_ATTRIBUTES:
    case STREAM_UNIX_ATTRIBUTES_EX:
      if (!UnpackAttributesRecord(jcr, t_rec->Stream, t_rec->data,
                                  t_rec->data_len, attr)) {
        if (!forge_on) {
          Emsg0(M_ERROR_TERM, 0, T_("Cannot continue.\n"));
        } else {
          Emsg0(M_ERROR, 0, T_("Attrib unpack error!\n"));
        }
        num_files++;
        return true;
      }

      attr->data_stream = DecodeStat(attr->attr, &attr->statp,
                                     sizeof(attr->statp), &attr->LinkFI);
      BuildAttrOutputFnames(jcr, attr);

      if (FileIsIncluded(ff, attr->fname) && !FileIsExcluded(ff, attr->fname)) {
        if (!g_verbose) {
          PrintLsOutput(jcr, attr);
        } else {
          Pmsg1(-1, "%s\n", record_to_str(record_str, jcr, t_rec));
        }
        num_files++;
      }
      break;
    default:
      if (g_verbose) {
        Pmsg1(-1, "%s\n", record_to_str(record_str, jcr, t_rec));
      }
      break;
  }

  /* debug output of record */
  DumpRecord("", t_rec);

  return true;
}

static void GetSessionRecord(Device* t_dev,
                             DeviceRecord* t_rec,
                             Session_Label* t_sessrec)
{
  const char* rtype;
  Session_Label empty_Session_Label;
  *t_sessrec = empty_Session_Label;
  jcr->JobId = 0;
  switch (t_rec->FileIndex) {
    case PRE_LABEL:
      rtype = T_("Fresh Volume Label");
      break;
    case VOL_LABEL:
      rtype = T_("Volume Label");
      UnserVolumeLabel(t_dev, t_rec);
      break;
    case SOS_LABEL:
      rtype = T_("Begin Job Session");
      UnserSessionLabel(t_sessrec, t_rec);
      jcr->JobId = t_sessrec->JobId;
      break;
    case EOS_LABEL:
      rtype = T_("End Job Session");
      break;
    case 0:
    case EOM_LABEL:
      rtype = T_("End of Medium");
      break;
    case EOT_LABEL:
      rtype = T_("End of Physical Medium");
      break;
    case SOB_LABEL:
      rtype = T_("Start of object");
      break;
    case EOB_LABEL:
      rtype = T_("End of object");
      break;
    default:
      rtype = T_("Unknown");
      Dmsg1(10, "FI rtype=%d unknown\n", t_rec->FileIndex);
      break;
  }
  Dmsg5(10,
        "%s Record: VolSessionId=%d VolSessionTime=%d JobId=%d DataLen=%d\n",
        rtype, t_rec->VolSessionId, t_rec->VolSessionTime, t_rec->Stream,
        t_rec->data_len);
  if (g_verbose) {
    Pmsg5(-1,
          T_("%s Record: VolSessionId=%d VolSessionTime=%d JobId=%d "
             "DataLen=%d\n"),
          rtype, t_rec->VolSessionId, t_rec->VolSessionTime, t_rec->Stream,
          t_rec->data_len);
  }
}
