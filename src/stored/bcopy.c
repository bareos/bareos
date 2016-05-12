/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2002-2012 Free Software Foundation Europe e.V.
   Copyright (C) 2011-2012 Planets Communications B.V.
   Copyright (C) 2013-2013 Bareos GmbH & Co. KG

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
 * Program to copy a Bareos from one volume to another.
 *
 * Kern E. Sibbald, October 2002
 */

#include "bareos.h"
#include "stored.h"
#include "lib/crypto_cache.h"

/* Dummy functions */
extern bool parse_sd_config(CONFIG *config, const char *configfile, int exit_code);

/* Forward referenced functions */
static void get_session_record(DEVICE *dev, DEV_RECORD *rec, SESSION_LABEL *sessrec);
static bool record_cb(DCR *dcr, DEV_RECORD *rec);


/* Global variables */
static DEVICE *in_dev = NULL;
static DEVICE *out_dev = NULL;
static JCR *in_jcr;                    /* input jcr */
static JCR *out_jcr;                   /* output jcr */
static BSR *bsr = NULL;
static const char *wd = "/tmp";
static bool list_records = false;
static uint32_t records = 0;
static uint32_t jobs = 0;
static DEV_BLOCK *out_block;
static SESSION_LABEL sessrec;

static void usage()
{
   fprintf(stderr, _(
PROG_COPYRIGHT
"\nVersion: %s (%s)\n\n"
"Usage: bcopy [-d debug_level] <input-archive> <output-archive>\n"
"       -b bootstrap    specify a bootstrap file\n"
"       -c <file>       specify a Storage configuration file\n"
"       -D <director>   specify a director name specified in the Storage\n"
"                       configuration file for the Key Encryption Key selection\n"
"       -d <nn>         set debug level to <nn>\n"
"       -dt             print timestamp in debug output\n"
"       -i              specify input Volume names (separated by |)\n"
"       -o              specify output Volume names (separated by |)\n"
"       -p              proceed inspite of errors\n"
"       -v              verbose\n"
"       -w <dir>        specify working directory (default /tmp)\n"
"       -?              print this message\n\n"), 2002, VERSION, BDATE);
   exit(1);
}

