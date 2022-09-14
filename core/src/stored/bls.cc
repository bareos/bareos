/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2000-2012 Free Software Foundation Europe e.V.
   Copyright (C) 2011-2012 Planets Communications B.V.
   Copyright (C) 2013-2022 Bareos GmbH & Co. KG

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

#include "include/bareos.h"
#include "stored/stored.h"
#include "stored/stored_globals.h"
#include "lib/berrno.h"
#include "lib/crypto_cache.h"
#include "findlib/find.h"
#include "stored/acquire.h"
#include "stored/butil.h"
#include "stored/device_control_record.h"
#include "stored/jcr_private.h"
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
              Pmsg2(0, _("Could not open exclude file: %s, ERR=%s\n"), optarg,
                    be.bstrerror());
              exit(1);
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
          [&line, &fd]([[maybe_unused]] std::vector<std::string> val) {
            if ((fd = fopen(optarg, "rb")) == nullptr) {
              BErrNo be;
              Pmsg2(0, _("Could not open include file: %s, ERR=%s\n"), optarg,
                    be.bstrerror());
              exit(1);
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
      [&ignore_label_errors]([[maybe_unused]] bool val) {
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
      ->required()
      ->type_name(" ");

  CLI11_PARSE(bls_app, argc, argv);

  my_config = InitSdConfig(configfile, M_ERROR_TERM);
  ParseSdConfig(configfile, M_ERROR_TERM);

  if (!DirectorName.empty()) {
    foreach_res (director, R_DIRECTOR) {
      if (bstrcmp(director->resource_name_, DirectorName.c_str())) { break; }
    }
    if (!director) {
      Emsg2(
          M_ERROR_TERM, 0,
          _("No Director resource named %s defined in %s. Cannot continue.\n"),
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
    if (!jcr) { exit(1); }
    jcr->impl->ignore_label_errors = ignore_label_errors;
    dev = jcr->impl->dcr->dev;
    if (!dev) { exit(1); }
    dcr = jcr->impl->dcr;
    rec = new_record();
    attr = new_attr(jcr);
    /*
     * Assume that we have already read the volume label.
     * If on second or subsequent volume, adjust buffer pointer
     */
    if (dev->VolHdr.PrevVolumeName[0] != 0) { /* second volume */
      Pmsg1(0,
            _("\n"
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
  return 0;
}

static void do_close(JobControlRecord* jcr)
{
  FreeAttr(attr);
  FreeRecord(rec);
  CleanDevice(jcr->impl->dcr);
  delete dev;
  FreeDeviceControlRecord(jcr->impl->dcr);
  FreeJcr(jcr);
}

/* List just block information */
static void DoBlocks([[maybe_unused]] char* infname)
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
               _("Got EOM at file %u on device %s, Volume \"%s\"\n"), dev->file,
               dev->print_name(), dcr->VolumeName);
          return;
        }
        /* Read and discard Volume label */
        DeviceRecord* record;
        record = new_record();
        dcr->ReadBlockFromDevice(NO_BLOCK_NUMBER_CHECK);
        ReadRecordFromBlock(dcr, record);
        GetSessionRecord(dev, record, &sessrec);
        FreeRecord(record);
        Jmsg(jcr, M_INFO, 0, _("Mounted Volume \"%s\".\n"), dcr->VolumeName);
        break;
      case DeviceControlRecord::ReadStatus::EndOfFile:
        Jmsg(jcr, M_INFO, 0, _("End of file %u on device %s, Volume \"%s\"\n"),
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
    if (verbose == 1) {
      ReadRecordFromBlock(dcr, rec);
      Pmsg9(-1,
            _("File:blk=%u:%u blk_num=%u blen=%u First rec FI=%s SessId=%u "
              "SessTim=%u Strm=%s rlen=%d\n"),
            dev->file, dev->block_num, block->BlockNumber, block->block_len,
            FI_to_ascii(buf1, rec->FileIndex), rec->VolSessionId,
            rec->VolSessionTime,
            stream_to_ascii(buf2, rec->Stream, rec->FileIndex), rec->data_len);
      rec->remainder = 0;
    } else if (verbose > 1) {
      DumpBlock(block, "");
    } else {
      printf(_("Block: %d size=%d\n"), block->BlockNumber, block->block_len);
    }
  }
  return;
}

// We are only looking for labels or in particular Job Session records
static bool jobs_cb(DeviceControlRecord* dcr, DeviceRecord* rec)
{
  if (rec->FileIndex < 0) { DumpLabelRecord(dcr->dev, rec, verbose); }
  rec->remainder = 0;
  return true;
}

/* Do list job records */
static void do_jobs([[maybe_unused]] char* infname)
{
  ReadRecords(dcr, jobs_cb, MountNextReadVolume);
}

/* Do an ls type listing of an archive */
static void do_ls([[maybe_unused]] char* infname)
{
  if (dump_label) {
    DumpVolumeLabel(dev);
    return;
  }
  ReadRecords(dcr, RecordCb, MountNextReadVolume);
  printf("%u files and directories found.\n", num_files);
}

// Called here for each record from ReadRecords()
static bool RecordCb([[maybe_unused]] DeviceControlRecord* dcr,
                     DeviceRecord* rec)
{
  PoolMem record_str(PM_MESSAGE);

  if (rec->FileIndex < 0) {
    GetSessionRecord(dev, rec, &sessrec);
    return true;
  }

  // File Attributes stream
  switch (rec->maskedStream) {
    case STREAM_UNIX_ATTRIBUTES:
    case STREAM_UNIX_ATTRIBUTES_EX:
      if (!UnpackAttributesRecord(jcr, rec->Stream, rec->data, rec->data_len,
                                  attr)) {
        if (!forge_on) {
          Emsg0(M_ERROR_TERM, 0, _("Cannot continue.\n"));
        } else {
          Emsg0(M_ERROR, 0, _("Attrib unpack error!\n"));
        }
        num_files++;
        return true;
      }

      attr->data_stream = DecodeStat(attr->attr, &attr->statp,
                                     sizeof(attr->statp), &attr->LinkFI);
      BuildAttrOutputFnames(jcr, attr);

      if (FileIsIncluded(ff, attr->fname) && !FileIsExcluded(ff, attr->fname)) {
        if (!verbose) {
          PrintLsOutput(jcr, attr);
        } else {
          Pmsg1(-1, "%s\n", record_to_str(record_str, jcr, rec));
        }
        num_files++;
      }
      break;
    default:
      if (verbose) { Pmsg1(-1, "%s\n", record_to_str(record_str, jcr, rec)); }
      break;
  }

  /* debug output of record */
  DumpRecord("", rec);

  return true;
}

static void GetSessionRecord(Device* dev,
                             DeviceRecord* rec,
                             Session_Label* sessrec)
{
  const char* rtype;
  Session_Label empty_Session_Label;
  *sessrec = empty_Session_Label;
  jcr->JobId = 0;
  switch (rec->FileIndex) {
    case PRE_LABEL:
      rtype = _("Fresh Volume Label");
      break;
    case VOL_LABEL:
      rtype = _("Volume Label");
      UnserVolumeLabel(dev, rec);
      break;
    case SOS_LABEL:
      rtype = _("Begin Job Session");
      UnserSessionLabel(sessrec, rec);
      jcr->JobId = sessrec->JobId;
      break;
    case EOS_LABEL:
      rtype = _("End Job Session");
      break;
    case 0:
    case EOM_LABEL:
      rtype = _("End of Medium");
      break;
    case EOT_LABEL:
      rtype = _("End of Physical Medium");
      break;
    case SOB_LABEL:
      rtype = _("Start of object");
      break;
    case EOB_LABEL:
      rtype = _("End of object");
      break;
    default:
      rtype = _("Unknown");
      Dmsg1(10, "FI rtype=%d unknown\n", rec->FileIndex);
      break;
  }
  Dmsg5(10,
        "%s Record: VolSessionId=%d VolSessionTime=%d JobId=%d DataLen=%d\n",
        rtype, rec->VolSessionId, rec->VolSessionTime, rec->Stream,
        rec->data_len);
  if (verbose) {
    Pmsg5(
        -1,
        _("%s Record: VolSessionId=%d VolSessionTime=%d JobId=%d DataLen=%d\n"),
        rtype, rec->VolSessionId, rec->VolSessionTime, rec->Stream,
        rec->data_len);
  }
}
