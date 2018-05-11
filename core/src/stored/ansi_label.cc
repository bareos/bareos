/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2005-2009 Free Software Foundation Europe e.V.
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
 * Kern Sibbald, MMV
 */
/**
 * @file
 * ansi_label.c routines to handle ANSI tape labels.
 */

#include "include/bareos.h"                   /* pull in global headers */
#include "stored/stored.h"                   /* pull in Storage Daemon headers */
#include "stored/label.h"
#include "include/jcr.h"

/* Imported functions */
void AsciiToEbcdic(char *dst, char *src, int count);
void EbcdicToAscii(char *dst, char *src, int count);

/* Forward referenced functions */
static char *ansi_date(time_t td, char *buf);
static bool SameLabelNames(char *bareos_name, char *ansi_name);

/**
 * We read an ANSI label and compare the Volume name. We require
 * a VOL1 record of 80 characters followed by a HDR1 record containing
 * BAREOS.DATA in the filename field. We then read up to 3 more
 * header records (they are not required) and an EOF, at which
 * point, all is good.
 *
 * Returns:
 *    VOL_OK            Volume name OK
 *    VOL_NO_LABEL      No ANSI label on Volume
 *    VOL_IO_ERROR      I/O error on read
 *    VOL_NAME_ERROR    Wrong name in VOL1 record
 *    VOL_LABEL_ERROR   Probably an ANSI label, but something wrong
 */
