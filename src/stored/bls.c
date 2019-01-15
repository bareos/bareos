/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2000-2012 Free Software Foundation Europe e.V.
   Copyright (C) 2011-2012 Planets Communications B.V.
   Copyright (C) 2013-2017 Bareos GmbH & Co. KG

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
 * Kern Sibbald, MM
 */
/**
 * @file
 * Dumb program to do an "ls" of a Bareos 2.0 mortal file.
 */

#include "bareos.h"
#include "stored.h"
#include "lib/crypto_cache.h"
#include "findlib/find.h"

/* Dummy functions */
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

static FF_PKT *ff;
static BSR *bsr = NULL;

static void usage()
{
   fprintf(stderr, _(
PROG_COPYRIGHT
"\nVersion: %s (%s)\n\n"
"Usage: bls [options] <device-name>\n"
"       -b <file>       specify a bootstrap file\n"
"       -c <path>       specify a Storage configuration file or directory\n"
"       -D <director>   specify a director name specified in the Storage\n"
"                       configuration file for the Key Encryption Key selection\n"
"       -d <nn>         set debug level to <nn>\n"
"       -dt             print timestamp in debug output\n"
"       -e <file>       exclude list\n"
"       -i <file>       include list\n"
"       -j              list jobs\n"
"       -k              list blocks\n"
"    (no j or k option) list saved files\n"
"       -L              dump label\n"
"       -p              proceed inspite of errors\n"
"       -v              be verbose (can be specified multiple times)\n"
"       -V              specify Volume names (separated by |)\n"
"       -?              print this message\n\n"), 2000, VERSION, BDATE);
   exit(1);
}

int main (int argc, char *argv[])
{
   int i, ch;
   FILE *fd;
   char line[1000];
   char *VolumeName = NULL;
   char *bsrName = NULL;
   char *DirectorName = NULL;
   bool ignore_label_errors = false;
   DIRRES *director = NULL;

   setlocale(LC_ALL, "");
   bindtextdomain("bareos", LOCALEDIR);
   textdomain("bareos");
   init_stack_dump();
   lmgr_init_thread();

   working_directory = "/tmp";
   my_name_is(argc, argv, "bls");
   init_msg(NULL, NULL);              /* initialize message handler */

   OSDependentInit();

   ff = init_find_files();

   while ((ch = getopt(argc, argv, "b:c:D:d:e:i:jkLpvV:?")) != -1) {
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

   if (ff->included_files_list == NULL) {
      add_fname_to_include_list(ff, 0, "/");
   }

   for (i=0; i < argc; i++) {
      if (bsrName) {
         bsr = parse_bsr(NULL, bsrName);
      }
      dcr = New(DCR);
      jcr = setup_jcr("bls", argv[i], bsr, director, dcr, VolumeName, true); /* read device */
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
   free_attr(attr);
   free_record(rec);
   clean_device(jcr->dcr);
   dev->term();
   free_dcr(jcr->dcr);
   free_jcr(jcr);
}

/* List just block information */
static void do_blocks(char *infname)
{
   DEV_BLOCK *block = dcr->block;
   char buf1[100], buf2[100];
   for ( ;; ) {
      switch(dcr->read_block_from_device(NO_BLOCK_NUMBER_CHECK)) {
         case Ok:
            // no special handling required
            break;
         case EndOfTape:
            if (!mount_next_read_volume(dcr)) {
               Jmsg(jcr, M_INFO, 0, _("Got EOM at file %u on device %s, Volume \"%s\"\n"),
                  dev->file, dev->print_name(), dcr->VolumeName);
               return;
            }
            /* Read and discard Volume label */
            DEV_RECORD *record;
            record = new_record();
            dcr->read_block_from_device(NO_BLOCK_NUMBER_CHECK);
            read_record_from_block(dcr, record);
            get_session_record(dev, record, &sessrec);
            free_record(record);
            Jmsg(jcr, M_INFO, 0, _("Mounted Volume \"%s\".\n"), dcr->VolumeName);
            break;
         case EndOfFile:
            Jmsg(jcr, M_INFO, 0, _("End of file %u on device %s, Volume \"%s\"\n"),
               dev->file, dev->print_name(), dcr->VolumeName);
            Dmsg0(20, "read_record got eof. try again\n");
            continue;
         default:
            Dmsg1(100, "!read_block(): ERR=%s\n", dev->bstrerror());
            if (dev->is_short_block()) {
               Jmsg(jcr, M_INFO, 0, "%s", dev->errmsg);
               continue;
            } else {
               /* I/O error */
               display_tape_error_status(jcr, dev);
               return;
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

/**
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
   printf("%u files and directories found.\n", num_files);
}

/**
 * Called here for each record from read_records()
 */
static bool record_cb(DCR *dcr, DEV_RECORD *rec)
{
   POOL_MEM record_str(PM_MESSAGE);

   if (rec->FileIndex < 0) {
      get_session_record(dev, rec, &sessrec);
      return true;
   }

   /*
    * File Attributes stream
    */
   switch (rec->maskedStream) {
   case STREAM_UNIX_ATTRIBUTES:
   case STREAM_UNIX_ATTRIBUTES_EX:
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
         if (!verbose) {
            print_ls_output(jcr, attr);
         } else {
            Pmsg1(-1, "%s\n",  record_to_str(record_str, jcr, rec));
         }
         num_files++;
      }
      break;
   default:
      if (verbose) {
         Pmsg1(-1, "%s\n",  record_to_str(record_str, jcr, rec));
      }
      break;
   }

   /* debug output of record */
   dump_record("", rec);

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
