/*
   Bacula® - The Network Backup Solution

   Copyright (C) 2000-2012 Free Software Foundation Europe e.V.

   The main author of Bacula is Kern Sibbald, with contributions from
   many others, a complete list can be found in the file AUTHORS.
   This program is Free Software; you can redistribute it and/or
   modify it under the terms of version three of the GNU Affero General Public
   License as published by the Free Software Foundation and included
   in the file LICENSE.

   This program is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
   General Public License for more details.

   You should have received a copy of the GNU Affero General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
   02110-1301, USA.

   Bacula® is a registered trademark of Kern Sibbald.
   The licensor of Bacula is the Free Software Foundation Europe
   (FSFE), Fiduciary Program, Sumatrastrasse 25, 8006 Zürich,
   Switzerland, email:ftf@fsfeurope.org.
*/
/*
 *
 *  Dumb program to do an "ls" of a Bacula 1.0 mortal file.
 * 
 *  Kern Sibbald, MM
 *
 */

#include "bacula.h"
#include "stored.h"
#include "findlib/find.h"

/* Dummy functions */
int generate_daemon_event(JCR *jcr, const char *event) { return 1; }
extern bool parse_sd_config(CONFIG *config, const char *configfile, int exit_code);

static void do_blocks(char *infname);
static void do_jobs(char *infname);
static void do_ls(char *fname);
static void do_close(JCR *jcr);
static void get_session_record(DEVICE *dev, DEV_RECORD *rec, SESSION_LABEL *sessrec);
static bool record_cb(DCR *dcr, DEV_RECORD *rec);

static DEVICE *dev;
static DCR *dcr;
static bool dump_label = false;
static bool list_blocks = false;
static bool list_jobs = false;
static DEV_RECORD *rec;
static JCR *jcr;
static SESSION_LABEL sessrec;
static uint32_t num_files = 0;
static ATTR *attr;
static CONFIG *config;

#define CONFIG_FILE "bacula-sd.conf"
char *configfile = NULL;
STORES *me = NULL;                    /* our Global resource */
bool forge_on = false;
pthread_mutex_t device_release_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t wait_device_release = PTHREAD_COND_INITIALIZER;


static FF_PKT *ff;

static BSR *bsr = NULL;

static void usage()
{
   fprintf(stderr, _(
PROG_COPYRIGHT
"\nVersion: %s (%s)\n\n"
"Usage: bls [options] <device-name>\n"
"       -b <file>       specify a bootstrap file\n"
"       -c <file>       specify a Storage configuration file\n"
"       -d <nn>         set debug level to <nn>\n"
"       -dt             print timestamp in debug output\n"
"       -e <file>       exclude list\n"
"       -i <file>       include list\n"
"       -j              list jobs\n"
"       -k              list blocks\n"
"    (no j or k option) list saved files\n"
"       -L              dump label\n"
"       -p              proceed inspite of errors\n"
"       -v              be verbose\n"
"       -V              specify Volume names (separated by |)\n"
"       -?              print this message\n\n"), 2000, VERSION, BDATE);
   exit(1);
}


int main (int argc, char *argv[])
{
   int i, ch;
   FILE *fd;
   char line[1000];
   char *VolumeName= NULL;
   char *bsrName = NULL;
   bool ignore_label_errors = false;

   setlocale(LC_ALL, "");
   bindtextdomain("bacula", LOCALEDIR);
   textdomain("bacula");
   init_stack_dump();
   lmgr_init_thread();

   working_directory = "/tmp";
   my_name_is(argc, argv, "bls");
   init_msg(NULL, NULL);              /* initialize message handler */

   OSDependentInit();

   ff = init_find_files();

   while ((ch = getopt(argc, argv, "b:c:d:e:i:jkLpvV:?")) != -1) {
      switch (ch) {
      case 'b':
         bsrName = optarg;
         break;

      case 'c':                    /* specify config file */
         if (configfile != NULL) {
            free(configfile);
         }
         configfile = bstrdup(optarg);
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

      case 'e':                    /* exclude list */
         if ((fd = fopen(optarg, "rb")) == NULL) {
            berrno be;
            Pmsg2(0, _("Could not open exclude file: %s, ERR=%s\n"),
               optarg, be.bstrerror());
            exit(1);
         }
         while (fgets(line, sizeof(line), fd) != NULL) {
            strip_trailing_junk(line);
            Dmsg1(100, "add_exclude %s\n", line);
            add_fname_to_exclude_list(ff, line);
         }
         fclose(fd);
         break;

      case 'i':                    /* include list */
         if ((fd = fopen(optarg, "rb")) == NULL) {
            berrno be;
            Pmsg2(0, _("Could not open include file: %s, ERR=%s\n"),
               optarg, be.bstrerror());
            exit(1);
         }
         while (fgets(line, sizeof(line), fd) != NULL) {
            strip_trailing_junk(line);
            Dmsg1(100, "add_include %s\n", line);
            add_fname_to_include_list(ff, 0, line);
         }
         fclose(fd);
         break;

      case 'j':
         list_jobs = true;
         break;

      case 'k':
         list_blocks = true;
         break;

      case 'L':
         dump_label = true;
         break;

      case 'p':
         ignore_label_errors = true;
         forge_on = true;
         break;

      case 'v':
         verbose++;
         break;

      case 'V':                    /* Volume name */
         VolumeName = optarg;
         break;

      case '?':
      default:
         usage();

      } /* end switch */
   } /* end while */
   argc -= optind;
   argv += optind;

   if (!argc) {
      Pmsg0(0, _("No archive name specified\n"));
      usage();
   }

   if (configfile == NULL) {
      configfile = bstrdup(CONFIG_FILE);
   }

   config = new_config_parser();
   parse_sd_config(config, configfile, M_ERROR_TERM);

   if (ff->included_files_list == NULL) {
      add_fname_to_include_list(ff, 0, "/");
   }

   for (i=0; i < argc; i++) {
      if (bsrName) {
         bsr = parse_bsr(NULL, bsrName);
      }
      jcr = setup_jcr("bls", argv[i], bsr, VolumeName, 1); /* acquire for read */
      if (!jcr) {
         exit(1);
      }
      jcr->ignore_label_errors = ignore_label_errors;
      dev = jcr->dcr->dev;
      if (!dev) {
         exit(1);
      }
      dcr = jcr->dcr;
      rec = new_record();
      attr = new_attr(jcr);
      /*
       * Assume that we have already read the volume label.
       * If on second or subsequent volume, adjust buffer pointer
       */
      if (dev->VolHdr.PrevVolumeName[0] != 0) { /* second volume */
         Pmsg1(0, _("\n"
                    "Warning, this Volume is a continuation of Volume %s\n"),
                dev->VolHdr.PrevVolumeName);
      }

      if (list_blocks) {
         do_blocks(argv[i]);
      } else if (list_jobs) {
         do_jobs(argv[i]);
      } else {
         do_ls(argv[i]);
      }
      do_close(jcr);
   }
   if (bsr) {
      free_bsr(bsr);
   }
   term_include_exclude_files(ff);
   term_find_files(ff);
   return 0;
}