int ReadAnsiIbmLabel(DeviceControlRecord *dcr)
{
   Device * volatile dev = dcr->dev;
   JobControlRecord *jcr = dcr->jcr;
   char label[80];                    /* tape label */
   int status, i;
   char *VolName = dcr->VolumeName;
   bool ok = false;

   /*
    * Read VOL1, HDR1, HDR2 labels, but ignore the data
    * If tape read the following EOF mark, on disk do not read.
    */
   Dmsg0(100, "Read ansi label.\n");
   if (!dev->IsTape()) {
      return VOL_OK;
   }

   dev->label_type = B_BAREOS_LABEL;  /* assume Bareos label */

   /*
    * Read a maximum of 5 records VOL1, HDR1, ... HDR4
    */
   for (i = 0; i < 6; i++) {
      do {
         status = dev->read(label, sizeof(label));
      } while (status == -1 && errno == EINTR);

      if (status < 0) {
         BErrNo be;
         dev->clrerror(-1);
         Dmsg1(100, "Read device got: ERR=%s\n", be.bstrerror());
         Mmsg2(jcr->errmsg, _("Read error on device %s in ANSI label. ERR=%s\n"),
            dev->dev_name, be.bstrerror());
         Jmsg(jcr, M_ERROR, 0, "%s", dev->errmsg);
         dev->VolCatInfo.VolCatErrors++;
         return VOL_IO_ERROR;
      }

      if (status == 0) {
         if (dev->AtEof()) {
            dev->SetEot();           /* second eof, set eot bit */
            Dmsg0(100, "EOM on ANSI label\n");
            Mmsg0(jcr->errmsg, _("Insane! End of tape while reading ANSI label.\n"));
            return VOL_LABEL_ERROR;   /* at EOM this shouldn't happen */
         } else {
            dev->SetAteof();        /* set eof state */
         }
      }

      switch (i) {
      case 0:                         /* Want VOL1 label */
         if (status == 80) {
            if (bstrncmp("VOL1", label, 4)) {
               ok = true;
               dev->label_type = B_ANSI_LABEL;
               Dmsg0(100, "Got ANSI VOL1 label\n");
            } else {
               /*
                * Try EBCDIC
                */
               EbcdicToAscii(label, label, sizeof(label));
               if (bstrncmp("VOL1", label, 4)) {
                  ok = true;;
                  dev->label_type = B_IBM_LABEL;
                  Dmsg0(100, "Found IBM label.\n");
                  Dmsg0(100, "Got IBM VOL1 label\n");
               }
            }
         }

         if (!ok) {
            Dmsg0(100, "No VOL1 label\n");
            Mmsg0(jcr->errmsg, _("No VOL1 label while reading ANSI/IBM label.\n"));
            return VOL_NO_LABEL;   /* No ANSI label */
         }

         /*
          * Compare Volume Names allow special wild card
          */
         if (VolName && *VolName && *VolName != '*') {
            if (!SameLabelNames(VolName, &label[4])) {
               char *p = &label[4];
               char *q;

               FreeVolume(dev);

               /*
                * Store new Volume name
                */
               q = dev->VolHdr.VolumeName;
               for (int i=0; *p != ' ' && i < 6; i++) {
                  *q++ = *p++;
               }
               *q = 0;
               Dmsg0(100, "Call reserve_volume\n");
               /*
                * ***FIXME***  why is this reserve_volume() needed???? KES
                */
               reserve_volume(dcr, dev->VolHdr.VolumeName);
               dev = dcr->dev;            /* may have changed in reserve_volume */
               Dmsg2(100, "Wanted ANSI Vol %s got %6s\n", VolName, dev->VolHdr.VolumeName);
               Mmsg2(jcr->errmsg, _("Wanted ANSI Volume \"%s\" got \"%s\"\n"), VolName, dev->VolHdr.VolumeName);
               return VOL_NAME_ERROR;
            }
         }
         break;
      case 1:
         if (dev->label_type == B_IBM_LABEL) {
            EbcdicToAscii(label, label, sizeof(label));
         }

         if (status != 80 || !bstrncmp("HDR1", label, 4)) {
            Dmsg0(100, "No HDR1 label\n");
            Mmsg0(jcr->errmsg, _("No HDR1 label while reading ANSI label.\n"));
            return VOL_LABEL_ERROR;
         }

         if (me->compatible) {
            if (!bstrncmp("BACULA.DATA", &label[4], 11) &&
                !bstrncmp("BAREOS.DATA", &label[4], 11)) {
               Dmsg1(100, "HD1 not Bacula/Bareos label. Wanted BACULA.DATA/BAREOS.DATA got %11s\n", &label[4]);
               Mmsg1(jcr->errmsg, _("ANSI/IBM Volume \"%s\" does not belong to Bareos.\n"), dev->VolHdr.VolumeName);
               return VOL_NAME_ERROR;     /* Not a Bareos label */
            }
         } else {
            if (!bstrncmp("BAREOS.DATA", &label[4], 11)) {
               Dmsg1(100, "HD1 not Bareos label. Wanted BAREOS.DATA got %11s\n", &label[4]);
               Mmsg1(jcr->errmsg, _("ANSI/IBM Volume \"%s\" does not belong to Bareos.\n"), dev->VolHdr.VolumeName);
               return VOL_NAME_ERROR;     /* Not a Bareos label */
            }
         }
         Dmsg0(100, "Got HDR1 label\n");
         break;
      case 2:
         if (dev->label_type == B_IBM_LABEL) {
            EbcdicToAscii(label, label, sizeof(label));
         }

         if (status != 80 || !bstrncmp("HDR2", label, 4)) {
            Dmsg0(100, "No HDR2 label\n");
            Mmsg0(jcr->errmsg, _("No HDR2 label while reading ANSI/IBM label.\n"));
            return VOL_LABEL_ERROR;
         }
         Dmsg0(100, "Got ANSI HDR2 label\n");
         break;
      default:
         if (status == 0) {
            Dmsg0(100, "ANSI label OK\n");
            return VOL_OK;
         }

         if (dev->label_type == B_IBM_LABEL) {
            EbcdicToAscii(label, label, sizeof(label));
         }

         if (status != 80 || !bstrncmp("HDR", label, 3)) {
            Dmsg0(100, "Unknown or bad ANSI/IBM label record.\n");
            Mmsg0(jcr->errmsg, _("Unknown or bad ANSI/IBM label record.\n"));
            return VOL_LABEL_ERROR;
         }

         Dmsg0(100, "Got HDR label\n");
         break;
      }
   }
   Dmsg0(100, "Too many records in ANSI/IBM label.\n");
   Mmsg0(jcr->errmsg, _("Too many records in while reading ANSI/IBM label.\n"));
   return VOL_LABEL_ERROR;
}

