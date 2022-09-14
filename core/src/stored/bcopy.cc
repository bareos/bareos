/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2002-2012 Free Software Foundation Europe e.V.
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
// Kern E. Sibbald, October 2002
/**
 * @file
 * Program to copy a Bareos from one volume to another.
 */

#include "include/bareos.h"
#include "stored/stored.h"
#include "stored/stored_globals.h"
#include "stored/device_control_record.h"
#include "lib/crypto_cache.h"
#include "stored/acquire.h"
#include "stored/butil.h"
#include "stored/jcr_private.h"
#include "stored/label.h"
#include "stored/mount.h"
#include "stored/read_record.h"
#include "lib/address_conf.h"
#include "lib/cli.h"
#include "lib/bsignal.h"
#include "lib/parse_bsr.h"
#include "lib/parse_conf.h"
#include "include/jcr.h"

namespace storagedaemon {
extern bool ParseSdConfig(const char* configfile, int exit_code);
}

using namespace storagedaemon;

/* Forward referenced functions */
static void GetSessionRecord(Device* dev,
                             DeviceRecord* rec,
                             Session_Label* sessrec);
static bool RecordCb(DeviceControlRecord* dcr, DeviceRecord* rec);


/* Global variables */
static Device* in_dev{};
static Device* out_dev{};
static JobControlRecord* in_jcr{};  /* input jcr */
static JobControlRecord* out_jcr{}; /* output jcr */
static BootStrapRecord* bsr{};
static bool list_records{};
static uint32_t records{};
static uint32_t jobs{};
static DeviceBlock* out_block{};
static Session_Label sessrec{};

int main(int argc, char* argv[])
{
  setlocale(LC_ALL, "");
  tzset();
  bindtextdomain("bareos", LOCALEDIR);
  textdomain("bareos");
  InitStackDump();

  MyNameIs(argc, argv, "bcopy");
  InitMsg(nullptr, nullptr);

  CLI::App bcopy_app;
  InitCLIApp(bcopy_app, "The Bareos Volume Copy tool.", 2002);

  bcopy_app
      .add_option(
          "-b,--parse-bootstrap",
          [](std::vector<std::string> vals) {
            bsr = libbareos::parse_bsr(nullptr, vals.front().data());
            return true;
          },
          "Specify a bootstrap file.")
      ->check(CLI::ExistingFile)
      ->type_name("<bootstrap>");

  std::string configfile;
  bcopy_app
      .add_option("-c,--config", configfile,
                  "Use <path> as configuration file or directory.")
      ->check(CLI::ExistingPath)
      ->type_name("<path>");

  std::string DirectorName{};
  bcopy_app
      .add_option("-D,--director", DirectorName,
                  "Specify a director name specified in the storage. "
                  "Configuration file for the Key Encryption Key selection.")
      ->type_name("<director>");

  AddDebugOptions(bcopy_app);

  std::string inputVolumes{};
  bcopy_app
      .add_option("-i,--input-volumes", inputVolumes,
                  "specify input Volume names (separated by |)")
      ->type_name("<vol1|vol2|...>");

  std::string outputVolumes{};
  bcopy_app
      .add_option("-o,--output-volumes", outputVolumes,
                  "specify output Volume names (separated by |)")
      ->type_name("<vol1|vol2|...>");

  bool ignore_label_errors = false;
  bcopy_app.add_flag(
      "-p,--ignore-errors",
      [&ignore_label_errors]([[maybe_unused]] bool val) {
        ignore_label_errors = true;
        forge_on = true;
      },
      "Proceed inspite of errors.");

  AddVerboseOption(bcopy_app);

  std::string work_dir = "/tmp";
  bcopy_app
      .add_option("-w,--working-directory", work_dir,
                  "specify working directory.")
      ->type_name("<directory>")
      ->capture_default_str();

  std::string input_archive;
  bcopy_app
      .add_option("input-archive", input_archive,
                  "Specify the input device name "
                  "(either as name of a Bareos Storage Daemon Device resource "
                  "or identical to the "
                  "Archive Device in a Bareos Storage Daemon Device resource).")
      ->required()
      ->type_name(" ");

  std::string output_archive;
  bcopy_app
      .add_option("output-archive", output_archive,
                  "Specify the output device name "
                  "(either as name of a Bareos Storage Daemon Device resource "
                  "or identical to the "
                  "Archive Device in a Bareos Storage Daemon Device resource).")
      ->required()
      ->type_name(" ");

  CLI11_PARSE(bcopy_app, argc, argv);

  OSDependentInit();

  working_directory = work_dir.c_str();

  my_config = InitSdConfig(configfile.c_str(), M_ERROR_TERM);
  ParseSdConfig(configfile.c_str(), M_ERROR_TERM);

  DirectorResource* director = nullptr;
  if (!DirectorName.empty()) {
    foreach_res (director, R_DIRECTOR) {
      if (bstrcmp(director->resource_name_, DirectorName.c_str())) { break; }
    }
    if (!director) {
      Emsg2(
          M_ERROR_TERM, 0,
          _("No Director resource named %s defined in %s. Cannot continue.\n"),
          DirectorName.c_str(), configfile.c_str());
    }
  }

  LoadSdPlugins(me->plugin_directory, me->plugin_names);

  ReadCryptoCache(me->working_directory, "bareos-sd",
                  GetFirstPortHostOrder(me->SDaddrs));

  // Setup and acquire input device for reading
  Dmsg0(100, "About to setup input jcr\n");

  DeviceControlRecord* in_dcr = new DeviceControlRecord;
  in_jcr = SetupJcr("bcopy", input_archive.data(), bsr, director, in_dcr,
                    inputVolumes, true); /* read device */
  if (!in_jcr) { exit(1); }

  in_jcr->impl->ignore_label_errors = ignore_label_errors;

  in_dev = in_jcr->impl->dcr->dev;
  if (!in_dev) { exit(1); }

  // Setup output device for writing
  Dmsg0(100, "About to setup output jcr\n");

  DeviceControlRecord* out_dcr = new DeviceControlRecord;
  out_jcr = SetupJcr("bcopy", output_archive.data(), bsr, director, out_dcr,
                     outputVolumes, false); /* write device */
  if (!out_jcr) { exit(1); }

  out_dev = out_jcr->impl->dcr->dev;
  if (!out_dev) { exit(1); }

  Dmsg0(100, "About to acquire device for writing\n");

  // For we must now acquire the device for writing
  out_dev->rLock(false);
  if (!out_dev->open(out_jcr->impl->dcr, DeviceMode::OPEN_READ_WRITE)) {
    Emsg1(M_FATAL, 0, _("dev open failed: %s\n"), out_dev->errmsg);
    out_dev->Unlock();
    exit(1);
  }
  out_dev->Unlock();
  if (!AcquireDeviceForAppend(out_jcr->impl->dcr)) {
    FreeJcr(in_jcr);
    exit(1);
  }
  out_block = out_jcr->impl->dcr->block;

  bool ok = ReadRecords(in_jcr->impl->dcr, RecordCb, MountNextReadVolume);

  if (ok || out_dev->CanWrite()) {
    if (!out_jcr->impl->dcr->WriteBlockToDevice()) {
      Pmsg0(000, _("Write of last block failed.\n"));
    }
  }

  Pmsg2(000, _("%u Jobs copied. %u records copied.\n"), jobs, records);


  FreeJcr(in_jcr);
  FreeJcr(out_jcr);

  delete in_dev;
  delete out_dev;

  return 0;
}


