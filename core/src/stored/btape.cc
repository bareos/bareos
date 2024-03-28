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
// Kern Sibbald, April MM
/**
 * @file
 * Bareos Tape manipulation program
 *
 * Has various tape manipulation commands -- mostly for
 * use in determining how tapes really work.
 *
 * Note, this program reads stored.conf, and will only
 * talk to devices that are configured.
 */

#if !defined(_MSC_VER)
#include <unistd.h>
#endif

#include "include/fcntl_def.h"
#include "include/bareos.h"
#include "include/exit_codes.h"
#include "include/streams.h"
#include "stored/stored.h"
#include "stored/stored_globals.h"
#include "lib/crypto_cache.h"
#include "stored/acquire.h"
#include "stored/autochanger.h"
#include "stored/bsr.h"
#include "stored/btape_device_control_record.h"
#include "stored/butil.h"
#include "stored/device.h"
#include "stored/stored_jcr_impl.h"
#include "stored/label.h"
#include "stored/read_record.h"
#include "stored/sd_backends.h"
#include "lib/address_conf.h"
#include "lib/berrno.h"
#include "lib/cli.h"
#include "lib/edit.h"
#include "lib/bsignal.h"
#include "lib/recent_job_results_list.h"
#include "lib/parse_bsr.h"
#include "lib/parse_conf.h"
#include "lib/util.h"
#include "lib/watchdog.h"
#include "include/jcr.h"
#include "lib/serial.h"

inline void read_with_check(int fd, void* buf, size_t count)
{
  if (read(fd, buf, count) < 0) {
    BErrNo be;
    Emsg1(M_FATAL, 0, T_("read failed: %s\n"), be.bstrerror());
  }
}

inline void write_with_check(int fd, void* buf, size_t count)
{
  if (write(fd, buf, count) < 0) {
    BErrNo be;
    Emsg1(M_FATAL, 0, T_("write failed: %s\n"), be.bstrerror());
  }
}

namespace storagedaemon {
extern bool ParseSdConfig(const char* configfile, int exit_code);
}

using namespace storagedaemon;

