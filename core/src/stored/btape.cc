/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2000-2012 Free Software Foundation Europe e.V.
   Copyright (C) 2011-2012 Planets Communications B.V.
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
 * Kern Sibbald, April MM
 */
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

#include "include/bareos.h"
#include "stored/stored.h"
#include "lib/crypto_cache.h"
#include "stored/acquire.h"
#include "stored/autochanger.h"
#include "stored/bsr.h"
#include "stored/butil.h"
#include "stored/device.h"
#include "stored/label.h"
#include "stored/read_record.h"
#include "stored/sd_backends.h"
#include "lib/edit.h"
#include "lib/bsignal.h"
#include "lib/parse_bsr.h"
#include "include/jcr.h"

namespace storagedaemon {
   extern bool ParseSdConfig(const char *configfile, int exit_code);
}

using namespace storagedaemon;

/* Exported variables */
int quit = 0;
char buf[100000];
int bsize = TAPE_BSIZE;
char VolName[MAX_NAME_LENGTH];

/**
 * If you change the format of the state file,
 *  increment this value
 */
static uint32_t btape_state_level = 2;

Device *dev = NULL;
DeviceControlRecord *dcr;
DeviceResource *device = NULL;
int exit_code = 0;

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
static int FlushBlock(DeviceBlock *block, int dump);
static bool QuickieCb(DeviceControlRecord *dcr, DeviceRecord *rec);
static bool CompareBlocks(DeviceBlock *last_block, DeviceBlock *block);
static bool MyMountNextReadVolume(DeviceControlRecord *dcr);
static void scan_blocks();
static void SetVolumeName(const char *VolName, int volnum);
static void rawfill_cmd();
static bool open_the_device();
static void autochangercmd();
static bool do_unfill();

/* Static variables */

#define MAX_CMD_ARGS 30

static POOLMEM *cmd;
static POOLMEM *args;
static char *argk[MAX_CMD_ARGS];
static char *argv[MAX_CMD_ARGS];
static int argc;

static int quickie_count = 0;
static uint64_t write_count = 0;
static BootStrapRecord *bsr = NULL;
static int signals = TRUE;
static bool ok;
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
static DeviceBlock *last_block1 = NULL;
static DeviceBlock *last_block2 = NULL;
static DeviceBlock *last_block = NULL;
static DeviceBlock *this_block = NULL;
static DeviceBlock *first_block = NULL;
static uint32_t last_file1 = 0;
static uint32_t last_file2 = 0;
static uint32_t last_file = 0;
static uint32_t last_block_num1 = 0;
static uint32_t last_block_num2 = 0;
static uint32_t last_block_num = 0;
static uint32_t BlockNumber = 0;
static bool simple = true;

static const char *volumename = NULL;
static int vol_num = 0;

static JobControlRecord *jcr = NULL;

static void usage();
static void TerminateBtape(int sig);
int GetCmd(const char *prompt);


/**
 *
 * Bareos tape testing program
 *
 */
int main(int margc, char *margv[])
{
   int ch, i;
   uint32_t x32, y32;
   uint64_t x64, y64;
   char buf[1000];
   char *DirectorName = NULL;
   DirectorResource *director = NULL;

   setlocale(LC_ALL, "");
   bindtextdomain("bareos", LOCALEDIR);
   textdomain("bareos");
   InitStackDump();
   LmgrInitThread();

   /* Sanity checks */
   if (TAPE_BSIZE % B_DEV_BSIZE != 0 || TAPE_BSIZE / B_DEV_BSIZE == 0) {
      Emsg2(M_ABORT, 0, _("Tape block size (%d) not multiple of system size (%d)\n"),
         TAPE_BSIZE, B_DEV_BSIZE);
   }
   if (TAPE_BSIZE != (1 << (ffs(TAPE_BSIZE)-1))) {
      Emsg1(M_ABORT, 0, _("Tape block size (%d) is not a power of 2\n"), TAPE_BSIZE);
   }
   if (sizeof(boffset_t) < 8) {
      Pmsg1(-1, _("\n\n!!!! Warning large disk addressing disabled. boffset_t=%d should be 8 or more !!!!!\n\n\n"),
         sizeof(boffset_t));
   }
   x32 = 123456789;
   Bsnprintf(buf, sizeof(buf), "%u", x32);
   i = bsscanf(buf, "%lu", &y32);
   if (i != 1 || x32 != y32) {
      Pmsg3(-1, _("32 bit printf/scanf problem. i=%d x32=%u y32=%u\n"), i, x32, y32);
      exit(1);
   }
   x64 = 123456789;
   x64 = x64 << 32;
   x64 += 123456789;
   Bsnprintf(buf, sizeof(buf), "%" llu, x64);
   i = bsscanf(buf, "%llu", &y64);
   if (i != 1 || x64 != y64) {
      Pmsg3(-1, _("64 bit printf/scanf problem. i=%d x64=%" llu " y64=%" llu "\n"),
            i, x64, y64);
      exit(1);
   }

   printf(_("Tape block granularity is %d bytes.\n"), TAPE_BSIZE);

   working_directory = "/tmp";
   MyNameIs(margc, margv, "btape");
   InitMsg(NULL, NULL);

   OSDependentInit();

   while ((ch = getopt(margc, margv, "b:c:D:d:psv?")) != -1) {
      switch (ch) {
      case 'b':                    /* bootstrap file */
         bsr = libbareos::parse_bsr(NULL, optarg);
//       DumpBsr(bsr, true);
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

      case 'd':                    /* set debug level */
         if (*optarg == 't') {
            dbg_timestamp = true;
         } else {
            debug_level = atoi(optarg);
            if (debug_level <= 0) {
               debug_level = 1;
            }
         }
         break;

      case 'p':
         forge_on = true;
         break;

      case 's':
         signals = false;
         break;

      case 'v':
         verbose++;
         break;

      case '?':
      default:
         HelpCmd();
         exit(0);

      }
   }
   margc -= optind;
   margv += optind;

   cmd = GetPoolMemory(PM_FNAME);
   args = GetPoolMemory(PM_FNAME);

   if (signals) {
      InitSignals(TerminateBtape);
   }

   daemon_start_time = time(NULL);

   my_config = InitSdConfig(configfile, M_ERROR_TERM);
   ParseSdConfig(configfile, M_ERROR_TERM);

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

   LoadSdPlugins(me->plugin_directory, me->plugin_names);

   ReadCryptoCache(me->working_directory, "bareos-sd",
                     GetFirstPortHostOrder(me->SDaddrs));

   /* See if we can open a device */
   if (margc == 0) {
      Pmsg0(000, _("No archive name specified.\n"));
      usage();
      exit(1);
   } else if (margc != 1) {
      Pmsg0(000, _("Improper number of arguments specified.\n"));
      usage();
      exit(1);
   }

   dcr = New(BTAPE_DCR);
   jcr = SetupJcr("btape", margv[0], bsr, director, dcr, NULL, false); /* write device */
   if (!jcr) {
      exit(1);
   }

   dev = jcr->dcr->dev;
   if (!dev) {
      exit(1);
   }

   if (!dev->IsTape()) {
      Pmsg0(000, _("btape only works with tape storage.\n"));
      usage();
      exit(1);
   }

   if (!open_the_device()) {
      exit(1);
   }

   Dmsg0(200, "Do tape commands\n");
   do_tape_cmds();

   TerminateBtape(exit_code);
}

static void TerminateBtape(int status)
{
   Dsm_check(200);
   FreeJcr(jcr);
   jcr = NULL;

   if (args) {
      FreePoolMemory(args);
      args = NULL;
   }

   if (cmd) {
      FreePoolMemory(cmd);
      cmd = NULL;
   }

   if (bsr) {
      libbareos::FreeBsr(bsr);
   }

   FreeVolumeLists();

   if (dev) {
      dev->term();
   }

#if defined(HAVE_DYNAMIC_SD_BACKENDS)
    DevFlushBackends();
#endif

   if (configfile) {
      free(configfile);
   }

   if (my_config) {
      delete my_config;
      my_config = NULL;
   }

   if (debug_level > 10) {
      PrintMemoryPoolStats();
   }

   if (this_block) {
      FreeBlock(this_block);
      this_block = NULL;
   }

   StopWatchdog();
   TermMsg();
   TermLastJobsList();
   CloseMemoryPool();               /* free memory in pool */
   LmgrCleanupMain();

   sm_dump(false, false);
   exit(status);
}


btime_t total_time=0;
uint64_t total_size=0;

static void init_total_speed()
{
   total_size = 0;
   total_time = 0;
}

static void print_total_speed()
{
   char ec1[50], ec2[50];
   uint64_t rate = total_size / total_time;
   Pmsg2(000, _("Total Volume bytes=%sB. Total Write rate = %sB/s\n"),
         edit_uint64_with_suffix(total_size, ec1),
         edit_uint64_with_suffix(rate, ec2));
}

static void init_speed()
{
   time(&jcr->run_time);              /* start counting time for rates */
   jcr->JobBytes=0;
}

static void PrintSpeed(uint64_t bytes)
{
   char ec1[50], ec2[50];
   uint64_t rate;

   now = time(NULL);
   now -= jcr->run_time;
   if (now <= 0) {
      now = 1;                     /* don't divide by zero */
   }

   total_time += now;
   total_size += bytes;

   rate = bytes / now;
   Pmsg2(000, _("Volume bytes=%sB. Write rate = %sB/s\n"),
         edit_uint64_with_suffix(bytes, ec1),
         edit_uint64_with_suffix(rate, ec2));
}

/**
 * Helper that fill a buffer with random data or not
 */
typedef enum {
   FILL_RANDOM,
   FILL_ZERO
} fill_mode_t;

static void FillBuffer(fill_mode_t mode, char *buf, uint32_t len)
{
   int fd;
   switch (mode) {
   case FILL_RANDOM:
      fd = open("/dev/urandom", O_RDONLY);
      if (fd != -1) {
         read(fd, buf, len);
         close(fd);
      } else {
         uint32_t *p = (uint32_t *)buf;
         srandom(time(NULL));
         for (uint32_t i=0; i<len/sizeof(uint32_t); i++) {
            p[i] = random();
         }
      }
      break;

   case FILL_ZERO:
      memset(buf, 0xFF, len);
      break;

   default:
      ASSERT(0);
   }
}

static void MixBuffer(fill_mode_t mode, char *data, uint32_t len)
{
   uint32_t i;
   uint32_t *lp = (uint32_t *)data;

   if (mode == FILL_ZERO) {
      return;
   }

   lp[0] += lp[13];
   for (i=1; i < (len-sizeof(uint32_t))/sizeof(uint32_t)-1; i+=100) {
      lp[i] += lp[0];
   }
}

static bool open_the_device()
{
   DeviceBlock *block;
   bool ok = true;

   block = new_block(dev);
   dev->rLock();
   Dmsg1(200, "Opening device %s\n", dcr->VolumeName);
   if (!dev->open(dcr, OPEN_READ_WRITE)) {
      Emsg1(M_FATAL, 0, _("dev open failed: %s\n"), dev->errmsg);
      ok = false;
      goto bail_out;
   }
   Pmsg1(000, _("open device %s: OK\n"), dev->print_name());
   dev->SetAppend();                 /* put volume in append mode */

bail_out:
   dev->Unlock();
   FreeBlock(block);
   return ok;
}


void QuitCmd()
{
   quit = 1;
}

/**
 * Write a label to the tape
 */