/**
 * ANSI/IBM VOL1 label
 *  80 characters blank filled
 * Pos   count   Function      What Bareos puts
 * 0-3     4     "VOL1"          VOL1
 * 4-9     6     Volume name     Volume name
 * 10-10   1     Access code
 * 11-36   26    Unused
 *
 * ANSI
 * 37-50   14    Owner
 * 51-78   28    reserved
 * 79       1    ANSI level        3
 *
 * IBM
 * 37-40   4     reserved
 * 41-50   10    Owner
 * 51-79   29    reserved
 *
 * ANSI/IBM HDR1 label
 *  80 characters blank filled
 * Pos   count   Function          What Bareos puts
 * 0-3     4     "HDR1"               HDR1
 * 4-20    17    File name           BAREOS.DATA
 * 21-26   6     Volume name          Volume name
 * 27-30   4     Vol seq num           0001
 * 31-34   4     file num              0001
 * 35-38   4     Generation            0001
 * 39-40   2     Gen version           00
 * 41-46   6     Create date bYYDDD    yesterday
 * 47-52   6     Expire date bYYDDD    today
 * 53-53   1     Access
 * 54-59   6     Block count           000000
 * 60-72   13    Software name         Bareos
 * 73-79   7     Reserved
 *
 * ANSI/IBM HDR2 label
 *  80 characters blank filled
 * Pos   count   Function          What Bareos puts
 * 0-3     4     "HDR2"               HDR2
 * 4-4     1     Record format        D   (V if IBM) => variable
 * 5-9     5     Block length         32000
 * 10-14   5     Rec length           32000
 * 15-15   1     Density
 * 16-16   1     Continued
 * 17-33   17    Job
 * 34-35   2     Recording
 * 36-36   1     cr/lf ctl
 * 37-37   1     reserved
 * 38-38   1     Blocked flag
 * 39-49   11    reserved
 * 50-51   2     offset
 * 52-79   28    reserved
 */
static const char *labels[] = {"HDR", "EOF", "EOV"};

/**
 * Write an ANSI or IBM 80 character tape label
 *
 * Type determines whether we are writing HDR, EOF, or EOV labels
 * Assume we are positioned to write the labels
 * Returns:  true of OK
 *           false if error
 */