static int quit = 0;
static char buf[100'000];

/**
 * If you change the format of the state file,
 *  increment this value
 */
static uint32_t btape_state_level = 2;

static Device* g_dev = nullptr;
static DeviceControlRecord* g_dcr;
static int exit_code = 0;

#define REC_SIZE 32768

/* Forward referenced subroutines */
static void do_tape_cmds();
static void HelpCmd();
static void scancmd();
static void rewindcmd();
static void clearcmd();
static void wrcmd();
static void rrcmd();
static void rbcmd();
static void eodcmd();
static void fillcmd();
static void qfillcmd();
static void statcmd();
static void unfillcmd();
static int FlushBlock(DeviceBlock* block);
static bool QuickieCb(DeviceControlRecord* dcr, DeviceRecord* rec);
static bool CompareBlocks(DeviceBlock* last_block, DeviceBlock* block);
static bool MyMountNextReadVolume(DeviceControlRecord* dcr);
static void scan_blocks();
static void SetVolumeName(const char* VolName, int volnum);
static void rawfill_cmd();
static bool open_the_device();
static void autochangercmd();
static bool do_unfill();

/* Static variables */

#define MAX_CMD_ARGS 30

static POOLMEM* cmd;
static POOLMEM* args;
static char* argk[MAX_CMD_ARGS];
static char* argv[MAX_CMD_ARGS];
static int argc;

static int quickie_count = 0;
static uint64_t write_count = 0;
static BootStrapRecord* bsr = nullptr;
static bool signals = true;
static bool g_ok;
static int stop = 0;
static uint64_t vol_size;
static uint64_t VolBytes;
static time_t now;
static int32_t file_index;
static int end_of_tape = 0;
static uint32_t LastBlock = 0;
static uint32_t eot_block;
static uint32_t eot_block_len;
static uint32_t eot_FileIndex;
static int dumped = 0;
static DeviceBlock* last_block1 = nullptr;
static DeviceBlock* last_block2 = nullptr;
static DeviceBlock* last_block = nullptr;
static DeviceBlock* this_block = nullptr;
static DeviceBlock* first_block = nullptr;
static uint32_t last_file1 = 0;
static uint32_t last_file2 = 0;
static uint32_t last_file = 0;
static uint32_t last_block_num1 = 0;
static uint32_t last_block_num2 = 0;
static uint32_t last_block_num = 0;
static uint32_t BlockNumber = 0;
static bool simple = true;

static const char* volumename = nullptr;
static int vol_num = 0;

static JobControlRecord* g_jcr = nullptr;

static std::string Generate_interactive_commands_help();
static void TerminateBtape(int sig);
int GetCmd(const char* prompt);


/**
 *
 * Bareos tape testing program
 *
 */
int main(int margc, char* margv[])
{
  setlocale(LC_ALL, "");
  tzset();
  bindtextdomain("bareos", LOCALEDIR);
  textdomain("bareos");
  InitStackDump();

  /* Sanity checks */
  if (TAPE_BSIZE % B_DEV_BSIZE != 0 || TAPE_BSIZE / B_DEV_BSIZE == 0) {
    Emsg2(M_ABORT, 0,
          T_("Tape block size (%d) not multiple of system size (%d)\n"),
          TAPE_BSIZE, B_DEV_BSIZE);
  }
  if (TAPE_BSIZE != (1 << (ffs(TAPE_BSIZE) - 1))) {
    Emsg1(M_ABORT, 0, T_("Tape block size (%d) is not a power of 2\n"),
          TAPE_BSIZE);
  }
  if (sizeof(boffset_t) < 8) {
    Pmsg1(-1,
          T_("\n\n!!!! Warning large disk addressing disabled. boffset_t=%d "
             "should be 8 or more !!!!!\n\n\n"),
          sizeof(boffset_t));
  }

  uint32_t x32 = 123456789;
  uint32_t y32;
  char buf2[1'000];
  Bsnprintf(buf2, sizeof(buf2), "%u", x32);
  int i = bsscanf(buf2, "%lu", &y32);
  if (i != 1 || x32 != y32) {
    Pmsg3(-1, T_("32 bit printf/scanf problem. i=%d x32=%u y32=%u\n"), i, x32,
          y32);
    exit(BEXIT_FAILURE);
  }

  uint64_t x64 = 123456789;
  x64 = x64 << 32;
  x64 += 123456789;
  Bsnprintf(buf2, sizeof(buf2), "%llu", x64);

  uint64_t y64;
  i = bsscanf(buf2, "%llu", &y64);
  if (i != 1 || x64 != y64) {
    Pmsg3(-1, T_("64 bit printf/scanf problem. i=%d x64=%llu y64=%llu\n"), i,
          x64, y64);
    exit(BEXIT_FAILURE);
  }

  working_directory = "/tmp";
  MyNameIs(margc, margv, "btape");
  InitMsg(nullptr, nullptr);

  OSDependentInit();
 
  (void)WSA_Init(); /* Initialize Windows sockets */

  CLI::App btape_app;
  InitCLIApp(btape_app, "The Bareos Tape Manipulation tool.", 2000);

  btape_app
      .add_option(
          "-b,--parse-bootstrap",
          [](std::vector<std::string> vals) {
            bsr = libbareos::parse_bsr(nullptr, vals.front().data());
            return true;
          },
          "Specify a bootstrap file.")
      ->check(CLI::ExistingFile)
      ->type_name("<file>");

  btape_app
      .add_option(
          "-c,--config",
          [](std::vector<std::string> val) {
            if (configfile != nullptr) { free(configfile); }
            configfile = strdup(val.front().c_str());
            return true;
          },
          "Specify a configuration file or directory.")
      ->check(CLI::ExistingPath)
      ->type_name("<path>");

  std::string DirectorName;
  btape_app
      .add_option("-D,--director", DirectorName,
                  "Specify a director name specified in the storage.\n"
                  "Configuration file for the Key Encryption Key selection.")
      ->type_name("<director>");

  AddDebugOptions(btape_app);

  btape_app.add_flag("-p,--proceed-io", forge_on,
                     "Proceed inspite of IO errors");

  btape_app.add_flag("-s{false},--no-signals{false}", signals,
                     "Turn off signals.");

  AddVerboseOption(btape_app);

  std::string archive_name;
  btape_app
      .add_option("bareos-archive-device-name", archive_name,
                  "Specify the input device name (either as name of a Bareos "
                  "Storage Daemon Device resource or identical to the Archive "
                  "Device in a Bareos Storage Daemon Device resource).")
      ->required()
      ->type_name(" ");

  btape_app.add_option_group("Interactive commands",
                             Generate_interactive_commands_help());

  ParseBareosApp(btape_app, margc, margv);

  printf(T_("Tape block granularity is %d bytes.\n"), TAPE_BSIZE);

  cmd = GetPoolMemory(PM_FNAME);
  args = GetPoolMemory(PM_FNAME);

  if (signals) { InitSignals(TerminateBtape); }

  daemon_start_time = time(nullptr);

  my_config = InitSdConfig(configfile, M_CONFIG_ERROR);
  ParseSdConfig(configfile, M_CONFIG_ERROR);

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

  g_dcr = new BTAPE_DCR;
  g_jcr = SetupJcr("btape", archive_name.data(), bsr, director, g_dcr, "",
                   false); /* write device */
  if (!g_jcr) { exit(BEXIT_FAILURE); }

  g_dev = g_jcr->sd_impl->dcr->dev;
  if (!g_dev) { exit(BEXIT_FAILURE); }

  if (!g_dev->IsTape()) {
    Pmsg0(000, T_("btape only works with tape storage.\n"));
    exit(BEXIT_FAILURE);
  }

  // Let SD plugins setup the record translation
  if (GeneratePluginEvent(g_jcr, bSdEventSetupRecordTranslation, g_dcr)
      != bRC_OK) {
    Jmsg(g_jcr, M_FATAL, 0,
         T_("bSdEventSetupRecordTranslation call failed!\n"));
  }

  if (!open_the_device()) { exit(BEXIT_FAILURE); }

  Dmsg0(200, "Do tape commands\n");
  do_tape_cmds();

  TerminateBtape(exit_code);
}

static void TerminateBtape(int status)
{
  FreeJcr(g_jcr);
  g_jcr = nullptr;

  if (args) {
    FreePoolMemory(args);
    args = nullptr;
  }

  if (cmd) {
    FreePoolMemory(cmd);
    cmd = nullptr;
  }

  if (bsr) { libbareos::FreeBsr(bsr); }

  FreeVolumeLists();

  if (g_dev) { delete g_dev; }

  if (configfile) { free(configfile); }

  if (my_config) {
    delete my_config;
    my_config = nullptr;
  }

  if (this_block) {
    FreeBlock(this_block);
    this_block = nullptr;
  }

  StopWatchdog();
  TermMsg();
  RecentJobResultsList::Cleanup();
  CleanupJcrChain();

  exit(status);
}


btime_t total_time = 0;
uint64_t total_size = 0;

static void init_total_speed()
{
  total_size = 0;
  total_time = 0;
}

static void print_total_speed()
{
  char ec1[50], ec2[50];
  uint64_t rate = total_size / total_time;
  Pmsg2(000, T_("Total Volume bytes=%sB. Total Write rate = %sB/s\n"),
        edit_uint64_with_suffix(total_size, ec1),
        edit_uint64_with_suffix(rate, ec2));
}

static void init_speed()
{
  time(&g_jcr->run_time); /* start counting time for rates */
  g_jcr->JobBytes = 0;
}

static void PrintSpeed(uint64_t bytes)
{
  char ec1[50], ec2[50];
  uint64_t rate;

  now = time(nullptr);
  now -= g_jcr->run_time;
  if (now <= 0) { now = 1; /* don't divide by zero */ }

  total_time += now;
  total_size += bytes;

  rate = bytes / now;
  Pmsg2(000, T_("Volume bytes=%sB. Write rate = %sB/s\n"),
        edit_uint64_with_suffix(bytes, ec1),
        edit_uint64_with_suffix(rate, ec2));
}

// Helper that fill a buffer with random data or not
typedef enum
{
  FILL_RANDOM,
  FILL_ZERO
} fill_mode_t;

static void FillBuffer(fill_mode_t mode, char* t_buf, uint32_t len)
{
  int fd;
  switch (mode) {
    case FILL_RANDOM:
      fd = open("/dev/urandom", O_RDONLY);
      if (fd != -1) {
        read_with_check(fd, t_buf, len);
        close(fd);
      } else {
        uint32_t* p = (uint32_t*)t_buf;
        srandom(time(nullptr));
        for (uint32_t i = 0; i < len / sizeof(uint32_t); i++) {
          p[i] = random();
        }
      }
      break;

    case FILL_ZERO:
      memset(t_buf, 0xFF, len);
      break;

    default:
      ASSERT(0);
  }
}

static void MixBuffer(fill_mode_t mode, char* data, uint32_t len)
{
  uint32_t i;
  uint32_t* lp = (uint32_t*)data;

  if (mode == FILL_ZERO) { return; }

  lp[0] += lp[13];
  for (i = 1; i < (len - sizeof(uint32_t)) / sizeof(uint32_t) - 1; i += 100) {
    lp[i] += lp[0];
  }
}

static bool open_the_device()
{
  DeviceBlock* block;
  bool ok = true;

  block = new_block(g_dev);
  g_dev->rLock();
  Dmsg1(200, "Opening device %s\n", g_dcr->VolumeName);
  if (!g_dev->open(g_dcr, DeviceMode::OPEN_READ_WRITE)) {
    Emsg1(M_FATAL, 0, T_("dev open failed: %s\n"), g_dev->errmsg);
    ok = false;
    goto bail_out;
  }
  Pmsg1(000, T_("open device %s: OK\n"), g_dev->print_name());
  g_dev->SetAppend(); /* put volume in append mode */

bail_out:
  g_dev->Unlock();
  FreeBlock(block);
  return ok;
}


void QuitCmd() { quit = 1; }

// Write a label to the tape
static void labelcmd()
{
  if (volumename) {
    PmStrcpy(cmd, volumename);
  } else {
    if (!GetCmd(T_("Enter Volume Name: "))) { return; }
  }

  if (!g_dev->IsOpen()) {
    if (!FirstOpenDevice(g_dcr)) {
      Pmsg1(0, T_("Device open failed. ERR=%s\n"), g_dev->bstrerror());
    }
  }
  g_dev->rewind(g_dcr);
  WriteNewVolumeLabelToDev(g_dcr, cmd, "Default", false /*no relabel*/);
  Pmsg1(-1, T_("Wrote Volume label for volume \"%s\".\n"), cmd);
}

// Read the tape label
static void readlabelcmd()
{
  int save_debug_level = debug_level;
  int status;

  status = ReadDevVolumeLabel(g_dcr);
  switch (status) {
    case VOL_NO_LABEL:
      Pmsg0(0, T_("Volume has no label.\n"));
      break;
    case VOL_OK:
      Pmsg0(0, T_("Volume label read correctly.\n"));
      debug_level = 20;
      DumpVolumeLabel(g_dev);
      break;
    case VOL_IO_ERROR:
      Pmsg1(0, T_("I/O error on device: ERR=%s"), g_dev->bstrerror());
      break;
    case VOL_NAME_ERROR:
      Pmsg0(0, T_("Volume name error\n"));
      break;
    case VOL_CREATE_ERROR:
      Pmsg1(0, T_("Error creating label. ERR=%s"), g_dev->bstrerror());
      break;
    case VOL_VERSION_ERROR:
      Pmsg0(0, T_("Volume version error.\n"));
      break;
    case VOL_LABEL_ERROR:
      Pmsg0(0, T_("Bad Volume label type.\n"));
      break;
    default:
      Pmsg0(0, T_("Unknown error.\n"));
      break;
  }
  debug_level = save_debug_level;
}


/**
 * Load the tape should have prevously been taken
 * off line, otherwise this command is not necessary.
 */
static void loadcmd()
{
  if (!g_dev->LoadDev()) {
    Pmsg1(0, T_("Bad status from load. ERR=%s\n"), g_dev->bstrerror());
  } else
    Pmsg1(0, T_("Loaded %s\n"), g_dev->print_name());
}

// Rewind the tape.
static void rewindcmd()
{
  if (!g_dev->rewind(g_dcr)) {
    Pmsg1(0, T_("Bad status from rewind. ERR=%s\n"), g_dev->bstrerror());
    g_dev->clrerror(-1);
  } else {
    Pmsg1(0, T_("Rewound %s\n"), g_dev->print_name());
  }
}

// Clear any tape error
static void clearcmd() { g_dev->clrerror(-1); }

// Write and end of file on the tape
static void weofcmd()
{
  int num = 1;
  if (argc > 1) { num = atoi(argk[1]); }
  if (num <= 0) { num = 1; }

  if (!g_dev->weof(num)) {
    Pmsg1(0, T_("Bad status from weof. ERR=%s\n"), g_dev->bstrerror());
    return;
  } else {
    if (num == 1) {
      Pmsg1(0, T_("Wrote 1 EOF to %s\n"), g_dev->print_name());
    } else {
      Pmsg2(0, T_("Wrote %d EOFs to %s\n"), num, g_dev->print_name());
    }
  }
}


/* Go to the end of the medium -- raw command
 * The idea was orginally that the end of the Bareos
 * medium would be flagged differently. This is not
 * currently the case. So, this is identical to the
 * eodcmd().
 */
static void eomcmd()
{
  if (!g_dev->eod(g_dcr)) {
    Pmsg1(0, "%s", g_dev->bstrerror());
    return;
  } else {
    Pmsg0(0, T_("Moved to end of medium.\n"));
  }
}

/**
 * Go to the end of the medium (either hardware determined
 *  or defined by two eofs.
 */
static void eodcmd() { eomcmd(); }

// Backspace file
static void bsfcmd()
{
  int num = 1;
  if (argc > 1) { num = atoi(argk[1]); }
  if (num <= 0) { num = 1; }

  if (!g_dev->bsf(num)) {
    Pmsg1(0, T_("Bad status from bsf. ERR=%s\n"), g_dev->bstrerror());
  } else {
    Pmsg2(0, T_("Backspaced %d file%s.\n"), num, num == 1 ? "" : "s");
  }
}

// Backspace record
static void bsrcmd()
{
  int num = 1;
  if (argc > 1) { num = atoi(argk[1]); }
  if (num <= 0) { num = 1; }
  if (!g_dev->bsr(num)) {
    Pmsg1(0, T_("Bad status from bsr. ERR=%s\n"), g_dev->bstrerror());
  } else {
    Pmsg2(0, T_("Backspaced %d record%s.\n"), num, num == 1 ? "" : "s");
  }
}

// List device capabilities as defined in the stored.conf file.
static void capcmd()
{
  printf(T_("Configured device capabilities:\n"));
  printf("%sEOF ", g_dev->HasCap(CAP_EOF) ? "" : "!");
  printf("%sBSR ", g_dev->HasCap(CAP_BSR) ? "" : "!");
  printf("%sBSF ", g_dev->HasCap(CAP_BSF) ? "" : "!");
  printf("%sFSR ", g_dev->HasCap(CAP_FSR) ? "" : "!");
  printf("%sFSF ", g_dev->HasCap(CAP_FSF) ? "" : "!");
  printf("%sFASTFSF ", g_dev->HasCap(CAP_FASTFSF) ? "" : "!");
  printf("%sBSFATEOM ", g_dev->HasCap(CAP_BSFATEOM) ? "" : "!");
  printf("%sEOM ", g_dev->HasCap(CAP_EOM) ? "" : "!");
  printf("%sREM ", g_dev->HasCap(CAP_REM) ? "" : "!");
  printf("%sRACCESS ", g_dev->HasCap(CAP_RACCESS) ? "" : "!");
  printf("%sAUTOMOUNT ", g_dev->HasCap(CAP_AUTOMOUNT) ? "" : "!");
  printf("%sLABEL ", g_dev->HasCap(CAP_LABEL) ? "" : "!");
  printf("%sANONVOLS ", g_dev->HasCap(CAP_ANONVOLS) ? "" : "!");
  printf("%sALWAYSOPEN ", g_dev->HasCap(CAP_ALWAYSOPEN) ? "" : "!");
  printf("%sMTIOCGET ", g_dev->HasCap(CAP_MTIOCGET) ? "" : "!");
  printf("\n");

  printf(T_("Device status:\n"));
  printf("%sOPENED ", g_dev->IsOpen() ? "" : "!");
  printf("%sTAPE ", g_dev->IsTape() ? "" : "!");
  printf("%sLABEL ", g_dev->IsLabeled() ? "" : "!");
  printf("%sMALLOC ", BitIsSet(ST_ALLOCATED, g_dev->state) ? "" : "!");
  printf("%sAPPEND ", g_dev->CanAppend() ? "" : "!");
  printf("%sREAD ", g_dev->CanRead() ? "" : "!");
  printf("%sEOT ", g_dev->AtEot() ? "" : "!");
  printf("%sWEOT ", BitIsSet(ST_WEOT, g_dev->state) ? "" : "!");
  printf("%sEOF ", g_dev->AtEof() ? "" : "!");
  printf("%sNEXTVOL ", BitIsSet(ST_NEXTVOL, g_dev->state) ? "" : "!");
  printf("%sSHORT ", BitIsSet(ST_SHORT, g_dev->state) ? "" : "!");
  printf("\n");

  printf(T_("Device parameters:\n"));
  printf("Device name: %s\n", g_dev->archive_device_string);
  printf("File=%u block=%u\n", g_dev->file, g_dev->block_num);
  printf("Min block=%u Max block=%u\n", g_dev->min_block_size,
         g_dev->max_block_size);

  printf(T_("Status:\n"));
  statcmd();
}

/**
 * Test writing larger and larger records.
 * This is a torture test for records.
 */
static void rectestcmd()
{
  DeviceBlock* save_block;
  DeviceRecord* rec;
  int i, blkno = 0;

  Pmsg0(0,
        T_("Test writing larger and larger records.\n"
           "This is a torture test for records.\nI am going to write\n"
           "larger and larger records. It will stop when the record size\n"
           "plus the header exceeds the block size (by default about 64K)\n"));


  GetCmd(T_("Do you want to continue? (y/n): "));
  if (cmd[0] != 'y') {
    Pmsg0(000, T_("Command aborted.\n"));
    return;
  }

  save_block = g_dcr->block;
  g_dcr->block = new_block(g_dev);
  rec = new_record();

  for (i = 1; i < 500000; i++) {
    rec->data = CheckPoolMemorySize(rec->data, i);
    memset(rec->data, i & 0xFF, i);
    rec->data_len = i;
    if (WriteRecordToBlock(g_dcr, rec)) {
      EmptyBlock(g_dcr->block);
      blkno++;
      Pmsg2(0, T_("Block %d i=%d\n"), blkno, i);
    } else {
      break;
    }
  }
  FreeRecord(rec);
  FreeBlock(g_dcr->block);
  g_dcr->block = save_block; /* restore block to dcr */
}

/**
 * This test attempts to re-read a block written by Bareos
 *   normally at the end of the tape. Bareos will then back up
 *   over the two eof marks, backup over the record and reread
 *   it to make sure it is valid.  Bareos can skip this validation
 *   if you set "Backward space record = no"
 */
static bool re_read_block_test()
{
  DeviceBlock* block = g_dcr->block;
  DeviceRecord* rec;
  bool rc = false;
  int len;

  if (!g_dev->HasCap(CAP_BSR)) {
    Pmsg0(-1, T_("Skipping read backwards test because BootStrapRecord turned "
                 "off.\n"));
    return true;
  }

  Pmsg0(-1, T_("\n=== Write, backup, and re-read test ===\n\n"
               "I'm going to write three records and an EOF\n"
               "then backup over the EOF and re-read the last record.\n"
               "Bareos does this after writing the last block on the\n"
               "tape to verify that the block was written correctly.\n\n"
               "This is not an *essential* feature ...\n\n"));
  rewindcmd();
  EmptyBlock(block);
  rec = new_record();
  rec->data = CheckPoolMemorySize(rec->data, block->buf_len);
  len = rec->data_len = block->buf_len - 100;
  memset(rec->data, 1, rec->data_len);
  if (!WriteRecordToBlock(g_dcr, rec)) {
    Pmsg0(0, T_("Error writing record to block.\n"));
    goto bail_out;
  }
  if (!g_dcr->WriteBlockToDev()) {
    Pmsg0(0, T_("Error writing block to device.\n"));
    goto bail_out;
  } else {
    Pmsg1(0, T_("Wrote first record of %d bytes.\n"), rec->data_len);
  }
  memset(rec->data, 2, rec->data_len);
  if (!WriteRecordToBlock(g_dcr, rec)) {
    Pmsg0(0, T_("Error writing record to block.\n"));
    goto bail_out;
  }
  if (!g_dcr->WriteBlockToDev()) {
    Pmsg0(0, T_("Error writing block to device.\n"));
    goto bail_out;
  } else {
    Pmsg1(0, T_("Wrote second record of %d bytes.\n"), rec->data_len);
  }
  memset(rec->data, 3, rec->data_len);
  if (!WriteRecordToBlock(g_dcr, rec)) {
    Pmsg0(0, T_("Error writing record to block.\n"));
    goto bail_out;
  }
  if (!g_dcr->WriteBlockToDev()) {
    Pmsg0(0, T_("Error writing block to device.\n"));
    goto bail_out;
  } else {
    Pmsg1(0, T_("Wrote third record of %d bytes.\n"), rec->data_len);
  }
  weofcmd();
  if (g_dev->HasCap(CAP_TWOEOF)) { weofcmd(); }
  if (!g_dev->bsf(1)) {
    Pmsg1(0, T_("Backspace file failed! ERR=%s\n"), g_dev->bstrerror());
    goto bail_out;
  }
  if (g_dev->HasCap(CAP_TWOEOF)) {
    if (!g_dev->bsf(1)) {
      Pmsg1(0, T_("Backspace file failed! ERR=%s\n"), g_dev->bstrerror());
      goto bail_out;
    }
  }
  Pmsg0(0, T_("Backspaced over EOF OK.\n"));
  if (!g_dev->bsr(1)) {
    Pmsg1(0, T_("Backspace record failed! ERR=%s\n"), g_dev->bstrerror());
    goto bail_out;
  }
  Pmsg0(0, T_("Backspace record OK.\n"));
  if (DeviceControlRecord::ReadStatus::Ok
      != g_dcr->ReadBlockFromDev(NO_BLOCK_NUMBER_CHECK)) {
    BErrNo be;
    Pmsg1(0, T_("Read block failed! ERR=%s\n"), be.bstrerror(g_dev->dev_errno));
    goto bail_out;
  }
  memset(rec->data, 0, rec->data_len);
  if (!ReadRecordFromBlock(g_dcr, rec)) {
    BErrNo be;
    Pmsg1(0, T_("Read block failed! ERR=%s\n"), be.bstrerror(g_dev->dev_errno));
    goto bail_out;
  }
  for (int i = 0; i < len; i++) {
    if (rec->data[i] != 3) {
      Pmsg0(0, T_("Bad data in record. Test failed!\n"));
      goto bail_out;
    }
  }
  Pmsg0(0, T_("\nBlock re-read correct. Test succeeded!\n"));
  Pmsg0(-1, T_("=== End Write, backup, and re-read test ===\n\n"));

  rc = true;

bail_out:
  FreeRecord(rec);
  if (!rc) {
    Pmsg0(0, T_("This is not terribly serious since Bareos only uses\n"
                "this function to verify the last block written to the\n"
                "tape. Bareos will skip the last block verification\n"
                "if you add:\n\n"
                "Backward Space Record = No\n\n"
                "to your Storage daemon's Device resource definition.\n"));
  }
  return rc;
}

static bool SpeedTestRaw(fill_mode_t mode, uint64_t nb_gb, uint32_t nb)
{
  DeviceBlock* block = g_dcr->block;
  int status;
  uint32_t block_num = 0;
  int my_errno;
  char ed1[200];
  nb_gb *= 1024 * 1024 * 1024; /* convert size from nb to GB */

  init_total_speed();
  FillBuffer(mode, block->buf, block->buf_len);

  Pmsg3(0, T_("Begin writing %i files of %sB with raw blocks of %u bytes.\n"),
        nb, edit_uint64_with_suffix(nb_gb, ed1), block->buf_len);

  for (uint32_t j = 0; j < nb; j++) {
    init_speed();
    for (; g_jcr->JobBytes < nb_gb;) {
      status = g_dev->d_write(g_dev->fd, block->buf, block->buf_len);
      if (status == (int)block->buf_len) {
        if ((block_num++ % 500) == 0) {
          printf("+");
          fflush(stdout);
        }

        MixBuffer(mode, block->buf, block->buf_len);

        g_jcr->JobBytes += status;

      } else {
        my_errno = errno;
        printf("\n");
        BErrNo be;
        printf(T_("Write failed at block %u. status=%d ERR=%s\n"), block_num,
               status, be.bstrerror(my_errno));
        return false;
      }
    }
    printf("\n");
    weofcmd();
    PrintSpeed(g_jcr->JobBytes);
  }
  print_total_speed();
  printf("\n");
  return true;
}


static bool SpeedTestBareos(fill_mode_t mode, uint64_t nb_gb, uint32_t nb)
{
  DeviceBlock* block = g_dcr->block;
  char ed1[200];
  DeviceRecord* rec;
  uint64_t last_bytes = g_dev->VolCatInfo.VolCatBytes;
  uint64_t written = 0;

  nb_gb *= 1024 * 1024 * 1024; /* convert size from nb to GB */

  init_total_speed();

  EmptyBlock(block);
  rec = new_record();
  rec->data = CheckPoolMemorySize(rec->data, block->buf_len);
  rec->data_len = block->buf_len - 100;

  FillBuffer(mode, rec->data, rec->data_len);

  Pmsg3(0, T_("Begin writing %i files of %sB with blocks of %u bytes.\n"), nb,
        edit_uint64_with_suffix(nb_gb, ed1), block->buf_len);

  for (uint32_t j = 0; j < nb; j++) {
    written = 0;
    init_speed();
    for (; written < nb_gb;) {
      if (!WriteRecordToBlock(g_dcr, rec)) {
        Pmsg0(0, T_("\nError writing record to block.\n"));
        goto bail_out;
      }
      if (!g_dcr->WriteBlockToDev()) {
        Pmsg0(0, T_("\nError writing block to device.\n"));
        goto bail_out;
      }

      if ((block->BlockNumber % 500) == 0) {
        printf("+");
        fflush(stdout);
      }
      written += g_dev->VolCatInfo.VolCatBytes - last_bytes;
      last_bytes = g_dev->VolCatInfo.VolCatBytes;
      MixBuffer(mode, rec->data, rec->data_len);
    }
    printf("\n");
    weofcmd();
    PrintSpeed(written);
  }
  print_total_speed();
  printf("\n");
  FreeRecord(rec);
  return true;

bail_out:
  FreeRecord(rec);
  return false;
}

/* TODO: use UaContext */
static int BtapeFindArg(const char* keyword)
{
  for (int i = 1; i < argc; i++) {
    if (Bstrcasecmp(keyword, argk[i])) { return i; }
  }
  return -1;
}

#define ok(a) \
  if (!(a)) return

/**
 * For file (/dev/zero, /dev/urandom, normal?)
 *    use raw mode to write a suite of 3 files of 1, 2, 4, 8 GB
 *    use qfill mode to write the same
 *
 */
static void speed_test()
{
  bool do_zero = true, do_random = true, do_block = true, do_raw = true;
  uint32_t file_size = 0, nb_file = 3;
  int32_t i;

  i = BtapeFindArg("file_size");
  if (i > 0) {
    file_size = atoi(argv[i]);
    if (file_size > 100) {
      Pmsg0(0, T_("The file_size is too big, stop this test with Ctrl-c.\n"));
    }
  }

  i = BtapeFindArg("nb_file");
  if (i > 0) { nb_file = atoi(argv[i]); }

  if (BtapeFindArg("skip_zero") > 0) { do_zero = false; }

  if (BtapeFindArg("skip_random") > 0) { do_random = false; }

  if (BtapeFindArg("skip_raw") > 0) { do_raw = false; }

  if (BtapeFindArg("skip_block") > 0) { do_block = false; }

  if (do_raw) {
    g_dev->rewind(g_dcr);
    if (do_zero) {
      Pmsg0(0, T_("Test with zero data, should give the "
                  "maximum throughput.\n"));
      if (file_size) {
        ok(SpeedTestRaw(FILL_ZERO, file_size, nb_file));
      } else {
        ok(SpeedTestRaw(FILL_ZERO, 1, nb_file));
        ok(SpeedTestRaw(FILL_ZERO, 2, nb_file));
        ok(SpeedTestRaw(FILL_ZERO, 4, nb_file));
      }
    }

    if (do_random) {
      Pmsg0(0, T_("Test with random data, should give the minimum "
                  "throughput.\n"));
      if (file_size) {
        ok(SpeedTestRaw(FILL_RANDOM, file_size, nb_file));
      } else {
        ok(SpeedTestRaw(FILL_RANDOM, 1, nb_file));
        ok(SpeedTestRaw(FILL_RANDOM, 2, nb_file));
        ok(SpeedTestRaw(FILL_RANDOM, 4, nb_file));
      }
    }
  }

  if (do_block) {
    g_dev->rewind(g_dcr);
    if (do_zero) {
      Pmsg0(0, T_("Test with zero data and bareos block structure.\n"));
      if (file_size) {
        ok(SpeedTestBareos(FILL_ZERO, file_size, nb_file));
      } else {
        ok(SpeedTestBareos(FILL_ZERO, 1, nb_file));
        ok(SpeedTestBareos(FILL_ZERO, 2, nb_file));
        ok(SpeedTestBareos(FILL_ZERO, 4, nb_file));
      }
    }

    if (do_random) {
      Pmsg0(0, T_("Test with random data, should give the minimum "
                  "throughput.\n"));
      if (file_size) {
        ok(SpeedTestBareos(FILL_RANDOM, file_size, nb_file));
      } else {
        ok(SpeedTestBareos(FILL_RANDOM, 1, nb_file));
        ok(SpeedTestBareos(FILL_RANDOM, 2, nb_file));
        ok(SpeedTestBareos(FILL_RANDOM, 4, nb_file));
      }
    }
  }
}

const uint64_t num_recs = 10'000LL;

static bool write_two_files()
{
  DeviceBlock* block;
  DeviceRecord* rec;
  uint32_t len;
  uint32_t* p;
  bool rc = false; /* bad return code */
  Device* dev = g_dcr->dev;

  /* Set big max_file_size so that write_record_to_block
   * doesn't insert any additional EOF marks */
  if (dev->max_block_size) {
    dev->max_file_size = 2LL * num_recs * (uint64_t)dev->max_block_size;
  } else {
    dev->max_file_size = 2LL * num_recs * (uint64_t)DEFAULT_BLOCK_SIZE;
  }
  Dmsg1(100, "max_file_size was set to %lld\n", dev->max_file_size);

  Pmsg2(-1,
        T_("\n=== Write, rewind, and re-read test ===\n\n"
           "I'm going to write %d records and an EOF\n"
           "then write %d records and an EOF, then rewind,\n"
           "and re-read the data to verify that it is correct.\n\n"
           "This is an *essential* feature ...\n\n"),
        num_recs, num_recs);

  block = g_dcr->block;
  EmptyBlock(block);
  rec = new_record();
  rec->data = CheckPoolMemorySize(rec->data, block->buf_len);
  rec->data_len = block->buf_len - 100;
  len = rec->data_len / sizeof(uint32_t);

  if (!dev->rewind(g_dcr)) {
    Pmsg1(0, T_("Bad status from rewind. ERR=%s\n"), dev->bstrerror());
    goto bail_out;
  }

  for (uint32_t i = 1; i <= num_recs; i++) {
    p = (uint32_t*)rec->data;
    for (uint32_t j = 0; j < len; j++) { *p++ = i; }
    if (!WriteRecordToBlock(g_dcr, rec)) {
      Pmsg0(0, T_("Error writing record to block.\n"));
      goto bail_out;
    }
    if (!g_dcr->WriteBlockToDev()) {
      Pmsg0(0, T_("Error writing block to device.\n"));
      goto bail_out;
    }
  }
  Pmsg2(0, T_("Wrote %d blocks of %d bytes.\n"), num_recs, rec->data_len);
  weofcmd();
  for (uint32_t i = num_recs + 1; i <= 2 * num_recs; i++) {
    p = (uint32_t*)rec->data;
    for (uint32_t j = 0; j < len; j++) { *p++ = i; }
    if (!WriteRecordToBlock(g_dcr, rec)) {
      Pmsg0(0, T_("Error writing record to block.\n"));
      goto bail_out;
    }
    if (!g_dcr->WriteBlockToDev()) {
      Pmsg0(0, T_("Error writing block to device.\n"));
      goto bail_out;
    }
  }
  Pmsg2(0, T_("Wrote %d blocks of %d bytes.\n"), num_recs, rec->data_len);
  weofcmd();
  if (dev->HasCap(CAP_TWOEOF)) { weofcmd(); }
  rc = true;

bail_out:
  FreeRecord(rec);
  if (!rc) { exit_code = 1; }

  return rc;
}

/**
 * This test writes Bareos blocks to the tape in
 * several files. It then rewinds the tape and attepts
 * to read these blocks back checking the data.
 */
static bool write_read_test()
{
  DeviceBlock* block;
  DeviceRecord* rec;
  bool rc = false;
  uint32_t len;
  uint32_t* p;

  rec = new_record();

  if (!write_two_files()) { goto bail_out; }

  block = g_dcr->block;
  EmptyBlock(block);

  if (!g_dev->rewind(g_dcr)) {
    Pmsg1(0, T_("Bad status from rewind. ERR=%s\n"), g_dev->bstrerror());
    goto bail_out;
  } else {
    Pmsg0(0, T_("Rewind OK.\n"));
  }

  rec->data = CheckPoolMemorySize(rec->data, block->buf_len);
  rec->data_len = block->buf_len - 100;
  len = rec->data_len / sizeof(uint32_t);

  // Now read it back
  for (uint32_t i = 1; i <= 2 * num_recs; i++) {
  read_again:
    if (DeviceControlRecord::ReadStatus::Ok
        != g_dcr->ReadBlockFromDev(NO_BLOCK_NUMBER_CHECK)) {
      BErrNo be;
      if (g_dev->AtEof()) {
        Pmsg0(-1, T_("Got EOF on tape.\n"));
        if (i == num_recs + 1) { goto read_again; }
      }
      Pmsg2(0, T_("Read block %d failed! ERR=%s\n"), i,
            be.bstrerror(g_dev->dev_errno));
      goto bail_out;
    }
    memset(rec->data, 0, rec->data_len);
    if (!ReadRecordFromBlock(g_dcr, rec)) {
      BErrNo be;
      Pmsg2(0, T_("Read record failed. Block %d! ERR=%s\n"), i,
            be.bstrerror(g_dev->dev_errno));
      goto bail_out;
    }
    p = (uint32_t*)rec->data;
    for (uint32_t j = 0; j < len; j++) {
      if (*p != i) {
        Pmsg3(0,
              T_("Bad data in record. Expected %d, got %d at byte %d. Test "
                 "failed!\n"),
              i, *p, j);
        goto bail_out;
      }
      p++;
    }
    if (i == num_recs || i == 2 * num_recs) {
      Pmsg1(-1, T_("%d blocks re-read correctly.\n"), num_recs);
    }
  }
  Pmsg0(-1,
        T_("=== Test Succeeded. End Write, rewind, and re-read test ===\n\n"));
  rc = true;

bail_out:
  FreeRecord(rec);
  if (!rc) { exit_code = 1; }
  return rc;
}

/**
 * This test writes Bareos blocks to the tape in
 * several files. It then rewinds the tape and attepts
 * to read these blocks back checking the data.
 */
static bool position_test()
{
  DeviceBlock* block = g_dcr->block;
  DeviceRecord* rec;
  bool rc = false;
  int len, j;
  bool more = true;
  int recno = 0;
  int file = 0, blk = 0;
  int* p;
  bool got_eof = false;

  Pmsg0(0, T_("Block position test\n"));
  block = g_dcr->block;
  EmptyBlock(block);
  rec = new_record();
  rec->data = CheckPoolMemorySize(rec->data, block->buf_len);
  rec->data_len = block->buf_len - 100;
  len = rec->data_len / sizeof(j);

  if (!g_dev->rewind(g_dcr)) {
    Pmsg1(0, T_("Bad status from rewind. ERR=%s\n"), g_dev->bstrerror());
    goto bail_out;
  } else {
    Pmsg0(0, T_("Rewind OK.\n"));
  }

  while (more) {
    /* Set up next item to read based on where we are */
    /* At each step, recno is what we print for the "block number"
     *  and file, blk are the real positions to go to.
     */
    switch (recno) {
      case 0:
        recno = 5;
        file = 0;
        blk = 4;
        break;
      case 5:
        recno = 201;
        file = 0;
        blk = 200;
        break;
      case 201:
        recno = num_recs;
        file = 0;
        blk = num_recs - 1;
        break;
      case num_recs:
        recno = num_recs + 1;
        file = 1;
        blk = 0;
        break;
      case num_recs + 1:
        recno = num_recs + 601;
        file = 1;
        blk = 600;
        break;
      case num_recs + 601:
        recno = 2 * num_recs;
        file = 1;
        blk = num_recs - 1;
        break;
      case 2 * num_recs:
        more = false;
        continue;
    }
    Pmsg2(-1, T_("Reposition to file:block %d:%d\n"), file, blk);
    if (!g_dev->Reposition(g_dcr, file, blk)) {
      Pmsg0(0, T_("Reposition error.\n"));
      goto bail_out;
    }
  read_again:
    if (DeviceControlRecord::ReadStatus::Ok
        != g_dcr->ReadBlockFromDev(NO_BLOCK_NUMBER_CHECK)) {
      BErrNo be;
      if (g_dev->AtEof()) {
        Pmsg0(-1, T_("Got EOF on tape.\n"));
        if (!got_eof) {
          got_eof = true;
          goto read_again;
        }
      }
      Pmsg4(0, T_("Read block %d failed! file=%d blk=%d. ERR=%s\n\n"), recno,
            file, blk, be.bstrerror(g_dev->dev_errno));
      Pmsg0(0, T_("This may be because the tape drive block size is not\n"
                  " set to variable blocking as normally used by Bareos.\n"
                  " Please see the Tape Testing chapter in the manual and \n"
                  " look for using mt with defblksize and setoptions\n"
                  "If your tape drive block size is correct, then perhaps\n"
                  " your SCSI driver is *really* stupid and does not\n"
                  " correctly report the file:block after a FSF. In this\n"
                  " case try setting:\n"
                  "    Fast Forward Space File = no\n"
                  " in your Device resource.\n"));

      goto bail_out;
    }
    memset(rec->data, 0, rec->data_len);
    if (!ReadRecordFromBlock(g_dcr, rec)) {
      BErrNo be;
      Pmsg1(0, T_("Read record failed! ERR=%s\n"),
            be.bstrerror(g_dev->dev_errno));
      goto bail_out;
    }
    p = (int*)rec->data;
    for (j = 0; j < len; j++) {
      if (p[j] != recno) {
        Pmsg3(0,
              T_("Bad data in record. Expected %d, got %d at byte %d. Test "
                 "failed!\n"),
              recno, p[j], j);
        goto bail_out;
      }
    }
    Pmsg1(-1, T_("Block %d re-read correctly.\n"), recno);
  }
  Pmsg0(-1,
        T_("=== Test Succeeded. End Write, rewind, and re-read test ===\n\n"));
  rc = true;

bail_out:
  FreeRecord(rec);
  return rc;
}


/**
 * This test writes some records, then writes an end of file,
 *   rewinds the tape, moves to the end of the data and attepts
 *   to append to the tape.  This function is essential for
 *   Bareos to be able to write multiple jobs to the tape.
 */
static int append_test()
{
  Pmsg0(-1, T_("\n\n=== Append files test ===\n\n"
               "This test is essential to Bareos.\n\n"
               "I'm going to write one record  in file 0,\n"
               "                   two records in file 1,\n"
               "             and three records in file 2\n\n"));
  argc = 1;
  rewindcmd();
  wrcmd();
  weofcmd(); /* end file 0 */
  wrcmd();
  wrcmd();
  weofcmd(); /* end file 1 */
  wrcmd();
  wrcmd();
  wrcmd();
  weofcmd(); /* end file 2 */
  if (g_dev->HasCap(CAP_TWOEOF)) { weofcmd(); }
  g_dev->close(g_dcr); /* release device */
  if (!open_the_device()) { return -1; }
  rewindcmd();
  Pmsg0(0, T_("Now moving to end of medium.\n"));
  eodcmd();
  Pmsg2(-1, T_("We should be in file 3. I am at file %d. %s\n"), g_dev->file,
        g_dev->file == 3 ? T_("This is correct!")
                         : T_("This is NOT correct!!!!"));

  if (g_dev->file != 3) { return -1; }

  Pmsg0(-1, T_("\nNow the important part, I am going to attempt to append to "
               "the tape.\n\n"));
  wrcmd();
  weofcmd();
  if (g_dev->HasCap(CAP_TWOEOF)) { weofcmd(); }
  rewindcmd();
  Pmsg0(-1, T_("Done appending, there should be no I/O errors\n\n"));
  Pmsg0(-1, T_("Doing Bareos scan of blocks:\n"));
  scan_blocks();
  Pmsg0(-1, T_("End scanning the tape.\n"));
  Pmsg2(-1, T_("We should be in file 4. I am at file %d. %s\n"), g_dev->file,
        g_dev->file == 4 ? T_("This is correct!")
                         : T_("This is NOT correct!!!!"));

  if (g_dev->file != 4) { return -2; }
  return 1;
}


// This test exercises the autochanger
static int autochanger_test()
{
  POOLMEM *results, *changer;
  slot_number_t slot, loaded;
  int status;
  int timeout = g_dcr->device_resource->max_changer_wait;
  int sleep_time = 0;

  Dmsg1(100, "Max changer wait = %d sec\n", timeout);
  if (!g_dev->HasCap(CAP_ATTACHED_TO_AUTOCHANGER)) { return 1; }
  if (!(g_dcr->device_resource && g_dcr->device_resource->changer_name
        && g_dcr->device_resource->changer_command)) {
    Pmsg0(-1, T_("\nAutochanger enabled, but no name or no command device "
                 "specified.\n"));
    return 1;
  }

  Pmsg0(-1, T_("\nAh, I see you have an autochanger configured.\n"
               "To test the autochanger you must have a blank tape\n"
               " that I can write on in Slot 1.\n"));
  if (!GetCmd(
          T_("\nDo you wish to continue with the Autochanger test? (y/n): "))) {
    return 0;
  }
  if (cmd[0] != 'y' && cmd[0] != 'Y') { return 0; }

  Pmsg0(-1, T_("\n\n=== Autochanger test ===\n\n"));

  results = GetPoolMemory(PM_MESSAGE);
  changer = GetPoolMemory(PM_FNAME);

try_again:
  slot = 1;
  g_dcr->VolCatInfo.Slot = slot;
  /* Find out what is loaded, zero means device is unloaded */
  Pmsg0(-1, T_("3301 Issuing autochanger \"loaded\" command.\n"));
  changer = edit_device_codes(
      g_dcr, changer, g_dcr->device_resource->changer_command, "loaded");
  status = RunProgram(changer, timeout, results);
  Dmsg3(100, "run_prog: %s stat=%d result=\"%s\"\n", changer, status, results);
  if (status == 0) {
    loaded = atoi(results);
  } else {
    BErrNo be;
    Pmsg1(-1, T_("3991 Bad autochanger command: %s\n"), changer);
    Pmsg2(-1, T_("3991 result=\"%s\": ERR=%s\n"), results,
          be.bstrerror(status));
    goto bail_out;
  }
  if (loaded) {
    Pmsg1(-1, T_("Slot %d loaded. I am going to unload it.\n"), loaded);
  } else {
    Pmsg0(-1, T_("Nothing loaded in the drive. OK.\n"));
  }
  Dmsg1(100, "Results from loaded query=%s\n", results);
  if (loaded) {
    g_dcr->VolCatInfo.Slot = loaded;
    /* We are going to load a new tape, so close the device */
    g_dev->close(g_dcr);
    Pmsg2(-1, T_("3302 Issuing autochanger \"unload %d %d\" command.\n"),
          loaded, g_dev->drive);
    changer = edit_device_codes(
        g_dcr, changer, g_dcr->device_resource->changer_command, "unload");
    status = RunProgram(changer, timeout, results);
    Pmsg2(-1, T_("unload status=%s %d\n"), status == 0 ? T_("OK") : T_("Bad"),
          status);
    if (status != 0) {
      BErrNo be;
      Pmsg1(-1, T_("3992 Bad autochanger command: %s\n"), changer);
      Pmsg2(-1, T_("3992 result=\"%s\": ERR=%s\n"), results,
            be.bstrerror(status));
    }
  }

  // Load the Slot 1

  slot = 1;
  g_dcr->VolCatInfo.Slot = slot;
  Pmsg2(-1, T_("3303 Issuing autochanger \"load %d %d\" command.\n"), slot,
        g_dev->drive);
  changer = edit_device_codes(g_dcr, changer,
                              g_dcr->device_resource->changer_command, "load");
  Dmsg1(100, "Changer=%s\n", changer);
  g_dev->close(g_dcr);
  status = RunProgram(changer, timeout, results);
  if (status == 0) {
    Pmsg2(-1, T_("3303 Autochanger \"load %d %d\" status is OK.\n"), slot,
          g_dev->drive);
  } else {
    BErrNo be;
    Pmsg1(-1, T_("3993 Bad autochanger command: %s\n"), changer);
    Pmsg2(-1, T_("3993 result=\"%s\": ERR=%s\n"), results,
          be.bstrerror(status));
    goto bail_out;
  }

  if (!open_the_device()) { goto bail_out; }
  /* Start with sleep_time 0 then increment by 30 seconds if we get
   * a failure. */
  Bmicrosleep(sleep_time, 0);
  if (!g_dev->rewind(g_dcr) || !g_dev->weof(1)) {
    Pmsg1(0, T_("Bad status from rewind. ERR=%s\n"), g_dev->bstrerror());
    g_dev->clrerror(-1);
    Pmsg0(-1, T_("\nThe test failed, probably because you need to put\n"
                 "a longer sleep time in the mtx-script in the load) case.\n"
                 "Adding a 30 second sleep and trying again ...\n"));
    sleep_time += 30;
    goto try_again;
  } else {
    Pmsg1(0, T_("Rewound %s\n"), g_dev->print_name());
  }

  if (!g_dev->weof(1)) {
    Pmsg1(0, T_("Bad status from weof. ERR=%s\n"), g_dev->bstrerror());
    goto bail_out;
  } else {
    Pmsg1(0, T_("Wrote EOF to %s\n"), g_dev->print_name());
  }

  if (sleep_time) {
    Pmsg1(-1,
          T_("\nThe test worked this time. Please add:\n\n"
             "   sleep %d\n\n"
             "to your mtx-changer script in the load) case.\n\n"),
          sleep_time);
  } else {
    Pmsg0(-1, T_("\nThe test autochanger worked!!\n\n"));
  }

  FreePoolMemory(changer);
  FreePoolMemory(results);
  return 1;


bail_out:
  FreePoolMemory(changer);
  FreePoolMemory(results);
  Pmsg0(-1,
        T_("You must correct this error or the Autochanger will not work.\n"));
  return -2;
}

static void autochangercmd() { autochanger_test(); }


/**
 * This test assumes that the append test has been done,
 *   then it tests the fsf function.
 */
static bool fsf_test()
{
  bool set_off = false;

  Pmsg0(-1, T_("\n\n=== Forward space files test ===\n\n"
               "This test is essential to Bareos.\n\n"
               "I'm going to write five files then test forward spacing\n\n"));
  argc = 1;
  rewindcmd();
  wrcmd();
  weofcmd(); /* end file 0 */
  wrcmd();
  wrcmd();
  weofcmd(); /* end file 1 */
  wrcmd();
  wrcmd();
  wrcmd();
  weofcmd(); /* end file 2 */
  wrcmd();
  wrcmd();
  weofcmd(); /* end file 3 */
  wrcmd();
  weofcmd(); /* end file 4 */
  if (g_dev->HasCap(CAP_TWOEOF)) { weofcmd(); }

test_again:
  rewindcmd();
  Pmsg0(0, T_("Now forward spacing 1 file.\n"));
  if (!g_dev->fsf(1)) {
    Pmsg1(0, T_("Bad status from fsr. ERR=%s\n"), g_dev->bstrerror());
    goto bail_out;
  }
  Pmsg2(-1, T_("We should be in file 1. I am at file %d. %s\n"), g_dev->file,
        g_dev->file == 1 ? T_("This is correct!")
                         : T_("This is NOT correct!!!!"));

  if (g_dev->file != 1) { goto bail_out; }

  Pmsg0(0, T_("Now forward spacing 2 files.\n"));
  if (!g_dev->fsf(2)) {
    Pmsg1(0, T_("Bad status from fsr. ERR=%s\n"), g_dev->bstrerror());
    goto bail_out;
  }
  Pmsg2(-1, T_("We should be in file 3. I am at file %d. %s\n"), g_dev->file,
        g_dev->file == 3 ? T_("This is correct!")
                         : T_("This is NOT correct!!!!"));

  if (g_dev->file != 3) { goto bail_out; }

  rewindcmd();
  Pmsg0(0, T_("Now forward spacing 4 files.\n"));
  if (!g_dev->fsf(4)) {
    Pmsg1(0, T_("Bad status from fsr. ERR=%s\n"), g_dev->bstrerror());
    goto bail_out;
  }
  Pmsg2(-1, T_("We should be in file 4. I am at file %d. %s\n"), g_dev->file,
        g_dev->file == 4 ? T_("This is correct!")
                         : T_("This is NOT correct!!!!"));

  if (g_dev->file != 4) { goto bail_out; }
  if (set_off) {
    Pmsg0(-1, T_("The test worked this time. Please add:\n\n"
                 "   Fast Forward Space File = no\n\n"
                 "to your Device resource for this drive.\n"));
  }

  Pmsg0(-1, "\n");
  Pmsg0(0, T_("Now forward spacing 1 more file.\n"));
  if (!g_dev->fsf(1)) {
    Pmsg1(0, T_("Bad status from fsr. ERR=%s\n"), g_dev->bstrerror());
  }
  Pmsg2(-1, T_("We should be in file 5. I am at file %d. %s\n"), g_dev->file,
        g_dev->file == 5 ? T_("This is correct!")
                         : T_("This is NOT correct!!!!"));
  if (g_dev->file != 5) { goto bail_out; }
  Pmsg0(-1, T_("\n=== End Forward space files test ===\n\n"));
  return true;

bail_out:
  Pmsg0(-1, T_("\nThe forward space file test failed.\n"));
  if (g_dev->HasCap(CAP_FASTFSF)) {
    Pmsg0(-1, T_("You have Fast Forward Space File enabled.\n"
                 "I am turning it off then retrying the test.\n"));
    g_dev->ClearCap(CAP_FASTFSF);
    set_off = true;
    goto test_again;
  }
  Pmsg0(-1, T_("You must correct this error or Bareos will not work.\n"
               "Some systems, e.g. OpenBSD, require you to set\n"
               "   Use MTIOCGET= no\n"
               "in your device resource. Use with caution.\n"));
  return false;
}


/**
 * This is a general test of Bareos's functions
 *   needed to read and write the tape.
 */
static void testcmd()
{
  int status;

  if (!write_read_test()) {
    exit_code = 1;
    return;
  }
  if (!position_test()) {
    exit_code = 1;
    return;
  }

  status = append_test();
  if (status == 1) { /* OK get out */
    goto all_done;
  }
  if (status == -1) { /* first test failed */
    if (g_dev->HasCap(CAP_EOM) || g_dev->HasCap(CAP_FASTFSF)) {
      Pmsg0(-1, T_("\nAppend test failed. Attempting again.\n"
                   "Setting \"Hardware End of Medium = no\n"
                   "    and \"Fast Forward Space File = no\n"
                   "and retrying append test.\n\n"));
      g_dev->ClearCap(CAP_EOM);     /* turn off eom */
      g_dev->ClearCap(CAP_FASTFSF); /* turn off fast fsf */
      status = append_test();
      if (status == 1) {
        Pmsg0(-1,
              T_("\n\nIt looks like the test worked this time, please add:\n\n"
                 "    Hardware End of Medium = No\n\n"
                 "    Fast Forward Space File = No\n"
                 "to your Device resource in the Storage conf file.\n"));
        goto all_done;
      }
      if (status == -1) {
        Pmsg0(-1,
              T_("\n\nThat appears *NOT* to have corrected the problem.\n"));
        goto failed;
      }
      /* Wrong count after append */
      if (status == -2) {
        Pmsg0(-1,
              T_("\n\nIt looks like the append failed. Attempting again.\n"
                 "Setting \"BSF at EOM = yes\" and retrying append test.\n"));
        g_dev->SetCap(CAP_BSFATEOM); /* Backspace on eom */
        status = append_test();
        if (status == 1) {
          Pmsg0(
              -1,
              T_("\n\nIt looks like the test worked this time, please add:\n\n"
                 "    Hardware End of Medium = No\n"
                 "    Fast Forward Space File = No\n"
                 "    BSF at EOM = yes\n\n"
                 "to your Device resource in the Storage conf file.\n"));
          goto all_done;
        }
      }
    }
  failed:
    Pmsg0(-1,
          T_("\nAppend test failed.\n\n"
             "\n!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!\n"
             "Unable to correct the problem. You MUST fix this\n"
             "problem before Bareos can use your tape drive correctly\n"
             "\nPerhaps running Bareos in fixed block mode will work.\n"
             "Do so by setting:\n\n"
             "Minimum Block Size = nnn\n"
             "Maximum Block Size = nnn\n\n"
             "in your Storage daemon's Device definition.\n"
             "nnn must match your tape driver's block size, which\n"
             "can be determined by reading your tape manufacturers\n"
             "information, and the information on your kernel dirver.\n"
             "Fixed block sizes, however, are not normally an ideal solution.\n"
             "\n"
             "Some systems, e.g. OpenBSD, require you to set\n"
             "   Use MTIOCGET= no\n"
             "in your device resource. Use with caution.\n"));
    exit_code = 1;
    return;
  }

all_done:
  Pmsg0(-1, T_("\nThe above Bareos scan should have output identical to what "
               "follows.\n"
               "Please double check it ...\n"
               "=== Sample correct output ===\n"
               "1 block of 64448 bytes in file 1\n"
               "End of File mark.\n"
               "2 blocks of 64448 bytes in file 2\n"
               "End of File mark.\n"
               "3 blocks of 64448 bytes in file 3\n"
               "End of File mark.\n"
               "1 block of 64448 bytes in file 4\n"
               "End of File mark.\n"
               "Total files=4, blocks=7, bytes = 451,136\n"
               "=== End sample correct output ===\n\n"
               "If the above scan output is not identical to the\n"
               "sample output, you MUST correct the problem\n"
               "or Bareos will not be able to write multiple Jobs to \n"
               "the tape.\n\n"));

  if (status == 1) {
    if (!re_read_block_test()) { exit_code = 1; }
  }

  if (!fsf_test()) { /* do fast forward space file test */
    exit_code = 1;
  }

  autochanger_test(); /* do autochanger test */
}

/* Forward space a file */
static void fsfcmd()
{
  int num = 1;
  if (argc > 1) { num = atoi(argk[1]); }
  if (num <= 0) { num = 1; }
  if (!g_dev->fsf(num)) {
    Pmsg1(0, T_("Bad status from fsf. ERR=%s\n"), g_dev->bstrerror());
    return;
  }
  if (num == 1) {
    Pmsg0(0, T_("Forward spaced 1 file.\n"));
  } else {
    Pmsg1(0, T_("Forward spaced %d files.\n"), num);
  }
}

/* Forward space a record */
static void fsrcmd()
{
  int num = 1;
  if (argc > 1) { num = atoi(argk[1]); }
  if (num <= 0) { num = 1; }
  if (!g_dev->fsr(num)) {
    Pmsg1(0, T_("Bad status from fsr. ERR=%s\n"), g_dev->bstrerror());
    return;
  }
  if (num == 1) {
    Pmsg0(0, T_("Forward spaced 1 record.\n"));
  } else {
    Pmsg1(0, T_("Forward spaced %d records.\n"), num);
  }
}

// Read a Bareos block from the tape
static void rbcmd()
{
  g_dev->open(g_dcr, DeviceMode::OPEN_READ_ONLY);
  g_dcr->ReadBlockFromDev(NO_BLOCK_NUMBER_CHECK);
}

// Write a Bareos block to the tape
static void wrcmd()
{
  DeviceBlock* block = g_dcr->block;
  DeviceRecord* rec = g_dcr->rec;
  int i;

  if (!g_dev->IsOpen()) { open_the_device(); }
  EmptyBlock(block);
  if (g_verbose > 1) { DumpBlock(block, "test"); }

  i = block->buf_len - 100;
  ASSERT(i > 0);
  rec->data = CheckPoolMemorySize(rec->data, i);
  memset(rec->data, i & 0xFF, i);
  rec->data_len = i;
  if (!WriteRecordToBlock(g_dcr, rec)) {
    Pmsg0(0, T_("Error writing record to block.\n"));
    goto bail_out;
  }
  if (!g_dcr->WriteBlockToDev()) {
    Pmsg0(0, T_("Error writing block to device.\n"));
    goto bail_out;
  } else {
    Pmsg1(0, T_("Wrote one record of %d bytes.\n"), i);
  }
  Pmsg0(0, T_("Wrote block to device.\n"));

bail_out:
  return;
}

// Read a record from the tape
static void rrcmd()
{
  char* buf2;
  int status, len;

  if (!GetCmd(T_("Enter length to read: "))) { return; }
  len = atoi(cmd);
  if (len < 0 || len > 1'000'000) {
    Pmsg0(0, T_("Bad length entered, using default of 1024 bytes.\n"));
    len = 1024;
  }
  buf2 = (char*)malloc(len);
  status = read(g_dev->fd, buf2, len);
  if (status > 0 && status <= len) { errno = 0; }
  BErrNo be;
  Pmsg3(0, T_("Read of %d bytes gives status=%d. ERR=%s\n"), len, status,
        be.bstrerror());
  free(buf2);
}


/**
 * Scan tape by reading block by block. Report what is
 * on the tape.  Note, this command does raw reads, and as such
 * will not work with fixed block size devices.
 */
static void scancmd()
{
  int status;
  int blocks, tot_blocks, tot_files;
  int block_size;
  uint64_t bytes;
  char ec1[50];


  blocks = block_size = tot_blocks = 0;
  bytes = 0;
  if (g_dev->AtEot()) {
    Pmsg0(0, T_("End of tape\n"));
    return;
  }
  g_dev->UpdatePos(g_dcr);
  tot_files = g_dev->file;
  Pmsg1(0, T_("Starting scan at file %u\n"), g_dev->file);
  for (;;) {
    if ((status = read(g_dev->fd, buf, sizeof(buf))) < 0) {
      BErrNo be;
      g_dev->clrerror(-1);
      Mmsg2(g_dev->errmsg, T_("read error on %s. ERR=%s.\n"),
            g_dev->archive_device_string, be.bstrerror());
      Pmsg2(0, T_("Bad status from read %d. ERR=%s\n"), status,
            g_dev->bstrerror());
      if (blocks > 0) {
        if (blocks == 1) {
          printf(T_("1 block of %d bytes in file %d\n"), block_size,
                 g_dev->file);
        } else {
          printf(T_("%d blocks of %d bytes in file %d\n"), blocks, block_size,
                 g_dev->file);
        }
      }
      return;
    }
    Dmsg1(200, "read status = %d\n", status);
    /*    sleep(1); */
    if (status != block_size) {
      g_dev->UpdatePos(g_dcr);
      if (blocks > 0) {
        if (blocks == 1) {
          printf(T_("1 block of %d bytes in file %d\n"), block_size,
                 g_dev->file);
        } else {
          printf(T_("%d blocks of %d bytes in file %d\n"), blocks, block_size,
                 g_dev->file);
        }
        blocks = 0;
      }
      block_size = status;
    }
    if (status == 0) { /* EOF */
      g_dev->UpdatePos(g_dcr);
      printf(T_("End of File mark.\n"));
      /* Two reads of zero means end of tape */
      if (g_dev->AtEof()) {
        g_dev->SetEot();
      } else {
        g_dev->SetEof();
        g_dev->file++;
      }
      if (g_dev->AtEot()) {
        printf(T_("End of tape\n"));
        break;
      }
    } else { /* Got data */
      g_dev->ClearEof();
      blocks++;
      tot_blocks++;
      bytes += status;
    }
  }
  g_dev->UpdatePos(g_dcr);
  tot_files = g_dev->file - tot_files;
  printf(T_("Total files=%d, blocks=%d, bytes = %s\n"), tot_files, tot_blocks,
         edit_uint64_with_commas(bytes, ec1));
}


/**
 * Scan tape by reading Bareos block by block. Report what is
 * on the tape.  This function reads Bareos blocks, so if your
 * Device resource is correctly defined, it should work with
 * either variable or fixed block sizes.
 */
static void scan_blocks()
{
  int blocks, tot_blocks, tot_files;
  uint32_t block_size;
  uint64_t bytes;
  DeviceBlock* block = g_dcr->block;
  char ec1[50];
  char buf1[100], buf2[100];

  blocks = block_size = tot_blocks = 0;
  bytes = 0;

  EmptyBlock(block);
  g_dev->UpdatePos(g_dcr);
  tot_files = g_dev->file;
  for (;;) {
    switch (g_dcr->ReadBlockFromDevice(NO_BLOCK_NUMBER_CHECK)) {
      case DeviceControlRecord::ReadStatus::Ok:
        // no special handling required
        break;
      case DeviceControlRecord::ReadStatus::EndOfTape:
        if (blocks > 0) {
          if (blocks == 1) {
            printf(T_("1 block of %d bytes in file %d\n"), block_size,
                   g_dev->file);
          } else {
            printf(T_("%d blocks of %d bytes in file %d\n"), blocks, block_size,
                   g_dev->file);
          }
          blocks = 0;
        }
        goto bail_out;
      case DeviceControlRecord::ReadStatus::EndOfFile:
        if (blocks > 0) {
          if (blocks == 1) {
            printf(T_("1 block of %d bytes in file %d\n"), block_size,
                   g_dev->file);
          } else {
            printf(T_("%d blocks of %d bytes in file %d\n"), blocks, block_size,
                   g_dev->file);
          }
          blocks = 0;
        }
        printf(T_("End of File mark.\n"));
        continue;
      default:
        Dmsg1(100, "!read_block(): ERR=%s\n", g_dev->bstrerror());
        if (BitIsSet(ST_SHORT, g_dev->state)) {
          if (blocks > 0) {
            if (blocks == 1) {
              printf(T_("1 block of %d bytes in file %d\n"), block_size,
                     g_dev->file);
            } else {
              printf(T_("%d blocks of %d bytes in file %d\n"), blocks,
                     block_size, g_dev->file);
            }
            blocks = 0;
          }
          printf(T_("Short block read.\n"));
          continue;
        }
        printf(T_("Error reading block. ERR=%s\n"), g_dev->bstrerror());
        goto bail_out;
    }
    if (block->block_len != block_size) {
      if (blocks > 0) {
        if (blocks == 1) {
          printf(T_("1 block of %d bytes in file %d\n"), block_size,
                 g_dev->file);
        } else {
          printf(T_("%d blocks of %d bytes in file %d\n"), blocks, block_size,
                 g_dev->file);
        }
        blocks = 0;
      }
      block_size = block->block_len;
    }
    blocks++;
    tot_blocks++;
    bytes += block->block_len;
    Dmsg7(100,
          "Blk_blk=%u file,blk=%u,%u blen=%u bVer=%d SessId=%u SessTim=%u\n",
          block->BlockNumber, g_dev->file, g_dev->block_num, block->block_len,
          block->BlockVer, block->VolSessionId, block->VolSessionTime);
    if (g_verbose == 1) {
      DeviceRecord* rec = new_record();
      ReadRecordFromBlock(g_dcr, rec);
      Pmsg9(-1,
            T_("Block=%u file,blk=%u,%u blen=%u First rec FI=%s SessId=%u "
               "SessTim=%u Strm=%s rlen=%d\n"),
            block->BlockNumber, g_dev->file, g_dev->block_num, block->block_len,
            FI_to_ascii(buf1, rec->FileIndex), rec->VolSessionId,
            rec->VolSessionTime,
            stream_to_ascii(buf2, rec->Stream, rec->FileIndex), rec->data_len);
      rec->remainder = 0;
      FreeRecord(rec);
    } else if (g_verbose > 1) {
      DumpBlock(block, "");
    }
  }
bail_out:
  tot_files = g_dev->file - tot_files;
  printf(T_("Total files=%d, blocks=%d, bytes = %s\n"), tot_files, tot_blocks,
         edit_uint64_with_commas(bytes, ec1));
}

static void statcmd()
{
  char* status = g_dev->StatusDev();

  printf(T_("Device status:"));
  if (BitIsSet(BMT_TAPE, status)) printf(" TAPE");
  if (BitIsSet(BMT_EOF, status)) printf(" EOF");
  if (BitIsSet(BMT_BOT, status)) printf(" BOT");
  if (BitIsSet(BMT_EOT, status)) printf(" EOT");
  if (BitIsSet(BMT_SM, status)) printf(" SETMARK");
  if (BitIsSet(BMT_EOD, status)) printf(" EOD");
  if (BitIsSet(BMT_WR_PROT, status)) printf(" WRPROT");
  if (BitIsSet(BMT_ONLINE, status)) printf(" ONLINE");
  if (BitIsSet(BMT_DR_OPEN, status)) printf(" DOOROPEN");
  if (BitIsSet(BMT_IM_REP_EN, status)) printf(" IMMREPORT");

  free(status);

  printf(T_(". ERR=%s\n"), g_dev->bstrerror());
}

/**
 * First we label the tape, then we fill
 *  it with data get a new tape and write a few blocks.
 */
static void fillcmd()
{
  DeviceBlock* block = g_dcr->block;
  char ec1[50], ec2[50];
  char buf1[100], buf2[100];
  uint64_t write_eof;
  uint64_t rate;
  uint32_t min_block_size;
  int fd;

  g_ok = true;
  stop = 0;
  vol_num = 0;
  last_file = 0;
  last_block_num = 0;
  BlockNumber = 0;
  exit_code = 0;

  Pmsg1(-1,
        T_("\n"
           "This command simulates Bareos writing to a tape.\n"
           "It requires either one or two blank tapes, which it\n"
           "will label and write.\n\n"
           "If you have an autochanger configured, it will use\n"
           "the tapes that are in slots 1 and 2, otherwise, you will\n"
           "be prompted to insert the tapes when necessary.\n\n"
           "It will print a status approximately\n"
           "every 322 MB, and write an EOF every %s.  If you have\n"
           "selected the simple test option, after writing the first tape\n"
           "it will rewind it and re-read the last block written.\n\n"
           "If you have selected the multiple tape test, when the first tape\n"
           "fills, it will ask for a second, and after writing a few more \n"
           "blocks, it will stop.  Then it will begin re-reading the\n"
           "two tapes.\n\n"
           "This may take a long time -- hours! ...\n\n"),
        edit_uint64_with_suffix(g_dev->max_file_size, buf1));

  GetCmd(
      T_("Do you want to run the simplified test (s) with one tape\n"
         "or the complete multiple tape (m) test: (s/m) "));
  if (cmd[0] == 's') {
    Pmsg0(-1, T_("Simple test (single tape) selected.\n"));
    simple = true;
  } else if (cmd[0] == 'm') {
    Pmsg0(-1, T_("Multiple tape test selected.\n"));
    simple = false;
  } else {
    Pmsg0(000, T_("Command aborted.\n"));
    exit_code = 1;
    return;
  }

  Dmsg1(20, "Begin append device=%s\n", g_dev->print_name());
  Dmsg1(20, "MaxVolSize=%s\n", edit_uint64(g_dev->max_volume_size, ec1));

  /* Use fixed block size to simplify read back */
  min_block_size = g_dev->min_block_size;
  g_dev->min_block_size = g_dev->max_block_size;
  write_eof = g_dev->max_file_size / REC_SIZE; /*compute when we add EOF*/
  ASSERT(write_eof > 0);

  SetVolumeName("TestVolume1", 1);
  g_dcr->DirAskSysopToCreateAppendableVolume();
  g_dev->SetAppend(); /* force volume to be relabeled */

  /* Acquire output device for writing.  Note, after acquiring a
   *   device, we MUST release it, which is done at the end of this
   *   subroutine. */
  Dmsg0(100, "just before acquire_device\n");
  if (!AcquireDeviceForAppend(g_dcr)) {
    g_jcr->setJobStatusWithPriorityCheck(JS_ErrorTerminated);
    exit_code = 1;
    return;
  }
  block = g_jcr->sd_impl->dcr->block;

  Dmsg0(100, "Just after AcquireDeviceForAppend\n");
  // Write Begin Session Record
  if (!WriteSessionLabel(g_dcr, SOS_LABEL)) {
    g_jcr->setJobStatusWithPriorityCheck(JS_ErrorTerminated);
    Jmsg1(g_jcr, M_FATAL, 0, T_("Write session label failed. ERR=%s\n"),
          g_dev->bstrerror());
    g_ok = false;
  }
  Pmsg0(-1, T_("Wrote Start of Session label.\n"));

  DeviceRecord rec;
  rec.data = GetMemory(100'000); /* max record size */
  rec.data_len = REC_SIZE;

  // Put some random data in the record
  FillBuffer(FILL_RANDOM, rec.data, rec.data_len);

  // Generate data as if from File daemon, write to device
  g_jcr->sd_impl->dcr->VolFirstIndex = 0;
  time(&g_jcr->run_time); /* start counting time for rates */

  bstrftime(buf1, sizeof(buf1), g_jcr->run_time, "%H:%M:%S");

  if (simple) {
    Pmsg1(-1, T_("%s Begin writing Bareos records to tape ...\n"), buf1);
  } else {
    Pmsg1(-1, T_("%s Begin writing Bareos records to first tape ...\n"), buf1);
  }
  for (file_index = 0; g_ok && !g_jcr->IsJobCanceled();) {
    rec.VolSessionId = g_jcr->VolSessionId;
    rec.VolSessionTime = g_jcr->VolSessionTime;
    rec.FileIndex = ++file_index;
    rec.Stream = STREAM_FILE_DATA;
    rec.maskedStream = STREAM_FILE_DATA;

    /* Mix up the data just a bit */
    MixBuffer(FILL_RANDOM, rec.data, rec.data_len);

    Dmsg4(250, "before write_rec FI=%d SessId=%d Strm=%s len=%d\n",
          rec.FileIndex, rec.VolSessionId,
          stream_to_ascii(buf1, rec.Stream, rec.FileIndex), rec.data_len);

    while (!WriteRecordToBlock(g_dcr, &rec)) {
      // When we get here we have just filled a block
      Dmsg2(150, "!WriteRecordToBlock data_len=%d rem=%d\n", rec.data_len,
            rec.remainder);

      /* Write block to tape */
      if (!FlushBlock(block)) {
        Pmsg0(000, T_("Flush block failed.\n"));
        exit_code = 1;
        break;
      }

      /* Every 5000 blocks (approx 322MB) report where we are.
       */
      if ((block->BlockNumber % 5000) == 0) {
        now = time(nullptr);
        now -= g_jcr->run_time;
        if (now <= 0) { now = 1; /* prevent divide error */ }
        rate = g_dev->VolCatInfo.VolCatBytes / now;
        Pmsg5(-1, T_("Wrote block=%u, file,blk=%u,%u VolBytes=%s rate=%sB/s\n"),
              block->BlockNumber, g_dev->file, g_dev->block_num,
              edit_uint64_with_commas(g_dev->VolCatInfo.VolCatBytes, ec1),
              edit_uint64_with_suffix(rate, ec2));
      }
      /* Every X blocks (dev->max_file_size) write an EOF.
       */
      if ((block->BlockNumber % write_eof) == 0) {
        now = time(nullptr);
        bstrftime(buf1, sizeof(buf1), now, "%H:%M:%S");
        Pmsg1(-1, T_("%s Flush block, write EOF\n"), buf1);
        FlushBlock(block);
      }

      /* Get out after writing 1000 blocks to the second tape */
      if (++BlockNumber > 1000 && stop != 0) { /* get out */
        Pmsg0(000, T_("Wrote 1000 blocks on second tape. Done.\n"));
        break;
      }
    }
    if (!g_ok) {
      Pmsg0(000, T_("Not OK\n"));
      exit_code = 1;
      break;
    }
    g_jcr->JobBytes += rec.data_len; /* increment bytes of this job */
    Dmsg4(190, "WriteRecord FI=%s SessId=%d Strm=%s len=%d\n",
          FI_to_ascii(buf1, rec.FileIndex), rec.VolSessionId,
          stream_to_ascii(buf2, rec.Stream, rec.FileIndex), rec.data_len);

    /* Get out after writing 1000 blocks to the second tape */
    if (BlockNumber > 1000 && stop != 0) { /* get out */
      char ed1[50];
      Pmsg1(-1, "Done writing %s records ...\n",
            edit_uint64_with_commas(write_count, ed1));
      break;
    }
  } /* end big for loop */

  if (vol_num > 1) {
    Dmsg0(100, "Write_end_session_label()\n");
    /* Create Job status for end of session label */
    if (!g_jcr->IsJobCanceled() && g_ok) {
      g_jcr->setJobStatusWithPriorityCheck(JS_Terminated);
    } else if (!g_ok) {
      Pmsg0(000, T_("Job canceled.\n"));
      g_jcr->setJobStatusWithPriorityCheck(JS_ErrorTerminated);
      exit_code = 1;
    }
    if (!WriteSessionLabel(g_dcr, EOS_LABEL)) {
      Pmsg1(000, T_("Error writing end session label. ERR=%s\n"),
            g_dev->bstrerror());
      g_ok = false;
      exit_code = 1;
    }
    /* Write out final block of this session */
    if (!g_dcr->WriteBlockToDevice()) {
      Pmsg0(-1, T_("Set ok=false after WriteBlockToDevice.\n"));
      g_ok = false;
      exit_code = 1;
    }
    Pmsg0(-1, T_("Wrote End of Session label.\n"));

    /* Save last block info for second tape */
    last_block_num2 = last_block_num;
    last_file2 = last_file;
    if (last_block2) { FreeBlock(last_block2); }
    last_block2 = dup_block(last_block);
  }

  sprintf(buf, "%s/btape.state", working_directory);
  fd = open(buf, O_CREAT | O_TRUNC | O_WRONLY, 0640);
  if (fd >= 0) {
    write_with_check(fd, &btape_state_level, sizeof(btape_state_level));
    write_with_check(fd, &simple, sizeof(simple));
    write_with_check(fd, &last_block_num1, sizeof(last_block_num1));
    write_with_check(fd, &last_block_num2, sizeof(last_block_num2));
    write_with_check(fd, &last_file1, sizeof(last_file1));
    write_with_check(fd, &last_file2, sizeof(last_file2));
    write_with_check(fd, last_block1->buf, last_block1->buf_len);
    write_with_check(fd, last_block2->buf, last_block2->buf_len);
    write_with_check(fd, first_block->buf, first_block->buf_len);
    close(fd);
    Pmsg2(0, T_("Wrote state file last_block_num1=%d last_block_num2=%d\n"),
          last_block_num1, last_block_num2);
  } else {
    BErrNo be;
    Pmsg2(0, T_("Could not create state file: %s ERR=%s\n"), buf,
          be.bstrerror());
    exit_code = 1;
    g_ok = false;
  }

  now = time(nullptr);
  bstrftime(buf1, sizeof(buf1), now, "%H:%M:%S");

  if (g_ok) {
    if (simple) {
      Pmsg3(0,
            T_("\n\n%s Done filling tape at %d:%d. Now beginning re-read of "
               "tape ...\n"),
            buf1, g_jcr->sd_impl->dcr->dev->file,
            g_jcr->sd_impl->dcr->dev->block_num);
    } else {
      Pmsg3(0,
            T_("\n\n%s Done filling tapes at %d:%d. Now beginning re-read of "
               "first tape ...\n"),
            buf1, g_jcr->sd_impl->dcr->dev->file,
            g_jcr->sd_impl->dcr->dev->block_num);
    }

    g_jcr->sd_impl->dcr->block = block;
    if (!do_unfill()) {
      Pmsg0(000, T_("do_unfill failed.\n"));
      exit_code = 1;
      g_ok = false;
    }
  } else {
    Pmsg1(000, T_("%s: Error during test.\n"), buf1);
  }
  g_dev->min_block_size = min_block_size;
  FreeMemory(rec.data);
}

/**
 * Read two tapes written by the "fill" command and ensure
 *  that the data is valid.  If stop==1 we simulate full read back
 *  of two tapes.  If stop==-1 we simply read the last block and
 *  verify that it is correct.
 */
static void unfillcmd()
{
  int fd;

  exit_code = 0;
  last_block1 = new_block(g_dev);
  last_block2 = new_block(g_dev);
  first_block = new_block(g_dev);
  sprintf(buf, "%s/btape.state", working_directory);
  fd = open(buf, O_RDONLY);
  if (fd >= 0) {
    uint32_t state_level;
    read_with_check(fd, &state_level, sizeof(btape_state_level));
    read_with_check(fd, &simple, sizeof(simple));
    read_with_check(fd, &last_block_num1, sizeof(last_block_num1));
    read_with_check(fd, &last_block_num2, sizeof(last_block_num2));
    read_with_check(fd, &last_file1, sizeof(last_file1));
    read_with_check(fd, &last_file2, sizeof(last_file2));
    read_with_check(fd, last_block1->buf, last_block1->buf_len);
    read_with_check(fd, last_block2->buf, last_block2->buf_len);
    read_with_check(fd, first_block->buf, first_block->buf_len);
    close(fd);
    if (state_level != btape_state_level) {
      Pmsg0(-1, T_("\nThe state file level has changed. You must redo\n"
                   "the fill command.\n"));
      exit_code = 1;
      return;
    }
  } else {
    BErrNo be;
    Pmsg2(-1,
          T_("\nCould not find the state file: %s ERR=%s\n"
             "You must redo the fill command.\n"),
          buf, be.bstrerror());
    exit_code = 1;
    return;
  }
  if (!do_unfill()) { exit_code = 1; }
  this_block = nullptr;
}

/**
 * This is the second part of the fill command. After the tape or
 *  tapes are written, we are called here to reread parts, particularly
 *  the last block.
 */
static bool do_unfill()
{
  DeviceBlock* block = g_dcr->block;
  int autochanger;
  bool rc = false;

  dumped = 0;
  VolBytes = 0;
  LastBlock = 0;

  Pmsg0(000, "Enter do_unfill\n");
  g_dev->SetCap(CAP_ANONVOLS); /* allow reading any volume */
  g_dev->ClearCap(CAP_LABEL);  /* don't label anything here */

  end_of_tape = 0;

  time(&g_jcr->run_time); /* start counting time for rates */
  stop = 0;
  file_index = 0;
  if (last_block) {
    FreeBlock(last_block);
    last_block = nullptr;
  }
  last_block_num = last_block_num1;
  last_file = last_file1;
  last_block = last_block1;

  FreeRestoreVolumeList(g_jcr);
  g_jcr->sd_impl->read_session.bsr = nullptr;
  bstrncpy(g_dcr->VolumeName, "TestVolume1|TestVolume2",
           sizeof(g_dcr->VolumeName));
  CreateRestoreVolumeList(g_jcr);
  if (g_jcr->sd_impl->VolList != nullptr) {
    g_jcr->sd_impl->VolList->Slot = 1;
    if (g_jcr->sd_impl->VolList->next != nullptr) {
      g_jcr->sd_impl->VolList->next->Slot = 2;
    }
  }

  SetVolumeName("TestVolume1", 1);

  if (!simple) {
    /* Multiple Volume tape */
    /* Close device so user can use autochanger if desired */
    if (g_dev->HasCap(CAP_OFFLINEUNMOUNT)) { g_dev->offline(); }
    autochanger = AutoloadDevice(g_dcr, 1, nullptr);
    if (autochanger != 1) {
      Pmsg1(100, "Autochanger returned: %d\n", autochanger);
      g_dev->close(g_dcr);
      GetCmd(T_("Mount first tape. Press enter when ready: "));
      Pmsg0(000, "\n");
    }
  }

  g_dev->close(g_dcr);
  g_dev->num_writers = 0;
  g_jcr->sd_impl->dcr->clear_will_write();

  if (!AcquireDeviceForRead(g_dcr)) {
    Pmsg1(-1, "%s", g_dev->errmsg);
    goto bail_out;
  }
  /* We now have the first tape mounted.
   * Note, re-reading last block may have caused us to
   *   loose track of where we are (block number unknown). */
  Pmsg0(-1, T_("Rewinding.\n"));
  if (!g_dev->rewind(g_dcr)) { /* get to a known place on tape */
    goto bail_out;
  }
  /* Read the first 10'000 records */
  Pmsg2(-1, T_("Reading the first 10'000 records from %u:%u.\n"), g_dev->file,
        g_dev->block_num);
  quickie_count = 0;
  ReadRecords(g_dcr, QuickieCb, MyMountNextReadVolume);
  Pmsg4(-1, T_("Reposition from %u:%u to %u:%u\n"), g_dev->file,
        g_dev->block_num, last_file, last_block_num);
  if (!g_dev->Reposition(g_dcr, last_file, last_block_num)) {
    Pmsg1(-1, T_("Reposition error. ERR=%s\n"), g_dev->bstrerror());
    goto bail_out;
  }
  Pmsg1(-1, T_("Reading block %u.\n"), last_block_num);
  if (DeviceControlRecord::ReadStatus::Ok
      != g_dcr->ReadBlockFromDevice(NO_BLOCK_NUMBER_CHECK)) {
    Pmsg1(-1, T_("Error reading block: ERR=%s\n"), g_dev->bstrerror());
    goto bail_out;
  }
  if (CompareBlocks(last_block, block)) {
    if (simple) {
      Pmsg0(-1,
            T_("\nThe last block on the tape matches. Test succeeded.\n\n"));
      rc = true;
    } else {
      Pmsg0(-1, T_("\nThe last block of the first tape matches.\n\n"));
    }
  }
  if (simple) { goto bail_out; }

  /* restore info for last block on second Volume */
  last_block_num = last_block_num2;
  last_file = last_file2;
  last_block = last_block2;

  /* Multiple Volume tape */
  /* Close device so user can use autochanger if desired */
  if (g_dev->HasCap(CAP_OFFLINEUNMOUNT)) { g_dev->offline(); }

  SetVolumeName("TestVolume2", 2);

  autochanger = AutoloadDevice(g_dcr, 1, nullptr);
  if (autochanger != 1) {
    Pmsg1(100, "Autochanger returned: %d\n", autochanger);
    g_dev->close(g_dcr);
    GetCmd(T_("Mount second tape. Press enter when ready: "));
    Pmsg0(000, "\n");
  }

  g_dev->ClearRead();
  if (!AcquireDeviceForRead(g_dcr)) {
    Pmsg1(-1, "%s", g_dev->errmsg);
    goto bail_out;
  }

  /* Space to "first" block which is last block not written
   * on the previous tape.
   */
  Pmsg2(-1, T_("Reposition from %u:%u to 0:1\n"), g_dev->file,
        g_dev->block_num);
  if (!g_dev->Reposition(g_dcr, 0, 1)) {
    Pmsg1(-1, T_("Reposition error. ERR=%s\n"), g_dev->bstrerror());
    goto bail_out;
  }
  Pmsg1(-1, T_("Reading block %d.\n"), g_dev->block_num);
  if (DeviceControlRecord::ReadStatus::Ok
      != g_dcr->ReadBlockFromDevice(NO_BLOCK_NUMBER_CHECK)) {
    Pmsg1(-1, T_("Error reading block: ERR=%s\n"), g_dev->bstrerror());
    goto bail_out;
  }
  if (CompareBlocks(first_block, block)) {
    Pmsg0(-1, T_("\nThe first block on the second tape matches.\n\n"));
  }

  /* Now find and compare the last block */
  Pmsg4(-1, T_("Reposition from %u:%u to %u:%u\n"), g_dev->file,
        g_dev->block_num, last_file, last_block_num);
  if (!g_dev->Reposition(g_dcr, last_file, last_block_num)) {
    Pmsg1(-1, T_("Reposition error. ERR=%s\n"), g_dev->bstrerror());
    goto bail_out;
  }
  Pmsg1(-1, T_("Reading block %d.\n"), g_dev->block_num);
  if (DeviceControlRecord::ReadStatus::Ok
      != g_dcr->ReadBlockFromDevice(NO_BLOCK_NUMBER_CHECK)) {
    Pmsg1(-1, T_("Error reading block: ERR=%s\n"), g_dev->bstrerror());
    goto bail_out;
  }
  if (CompareBlocks(last_block, block)) {
    Pmsg0(-1, T_("\nThe last block on the second tape matches. Test "
                 "succeeded.\n\n"));
    rc = true;
  }

bail_out:
  FreeBlock(last_block1);
  FreeBlock(last_block2);
  FreeBlock(first_block);

  last_block1 = nullptr;
  last_block2 = nullptr;
  last_block = nullptr;
  first_block = nullptr;

  return rc;
}

/* Read 10'000 records then stop */
static bool QuickieCb(DeviceControlRecord* t_dcr, DeviceRecord*)
{
  Device* dev = t_dcr->dev;
  quickie_count++;
  if (quickie_count == 10'000) {
    Pmsg2(-1, T_("10'000 records read now at %d:%d\n"), dev->file,
          dev->block_num);
  }
  return quickie_count < 10'000;
}

static bool CompareBlocks(DeviceBlock* t_last_block, DeviceBlock* block)
{
  char *p, *q;
  union {
    uint32_t CheckSum;
    uint32_t block_len;
  };
  ser_declare;

  p = t_last_block->buf;
  q = block->buf;
  UnserBegin(q, BLKHDR2_LENGTH);
  unser_uint32(CheckSum);
  unser_uint32(block_len);
  while (q < (block->buf + block_len)) {
    if (*p == *q) {
      p++;
      q++;
      continue;
    }
    Pmsg0(-1, "\n");
    DumpBlock(t_last_block, T_("Last block written"));
    Pmsg0(-1, "\n");
    DumpBlock(block, T_("Block read back"));
    Pmsg1(-1, T_("\n\nThe blocks differ at byte %u\n"), p - t_last_block->buf);
    Pmsg0(-1, T_("\n\n!!!! The last block written and the block\n"
                 "that was read back differ. The test FAILED !!!!\n"
                 "This must be corrected before you use Bareos\n"
                 "to write multi-tape Volumes.!!!!\n"));
    return false;
  }
  if (g_verbose) {
    DumpBlock(t_last_block, T_("Last block written"));
    DumpBlock(block, T_("Block read back"));
  }
  return true;
}

/**
 * Write current block to tape regardless of whether or
 *   not it is full. If the tape fills, attempt to
 *   acquire another tape.
 */
static int FlushBlock(DeviceBlock* block)
{
  char ec1[50], ec2[50];
  uint64_t rate;
  DeviceBlock* tblock;
  uint32_t thIsFile, this_block_num;

  g_dev->rLock();
  if (!this_block) { this_block = new_block(g_dev); }
  if (!last_block) { last_block = new_block(g_dev); }
  /* Copy block */
  thIsFile = g_dev->file;
  this_block_num = g_dev->block_num;
  if (!g_dcr->WriteBlockToDev()) {
    Pmsg3(000, T_("Last block at: %u:%u this_dev_block_num=%d\n"), last_file,
          last_block_num, this_block_num);
    if (vol_num == 1) {
      /* This is 1st tape, so save first tape info separate
       *  from second tape info */
      last_block_num1 = last_block_num;
      last_file1 = last_file;
      last_block1 = dup_block(last_block);
      last_block2 = dup_block(last_block);
      first_block = dup_block(block); /* first block second tape */
    }
    if (g_verbose) {
      Pmsg3(000, T_("Block not written: FileIndex=%u blk_block=%u Size=%u\n"),
            (unsigned)file_index, block->BlockNumber, block->block_len);
      DumpBlock(last_block, T_("Last block written"));
      Pmsg0(-1, "\n");
      DumpBlock(block, T_("Block not written"));
    }
    if (stop == 0) {
      eot_block = block->BlockNumber;
      eot_block_len = block->block_len;
      eot_FileIndex = file_index;
      stop = 1;
    }
    now = time(nullptr);
    now -= g_jcr->run_time;
    if (now <= 0) { now = 1; /* don't divide by zero */ }
    rate = g_dev->VolCatInfo.VolCatBytes / now;
    vol_size = g_dev->VolCatInfo.VolCatBytes;
    Pmsg4(000, T_("End of tape %d:%d. Volume Bytes=%s. Write rate = %sB/s\n"),
          g_dev->file, g_dev->block_num,
          edit_uint64_with_commas(g_dev->VolCatInfo.VolCatBytes, ec1),
          edit_uint64_with_suffix(rate, ec2));

    if (simple) {
      stop = -1; /* stop, but do simplified test */
    } else {
      /* Full test in progress */
      if (!FixupDeviceBlockWriteError(g_jcr->sd_impl->dcr)) {
        Pmsg1(000, T_("Cannot fixup device error. %s\n"), g_dev->bstrerror());
        g_ok = false;
        g_dev->Unlock();
        return 0;
      }
      BlockNumber = 0; /* start counting for second tape */
    }
    g_dev->Unlock();
    return 1; /* end of tape reached */
  }

  /* Save contents after write so that the header is serialized */
  memcpy(this_block->buf, block->buf, this_block->buf_len);

  /* Note, we always read/write to block, but we toggle
   *  copying it to one or another of two allocated blocks.
   * Switch blocks so that the block just successfully written is
   *  always in last_block. */
  tblock = last_block;
  last_block = this_block;
  this_block = tblock;
  last_file = thIsFile;
  last_block_num = this_block_num;

  g_dev->Unlock();
  return 1;
}


/**
 * First we label the tape, then we fill
 *  it with data get a new tape and write a few blocks.
 */
static void qfillcmd()
{
  DeviceBlock* block = g_dcr->block;
  DeviceRecord* rec = g_dcr->rec;
  int i, count;

  Pmsg0(0, T_("Test writing blocks of 64512 bytes to tape.\n"));

  GetCmd(T_("How many blocks do you want to write? (1000): "));

  count = atoi(cmd);
  if (count <= 0) { count = 1000; }


  i = block->buf_len - 100;
  ASSERT(i > 0);
  rec->data = CheckPoolMemorySize(rec->data, i);
  memset(rec->data, i & 0xFF, i);
  rec->data_len = i;
  rewindcmd();
  init_speed();

  Pmsg1(0, T_("Begin writing %d Bareos blocks to tape ...\n"), count);
  for (i = 0; i < count; i++) {
    if (i % 100 == 0) {
      printf("+");
      fflush(stdout);
    }
    if (!WriteRecordToBlock(g_dcr, rec)) {
      Pmsg0(0, T_("Error writing record to block.\n"));
      goto bail_out;
    }
    if (!g_dcr->WriteBlockToDev()) {
      Pmsg0(0, T_("Error writing block to device.\n"));
      goto bail_out;
    }
  }
  printf("\n");
  PrintSpeed(g_dev->VolCatInfo.VolCatBytes);
  weofcmd();
  if (g_dev->HasCap(CAP_TWOEOF)) { weofcmd(); }
  rewindcmd();
  scan_blocks();

bail_out:
  return;
}

// Fill a tape using raw write() command
static void rawfill_cmd()
{
  DeviceBlock* block = g_dcr->block;
  int status;
  uint32_t block_num = 0;
  uint32_t* p;
  int my_errno;

  FillBuffer(FILL_RANDOM, block->buf, block->buf_len);
  init_speed();

  p = (uint32_t*)block->buf;
  Pmsg1(0, T_("Begin writing raw blocks of %u bytes.\n"), block->buf_len);
  for (;;) {
    *p = block_num;
    status = g_dev->d_write(g_dev->fd, block->buf, block->buf_len);
    if (status == (int)block->buf_len) {
      if ((block_num++ % 100) == 0) {
        printf("+");
        fflush(stdout);
      }

      MixBuffer(FILL_RANDOM, block->buf, block->buf_len);

      g_jcr->JobBytes += status;
      continue;
    }
    break;
  }
  my_errno = errno;
  printf("\n");
  BErrNo be;
  printf(T_("Write failed at block %u. status=%d ERR=%s\n"), block_num, status,
         be.bstrerror(my_errno));

  PrintSpeed(g_jcr->JobBytes);
  weofcmd();
}


struct cmdstruct {
  const char* key;
  void (*func)();
  const char* help;
};
static struct cmdstruct commands[] = {
    {NT_("autochanger"), autochangercmd, T_("test autochanger")},
    {NT_("bsf"), bsfcmd, T_("backspace file")},
    {NT_("bsr"), bsrcmd, T_("backspace record")},
    {NT_("cap"), capcmd, T_("list device capabilities")},
    {NT_("clear"), clearcmd, T_("clear tape errors")},
    {NT_("eod"), eodcmd, T_("go to end of Bareos data for append")},
    {NT_("eom"), eomcmd, T_("go to the physical end of medium")},
    {NT_("fill"), fillcmd, T_("fill tape, write onto second volume")},
    {NT_("unfill"), unfillcmd, T_("read filled tape")},
    {NT_("fsf"), fsfcmd, T_("forward space a file")},
    {NT_("fsr"), fsrcmd, T_("forward space a record")},
    {NT_("help"), HelpCmd, T_("print this command")},
    {NT_("label"), labelcmd, T_("write a Bareos label to the tape")},
    {NT_("load"), loadcmd, T_("load a tape")},
    {NT_("quit"), QuitCmd, T_("quit btape")},
    {NT_("rawfill"), rawfill_cmd, T_("use write() to fill tape")},
    {NT_("readlabel"), readlabelcmd,
     T_("read and print the Bareos tape label")},
    {NT_("rectest"), rectestcmd, T_("test record handling functions")},
    {NT_("rewind"), rewindcmd, T_("rewind the tape")},
    {NT_("scan"), scancmd, T_("read() tape block by block to EOT and report")},
    {NT_("scanblocks"), scan_blocks,
     T_("Bareos read block by block to EOT and report")},
    {NT_("speed"), speed_test,
     T_("[file_size=n(GB)|nb_file=3|skip_zero|skip_random|skip_raw|skip_block]"
        " "
        "report drive speed")},
    {NT_("status"), statcmd, T_("print tape status")},
    {NT_("test"), testcmd, T_("General test Bareos tape functions")},
    {NT_("weof"), weofcmd, T_("write an EOF on the tape")},
    {NT_("wr"), wrcmd, T_("write a single Bareos block")},
    {NT_("rr"), rrcmd, T_("read a single record")},
    {NT_("rb"), rbcmd, T_("read a single Bareos block")},
    {NT_("qfill"), qfillcmd, T_("quick fill command")}};
#define comsize (sizeof(commands) / sizeof(struct cmdstruct))

static void do_tape_cmds()
{
  unsigned int i;
  bool found;

  while (!quit && GetCmd("*")) {
    found = false;
    ParseArgs(cmd, args, &argc, argk, argv, MAX_CMD_ARGS);
    /* search for command */
    for (i = 0; i < comsize; i++) {
      if (argc > 0 && fstrsch(argk[0], commands[i].key)) {
        /* execute command */
        (*commands[i].func)();
        found = true;
        break;
      }
    }
    if (*cmd && !found) { Pmsg1(0, T_("\"%s\" is an invalid command\n"), cmd); }
  }
}

static std::string Generate_interactive_commands_help()
{
  std::string output{
      "Interactive commands:\n"
      "  Command    Description\n  =======    ===========\n"};
  char tmp[1024];
  for (unsigned int i = 0; i < comsize; i++) {
    sprintf(tmp, "  %-10s %s\n", commands[i].key, commands[i].help);
    output += tmp;
  }

  output += "\n";
  return output;
}

static void HelpCmd()
{
  printf("%s", Generate_interactive_commands_help().c_str());
}

/**
 * Get next input command from terminal.  This
 * routine is REALLY primitive, and should be enhanced
 * to have correct backspacing, etc.
 */
int GetCmd(const char* prompt)
{
  int i = 0;
  int ch;

  fprintf(stdout, "%s", prompt);

  /* We really should turn off echoing and pretty this
   * up a bit.
   */
  cmd[i] = 0;
  while ((ch = fgetc(stdin)) != EOF) {
    if (ch == '\n') {
      StripTrailingJunk(cmd);
      return 1;
    } else if (ch == 4 || ch == 0xd3 || ch == 0x8) {
      if (i > 0) { cmd[--i] = 0; }
      continue;
    }

    cmd[i++] = ch;
    cmd[i] = 0;
  }
  quit = 1;
  return 0;
}

bool BTAPE_DCR::DirCreateJobmediaRecord(bool)
{
  WroteVol = false;
  return 1;
}

bool BTAPE_DCR::DirFindNextAppendableVolume()
{
  Dmsg1(20, "Enter DirFindNextAppendableVolume. stop=%d\n", stop);
  return VolumeName[0] != 0;
}

bool BTAPE_DCR::DirAskSysopToMountVolume(int)
{
  Dmsg0(20, "Enter DirAskSysopToMountVolume\n");
  if (VolumeName[0] == 0) { return DirAskSysopToCreateAppendableVolume(); }
  Pmsg1(-1, "%s", dev->errmsg); /* print reason */

  if (VolumeName[0] == 0 || bstrcmp(VolumeName, "TestVolume2")) {
    fprintf(
        stderr,
        T_("Mount second Volume on device %s and press return when ready: "),
        dev->print_name());
  } else {
    fprintf(
        stderr,
        T_("Mount Volume \"%s\" on device %s and press return when ready: "),
        VolumeName, dev->print_name());
  }

  dev->close(this);
  getchar();

  return true;
}

bool BTAPE_DCR::DirAskSysopToCreateAppendableVolume()
{
  int autochanger;

  Dmsg0(20, "Enter DirAskSysopToCreateAppendableVolume\n");
  if (stop == 0) {
    SetVolumeName("TestVolume1", 1);
  } else {
    SetVolumeName("TestVolume2", 2);
  }
  /* Close device so user can use autochanger if desired */
  if (dev->HasCap(CAP_OFFLINEUNMOUNT)) { dev->offline(); }
  autochanger = AutoloadDevice(this, 1, nullptr);
  if (autochanger != 1) {
    Pmsg1(100, "Autochanger returned: %d\n", autochanger);
    fprintf(stderr,
            T_("Mount blank Volume on device %s and press return when ready: "),
            dev->print_name());
    dev->close(this);
    getchar();
    Pmsg0(000, "\n");
  }
  labelcmd();
  volumename = nullptr;
  BlockNumber = 0;

  return true;
}

DeviceControlRecord* BTAPE_DCR::get_new_spooling_dcr() { return new BTAPE_DCR; }

static bool MyMountNextReadVolume(DeviceControlRecord* t_dcr)
{
  char ec1[50], ec2[50];
  uint64_t rate;
  JobControlRecord* jcr = t_dcr->jcr;
  DeviceBlock* block = t_dcr->block;

  Dmsg0(20, "Enter MyMountNextReadVolume\n");
  Pmsg2(000, T_("End of Volume \"%s\" %d records.\n"), t_dcr->VolumeName,
        quickie_count);

  VolumeUnused(t_dcr); /* release current volume */
  if (LastBlock != block->BlockNumber) { VolBytes += block->block_len; }
  LastBlock = block->BlockNumber;
  now = time(nullptr);
  now -= jcr->run_time;
  if (now <= 0) { now = 1; }
  rate = VolBytes / now;
  Pmsg3(-1, T_("Read block=%u, VolBytes=%s rate=%sB/s\n"), block->BlockNumber,
        edit_uint64_with_commas(VolBytes, ec1),
        edit_uint64_with_suffix(rate, ec2));

  if (bstrcmp(t_dcr->VolumeName, "TestVolume2")) {
    end_of_tape = 1;
    return false;
  }

  SetVolumeName("TestVolume2", 2);

  g_dev->close(t_dcr);
  if (!AcquireDeviceForRead(t_dcr)) {
    Pmsg2(0, T_("Cannot open Dev=%s, Vol=%s\n"), g_dev->print_name(),
          t_dcr->VolumeName);
    return false;
  }
  return true; /* next volume mounted */
}

static void SetVolumeName(const char* VolName, int volnum)
{
  DeviceControlRecord* dcr = g_jcr->sd_impl->dcr;
  volumename = VolName;
  vol_num = volnum;
  g_dev->setVolCatName(VolName);
  dcr->setVolCatName(VolName);
  bstrncpy(dcr->VolumeName, VolName, sizeof(dcr->VolumeName));
  dcr->VolCatInfo.Slot = volnum;
  dcr->VolCatInfo.InChanger = true;
}