static void labelcmd()
{
   if (volumename) {
      PmStrcpy(cmd, volumename);
   } else {
      if (!GetCmd(_("Enter Volume Name: "))) {
         return;
      }
   }

   if (!dev->IsOpen()) {
      if (!FirstOpenDevice(dcr)) {
         Pmsg1(0, _("Device open failed. ERR=%s\n"), dev->bstrerror());
      }
   }
   dev->rewind(dcr);
   WriteNewVolumeLabelToDev(dcr, cmd, "Default", false /*no relabel*/);
   Pmsg1(-1, _("Wrote Volume label for volume \"%s\".\n"), cmd);
}

/**
 * Read the tape label
 */
static void readlabelcmd()
{
   int save_debug_level = debug_level;
   int status;

   status = ReadDevVolumeLabel(dcr);
   switch (status) {
   case VOL_NO_LABEL:
      Pmsg0(0, _("Volume has no label.\n"));
      break;
   case VOL_OK:
      Pmsg0(0, _("Volume label read correctly.\n"));
      break;
   case VOL_IO_ERROR:
      Pmsg1(0, _("I/O error on device: ERR=%s"), dev->bstrerror());
      break;
   case VOL_NAME_ERROR:
      Pmsg0(0, _("Volume name error\n"));
      break;
   case VOL_CREATE_ERROR:
      Pmsg1(0, _("Error creating label. ERR=%s"), dev->bstrerror());
      break;
   case VOL_VERSION_ERROR:
      Pmsg0(0, _("Volume version error.\n"));
      break;
   case VOL_LABEL_ERROR:
      Pmsg0(0, _("Bad Volume label type.\n"));
      break;
   default:
      Pmsg0(0, _("Unknown error.\n"));
      break;
   }

   debug_level = 20;
   DumpVolumeLabel(dev);
   debug_level = save_debug_level;
}


/**
 * Load the tape should have prevously been taken
 * off line, otherwise this command is not necessary.
 */
static void loadcmd()
{

   if (!dev->LoadDev()) {
      Pmsg1(0, _("Bad status from load. ERR=%s\n"), dev->bstrerror());
   } else
      Pmsg1(0, _("Loaded %s\n"), dev->print_name());
}

/**
 * Rewind the tape.
 */
static void rewindcmd()
{
   if (!dev->rewind(dcr)) {
      Pmsg1(0, _("Bad status from rewind. ERR=%s\n"), dev->bstrerror());
      dev->clrerror(-1);
   } else {
      Pmsg1(0, _("Rewound %s\n"), dev->print_name());
   }
}

/**
 * Clear any tape error
 */
static void clearcmd()
{
   dev->clrerror(-1);
}

/**
 * Write and end of file on the tape
 */