bool WriteAnsiIbmLabels(DeviceControlRecord *dcr, int type, const char *VolName)
{
   Device *dev = dcr->dev;
   JobControlRecord *jcr = dcr->jcr;
   char ansi_volname[7];              /* 6 char + \0 */
   char label[80];                    /* tape label */
   char date[20];                     /* ansi date buffer */
   time_t now;
   int len, status, label_type;

   /*
    * If the Device requires a specific label type use it,
    * otherwise, use the type requested by the Director
    */
   if (dcr->device->label_type != B_BAREOS_LABEL) {
      label_type = dcr->device->label_type;   /* force label type */
   } else {
      label_type = dcr->VolCatInfo.LabelType; /* accept Dir type */
   }

   switch (label_type) {
   case B_BAREOS_LABEL:
      return true;
   case B_ANSI_LABEL:
   case B_IBM_LABEL:
      ser_declare;
      Dmsg1(100, "Write ANSI label type=%d\n", label_type);
      len = strlen(VolName);
      if (len > 6) {
         Jmsg1(jcr, M_FATAL, 0, _("ANSI Volume label name \"%s\" longer than 6 chars.\n"),
            VolName);
         return false;
      }

      /*
       * ANSI labels have 6 characters, and are padded with spaces 'vol1\0' => 'vol1   \0'
       */
      strcpy(ansi_volname, VolName);
      for(int i=len; i < 6; i++) {
         ansi_volname[i]=' ';
      }
      ansi_volname[6]='\0';     /* only for debug */

      if (type == ANSI_VOL_LABEL) {
         SerBegin(label, sizeof(label));
         SerBytes("VOL1", 4);
         SerBytes(ansi_volname, 6);

         /*
          * Write VOL1 label
          */
         if (label_type == B_IBM_LABEL) {
            AsciiToEbcdic(label, label, sizeof(label));
         } else {
            label[79] = '3';                /* ANSI label flag */
         }

         status = dev->write(label, sizeof(label));
         if (status != sizeof(label)) {
            BErrNo be;
            Jmsg3(jcr, M_FATAL, 0,  _("Could not write ANSI VOL1 label. Wanted size=%d got=%d ERR=%s\n"),
                  sizeof(label), status, be.bstrerror());
            return false;
         }
      }

      /*
       * Now construct HDR1 label
       */
      memset(label, ' ', sizeof(label));
      SerBegin(label, sizeof(label));
      SerBytes(labels[type], 3);
      SerBytes("1", 1);
      if (me->compatible) {
         SerBytes("BACULA.DATA", 11);            /* Filename field */
      } else {
         SerBytes("BAREOS.DATA", 11);            /* Filename field */
      }
      SerBegin(&label[21], sizeof(label)-21); /* fileset field */
      SerBytes(ansi_volname, 6);              /* write Vol Ser No. */
      SerBegin(&label[27], sizeof(label)-27);
      SerBytes("00010001000100", 14);  /* File section, File seq no, Generation no */
      now = time(NULL);
      SerBytes(ansi_date(now, date), 6); /* current date */
      SerBytes(ansi_date(now - 24 * 3600, date), 6); /* created yesterday */
      SerBytes(" 000000Bareos              ", 27);

      /*
       * Write HDR1 label
       */
      if (label_type == B_IBM_LABEL) {
         AsciiToEbcdic(label, label, sizeof(label));
      }

      /*
       * This could come at the end of a tape, ignore EOT errors.
       */
      status = dev->write(label, sizeof(label));
      if (status != sizeof(label)) {
         BErrNo be;
         if (status == -1) {
            dev->clrerror(-1);
            if (dev->dev_errno == 0) {
               dev->dev_errno = ENOSPC; /* out of space */
            }
            if (dev->dev_errno != ENOSPC) {
               Jmsg1(jcr, M_FATAL, 0, _("Could not write ANSI HDR1 label. ERR=%s\n"),
               be.bstrerror());
               return false;
            }
         } else {
            Jmsg(jcr, M_FATAL, 0, _("Could not write ANSI HDR1 label.\n"));
            return false;
         }
      }

      /*
       * Now construct HDR2 label
       */
      memset(label, ' ', sizeof(label));
      SerBegin(label, sizeof(label));
      SerBytes(labels[type], 3);
      SerBytes("2D3200032000", 12);

      /*
       * Write HDR2 label
       */
      if (label_type == B_IBM_LABEL) {
         label[4] = 'V';
         AsciiToEbcdic(label, label, sizeof(label));
      }
      status = dev->write(label, sizeof(label));
      if (status != sizeof(label)) {
         BErrNo be;
         if (status == -1) {
            dev->clrerror(-1);
            if (dev->dev_errno == 0) {
               dev->dev_errno = ENOSPC; /* out of space */
            }
            if (dev->dev_errno != ENOSPC) {
               Jmsg1(jcr, M_FATAL, 0, _("Could not write ANSI HDR1 label. ERR=%s\n"),
               be.bstrerror());
               return false;
            }
            dev->weof(1);
            return true;
         } else {
            Jmsg(jcr, M_FATAL, 0, _("Could not write ANSI HDR1 label.\n"));
            return false;
         }
      }
      if (!dev->weof(1)) {
         Jmsg(jcr, M_FATAL, 0, _("Error writing EOF to tape. ERR=%s"), dev->errmsg);
         return false;
      }
      return true;
   default:
      Jmsg0(jcr, M_ABORT, 0, _("write_ansi_ibm_label called for non-ANSI/IBM type\n"));
      return false; /* should not get here */
   }
}

/**
 * Check a Bareos Volume name against an ANSI Volume name
 */
static bool SameLabelNames(char *bareos_name, char *ansi_name)
{
   char *a = ansi_name;
   char *b = bareos_name;

   /*
    * Six characters max
    */
   for (int i=0; i < 6; i++) {
      if (*a == *b) {
         a++;
         b++;
         continue;
      }
      /*
       * ANSI labels are blank filled, Bareos's are zero terminated
       */
      if (*a == ' ' && *b == 0) {
         return true;
      }

      return false;
   }

   /*
    * Reached 6 characters
    */
   b++;
   if (*b == 0) {
      return true;
   }

   return false;
}

/**
 * ANSI date
 *  ' 'YYDDD
 */
static char *ansi_date(time_t td, char *buf)
{
   struct tm *tm;

   if (td == 0) {
      td = time(NULL);
   }
   tm = gmtime(&td);
   Bsnprintf(buf, 10, " %05d ", 1000 * (tm->tm_year + 1900 - 2000) + tm->tm_yday);

   return buf;
}