static void do_close(JCR *jcr)
{
   release_device(jcr->dcr);
   free_attr(attr);
   free_record(rec);
   free_jcr(jcr);
   dev->term();
}


/* List just block information */
static void do_blocks(char *infname)
{
   DEV_BLOCK *block = dcr->block;
   char buf1[100], buf2[100];
   for ( ;; ) {
      if (!dcr->read_block_from_device(NO_BLOCK_NUMBER_CHECK)) {
         Dmsg1(100, "!read_block(): ERR=%s\n", dev->bstrerror());
         if (dev->at_eot()) {
            if (!mount_next_read_volume(dcr)) {
               Jmsg(jcr, M_INFO, 0, _("Got EOM at file %u on device %s, Volume \"%s\"\n"),
                  dev->file, dev->print_name(), dcr->VolumeName);
               break;
            }
            /* Read and discard Volume label */
            DEV_RECORD *record;
            record = new_record();
            dcr->read_block_from_device(NO_BLOCK_NUMBER_CHECK);
            read_record_from_block(dcr, record);
            get_session_record(dev, record, &sessrec);
            free_record(record);
            Jmsg(jcr, M_INFO, 0, _("Mounted Volume \"%s\".\n"), dcr->VolumeName);
         } else if (dev->at_eof()) {
            Jmsg(jcr, M_INFO, 0, _("End of file %u on device %s, Volume \"%s\"\n"),
               dev->file, dev->print_name(), dcr->VolumeName);
            Dmsg0(20, "read_record got eof. try again\n");
            continue;
         } else if (dev->is_short_block()) {
            Jmsg(jcr, M_INFO, 0, "%s", dev->errmsg);
            continue;
         } else {
            /* I/O error */
            display_tape_error_status(jcr, dev);
            break;
         }
      }
      if (!match_bsr_block(bsr, block)) {
         Dmsg5(100, "reject Blk=%u blen=%u bVer=%d SessId=%u SessTim=%u\n",
            block->BlockNumber, block->block_len, block->BlockVer,
            block->VolSessionId, block->VolSessionTime);
         continue;
      }
      Dmsg5(100, "Blk=%u blen=%u bVer=%d SessId=%u SessTim=%u\n",
        block->BlockNumber, block->block_len, block->BlockVer,
        block->VolSessionId, block->VolSessionTime);
      if (verbose == 1) {
         read_record_from_block(dcr, rec);
         Pmsg9(-1, _("File:blk=%u:%u blk_num=%u blen=%u First rec FI=%s SessId=%u SessTim=%u Strm=%s rlen=%d\n"),
              dev->file, dev->block_num,
              block->BlockNumber, block->block_len,
              FI_to_ascii(buf1, rec->FileIndex), rec->VolSessionId, rec->VolSessionTime,
              stream_to_ascii(buf2, rec->Stream, rec->FileIndex), rec->data_len);
         rec->remainder = 0;
      } else if (verbose > 1) {
         dump_block(block, "");
      } else {
         printf(_("Block: %d size=%d\n"), block->BlockNumber, block->block_len);
      }

   }
   return;
}