int main (int argc, char *argv[])
{
   int ch;
   bool ok;
   char *iVolumeName = NULL;
   char *oVolumeName = NULL;
   char *DirectorName = NULL;
   DIRRES *director = NULL;
   bool ignore_label_errors = false;
   DCR *in_dcr, *out_dcr;

   setlocale(LC_ALL, "");
   bindtextdomain("bareos", LOCALEDIR);
   textdomain("bareos");
   init_stack_dump();

   my_name_is(argc, argv, "bcopy");
   lmgr_init_thread();
   init_msg(NULL, NULL);

   while ((ch = getopt(argc, argv, "b:c:D:d:i:o:pvw:?")) != -1) {
      switch (ch) {
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

      case 'i':                    /* input Volume name */
         iVolumeName = optarg;
         break;

      case 'o':                    /* output Volume name */
         oVolumeName = optarg;
         break;

      case 'p':
         ignore_label_errors = true;
         forge_on = true;
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

   if (argc != 2) {
      Pmsg0(0, _("Wrong number of arguments: \n"));
      usage();
   }

   OSDependentInit();

   working_directory = wd;

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

   /*
    * Setup and acquire input device for reading
    */
   Dmsg0(100, "About to setup input jcr\n");

   in_dcr = New(DCR);
   in_jcr = setup_jcr("bcopy", argv[0], bsr, director, in_dcr, iVolumeName, true); /* read device */
   if (!in_jcr) {
      exit(1);
   }

   in_jcr->ignore_label_errors = ignore_label_errors;

   in_dev = in_jcr->dcr->dev;
   if (!in_dev) {
      exit(1);
   }

   /*
    * Setup output device for writing
    */
   Dmsg0(100, "About to setup output jcr\n");

   out_dcr = New(DCR);
   out_jcr = setup_jcr("bcopy", argv[1], bsr, director, out_dcr, oVolumeName, false); /* write device */
   if (!out_jcr) {
      exit(1);
   }

   out_dev = out_jcr->dcr->dev;
   if (!out_dev) {
      exit(1);
   }

   Dmsg0(100, "About to acquire device for writing\n");

   /*
    * For we must now acquire the device for writing
    */
   out_dev->rLock(false);
   if (!out_dev->open(out_jcr->dcr, OPEN_READ_WRITE)) {
      Emsg1(M_FATAL, 0, _("dev open failed: %s\n"), out_dev->errmsg);
      out_dev->Unlock();
      exit(1);
   }
   out_dev->Unlock();
   if (!acquire_device_for_append(out_jcr->dcr)) {
      free_jcr(in_jcr);
      exit(1);
   }
   out_block = out_jcr->dcr->block;

   ok = read_records(in_jcr->dcr, record_cb, mount_next_read_volume);

   if (ok || out_dev->can_write()) {
      if (!out_jcr->dcr->write_block_to_device()) {
         Pmsg0(000, _("Write of last block failed.\n"));
      }
   }

   Pmsg2(000, _("%u Jobs copied. %u records copied.\n"), jobs, records);

   in_dev->term();
   out_dev->term();

   free_jcr(in_jcr);
   free_jcr(out_jcr);

   return 0;
}


/*
 * read_records() calls back here for each record it gets
 */
static bool record_cb(DCR *in_dcr, DEV_RECORD *rec)
{
   if (list_records) {
      Pmsg5(000, _("Record: SessId=%u SessTim=%u FileIndex=%d Stream=%d len=%u\n"),
            rec->VolSessionId, rec->VolSessionTime, rec->FileIndex,
            rec->Stream, rec->data_len);
   }
   /*
    * Check for Start or End of Session Record
    *
    */
   if (rec->FileIndex < 0) {
      get_session_record(in_dcr->dev, rec, &sessrec);

      if (verbose > 1) {
         dump_label_record(in_dcr->dev, rec, true);
      }
      switch (rec->FileIndex) {
      case PRE_LABEL:
         Pmsg0(000, _("Volume is prelabeled. This volume cannot be copied.\n"));
         return false;
      case VOL_LABEL:
         Pmsg0(000, _("Volume label not copied.\n"));
         return true;
      case SOS_LABEL:
         if (bsr && rec->match_stat < 1) {
            /* Skipping record, because does not match BSR filter */
            if (verbose) {
             Pmsg0(-1, _("Copy skipped. Record does not match BSR filter.\n"));
            }
         } else {
            jobs++;
         }
         break;
      case EOS_LABEL:
         if (bsr && rec->match_stat < 1) {
            /* Skipping record, because does not match BSR filter */
           return true;
        }
         while (!write_record_to_block(out_jcr->dcr, rec)) {
            Dmsg2(150, "!write_record_to_block data_len=%d rem=%d\n", rec->data_len,
                       rec->remainder);
            if (!out_jcr->dcr->write_block_to_device()) {
               Dmsg2(90, "Got write_block_to_dev error on device %s: ERR=%s\n",
                  out_dev->print_name(), out_dev->bstrerror());
               Jmsg(out_jcr, M_FATAL, 0, _("Cannot fixup device error. %s\n"),
                     out_dev->bstrerror());
               return false;
            }
         }
         if (!out_jcr->dcr->write_block_to_device()) {
            Dmsg2(90, "Got write_block_to_dev error on device %s: ERR=%s\n",
               out_dev->print_name(), out_dev->bstrerror());
            Jmsg(out_jcr, M_FATAL, 0, _("Cannot fixup device error. %s\n"),
                  out_dev->bstrerror());
            return false;
         }
         return true;
      case EOM_LABEL:
         Pmsg0(000, _("EOM label not copied.\n"));
         return true;
      case EOT_LABEL:              /* end of all tapes */
         Pmsg0(000, _("EOT label not copied.\n"));
         return true;
      default:
         return true;
      }
   }

   /*  Write record */
   if (bsr && rec->match_stat < 1) {
      /* Skipping record, because does not match BSR filter */
      return true;
   }
   records++;
   while (!write_record_to_block(out_jcr->dcr, rec)) {
      Dmsg2(150, "!write_record_to_block data_len=%d rem=%d\n", rec->data_len,
                 rec->remainder);
      if (!out_jcr->dcr->write_block_to_device()) {
         Dmsg2(90, "Got write_block_to_dev error on device %s: ERR=%s\n",
            out_dev->print_name(), out_dev->bstrerror());
         Jmsg(out_jcr, M_FATAL, 0, _("Cannot fixup device error. %s\n"),
               out_dev->bstrerror());
         return false;
      }
   }
   return true;
}

static void get_session_record(DEVICE *dev, DEV_RECORD *rec, SESSION_LABEL *sessrec)
{
   const char *rtype;
   memset(sessrec, 0, sizeof(SESSION_LABEL));
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
      break;
   case EOS_LABEL:
      rtype = _("End Job Session");
      unser_session_label(sessrec, rec);
      break;
   case 0:
   case EOM_LABEL:
      rtype = _("End of Medium");
      break;
   default:
      rtype = _("Unknown");
      break;
   }
   Dmsg5(10, "%s Record: VolSessionId=%d VolSessionTime=%d JobId=%d DataLen=%d\n",
         rtype, rec->VolSessionId, rec->VolSessionTime, rec->Stream, rec->data_len);
   if (verbose) {
      Pmsg5(-1, _("%s Record: VolSessionId=%d VolSessionTime=%d JobId=%d DataLen=%d\n"),
            rtype, rec->VolSessionId, rec->VolSessionTime, rec->Stream, rec->data_len);
   }
}