// ReadRecords() calls back here for each record it gets
static bool RecordCb(DeviceControlRecord* in_dcr, DeviceRecord* rec)
{
  if (list_records) {
    Pmsg5(000,
          _("Record: SessId=%u SessTim=%u FileIndex=%d Stream=%d len=%u\n"),
          rec->VolSessionId, rec->VolSessionTime, rec->FileIndex, rec->Stream,
          rec->data_len);
  }
  // Check for Start or End of Session Record
  if (rec->FileIndex < 0) {
    GetSessionRecord(in_dcr->dev, rec, &sessrec);

    if (verbose > 1) { DumpLabelRecord(in_dcr->dev, rec, true); }
    switch (rec->FileIndex) {
      case PRE_LABEL:
        Pmsg0(000, _("Volume is prelabeled. This volume cannot be copied.\n"));
        return false;
      case VOL_LABEL:
        Pmsg0(000, _("Volume label not copied.\n"));
        return true;
      case SOS_LABEL:
        if (bsr && rec->match_stat < 1) {
          /* Skipping record, because does not match BootStrapRecord filter */
          if (verbose) {
            Pmsg0(-1, _("Copy skipped. Record does not match BootStrapRecord "
                        "filter.\n"));
          }
        } else {
          jobs++;
        }
        break;
      case EOS_LABEL:
        if (bsr && rec->match_stat < 1) {
          /* Skipping record, because does not match BootStrapRecord filter */
          return true;
        }
        while (!WriteRecordToBlock(out_jcr->impl->dcr, rec)) {
          Dmsg2(150, "!WriteRecordToBlock data_len=%d rem=%d\n", rec->data_len,
                rec->remainder);
          if (!out_jcr->impl->dcr->WriteBlockToDevice()) {
            Dmsg2(90, "Got WriteBlockToDev error on device %s: ERR=%s\n",
                  out_dev->print_name(), out_dev->bstrerror());
            Jmsg(out_jcr, M_FATAL, 0, _("Cannot fixup device error. %s\n"),
                 out_dev->bstrerror());
            return false;
          }
        }
        if (!out_jcr->impl->dcr->WriteBlockToDevice()) {
          Dmsg2(90, "Got WriteBlockToDev error on device %s: ERR=%s\n",
                out_dev->print_name(), out_dev->bstrerror());
          Jmsg(out_jcr, M_FATAL, 0, _("Cannot fixup device error. %s\n"),
               out_dev->bstrerror());
          return false;
        }
        return true;
      case EOM_LABEL:
        Pmsg0(000, _("EOM label not copied.\n"));
        return true;
      case EOT_LABEL: /* end of all tapes */
        Pmsg0(000, _("EOT label not copied.\n"));
        return true;
      default:
        return true;
    }
  }

  /*  Write record */
  if (bsr && rec->match_stat < 1) {
    /* Skipping record, because does not match BootStrapRecord filter */
    return true;
  }
  records++;
  while (!WriteRecordToBlock(out_jcr->impl->dcr, rec)) {
    Dmsg2(150, "!WriteRecordToBlock data_len=%d rem=%d\n", rec->data_len,
          rec->remainder);
    if (!out_jcr->impl->dcr->WriteBlockToDevice()) {
      Dmsg2(90, "Got WriteBlockToDev error on device %s: ERR=%s\n",
            out_dev->print_name(), out_dev->bstrerror());
      Jmsg(out_jcr, M_FATAL, 0, _("Cannot fixup device error. %s\n"),
           out_dev->bstrerror());
      return false;
    }
  }
  return true;
}

static void GetSessionRecord(Device* dev,
                             DeviceRecord* rec,
                             Session_Label* sessrec)
{
  const char* rtype;
  *sessrec = Session_Label{};
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
      break;
    case EOS_LABEL:
      rtype = _("End Job Session");
      UnserSessionLabel(sessrec, rec);
      break;
    case 0:
    case EOM_LABEL:
      rtype = _("End of Medium");
      break;
    default:
      rtype = _("Unknown");
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