/*
 * We are only looking for labels or in particular Job Session records
 */
static bool jobs_cb(DCR *dcr, DEV_RECORD *rec)
{
   if (rec->FileIndex < 0) {
      dump_label_record(dcr->dev, rec, verbose);
   }
   rec->remainder = 0;
   return true;
}

/* Do list job records */
static void do_jobs(char *infname)
{
   read_records(dcr, jobs_cb, mount_next_read_volume);
}

/* Do an ls type listing of an archive */
static void do_ls(char *infname)
{
   if (dump_label) {
      dump_volume_label(dev);
      return;
   }
   read_records(dcr, record_cb, mount_next_read_volume);
   printf("%u files found.\n", num_files);
}

/*
 * Called here for each record from read_records()
 */
static bool record_cb(DCR *dcr, DEV_RECORD *rec)
{
   if (rec->FileIndex < 0) {
      get_session_record(dev, rec, &sessrec);
      return true;
   }
   /* File Attributes stream */
   if (rec->maskedStream == STREAM_UNIX_ATTRIBUTES ||
       rec->maskedStream == STREAM_UNIX_ATTRIBUTES_EX) {
      if (!unpack_attributes_record(jcr, rec->Stream, rec->data, rec->data_len, attr)) {
         if (!forge_on) {
            Emsg0(M_ERROR_TERM, 0, _("Cannot continue.\n"));
         } else {
            Emsg0(M_ERROR, 0, _("Attrib unpack error!\n"));
         }
         num_files++;
         return true;
      }

      attr->data_stream = decode_stat(attr->attr, &attr->statp, sizeof(attr->statp), &attr->LinkFI);
      build_attr_output_fnames(jcr, attr);

      if (file_is_included(ff, attr->fname) && !file_is_excluded(ff, attr->fname)) {
         if (verbose) {
            Pmsg5(-1, _("FileIndex=%d VolSessionId=%d VolSessionTime=%d Stream=%d DataLen=%d\n"),
                  rec->FileIndex, rec->VolSessionId, rec->VolSessionTime, rec->Stream, rec->data_len);
         }
         print_ls_output(jcr, attr);
         num_files++;
      }
   } else if (rec->Stream == STREAM_PLUGIN_NAME) {
      char data[100];
      int len = MIN(rec->data_len+1, sizeof(data));
      bstrncpy(data, rec->data, len);
      Pmsg1(000, "Plugin data: %s\n", data);
   } else if (rec->Stream == STREAM_RESTORE_OBJECT) {
      Pmsg0(000, "Restore Object record\n");
   }
      
   return true;
}


static void get_session_record(DEVICE *dev, DEV_RECORD *rec, SESSION_LABEL *sessrec)
{
   const char *rtype;
   memset(sessrec, 0, sizeof(SESSION_LABEL));
   jcr->JobId = 0;
   switch (rec->FileIndex) {
   case PRE_LABEL:
      rtype = _("Fresh Volume Label");
      break;
   case VOL_LABEL:
      rtype = _("Volume Label");
      unser_volume_label(dev, rec);
      break;
   case SOS_LABEL:
      rtype = _("Begin Job Session");
      unser_session_label(sessrec, rec);
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
   Dmsg5(10, "%s Record: VolSessionId=%d VolSessionTime=%d JobId=%d DataLen=%d\n",
         rtype, rec->VolSessionId, rec->VolSessionTime, rec->Stream, rec->data_len);
   if (verbose) {
      Pmsg5(-1, _("%s Record: VolSessionId=%d VolSessionTime=%d JobId=%d DataLen=%d\n"),
            rtype, rec->VolSessionId, rec->VolSessionTime, rec->Stream, rec->data_len);
   }
}


/* Dummies to replace askdir.c */
bool    dir_find_next_appendable_volume(DCR *dcr) { return 1;}
bool    dir_update_volume_info(DCR *dcr, bool relabel, bool update_LastWritten) { return 1; }
bool    dir_create_jobmedia_record(DCR *dcr, bool zero) { return 1; }
bool    dir_ask_sysop_to_create_appendable_volume(DCR *dcr) { return 1; }
bool    dir_update_file_attributes(DCR *dcr, DEV_RECORD *rec) { return 1;}
bool    dir_send_job_status(JCR *jcr) {return 1;}
int     generate_job_event(JCR *jcr, const char *event) { return 1; }
       

bool dir_ask_sysop_to_mount_volume(DCR *dcr, int /*mode*/)
{
   DEVICE *dev = dcr->dev;
   fprintf(stderr, _("Mount Volume \"%s\" on device %s and press return when ready: "),
      dcr->VolumeName, dev->print_name());
   dev->close();
   getchar();
   return true;
}

bool dir_get_volume_info(DCR *dcr, enum get_vol_info_rw  writing)
{
   Dmsg0(100, "Fake dir_get_volume_info\n");
   dcr->setVolCatName(dcr->VolumeName);
   dcr->VolCatInfo.VolCatParts = find_num_dvd_parts(dcr);
   Dmsg2(500, "Vol=%s num_parts=%d\n", dcr->getVolCatName(), dcr->VolCatInfo.VolCatParts);
   return 1;
}