static void weofcmd()
{
   int num = 1;
   if (argc > 1) {
      num = atoi(argk[1]);
   }
   if (num <= 0) {
      num = 1;
   }

   if (!dev->weof(num)) {
      Pmsg1(0, _("Bad status from weof. ERR=%s\n"), dev->bstrerror());
      return;
   } else {
      if (num==1) {
         Pmsg1(0, _("Wrote 1 EOF to %s\n"), dev->print_name());
      }
      else {
         Pmsg2(0, _("Wrote %d EOFs to %s\n"), num, dev->print_name());
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
   if (!dev->eod(dcr)) {
      Pmsg1(0, "%s", dev->bstrerror());
      return;
   } else {
      Pmsg0(0, _("Moved to end of medium.\n"));
   }
}

/**
 * Go to the end of the medium (either hardware determined
 *  or defined by two eofs.
 */
static void eodcmd()
{
   eomcmd();
}

/**
 * Backspace file
 */
static void bsfcmd()
{
   int num = 1;
   if (argc > 1) {
      num = atoi(argk[1]);
   }
   if (num <= 0) {
      num = 1;
   }

   if (!dev->bsf(num)) {
      Pmsg1(0, _("Bad status from bsf. ERR=%s\n"), dev->bstrerror());
   } else {
      Pmsg2(0, _("Backspaced %d file%s.\n"), num, num==1?"":"s");
   }
}

/**
 * Backspace record
 */
static void bsrcmd()
{
   int num = 1;
   if (argc > 1) {
      num = atoi(argk[1]);
   }
   if (num <= 0) {
      num = 1;
   }
   if (!dev->bsr(num)) {
      Pmsg1(0, _("Bad status from bsr. ERR=%s\n"), dev->bstrerror());
   } else {
      Pmsg2(0, _("Backspaced %d record%s.\n"), num, num==1?"":"s");
   }
}

/**
 * List device capabilities as defined in the stored.conf file.
 */
static void capcmd()
{
   printf(_("Configured device capabilities:\n"));
   printf("%sEOF ", dev->HasCap(CAP_EOF) ? "" : "!");
   printf("%sBSR ", dev->HasCap(CAP_BSR) ? "" : "!");
   printf("%sBSF ", dev->HasCap(CAP_BSF) ? "" : "!");
   printf("%sFSR ", dev->HasCap(CAP_FSR) ? "" : "!");
   printf("%sFSF ", dev->HasCap(CAP_FSF) ? "" : "!");
   printf("%sFASTFSF ", dev->HasCap(CAP_FASTFSF) ? "" : "!");
   printf("%sBSFATEOM ", dev->HasCap(CAP_BSFATEOM) ? "" : "!");
   printf("%sEOM ", dev->HasCap(CAP_EOM) ? "" : "!");
   printf("%sREM ", dev->HasCap(CAP_REM) ? "" : "!");
   printf("%sRACCESS ", dev->HasCap(CAP_RACCESS) ? "" : "!");
   printf("%sAUTOMOUNT ", dev->HasCap(CAP_AUTOMOUNT) ? "" : "!");
   printf("%sLABEL ", dev->HasCap(CAP_LABEL) ? "" : "!");
   printf("%sANONVOLS ", dev->HasCap(CAP_ANONVOLS) ? "" : "!");
   printf("%sALWAYSOPEN ", dev->HasCap(CAP_ALWAYSOPEN) ? "" : "!");
   printf("%sMTIOCGET ", dev->HasCap(CAP_MTIOCGET) ? "" : "!");
   printf("\n");

   printf(_("Device status:\n"));
   printf("%sOPENED ", dev->IsOpen() ? "" : "!");
   printf("%sTAPE ", dev->IsTape() ? "" : "!");
   printf("%sLABEL ", dev->IsLabeled() ? "" : "!");
   printf("%sMALLOC ", BitIsSet(ST_MALLOC, dev->state) ? "" : "!");
   printf("%sAPPEND ", dev->CanAppend() ? "" : "!");
   printf("%sREAD ", dev->CanRead() ? "" : "!");
   printf("%sEOT ", dev->AtEot() ? "" : "!");
   printf("%sWEOT ", BitIsSet(ST_WEOT, dev->state) ? "" : "!");
   printf("%sEOF ", dev->AtEof() ? "" : "!");
   printf("%sNEXTVOL ", BitIsSet(ST_NEXTVOL, dev->state) ? "" : "!");
   printf("%sSHORT ", BitIsSet(ST_SHORT, dev->state) ? "" : "!");
   printf("\n");

   printf(_("Device parameters:\n"));
   printf("Device name: %s\n", dev->dev_name);
   printf("File=%u block=%u\n", dev->file, dev->block_num);
   printf("Min block=%u Max block=%u\n", dev->min_block_size, dev->max_block_size);

   printf(_("Status:\n"));
   statcmd();

}

/**
 * Test writing larger and larger records.
 * This is a torture test for records.
 */
static void rectestcmd()
{
   DeviceBlock *save_block;
   DeviceRecord *rec;
   int i, blkno = 0;

   Pmsg0(0, _("Test writing larger and larger records.\n"
"This is a torture test for records.\nI am going to write\n"
"larger and larger records. It will stop when the record size\n"
"plus the header exceeds the block size (by default about 64K)\n"));


   GetCmd(_("Do you want to continue? (y/n): "));
   if (cmd[0] != 'y') {
      Pmsg0(000, _("Command aborted.\n"));
      return;
   }

   Dsm_check(200);
   save_block = dcr->block;
   dcr->block = new_block(dev);
   rec = new_record();

   for (i=1; i<500000; i++) {
      rec->data = CheckPoolMemorySize(rec->data, i);
      memset(rec->data, i & 0xFF, i);
      rec->data_len = i;
      Dsm_check(200);
      if (WriteRecordToBlock(dcr, rec)) {
         EmptyBlock(dcr->block);
         blkno++;
         Pmsg2(0, _("Block %d i=%d\n"), blkno, i);
      } else {
         break;
      }
      Dsm_check(200);
   }
   FreeRecord(rec);
   FreeBlock(dcr->block);
   dcr->block = save_block;     /* restore block to dcr */
   Dsm_check(200);
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
   DeviceBlock *block = dcr->block;
   DeviceRecord *rec;
   bool rc = false;
   int len;

   if (!dev->HasCap(CAP_BSR)) {
      Pmsg0(-1, _("Skipping read backwards test because BootStrapRecord turned off.\n"));
      return true;
   }

   Pmsg0(-1, _("\n=== Write, backup, and re-read test ===\n\n"
      "I'm going to write three records and an EOF\n"
      "then backup over the EOF and re-read the last record.\n"
      "Bareos does this after writing the last block on the\n"
      "tape to verify that the block was written correctly.\n\n"
      "This is not an *essential* feature ...\n\n"));
   rewindcmd();
   EmptyBlock(block);
   rec = new_record();
   rec->data = CheckPoolMemorySize(rec->data, block->buf_len);
   len = rec->data_len = block->buf_len-100;
   memset(rec->data, 1, rec->data_len);
   if (!WriteRecordToBlock(dcr, rec)) {
      Pmsg0(0, _("Error writing record to block.\n"));
      goto bail_out;
   }
   if (!dcr->WriteBlockToDev()) {
      Pmsg0(0, _("Error writing block to device.\n"));
      goto bail_out;
   } else {
      Pmsg1(0, _("Wrote first record of %d bytes.\n"), rec->data_len);
   }
   memset(rec->data, 2, rec->data_len);
   if (!WriteRecordToBlock(dcr, rec)) {
      Pmsg0(0, _("Error writing record to block.\n"));
      goto bail_out;
   }
   if (!dcr->WriteBlockToDev()) {
      Pmsg0(0, _("Error writing block to device.\n"));
      goto bail_out;
   } else {
      Pmsg1(0, _("Wrote second record of %d bytes.\n"), rec->data_len);
   }
   memset(rec->data, 3, rec->data_len);
   if (!WriteRecordToBlock(dcr, rec)) {
      Pmsg0(0, _("Error writing record to block.\n"));
      goto bail_out;
   }
   if (!dcr->WriteBlockToDev()) {
      Pmsg0(0, _("Error writing block to device.\n"));
      goto bail_out;
   } else {
      Pmsg1(0, _("Wrote third record of %d bytes.\n"), rec->data_len);
   }
   weofcmd();
   if (dev->HasCap(CAP_TWOEOF)) {
      weofcmd();
   }
   if (!dev->bsf(1)) {
      Pmsg1(0, _("Backspace file failed! ERR=%s\n"), dev->bstrerror());
      goto bail_out;
   }
   if (dev->HasCap(CAP_TWOEOF)) {
      if (!dev->bsf(1)) {
         Pmsg1(0, _("Backspace file failed! ERR=%s\n"), dev->bstrerror());
         goto bail_out;
      }
   }
   Pmsg0(0, _("Backspaced over EOF OK.\n"));
   if (!dev->bsr(1)) {
      Pmsg1(0, _("Backspace record failed! ERR=%s\n"), dev->bstrerror());
      goto bail_out;
   }
   Pmsg0(0, _("Backspace record OK.\n"));
   if (!dcr->ReadBlockFromDev(NO_BLOCK_NUMBER_CHECK)) {
      BErrNo be;
      Pmsg1(0, _("Read block failed! ERR=%s\n"), be.bstrerror(dev->dev_errno));
      goto bail_out;
   }
   memset(rec->data, 0, rec->data_len);
   if (!ReadRecordFromBlock(dcr, rec)) {
      BErrNo be;
      Pmsg1(0, _("Read block failed! ERR=%s\n"), be.bstrerror(dev->dev_errno));
      goto bail_out;
   }
   for (int i=0; i<len; i++) {
      if (rec->data[i] != 3) {
         Pmsg0(0, _("Bad data in record. Test failed!\n"));
         goto bail_out;
      }
   }
   Pmsg0(0, _("\nBlock re-read correct. Test succeeded!\n"));
   Pmsg0(-1, _("=== End Write, backup, and re-read test ===\n\n"));

   rc = true;

bail_out:
   FreeRecord(rec);
   if (!rc) {
      Pmsg0(0, _("This is not terribly serious since Bareos only uses\n"
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
   DeviceBlock *block = dcr->block;
   int status;
   uint32_t block_num = 0;
   int my_errno;
   char ed1[200];
   nb_gb *= 1024*1024*1024;      /* convert size from nb to GB */

   init_total_speed();
   FillBuffer(mode, block->buf, block->buf_len);

   Pmsg3(0, _("Begin writing %i files of %sB with raw blocks of %u bytes.\n"),
         nb, edit_uint64_with_suffix(nb_gb, ed1), block->buf_len);

   for (uint32_t j=0; j<nb; j++) {
      init_speed();
      for ( ;jcr->JobBytes < nb_gb; ) {
         status = dev->d_write(dev->fd(), block->buf, block->buf_len);
         if (status == (int)block->buf_len) {
            if ((block_num++ % 500) == 0) {
               printf("+");
               fflush(stdout);
            }

            MixBuffer(mode, block->buf, block->buf_len);

            jcr->JobBytes += status;

         } else {
            my_errno = errno;
            printf("\n");
            BErrNo be;
            printf(_("Write failed at block %u. status=%d ERR=%s\n"), block_num,
                   status, be.bstrerror(my_errno));
            return false;
         }
      }
      printf("\n");
      weofcmd();
      PrintSpeed(jcr->JobBytes);
   }
   print_total_speed();
   printf("\n");
   return true;
}


static bool SpeedTestBareos(fill_mode_t mode, uint64_t nb_gb, uint32_t nb)
{
   DeviceBlock *block = dcr->block;
   char ed1[200];
   DeviceRecord *rec;
   uint64_t last_bytes = dev->VolCatInfo.VolCatBytes;
   uint64_t written=0;

   nb_gb *= 1024*1024*1024;      /* convert size from nb to GB */

   init_total_speed();

   EmptyBlock(block);
   rec = new_record();
   rec->data = CheckPoolMemorySize(rec->data, block->buf_len);
   rec->data_len = block->buf_len-100;

   FillBuffer(mode, rec->data, rec->data_len);

   Pmsg3(0, _("Begin writing %i files of %sB with blocks of %u bytes.\n"),
         nb, edit_uint64_with_suffix(nb_gb, ed1), block->buf_len);

   for (uint32_t j=0; j<nb; j++) {
      written = 0;
      init_speed();
      for ( ; written < nb_gb; ) {

         if (!WriteRecordToBlock(dcr, rec)) {
            Pmsg0(0, _("\nError writing record to block.\n"));
            goto bail_out;
         }
         if (!dcr->WriteBlockToDev()) {
            Pmsg0(0, _("\nError writing block to device.\n"));
            goto bail_out;
         }

         if ((block->BlockNumber % 500) == 0) {
            printf("+");
            fflush(stdout);
         }
         written += dev->VolCatInfo.VolCatBytes - last_bytes;
         last_bytes = dev->VolCatInfo.VolCatBytes;
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
static int BtapeFindArg(const char *keyword)
{
   for (int i=1; i<argc; i++) {
      if (Bstrcasecmp(keyword, argk[i])) {
         return i;
      }
   }
   return -1;
}

#define ok(a)    if (!(a)) return

/**
 * For file (/dev/zero, /dev/urandom, normal?)
 *    use raw mode to write a suite of 3 files of 1, 2, 4, 8 GB
 *    use qfill mode to write the same
 *
 */
static void speed_test()
{
   bool do_zero=true, do_random=true, do_block=true, do_raw=true;
   uint32_t file_size=0, nb_file=3;
   int32_t i;

   i = BtapeFindArg("file_size");
   if (i > 0) {
      file_size = atoi(argv[i]);
      if (file_size > 100) {
         Pmsg0(0, _("The file_size is too big, stop this test with Ctrl-c.\n"));
      }
   }

   i = BtapeFindArg("nb_file");
   if (i > 0) {
      nb_file = atoi(argv[i]);
   }

   if (BtapeFindArg("skip_zero") > 0) {
      do_zero = false;
   }

   if (BtapeFindArg("skip_random") > 0) {
      do_random = false;
   }

   if (BtapeFindArg("skip_raw") > 0) {
      do_raw = false;
   }

   if (BtapeFindArg("skip_block") > 0) {
      do_block = false;
   }

   if (do_raw) {
      dev->rewind(dcr);
      if (do_zero) {
         Pmsg0(0, _("Test with zero data, should give the "
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
         Pmsg0(0, _("Test with random data, should give the minimum "
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
      dev->rewind(dcr);
      if (do_zero) {
         Pmsg0(0, _("Test with zero data and bareos block structure.\n"));
         if (file_size) {
            ok(SpeedTestBareos(FILL_ZERO, file_size, nb_file));
         } else {
            ok(SpeedTestBareos(FILL_ZERO, 1, nb_file));
            ok(SpeedTestBareos(FILL_ZERO, 2, nb_file));
               ok(SpeedTestBareos(FILL_ZERO, 4, nb_file));
         }
      }

      if (do_random) {
         Pmsg0(0, _("Test with random data, should give the minimum "
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

const uint64_t num_recs = 10000LL;

static bool write_two_files()
{
   DeviceBlock *block;
   DeviceRecord *rec;
   uint32_t len;
   uint32_t *p;
   bool rc = false;       /* bad return code */
   Device *dev = dcr->dev;

   /*
    * Set big max_file_size so that write_record_to_block
    * doesn't insert any additional EOF marks
    */
   if (dev->max_block_size) {
      dev->max_file_size = 2LL * num_recs * (uint64_t)dev->max_block_size;
   } else {
      dev->max_file_size = 2LL * num_recs * (uint64_t)DEFAULT_BLOCK_SIZE;
   }
   Dmsg1(100, "max_file_size was set to %lld\n", dev->max_file_size);

   Pmsg2(-1, _("\n=== Write, rewind, and re-read test ===\n\n"
      "I'm going to write %d records and an EOF\n"
      "then write %d records and an EOF, then rewind,\n"
      "and re-read the data to verify that it is correct.\n\n"
      "This is an *essential* feature ...\n\n"), num_recs, num_recs);

   block = dcr->block;
   EmptyBlock(block);
   rec = new_record();
   rec->data = CheckPoolMemorySize(rec->data, block->buf_len);
   rec->data_len = block->buf_len-100;
   len = rec->data_len / sizeof(uint32_t);

   if (!dev->rewind(dcr)) {
      Pmsg1(0, _("Bad status from rewind. ERR=%s\n"), dev->bstrerror());
      goto bail_out;
   }

   for (uint32_t i = 1; i <= num_recs; i++) {
      p = (uint32_t *)rec->data;
      for (uint32_t j = 0; j < len; j++) {
         *p++ = i;
      }
      if (!WriteRecordToBlock(dcr, rec)) {
         Pmsg0(0, _("Error writing record to block.\n"));
         goto bail_out;
      }
      if (!dcr->WriteBlockToDev()) {
         Pmsg0(0, _("Error writing block to device.\n"));
         goto bail_out;
      }
   }
   Pmsg2(0, _("Wrote %d blocks of %d bytes.\n"), num_recs, rec->data_len);
   weofcmd();
   for (uint32_t i = num_recs + 1; i <= 2 * num_recs; i++) {
      p = (uint32_t *)rec->data;
      for (uint32_t j = 0; j < len; j++) {
         *p++ = i;
      }
      if (!WriteRecordToBlock(dcr, rec)) {
         Pmsg0(0, _("Error writing record to block.\n"));
         goto bail_out;
      }
      if (!dcr->WriteBlockToDev()) {
         Pmsg0(0, _("Error writing block to device.\n"));
         goto bail_out;
      }
   }
   Pmsg2(0, _("Wrote %d blocks of %d bytes.\n"), num_recs, rec->data_len);
   weofcmd();
   if (dev->HasCap(CAP_TWOEOF)) {
      weofcmd();
   }
   rc = true;

bail_out:
   FreeRecord(rec);
   if (!rc) {
      exit_code = 1;
   }

   return rc;
}

/**
 * This test writes Bareos blocks to the tape in
 * several files. It then rewinds the tape and attepts
 * to read these blocks back checking the data.
 */
static bool write_read_test()
{
   DeviceBlock *block;
   DeviceRecord *rec;
   bool rc = false;
   uint32_t len;
   uint32_t *p;

   rec = new_record();

   if (!write_two_files()) {
      goto bail_out;
   }

   block = dcr->block;
   EmptyBlock(block);

   if (!dev->rewind(dcr)) {
      Pmsg1(0, _("Bad status from rewind. ERR=%s\n"), dev->bstrerror());
      goto bail_out;
   } else {
      Pmsg0(0, _("Rewind OK.\n"));
   }

   rec->data = CheckPoolMemorySize(rec->data, block->buf_len);
   rec->data_len = block->buf_len - 100;
   len = rec->data_len / sizeof(uint32_t);

   /*
    * Now read it back
    */
   for (uint32_t i = 1; i <= 2 * num_recs; i++) {
read_again:
      if (!dcr->ReadBlockFromDev(NO_BLOCK_NUMBER_CHECK)) {
         BErrNo be;
         if (dev->AtEof()) {
            Pmsg0(-1, _("Got EOF on tape.\n"));
            if (i == num_recs+1) {
               goto read_again;
            }
         }
         Pmsg2(0, _("Read block %d failed! ERR=%s\n"), i, be.bstrerror(dev->dev_errno));
         goto bail_out;
      }
      memset(rec->data, 0, rec->data_len);
      if (!ReadRecordFromBlock(dcr, rec)) {
         BErrNo be;
         Pmsg2(0, _("Read record failed. Block %d! ERR=%s\n"), i, be.bstrerror(dev->dev_errno));
         goto bail_out;
      }
      p = (uint32_t *)rec->data;
      for (uint32_t j = 0; j < len; j++) {
         if (*p != i) {
            Pmsg3(0, _("Bad data in record. Expected %d, got %d at byte %d. Test failed!\n"), i, *p, j);
            goto bail_out;
         }
         p++;
      }
      if (i == num_recs || i == 2 * num_recs) {
         Pmsg1(-1, _("%d blocks re-read correctly.\n"), num_recs);
      }
   }
   Pmsg0(-1, _("=== Test Succeeded. End Write, rewind, and re-read test ===\n\n"));
   rc = true;

bail_out:
   FreeRecord(rec);
   if (!rc) {
      exit_code = 1;
   }
   return rc;
}

/**
 * This test writes Bareos blocks to the tape in
 * several files. It then rewinds the tape and attepts
 * to read these blocks back checking the data.
 */
static bool position_test()
{
   DeviceBlock *block = dcr->block;
   DeviceRecord *rec;
   bool rc = false;
   int len, j;
   bool more = true;
   int recno = 0;
   int file = 0, blk = 0;
   int *p;
   bool got_eof = false;

   Pmsg0(0, _("Block position test\n"));
   block = dcr->block;
   EmptyBlock(block);
   rec = new_record();
   rec->data = CheckPoolMemorySize(rec->data, block->buf_len);
   rec->data_len = block->buf_len-100;
   len = rec->data_len/sizeof(j);

   if (!dev->rewind(dcr)) {
      Pmsg1(0, _("Bad status from rewind. ERR=%s\n"), dev->bstrerror());
      goto bail_out;
   } else {
      Pmsg0(0, _("Rewind OK.\n"));
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
         blk = num_recs-1;
         break;
      case num_recs:
         recno = num_recs+1;
         file = 1;
         blk = 0;
         break;
      case num_recs+1:
         recno = num_recs+601;
         file = 1;
         blk = 600;
         break;
      case num_recs+601:
         recno = 2*num_recs;
         file = 1;
         blk = num_recs-1;
         break;
      case 2*num_recs:
         more = false;
         continue;
      }
      Pmsg2(-1, _("Reposition to file:block %d:%d\n"), file, blk);
      if (!dev->Reposition(dcr, file, blk)) {
         Pmsg0(0, _("Reposition error.\n"));
         goto bail_out;
      }
read_again:
      if (!dcr->ReadBlockFromDev(NO_BLOCK_NUMBER_CHECK)) {
         BErrNo be;
         if (dev->AtEof()) {
            Pmsg0(-1, _("Got EOF on tape.\n"));
            if (!got_eof) {
               got_eof = true;
               goto read_again;
            }
         }
         Pmsg4(0, _("Read block %d failed! file=%d blk=%d. ERR=%s\n\n"),
            recno, file, blk, be.bstrerror(dev->dev_errno));
         Pmsg0(0, _("This may be because the tape drive block size is not\n"
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
      if (!ReadRecordFromBlock(dcr, rec)) {
         BErrNo be;
         Pmsg1(0, _("Read record failed! ERR=%s\n"), be.bstrerror(dev->dev_errno));
         goto bail_out;
      }
      p = (int *)rec->data;
      for (j=0; j<len; j++) {
         if (p[j] != recno) {
            Pmsg3(0, _("Bad data in record. Expected %d, got %d at byte %d. Test failed!\n"),
               recno, p[j], j);
            goto bail_out;
         }
      }
      Pmsg1(-1, _("Block %d re-read correctly.\n"), recno);
   }
   Pmsg0(-1, _("=== Test Succeeded. End Write, rewind, and re-read test ===\n\n"));
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
   Pmsg0(-1, _("\n\n=== Append files test ===\n\n"
               "This test is essential to Bareos.\n\n"
"I'm going to write one record  in file 0,\n"
"                   two records in file 1,\n"
"             and three records in file 2\n\n"));
   argc = 1;
   rewindcmd();
   wrcmd();
   weofcmd();      /* end file 0 */
   wrcmd();
   wrcmd();
   weofcmd();      /* end file 1 */
   wrcmd();
   wrcmd();
   wrcmd();
   weofcmd();     /* end file 2 */
   if (dev->HasCap(CAP_TWOEOF)) {
      weofcmd();
   }
   dev->close(dcr);           /* release device */
   if (!open_the_device()) {
      return -1;
   }
   rewindcmd();
   Pmsg0(0, _("Now moving to end of medium.\n"));
   eodcmd();
   Pmsg2(-1, _("We should be in file 3. I am at file %d. %s\n"),
      dev->file, dev->file == 3 ? _("This is correct!") : _("This is NOT correct!!!!"));

   if (dev->file != 3) {
      return -1;
   }

   Pmsg0(-1, _("\nNow the important part, I am going to attempt to append to the tape.\n\n"));
   wrcmd();
   weofcmd();
   if (dev->HasCap(CAP_TWOEOF)) {
      weofcmd();
   }
   rewindcmd();
   Pmsg0(-1, _("Done appending, there should be no I/O errors\n\n"));
   Pmsg0(-1, _("Doing Bareos scan of blocks:\n"));
   scan_blocks();
   Pmsg0(-1, _("End scanning the tape.\n"));
   Pmsg2(-1, _("We should be in file 4. I am at file %d. %s\n"),
      dev->file, dev->file == 4 ? _("This is correct!") : _("This is NOT correct!!!!"));

   if (dev->file != 4) {
      return -2;
   }
   return 1;
}


/**
 * This test exercises the autochanger
 */
static int autochanger_test()
{
   POOLMEM *results, *changer;
   slot_number_t slot, loaded;
   int status;
   int timeout = dcr->device->max_changer_wait;
   int sleep_time = 0;

   Dmsg1(100, "Max changer wait = %d sec\n", timeout);
   if (!dev->HasCap(CAP_AUTOCHANGER)) {
      return 1;
   }
   if (!(dcr->device && dcr->device->changer_name && dcr->device->changer_command)) {
      Pmsg0(-1, _("\nAutochanger enabled, but no name or no command device specified.\n"));
      return 1;
   }

   Pmsg0(-1, _("\nAh, I see you have an autochanger configured.\n"
             "To test the autochanger you must have a blank tape\n"
             " that I can write on in Slot 1.\n"));
   if (!GetCmd(_("\nDo you wish to continue with the Autochanger test? (y/n): "))) {
      return 0;
   }
   if (cmd[0] != 'y' && cmd[0] != 'Y') {
      return 0;
   }

   Pmsg0(-1, _("\n\n=== Autochanger test ===\n\n"));

   results = GetPoolMemory(PM_MESSAGE);
   changer = GetPoolMemory(PM_FNAME);

try_again:
   slot = 1;
   dcr->VolCatInfo.Slot = slot;
   /* Find out what is loaded, zero means device is unloaded */
   Pmsg0(-1, _("3301 Issuing autochanger \"loaded\" command.\n"));
   changer = edit_device_codes(dcr, changer,
                dcr->device->changer_command, "loaded");
   status = RunProgram(changer, timeout, results);
   Dmsg3(100, "run_prog: %s stat=%d result=\"%s\"\n", changer, status, results);
   if (status == 0) {
      loaded = atoi(results);
   } else {
      BErrNo be;
      Pmsg1(-1, _("3991 Bad autochanger command: %s\n"), changer);
      Pmsg2(-1, _("3991 result=\"%s\": ERR=%s\n"), results, be.bstrerror(status));
      goto bail_out;
   }
   if (loaded) {
      Pmsg1(-1, _("Slot %d loaded. I am going to unload it.\n"), loaded);
   } else {
      Pmsg0(-1, _("Nothing loaded in the drive. OK.\n"));
   }
   Dmsg1(100, "Results from loaded query=%s\n", results);
   if (loaded) {
      dcr->VolCatInfo.Slot = loaded;
      /* We are going to load a new tape, so close the device */
      dev->close(dcr);
      Pmsg2(-1, _("3302 Issuing autochanger \"unload %d %d\" command.\n"),
            loaded, dev->drive);
      changer = edit_device_codes(dcr, changer,
                                  dcr->device->changer_command, "unload");
      status = RunProgram(changer, timeout, results);
      Pmsg2(-1, _("unload status=%s %d\n"), status==0?_("OK"):_("Bad"), status);
      if (status != 0) {
         BErrNo be;
         Pmsg1(-1, _("3992 Bad autochanger command: %s\n"), changer);
         Pmsg2(-1, _("3992 result=\"%s\": ERR=%s\n"), results, be.bstrerror(status));
      }
   }

   /*
    * Load the Slot 1
    */

   slot = 1;
   dcr->VolCatInfo.Slot = slot;
   Pmsg2(-1, _("3303 Issuing autochanger \"load %d %d\" command.\n"),
         slot, dev->drive);
   changer = edit_device_codes(dcr, changer,
                               dcr->device->changer_command, "load");
   Dmsg1(100, "Changer=%s\n", changer);
   dev->close(dcr);
   status = RunProgram(changer, timeout, results);
   if (status == 0) {
      Pmsg2(-1,  _("3303 Autochanger \"load %d %d\" status is OK.\n"),
            slot, dev->drive);
   } else {
      BErrNo be;
      Pmsg1(-1, _("3993 Bad autochanger command: %s\n"), changer);
      Pmsg2(-1, _("3993 result=\"%s\": ERR=%s\n"), results, be.bstrerror(status));
      goto bail_out;
   }

   if (!open_the_device()) {
      goto bail_out;
   }
   /*
    * Start with sleep_time 0 then increment by 30 seconds if we get
    * a failure.
    */
   Bmicrosleep(sleep_time, 0);
   if (!dev->rewind(dcr) || !dev->weof(1)) {
      Pmsg1(0, _("Bad status from rewind. ERR=%s\n"), dev->bstrerror());
      dev->clrerror(-1);
      Pmsg0(-1, _("\nThe test failed, probably because you need to put\n"
                "a longer sleep time in the mtx-script in the load) case.\n"
                "Adding a 30 second sleep and trying again ...\n"));
      sleep_time += 30;
      goto try_again;
   } else {
      Pmsg1(0, _("Rewound %s\n"), dev->print_name());
   }

   if (!dev->weof(1)) {
      Pmsg1(0, _("Bad status from weof. ERR=%s\n"), dev->bstrerror());
      goto bail_out;
   } else {
      Pmsg1(0, _("Wrote EOF to %s\n"), dev->print_name());
   }

   if (sleep_time) {
      Pmsg1(-1, _("\nThe test worked this time. Please add:\n\n"
                "   sleep %d\n\n"
                "to your mtx-changer script in the load) case.\n\n"),
                sleep_time);
   } else {
      Pmsg0(-1, _("\nThe test autochanger worked!!\n\n"));
   }

   FreePoolMemory(changer);
   FreePoolMemory(results);
   return 1;


bail_out:
   FreePoolMemory(changer);
   FreePoolMemory(results);
   Pmsg0(-1, _("You must correct this error or the Autochanger will not work.\n"));
   return -2;
}

static void autochangercmd()
{
   autochanger_test();
}


/**
 * This test assumes that the append test has been done,
 *   then it tests the fsf function.
 */
static bool fsf_test()
{
   bool set_off = false;

   Pmsg0(-1, _("\n\n=== Forward space files test ===\n\n"
               "This test is essential to Bareos.\n\n"
               "I'm going to write five files then test forward spacing\n\n"));
   argc = 1;
   rewindcmd();
   wrcmd();
   weofcmd();      /* end file 0 */
   wrcmd();
   wrcmd();
   weofcmd();      /* end file 1 */
   wrcmd();
   wrcmd();
   wrcmd();
   weofcmd();     /* end file 2 */
   wrcmd();
   wrcmd();
   weofcmd();     /* end file 3 */
   wrcmd();
   weofcmd();     /* end file 4 */
   if (dev->HasCap(CAP_TWOEOF)) {
      weofcmd();
   }

test_again:
   rewindcmd();
   Pmsg0(0, _("Now forward spacing 1 file.\n"));
   if (!dev->fsf(1)) {
      Pmsg1(0, _("Bad status from fsr. ERR=%s\n"), dev->bstrerror());
      goto bail_out;
   }
   Pmsg2(-1, _("We should be in file 1. I am at file %d. %s\n"),
      dev->file, dev->file == 1 ? _("This is correct!") : _("This is NOT correct!!!!"));

   if (dev->file != 1) {
      goto bail_out;
   }

   Pmsg0(0, _("Now forward spacing 2 files.\n"));
   if (!dev->fsf(2)) {
      Pmsg1(0, _("Bad status from fsr. ERR=%s\n"), dev->bstrerror());
      goto bail_out;
   }
   Pmsg2(-1, _("We should be in file 3. I am at file %d. %s\n"),
      dev->file, dev->file == 3 ? _("This is correct!") : _("This is NOT correct!!!!"));

   if (dev->file != 3) {
      goto bail_out;
   }

   rewindcmd();
   Pmsg0(0, _("Now forward spacing 4 files.\n"));
   if (!dev->fsf(4)) {
      Pmsg1(0, _("Bad status from fsr. ERR=%s\n"), dev->bstrerror());
      goto bail_out;
   }
   Pmsg2(-1, _("We should be in file 4. I am at file %d. %s\n"),
      dev->file, dev->file == 4 ? _("This is correct!") : _("This is NOT correct!!!!"));

   if (dev->file != 4) {
      goto bail_out;
   }
   if (set_off) {
      Pmsg0(-1, _("The test worked this time. Please add:\n\n"
                "   Fast Forward Space File = no\n\n"
                "to your Device resource for this drive.\n"));
   }

   Pmsg0(-1, "\n");
   Pmsg0(0, _("Now forward spacing 1 more file.\n"));
   if (!dev->fsf(1)) {
      Pmsg1(0, _("Bad status from fsr. ERR=%s\n"), dev->bstrerror());
   }
   Pmsg2(-1, _("We should be in file 5. I am at file %d. %s\n"),
      dev->file, dev->file == 5 ? _("This is correct!") : _("This is NOT correct!!!!"));
   if (dev->file != 5) {
      goto bail_out;
   }
   Pmsg0(-1, _("\n=== End Forward space files test ===\n\n"));
   return true;

bail_out:
   Pmsg0(-1, _("\nThe forward space file test failed.\n"));
   if (dev->HasCap(CAP_FASTFSF)) {
      Pmsg0(-1, _("You have Fast Forward Space File enabled.\n"
              "I am turning it off then retrying the test.\n"));
      dev->ClearCap(CAP_FASTFSF);
      set_off = true;
      goto test_again;
   }
   Pmsg0(-1, _("You must correct this error or Bareos will not work.\n"
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
   if (status == 1) {                   /* OK get out */
      goto all_done;
   }
   if (status == -1) {                  /* first test failed */
      if (dev->HasCap(CAP_EOM) || dev->HasCap(CAP_FASTFSF)) {
         Pmsg0(-1, _("\nAppend test failed. Attempting again.\n"
                   "Setting \"Hardware End of Medium = no\n"
                   "    and \"Fast Forward Space File = no\n"
                   "and retrying append test.\n\n"));
         dev->ClearCap(CAP_EOM);      /* turn off eom */
         dev->ClearCap(CAP_FASTFSF);  /* turn off fast fsf */
         status = append_test();
         if (status == 1) {
            Pmsg0(-1, _("\n\nIt looks like the test worked this time, please add:\n\n"
                     "    Hardware End of Medium = No\n\n"
                     "    Fast Forward Space File = No\n"
                     "to your Device resource in the Storage conf file.\n"));
            goto all_done;
         }
         if (status == -1) {
            Pmsg0(-1, _("\n\nThat appears *NOT* to have corrected the problem.\n"));
            goto failed;
         }
         /* Wrong count after append */
         if (status == -2) {
            Pmsg0(-1, _("\n\nIt looks like the append failed. Attempting again.\n"
                     "Setting \"BSF at EOM = yes\" and retrying append test.\n"));
            dev->SetCap(CAP_BSFATEOM); /* Backspace on eom */
            status = append_test();
            if (status == 1) {
               Pmsg0(-1, _("\n\nIt looks like the test worked this time, please add:\n\n"
                     "    Hardware End of Medium = No\n"
                     "    Fast Forward Space File = No\n"
                     "    BSF at EOM = yes\n\n"
                     "to your Device resource in the Storage conf file.\n"));
               goto all_done;
            }
         }

      }
failed:
      Pmsg0(-1, _("\nAppend test failed.\n\n"
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
   Pmsg0(-1, _("\nThe above Bareos scan should have output identical to what follows.\n"
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
      if (!re_read_block_test()) {
         exit_code = 1;
      }
   }

   if (!fsf_test()) {                  /* do fast forward space file test */
      exit_code = 1;
   }

   autochanger_test();                /* do autochanger test */

}

/* Forward space a file */
static void fsfcmd()
{
   int num = 1;
   if (argc > 1) {
      num = atoi(argk[1]);
   }
   if (num <= 0) {
      num = 1;
   }
   if (!dev->fsf(num)) {
      Pmsg1(0, _("Bad status from fsf. ERR=%s\n"), dev->bstrerror());
      return;
   }
   if (num == 1) {
      Pmsg0(0, _("Forward spaced 1 file.\n"));
   }
   else {
      Pmsg1(0, _("Forward spaced %d files.\n"), num);
   }
}

/* Forward space a record */
static void fsrcmd()
{
   int num = 1;
   if (argc > 1) {
      num = atoi(argk[1]);
   }
   if (num <= 0) {
      num = 1;
   }
   if (!dev->fsr(num)) {
      Pmsg1(0, _("Bad status from fsr. ERR=%s\n"), dev->bstrerror());
      return;
   }
   if (num == 1) {
      Pmsg0(0, _("Forward spaced 1 record.\n"));
   }
   else {
      Pmsg1(0, _("Forward spaced %d records.\n"), num);
   }
}

/**
 * Read a Bareos block from the tape
 */
static void rbcmd()
{
   dev->open(dcr, OPEN_READ_ONLY);
   dcr->ReadBlockFromDev(NO_BLOCK_NUMBER_CHECK);
}

/**
 * Write a Bareos block to the tape
 */
static void wrcmd()
{
   DeviceBlock *block = dcr->block;
   DeviceRecord *rec = dcr->rec;
   int i;

   if (!dev->IsOpen()) {
      open_the_device();
   }
   Dsm_check(200);
   EmptyBlock(block);
   if (verbose > 1) {
      DumpBlock(block, "test");
   }

   i = block->buf_len - 100;
   ASSERT (i > 0);
   rec->data = CheckPoolMemorySize(rec->data, i);
   memset(rec->data, i & 0xFF, i);
   rec->data_len = i;
   Dsm_check(200);
   if (!WriteRecordToBlock(dcr, rec)) {
      Pmsg0(0, _("Error writing record to block.\n"));
      goto bail_out;
   }
   if (!dcr->WriteBlockToDev()) {
      Pmsg0(0, _("Error writing block to device.\n"));
      goto bail_out;
   } else {
      Pmsg1(0, _("Wrote one record of %d bytes.\n"), i);
   }
   Pmsg0(0, _("Wrote block to device.\n"));

bail_out:
   Dsm_check(200);
}

/**
 * Read a record from the tape
 */
static void rrcmd()
{
   char *buf;
   int status, len;

   if (!GetCmd(_("Enter length to read: "))) {
      return;
   }
   len = atoi(cmd);
   if (len < 0 || len > 1000000) {
      Pmsg0(0, _("Bad length entered, using default of 1024 bytes.\n"));
      len = 1024;
   }
   buf = (char *)malloc(len);
   status = read(dev->fd(), buf, len);
   if (status > 0 && status <= len) {
      errno = 0;
   }
   BErrNo be;
   Pmsg3(0, _("Read of %d bytes gives status=%d. ERR=%s\n"),
      len, status, be.bstrerror());
   free(buf);
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
   if (dev->AtEot()) {
      Pmsg0(0, _("End of tape\n"));
      return;
   }
   dev->UpdatePos(dcr);
   tot_files = dev->file;
   Pmsg1(0, _("Starting scan at file %u\n"), dev->file);
   for (;;) {
      if ((status = read(dev->fd(), buf, sizeof(buf))) < 0) {
         BErrNo be;
         dev->clrerror(-1);
         Mmsg2(dev->errmsg, _("read error on %s. ERR=%s.\n"),
            dev->dev_name, be.bstrerror());
         Pmsg2(0, _("Bad status from read %d. ERR=%s\n"), status, dev->bstrerror());
         if (blocks > 0) {
            if (blocks==1) {
               printf(_("1 block of %d bytes in file %d\n"), block_size, dev->file);
            }
            else {
               printf(_("%d blocks of %d bytes in file %d\n"), blocks, block_size, dev->file);
            }
         }
         return;
      }
      Dmsg1(200, "read status = %d\n", status);
/*    sleep(1); */
      if (status != block_size) {
         dev->UpdatePos(dcr);
         if (blocks > 0) {
            if (blocks==1) {
               printf(_("1 block of %d bytes in file %d\n"), block_size, dev->file);
            }
            else {
               printf(_("%d blocks of %d bytes in file %d\n"), blocks, block_size, dev->file);
            }
            blocks = 0;
         }
         block_size = status;
      }
      if (status == 0) {                /* EOF */
         dev->UpdatePos(dcr);
         printf(_("End of File mark.\n"));
         /* Two reads of zero means end of tape */
         if (dev->AtEof()) {
            dev->SetEot();
         } else {
            dev->SetEof();
            dev->file++;
         }
         if (dev->AtEot()) {
            printf(_("End of tape\n"));
            break;
         }
      } else {                        /* Got data */
         dev->ClearEof();
         blocks++;
         tot_blocks++;
         bytes += status;
      }
   }
   dev->UpdatePos(dcr);
   tot_files = dev->file - tot_files;
   printf(_("Total files=%d, blocks=%d, bytes = %s\n"), tot_files, tot_blocks,
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
   DeviceBlock *block = dcr->block;
   char ec1[50];
   char buf1[100], buf2[100];

   blocks = block_size = tot_blocks = 0;
   bytes = 0;

   EmptyBlock(block);
   dev->UpdatePos(dcr);
   tot_files = dev->file;
   for (;;) {
      if (!dcr->ReadBlockFromDevice(NO_BLOCK_NUMBER_CHECK)) {
         Dmsg1(100, "!read_block(): ERR=%s\n", dev->bstrerror());
         if (dev->AtEot()) {
            if (blocks > 0) {
               if (blocks==1) {
                  printf(_("1 block of %d bytes in file %d\n"), block_size, dev->file);
               }
               else {
                  printf(_("%d blocks of %d bytes in file %d\n"), blocks, block_size, dev->file);
               }
               blocks = 0;
            }
            goto bail_out;
         }
         if (dev->AtEof()) {
            if (blocks > 0) {
               if (blocks==1) {
                  printf(_("1 block of %d bytes in file %d\n"), block_size, dev->file);
               }
               else {
                  printf(_("%d blocks of %d bytes in file %d\n"), blocks, block_size, dev->file);
               }
               blocks = 0;
            }
            printf(_("End of File mark.\n"));
            continue;
         }
         if (BitIsSet(ST_SHORT, dev->state)) {
            if (blocks > 0) {
               if (blocks==1) {
                  printf(_("1 block of %d bytes in file %d\n"), block_size, dev->file);
               }
               else {
                  printf(_("%d blocks of %d bytes in file %d\n"), blocks, block_size, dev->file);
               }
               blocks = 0;
            }
            printf(_("Short block read.\n"));
            continue;
         }
         printf(_("Error reading block. ERR=%s\n"), dev->bstrerror());
         goto bail_out;
      }
      if (block->block_len != block_size) {
         if (blocks > 0) {
            if (blocks==1) {
               printf(_("1 block of %d bytes in file %d\n"), block_size, dev->file);
            }
            else {
               printf(_("%d blocks of %d bytes in file %d\n"), blocks, block_size, dev->file);
            }
            blocks = 0;
         }
         block_size = block->block_len;
      }
      blocks++;
      tot_blocks++;
      bytes += block->block_len;
      Dmsg7(100, "Blk_blk=%u file,blk=%u,%u blen=%u bVer=%d SessId=%u SessTim=%u\n",
         block->BlockNumber, dev->file, dev->block_num, block->block_len, block->BlockVer,
         block->VolSessionId, block->VolSessionTime);
      if (verbose == 1) {
         DeviceRecord *rec = new_record();
         ReadRecordFromBlock(dcr, rec);
         Pmsg9(-1, _("Block=%u file,blk=%u,%u blen=%u First rec FI=%s SessId=%u SessTim=%u Strm=%s rlen=%d\n"),
              block->BlockNumber, dev->file, dev->block_num, block->block_len,
              FI_to_ascii(buf1, rec->FileIndex), rec->VolSessionId, rec->VolSessionTime,
              stream_to_ascii(buf2, rec->Stream, rec->FileIndex), rec->data_len);
         rec->remainder = 0;
         FreeRecord(rec);
      } else if (verbose > 1) {
         DumpBlock(block, "");
      }

   }
bail_out:
   tot_files = dev->file - tot_files;
   printf(_("Total files=%d, blocks=%d, bytes = %s\n"), tot_files, tot_blocks,
      edit_uint64_with_commas(bytes, ec1));
}

static void statcmd()
{
#ifdef xxxx
   int debug = debug_level;
#endif
   char *status;

   status = dev->StatusDev();

   printf( _("Device status:"));
   if (BitIsSet(BMT_TAPE, status))
      printf( " TAPE");
   if (BitIsSet(BMT_EOF, status))
      printf( " EOF");
   if (BitIsSet(BMT_BOT, status))
      printf( " BOT");
   if (BitIsSet(BMT_EOT, status))
      printf( " EOT");
   if (BitIsSet(BMT_SM, status))
      printf( " SETMARK");
   if (BitIsSet(BMT_EOD, status))
      printf( " EOD");
   if (BitIsSet(BMT_WR_PROT, status))
      printf( " WRPROT");
   if (BitIsSet(BMT_ONLINE, status))
      printf( " ONLINE");
   if (BitIsSet(BMT_DR_OPEN, status))
      printf( " DOOROPEN");
   if (BitIsSet(BMT_IM_REP_EN, status))
      printf( " IMMREPORT");

   free(status);

   printf(_(". ERR=%s\n"), dev->bstrerror());

#ifdef xxxx
   debug_level = 30;
   DumpVolumeLabel(dev);
   debug_level = debug;
#endif
}

/**
 * First we label the tape, then we fill
 *  it with data get a new tape and write a few blocks.
 */
static void fillcmd()
{
   DeviceRecord rec;
   DeviceBlock  *block = dcr->block;
   char ec1[50], ec2[50];
   char buf1[100], buf2[100];
   uint64_t write_eof;
   uint64_t rate;
   uint32_t min_block_size;
   int fd;

   ok = true;
   stop = 0;
   vol_num = 0;
   last_file = 0;
   last_block_num = 0;
   BlockNumber = 0;
   exit_code = 0;

   Pmsg1(-1, _("\n"
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
         edit_uint64_with_suffix(dev->max_file_size, buf1));

   GetCmd(_("Do you want to run the simplified test (s) with one tape\n"
           "or the complete multiple tape (m) test: (s/m) "));
   if (cmd[0] == 's') {
      Pmsg0(-1, _("Simple test (single tape) selected.\n"));
      simple = true;
   } else if (cmd[0] == 'm') {
      Pmsg0(-1, _("Multiple tape test selected.\n"));
      simple = false;
   } else {
      Pmsg0(000, _("Command aborted.\n"));
      exit_code = 1;
      return;
   }

   Dmsg1(20, "Begin append device=%s\n", dev->print_name());
   Dmsg1(20, "MaxVolSize=%s\n", edit_uint64(dev->max_volume_size, ec1));

   /* Use fixed block size to simplify read back */
   min_block_size = dev->min_block_size;
   dev->min_block_size = dev->max_block_size;
   write_eof = dev->max_file_size / REC_SIZE; /*compute when we add EOF*/
   ASSERT(write_eof > 0);

   SetVolumeName("TestVolume1", 1);
   dcr->DirAskSysopToCreateAppendableVolume();
   dev->SetAppend();                 /* force volume to be relabeled */

   /*
    * Acquire output device for writing.  Note, after acquiring a
    *   device, we MUST release it, which is done at the end of this
    *   subroutine.
    */
   Dmsg0(100, "just before acquire_device\n");
   if (!AcquireDeviceForAppend(dcr)) {
      jcr->setJobStatus(JS_ErrorTerminated);
      exit_code = 1;
      return;
   }
   block = jcr->dcr->block;

   Dmsg0(100, "Just after AcquireDeviceForAppend\n");
   /*
    * Write Begin Session Record
    */
   if (!WriteSessionLabel(dcr, SOS_LABEL)) {
      jcr->setJobStatus(JS_ErrorTerminated);
      Jmsg1(jcr, M_FATAL, 0, _("Write session label failed. ERR=%s\n"),
         dev->bstrerror());
      ok = false;
   }
   Pmsg0(-1, _("Wrote Start of Session label.\n"));

   memset(&rec, 0, sizeof(rec));
   rec.data = GetMemory(100000);     /* max record size */
   rec.data_len = REC_SIZE;

   /*
    * Put some random data in the record
    */
   FillBuffer(FILL_RANDOM, rec.data, rec.data_len);

   /*
    * Generate data as if from File daemon, write to device
    */
   jcr->dcr->VolFirstIndex = 0;
   time(&jcr->run_time);              /* start counting time for rates */

   bstrftime(buf1, sizeof(buf1), jcr->run_time, "%H:%M:%S");

   if (simple) {
      Pmsg1(-1, _("%s Begin writing Bareos records to tape ...\n"), buf1);
   } else {
      Pmsg1(-1, _("%s Begin writing Bareos records to first tape ...\n"), buf1);
   }
   for (file_index = 0; ok && !JobCanceled(jcr); ) {
      rec.VolSessionId = jcr->VolSessionId;
      rec.VolSessionTime = jcr->VolSessionTime;
      rec.FileIndex = ++file_index;
      rec.Stream = STREAM_FILE_DATA;
      rec.maskedStream = STREAM_FILE_DATA;

      /* Mix up the data just a bit */
      MixBuffer(FILL_RANDOM, rec.data, rec.data_len);

      Dmsg4(250, "before write_rec FI=%d SessId=%d Strm=%s len=%d\n",
         rec.FileIndex, rec.VolSessionId,
         stream_to_ascii(buf1, rec.Stream, rec.FileIndex),
         rec.data_len);

      while (!WriteRecordToBlock(dcr, &rec)) {
         /*
          * When we get here we have just filled a block
          */
         Dmsg2(150, "!WriteRecordToBlock data_len=%d rem=%d\n", rec.data_len,
                    rec.remainder);

         /* Write block to tape */
         if (!FlushBlock(block, 1)) {
            Pmsg0(000, _("Flush block failed.\n"));
            exit_code = 1;
            break;
         }

         /* Every 5000 blocks (approx 322MB) report where we are.
          */
         if ((block->BlockNumber % 5000) == 0) {
            now = time(NULL);
            now -= jcr->run_time;
            if (now <= 0) {
               now = 1;          /* prevent divide error */
            }
            rate = dev->VolCatInfo.VolCatBytes / now;
            Pmsg5(-1, _("Wrote block=%u, file,blk=%u,%u VolBytes=%s rate=%sB/s\n"),
               block->BlockNumber, dev->file, dev->block_num,
               edit_uint64_with_commas(dev->VolCatInfo.VolCatBytes, ec1),
               edit_uint64_with_suffix(rate, ec2));
         }
         /* Every X blocks (dev->max_file_size) write an EOF.
          */
         if ((block->BlockNumber % write_eof) == 0) {
            now = time(NULL);
            bstrftime(buf1, sizeof(buf1), now, "%H:%M:%S");
            Pmsg1(-1, _("%s Flush block, write EOF\n"), buf1);
            FlushBlock(block, 0);
#ifdef needed_xxx
            dev->weof(1);
#endif
         }

         /* Get out after writing 1000 blocks to the second tape */
         if (++BlockNumber > 1000 && stop != 0) {      /* get out */
            Pmsg0(000, _("Wrote 1000 blocks on second tape. Done.\n"));
            break;
         }
      }
      if (!ok) {
         Pmsg0(000, _("Not OK\n"));
         exit_code = 1;
         break;
      }
      jcr->JobBytes += rec.data_len;   /* increment bytes of this job */
      Dmsg4(190, "WriteRecord FI=%s SessId=%d Strm=%s len=%d\n",
         FI_to_ascii(buf1, rec.FileIndex), rec.VolSessionId,
         stream_to_ascii(buf2, rec.Stream, rec.FileIndex), rec.data_len);

      /* Get out after writing 1000 blocks to the second tape */
      if (BlockNumber > 1000 && stop != 0) {      /* get out */
         char ed1[50];
         Pmsg1(-1, "Done writing %s records ...\n",
             edit_uint64_with_commas(write_count, ed1));
         break;
      }
   } /* end big for loop */

   if (vol_num > 1) {
      Dmsg0(100, "Write_end_session_label()\n");
      /* Create Job status for end of session label */
      if (!JobCanceled(jcr) && ok) {
         jcr->setJobStatus(JS_Terminated);
      } else if (!ok) {
         Pmsg0(000, _("Job canceled.\n"));
         jcr->setJobStatus(JS_ErrorTerminated);
         exit_code = 1;
      }
      if (!WriteSessionLabel(dcr, EOS_LABEL)) {
         Pmsg1(000, _("Error writing end session label. ERR=%s\n"), dev->bstrerror());
         ok = false;
         exit_code = 1;
      }
      /* Write out final block of this session */
      if (!dcr->WriteBlockToDevice()) {
         Pmsg0(-1, _("Set ok=false after WriteBlockToDevice.\n"));
         ok = false;
         exit_code = 1;
      }
      Pmsg0(-1, _("Wrote End of Session label.\n"));

      /* Save last block info for second tape */
      last_block_num2 = last_block_num;
      last_file2 = last_file;
      if (last_block2) {
         FreeBlock(last_block2);
      }
      last_block2 = dup_block(last_block);
   }

   sprintf(buf, "%s/btape.state", working_directory);
   fd = open(buf, O_CREAT|O_TRUNC|O_WRONLY, 0640);
   if (fd >= 0) {
      write(fd, &btape_state_level, sizeof(btape_state_level));
      write(fd, &simple, sizeof(simple));
      write(fd, &last_block_num1, sizeof(last_block_num1));
      write(fd, &last_block_num2, sizeof(last_block_num2));
      write(fd, &last_file1, sizeof(last_file1));
      write(fd, &last_file2, sizeof(last_file2));
      write(fd, last_block1->buf, last_block1->buf_len);
      write(fd, last_block2->buf, last_block2->buf_len);
      write(fd, first_block->buf, first_block->buf_len);
      close(fd);
      Pmsg2(0, _("Wrote state file last_block_num1=%d last_block_num2=%d\n"),
         last_block_num1, last_block_num2);
   } else {
      BErrNo be;
      Pmsg2(0, _("Could not create state file: %s ERR=%s\n"), buf,
                 be.bstrerror());
      exit_code = 1;
      ok = false;
   }

   now = time(NULL);
   bstrftime(buf1, sizeof(buf1), now, "%H:%M:%S");

   if (ok) {
      if (simple) {
         Pmsg3(0, _("\n\n%s Done filling tape at %d:%d. Now beginning re-read of tape ...\n"),
               buf1, jcr->dcr->dev->file, jcr->dcr->dev->block_num);
      } else {
         Pmsg3(0, _("\n\n%s Done filling tapes at %d:%d. Now beginning re-read of first tape ...\n"),
               buf1, jcr->dcr->dev->file, jcr->dcr->dev->block_num);
      }

      jcr->dcr->block = block;
      if (!do_unfill()) {
         Pmsg0(000, _("do_unfill failed.\n"));
         exit_code = 1;
         ok = false;
      }
   } else {
      Pmsg1(000, _("%s: Error during test.\n"), buf1);
   }
   dev->min_block_size = min_block_size;
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
   last_block1 = new_block(dev);
   last_block2 = new_block(dev);
   first_block = new_block(dev);
   sprintf(buf, "%s/btape.state", working_directory);
   fd = open(buf, O_RDONLY);
   if (fd >= 0) {
      uint32_t state_level;
      read(fd, &state_level, sizeof(btape_state_level));
      read(fd, &simple, sizeof(simple));
      read(fd, &last_block_num1, sizeof(last_block_num1));
      read(fd, &last_block_num2, sizeof(last_block_num2));
      read(fd, &last_file1, sizeof(last_file1));
      read(fd, &last_file2, sizeof(last_file2));
      read(fd, last_block1->buf, last_block1->buf_len);
      read(fd, last_block2->buf, last_block2->buf_len);
      read(fd, first_block->buf, first_block->buf_len);
      close(fd);
      if (state_level != btape_state_level) {
          Pmsg0(-1, _("\nThe state file level has changed. You must redo\n"
                  "the fill command.\n"));
          exit_code = 1;
          return;
       }
   } else {
      BErrNo be;
      Pmsg2(-1, _("\nCould not find the state file: %s ERR=%s\n"
             "You must redo the fill command.\n"), buf, be.bstrerror());
      exit_code = 1;
      return;
   }
   if (!do_unfill()) {
      exit_code = 1;
   }
   this_block = NULL;
}

/**
 * This is the second part of the fill command. After the tape or
 *  tapes are written, we are called here to reread parts, particularly
 *  the last block.
 */
static bool do_unfill()
{
   DeviceBlock *block = dcr->block;
   int autochanger;
   bool rc = false;

   dumped = 0;
   VolBytes = 0;
   LastBlock = 0;

   Pmsg0(000, "Enter do_unfill\n");
   dev->SetCap(CAP_ANONVOLS);        /* allow reading any volume */
   dev->ClearCap(CAP_LABEL);         /* don't label anything here */

   end_of_tape = 0;

   time(&jcr->run_time);              /* start counting time for rates */
   stop = 0;
   file_index = 0;
   if (last_block) {
      FreeBlock(last_block);
      last_block = NULL;
   }
   last_block_num = last_block_num1;
   last_file = last_file1;
   last_block = last_block1;

   FreeRestoreVolumeList(jcr);
   jcr->bsr = NULL;
   bstrncpy(dcr->VolumeName, "TestVolume1|TestVolume2", sizeof(dcr->VolumeName));
   CreateRestoreVolumeList(jcr);
   if (jcr->VolList != NULL) {
      jcr->VolList->Slot = 1;
      if (jcr->VolList->next != NULL) {
         jcr->VolList->next->Slot = 2;
      }
   }

   SetVolumeName("TestVolume1", 1);

   if (!simple) {
      /* Multiple Volume tape */
      /* Close device so user can use autochanger if desired */
      if (dev->HasCap(CAP_OFFLINEUNMOUNT)) {
         dev->offline();
      }
      autochanger = AutoloadDevice(dcr, 1, NULL);
      if (autochanger != 1) {
         Pmsg1(100, "Autochanger returned: %d\n", autochanger);
         dev->close(dcr);
         GetCmd(_("Mount first tape. Press enter when ready: "));
         Pmsg0(000, "\n");
      }
   }

   dev->close(dcr);
   dev->num_writers = 0;
   jcr->dcr->clear_will_write();

   if (!AcquireDeviceForRead(dcr)) {
      Pmsg1(-1, "%s", dev->errmsg);
      goto bail_out;
   }
   /*
    * We now have the first tape mounted.
    * Note, re-reading last block may have caused us to
    *   loose track of where we are (block number unknown).
    */
   Pmsg0(-1, _("Rewinding.\n"));
   if (!dev->rewind(dcr)) {                /* get to a known place on tape */
      goto bail_out;
   }
   /* Read the first 10000 records */
   Pmsg2(-1, _("Reading the first 10000 records from %u:%u.\n"),
      dev->file, dev->block_num);
   quickie_count = 0;
   ReadRecords(dcr, QuickieCb, MyMountNextReadVolume);
   Pmsg4(-1, _("Reposition from %u:%u to %u:%u\n"), dev->file, dev->block_num,
         last_file, last_block_num);
   if (!dev->Reposition(dcr, last_file, last_block_num)) {
      Pmsg1(-1, _("Reposition error. ERR=%s\n"), dev->bstrerror());
      goto bail_out;
   }
   Pmsg1(-1, _("Reading block %u.\n"), last_block_num);
   if (!dcr->ReadBlockFromDevice(NO_BLOCK_NUMBER_CHECK)) {
      Pmsg1(-1, _("Error reading block: ERR=%s\n"), dev->bstrerror());
      goto bail_out;
   }
   if (CompareBlocks(last_block, block)) {
      if (simple) {
         Pmsg0(-1, _("\nThe last block on the tape matches. Test succeeded.\n\n"));
         rc = true;
      } else {
         Pmsg0(-1, _("\nThe last block of the first tape matches.\n\n"));
      }
   }
   if (simple) {
      goto bail_out;
   }

   /* restore info for last block on second Volume */
   last_block_num = last_block_num2;
   last_file = last_file2;
   last_block = last_block2;

   /* Multiple Volume tape */
   /* Close device so user can use autochanger if desired */
   if (dev->HasCap(CAP_OFFLINEUNMOUNT)) {
      dev->offline();
   }

   SetVolumeName("TestVolume2", 2);

   autochanger = AutoloadDevice(dcr, 1, NULL);
   if (autochanger != 1) {
      Pmsg1(100, "Autochanger returned: %d\n", autochanger);
      dev->close(dcr);
      GetCmd(_("Mount second tape. Press enter when ready: "));
      Pmsg0(000, "\n");
   }

   dev->ClearRead();
   if (!AcquireDeviceForRead(dcr)) {
      Pmsg1(-1, "%s", dev->errmsg);
      goto bail_out;
   }

   /* Space to "first" block which is last block not written
    * on the previous tape.
    */
   Pmsg2(-1, _("Reposition from %u:%u to 0:1\n"), dev->file, dev->block_num);
   if (!dev->Reposition(dcr, 0, 1)) {
      Pmsg1(-1, _("Reposition error. ERR=%s\n"), dev->bstrerror());
      goto bail_out;
   }
   Pmsg1(-1, _("Reading block %d.\n"), dev->block_num);
   if (!dcr->ReadBlockFromDevice(NO_BLOCK_NUMBER_CHECK)) {
      Pmsg1(-1, _("Error reading block: ERR=%s\n"), dev->bstrerror());
      goto bail_out;
   }
   if (CompareBlocks(first_block, block)) {
      Pmsg0(-1, _("\nThe first block on the second tape matches.\n\n"));
   }

   /* Now find and compare the last block */
   Pmsg4(-1, _("Reposition from %u:%u to %u:%u\n"), dev->file, dev->block_num,
         last_file, last_block_num);
   if (!dev->Reposition(dcr, last_file, last_block_num)) {
      Pmsg1(-1, _("Reposition error. ERR=%s\n"), dev->bstrerror());
      goto bail_out;
   }
   Pmsg1(-1, _("Reading block %d.\n"), dev->block_num);
   if (!dcr->ReadBlockFromDevice(NO_BLOCK_NUMBER_CHECK)) {
      Pmsg1(-1, _("Error reading block: ERR=%s\n"), dev->bstrerror());
      goto bail_out;
   }
   if (CompareBlocks(last_block, block)) {
      Pmsg0(-1, _("\nThe last block on the second tape matches. Test succeeded.\n\n"));
      rc = true;
   }

bail_out:
   FreeBlock(last_block1);
   FreeBlock(last_block2);
   FreeBlock(first_block);

   last_block1 = NULL;
   last_block2 = NULL;
   last_block = NULL;
   first_block = NULL;

   return rc;
}

/* Read 10000 records then stop */
static bool QuickieCb(DeviceControlRecord *dcr, DeviceRecord *rec)
{
   Device *dev = dcr->dev;
   quickie_count++;
   if (quickie_count == 10000) {
      Pmsg2(-1, _("10000 records read now at %d:%d\n"), dev->file, dev->block_num);
   }
   return quickie_count < 10000;
}

static bool CompareBlocks(DeviceBlock *last_block, DeviceBlock *block)
{
   char *p, *q;
   union {
      uint32_t CheckSum;
      uint32_t block_len;
   };
   ser_declare;

   p = last_block->buf;
   q = block->buf;
   UnserBegin(q, BLKHDR2_LENGTH);
   unser_uint32(CheckSum);
   unser_uint32(block_len);
   while (q < (block->buf+block_len)) {
      if (*p == *q) {
         p++;
         q++;
         continue;
      }
      Pmsg0(-1, "\n");
      DumpBlock(last_block, _("Last block written"));
      Pmsg0(-1, "\n");
      DumpBlock(block, _("Block read back"));
      Pmsg1(-1, _("\n\nThe blocks differ at byte %u\n"), p - last_block->buf);
      Pmsg0(-1, _("\n\n!!!! The last block written and the block\n"
                "that was read back differ. The test FAILED !!!!\n"
                "This must be corrected before you use Bareos\n"
                "to write multi-tape Volumes.!!!!\n"));
      return false;
   }
   if (verbose) {
      DumpBlock(last_block, _("Last block written"));
      DumpBlock(block, _("Block read back"));
   }
   return true;
}

/**
 * Write current block to tape regardless of whether or
 *   not it is full. If the tape fills, attempt to
 *   acquire another tape.
 */
static int FlushBlock(DeviceBlock *block, int dump)
{
   char ec1[50], ec2[50];
   uint64_t rate;
   DeviceBlock *tblock;
   uint32_t thIsFile, this_block_num;

   dev->rLock();
   if (!this_block) {
      this_block = new_block(dev);
   }
   if (!last_block) {
      last_block = new_block(dev);
   }
   /* Copy block */
   thIsFile = dev->file;
   this_block_num = dev->block_num;
   if (!dcr->WriteBlockToDev()) {
      Pmsg3(000, _("Last block at: %u:%u this_dev_block_num=%d\n"),
                  last_file, last_block_num, this_block_num);
      if (vol_num == 1) {
         /*
          * This is 1st tape, so save first tape info separate
          *  from second tape info
          */
         last_block_num1 = last_block_num;
         last_file1 = last_file;
         last_block1 = dup_block(last_block);
         last_block2 = dup_block(last_block);
         first_block = dup_block(block); /* first block second tape */
      }
      if (verbose) {
         Pmsg3(000, _("Block not written: FileIndex=%u blk_block=%u Size=%u\n"),
            (unsigned)file_index, block->BlockNumber, block->block_len);
         DumpBlock(last_block, _("Last block written"));
         Pmsg0(-1, "\n");
         DumpBlock(block, _("Block not written"));
      }
      if (stop == 0) {
         eot_block = block->BlockNumber;
         eot_block_len = block->block_len;
         eot_FileIndex = file_index;
         stop = 1;
      }
      now = time(NULL);
      now -= jcr->run_time;
      if (now <= 0) {
         now = 1;                     /* don't divide by zero */
      }
      rate = dev->VolCatInfo.VolCatBytes / now;
      vol_size = dev->VolCatInfo.VolCatBytes;
      Pmsg4(000, _("End of tape %d:%d. Volume Bytes=%s. Write rate = %sB/s\n"),
         dev->file, dev->block_num,
         edit_uint64_with_commas(dev->VolCatInfo.VolCatBytes, ec1),
         edit_uint64_with_suffix(rate, ec2));

      if (simple) {
         stop = -1;                   /* stop, but do simplified test */
      } else {
         /* Full test in progress */
         if (!FixupDeviceBlockWriteError(jcr->dcr)) {
            Pmsg1(000, _("Cannot fixup device error. %s\n"), dev->bstrerror());
            ok = false;
            dev->Unlock();
            return 0;
         }
         BlockNumber = 0;             /* start counting for second tape */
      }
      dev->Unlock();
      return 1;                       /* end of tape reached */
   }

   /* Save contents after write so that the header is serialized */
   memcpy(this_block->buf, block->buf, this_block->buf_len);

   /*
    * Note, we always read/write to block, but we toggle
    *  copying it to one or another of two allocated blocks.
    * Switch blocks so that the block just successfully written is
    *  always in last_block.
    */
   tblock = last_block;
   last_block = this_block;
   this_block = tblock;
   last_file = thIsFile;
   last_block_num = this_block_num;

   dev->Unlock();
   return 1;
}


/**
 * First we label the tape, then we fill
 *  it with data get a new tape and write a few blocks.
 */
static void qfillcmd()
{
   DeviceBlock *block = dcr->block;
   DeviceRecord *rec = dcr->rec;
   int i, count;

   Pmsg0(0, _("Test writing blocks of 64512 bytes to tape.\n"));

   GetCmd(_("How many blocks do you want to write? (1000): "));

   count = atoi(cmd);
   if (count <= 0) {
      count = 1000;
   }

   Dsm_check(200);

   i = block->buf_len - 100;
   ASSERT (i > 0);
   rec->data = CheckPoolMemorySize(rec->data, i);
   memset(rec->data, i & 0xFF, i);
   rec->data_len = i;
   rewindcmd();
   init_speed();

   Pmsg1(0, _("Begin writing %d Bareos blocks to tape ...\n"), count);
   for (i=0; i < count; i++) {
      if (i % 100 == 0) {
         printf("+");
         fflush(stdout);
      }
      if (!WriteRecordToBlock(dcr, rec)) {
         Pmsg0(0, _("Error writing record to block.\n"));
         goto bail_out;
      }
      if (!dcr->WriteBlockToDev()) {
         Pmsg0(0, _("Error writing block to device.\n"));
         goto bail_out;
      }
   }
   printf("\n");
   PrintSpeed(dev->VolCatInfo.VolCatBytes);
   weofcmd();
   if (dev->HasCap(CAP_TWOEOF)) {
      weofcmd();
   }
   rewindcmd();
   scan_blocks();

bail_out:
   Dsm_check(200);
}

/**
 * Fill a tape using raw write() command
 */
static void rawfill_cmd()
{
   DeviceBlock *block = dcr->block;
   int status;
   uint32_t block_num = 0;
   uint32_t *p;
   int my_errno;

   FillBuffer(FILL_RANDOM, block->buf, block->buf_len);
   init_speed();

   p = (uint32_t *)block->buf;
   Pmsg1(0, _("Begin writing raw blocks of %u bytes.\n"), block->buf_len);
   for ( ;; ) {
      *p = block_num;
      status = dev->d_write(dev->fd(), block->buf, block->buf_len);
      if (status == (int)block->buf_len) {
         if ((block_num++ % 100) == 0) {
            printf("+");
            fflush(stdout);
         }

         MixBuffer(FILL_RANDOM, block->buf, block->buf_len);

         jcr->JobBytes += status;
         continue;
      }
      break;
   }
   my_errno = errno;
   printf("\n");
   BErrNo be;
   printf(_("Write failed at block %u. status=%d ERR=%s\n"), block_num, status,
      be.bstrerror(my_errno));

   PrintSpeed(jcr->JobBytes);
   weofcmd();
}



struct cmdstruct { const char *key; void (*func)(); const char *help; };
static struct cmdstruct commands[] = {
 {NT_("autochanger"),autochangercmd, _("test autochanger")},
 {NT_("bsf"),       bsfcmd,       _("backspace file")},
 {NT_("bsr"),       bsrcmd,       _("backspace record")},
 {NT_("cap"),       capcmd,       _("list device capabilities")},
 {NT_("clear"),     clearcmd,     _("clear tape errors")},
 {NT_("eod"),       eodcmd,       _("go to end of Bareos data for append")},
 {NT_("eom"),       eomcmd,       _("go to the physical end of medium")},
 {NT_("fill"),      fillcmd,      _("fill tape, write onto second volume")},
 {NT_("unfill"),    unfillcmd,    _("read filled tape")},
 {NT_("fsf"),       fsfcmd,       _("forward space a file")},
 {NT_("fsr"),       fsrcmd,       _("forward space a record")},
 {NT_("help"),      HelpCmd,      _("print this command")},
 {NT_("label"),     labelcmd,     _("write a Bareos label to the tape")},
 {NT_("load"),      loadcmd,      _("load a tape")},
 {NT_("quit"),      QuitCmd,      _("quit btape")},
 {NT_("rawfill"),   rawfill_cmd,  _("use write() to fill tape")},
 {NT_("readlabel"), readlabelcmd, _("read and print the Bareos tape label")},
 {NT_("rectest"),   rectestcmd,   _("test record handling functions")},
 {NT_("rewind"),    rewindcmd,    _("rewind the tape")},
 {NT_("scan"),      scancmd,      _("read() tape block by block to EOT and report")},
 {NT_("scanblocks"),scan_blocks,  _("Bareos read block by block to EOT and report")},
 {NT_("speed"),     speed_test,   _("[file_size=n(GB)|nb_file=3|skip_zero|skip_random|skip_raw|skip_block] report drive speed")},
 {NT_("status"),    statcmd,      _("print tape status")},
 {NT_("test"),      testcmd,      _("General test Bareos tape functions")},
 {NT_("weof"),      weofcmd,      _("write an EOF on the tape")},
 {NT_("wr"),        wrcmd,        _("write a single Bareos block")},
 {NT_("rr"),        rrcmd,        _("read a single record")},
 {NT_("rb"),        rbcmd,        _("read a single Bareos block")},
 {NT_("qfill"),     qfillcmd,     _("quick fill command")}
             };
#define comsize (sizeof(commands)/sizeof(struct cmdstruct))

static void
do_tape_cmds()
{
   unsigned int i;
   bool found;

   while (!quit && GetCmd("*")) {
      Dsm_check(200);
      found = false;
      ParseArgs(cmd, args, &argc, argk, argv, MAX_CMD_ARGS);
      /* search for command */
      for (i=0; i<comsize; i++) {
         if (argc > 0 && fstrsch(argk[0],  commands[i].key)) {
            /* execute command */
            (*commands[i].func)();
            found = true;
            break;
         }
      }
      if (*cmd && !found) {
         Pmsg1(0, _("\"%s\" is an invalid command\n"), cmd);
      }
   }
}

static void HelpCmd()
{
   unsigned int i;
   usage();
   printf(_("Interactive commands:\n"));
   printf(_("  Command    Description\n  =======    ===========\n"));
   for (i=0; i<comsize; i++)
      printf("  %-10s %s\n", commands[i].key, commands[i].help);
   printf("\n");
}

static void usage()
{
   fprintf(stderr, _(
PROG_COPYRIGHT
"\nVersion: %s (%s)\n\n"
"Usage: btape <options> <device_name>\n"
"       -b <file>     specify bootstrap file\n"
"       -c <path>     specify Storage configuration file or directory\n"
"       -D <director> specify a director name specified in the Storage\n"
"                     configuration file for the Key Encryption Key selection\n"
"       -d <nn>       set debug level to <nn>\n"
"       -dt           print timestamp in debug output\n"
"       -p            proceed inspite of I/O errors\n"
"       -s            turn off signals\n"
"       -v            be verbose\n"
"       -?            print this message.\n"
"\n"), 2000, VERSION, BDATE);

}

/**
 * Get next input command from terminal.  This
 * routine is REALLY primitive, and should be enhanced
 * to have correct backspacing, etc.
 */
int GetCmd(const char *prompt)
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
         if (i > 0) {
            cmd[--i] = 0;
         }
         continue;
      }

      cmd[i++] = ch;
      cmd[i] = 0;
   }
   quit = 1;
   return 0;
}

bool BTAPE_DCR::DirCreateJobmediaRecord(bool zero)
{
   WroteVol = false;
   return 1;
}

bool BTAPE_DCR::DirFindNextAppendableVolume()
{
   Dmsg1(20, "Enter DirFindNextAppendableVolume. stop=%d\n", stop);
   return VolumeName[0] != 0;
}

bool BTAPE_DCR::DirAskSysopToMountVolume(int mode)
{
   Dmsg0(20, "Enter DirAskSysopToMountVolume\n");
   if (VolumeName[0] == 0) {
      return DirAskSysopToCreateAppendableVolume();
   }
   Pmsg1(-1, "%s", dev->errmsg);           /* print reason */

   if (VolumeName[0] == 0 || bstrcmp(VolumeName, "TestVolume2")) {
      fprintf(stderr,
              _("Mount second Volume on device %s and press return when ready: "),
              dev->print_name());
   } else {
      fprintf(stderr,
              _("Mount Volume \"%s\" on device %s and press return when ready: "),
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
   if (dev->HasCap(CAP_OFFLINEUNMOUNT)) {
      dev->offline();
   }
   autochanger = AutoloadDevice(this, 1, NULL);
   if (autochanger != 1) {
      Pmsg1(100, "Autochanger returned: %d\n", autochanger);
      fprintf(stderr, _("Mount blank Volume on device %s and press return when ready: "),
         dev->print_name());
      dev->close(this);
      getchar();
      Pmsg0(000, "\n");
   }
   labelcmd();
   volumename = NULL;
   BlockNumber = 0;

   return true;
}

DeviceControlRecord *BTAPE_DCR::get_new_spooling_dcr()
{
   DeviceControlRecord *dcr;

   dcr = New(BTAPE_DCR);

   return dcr;
}

static bool MyMountNextReadVolume(DeviceControlRecord *dcr)
{
   char ec1[50], ec2[50];
   uint64_t rate;
   JobControlRecord *jcr = dcr->jcr;
   DeviceBlock *block = dcr->block;

   Dmsg0(20, "Enter MyMountNextReadVolume\n");
   Pmsg2(000, _("End of Volume \"%s\" %d records.\n"), dcr->VolumeName,
      quickie_count);

   VolumeUnused(dcr);             /* release current volume */
   if (LastBlock != block->BlockNumber) {
      VolBytes += block->block_len;
   }
   LastBlock = block->BlockNumber;
   now = time(NULL);
   now -= jcr->run_time;
   if (now <= 0) {
      now = 1;
   }
   rate = VolBytes / now;
   Pmsg3(-1, _("Read block=%u, VolBytes=%s rate=%sB/s\n"), block->BlockNumber,
            edit_uint64_with_commas(VolBytes, ec1),
            edit_uint64_with_suffix(rate, ec2));

   if (bstrcmp(dcr->VolumeName, "TestVolume2")) {
      end_of_tape = 1;
      return false;
   }

   SetVolumeName("TestVolume2", 2);

   dev->close(dcr);
   if (!AcquireDeviceForRead(dcr)) {
      Pmsg2(0, _("Cannot open Dev=%s, Vol=%s\n"), dev->print_name(), dcr->VolumeName);
      return false;
   }
   return true;                    /* next volume mounted */
}

static void SetVolumeName(const char *VolName, int volnum)
{
   DeviceControlRecord *dcr = jcr->dcr;
   volumename = VolName;
   vol_num = volnum;
   dev->setVolCatName(VolName);
   dcr->setVolCatName(VolName);
   bstrncpy(dcr->VolumeName, VolName, sizeof(dcr->VolumeName));
   dcr->VolCatInfo.Slot = volnum;
   dcr->VolCatInfo.InChanger = true;
}
