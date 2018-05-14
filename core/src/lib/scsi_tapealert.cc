/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2013-2013 Planets Communications B.V.
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
 * Low level SCSI interface for SCSI TapeAlert services.
 *
 * Marco van Wieringen, November 2013
 */

#include "include/bareos.h"

/* Forward referenced functions */

#ifdef HAVE_LOWLEVEL_SCSI_INTERFACE

#include "scsi_tapealert.h"

struct tapealert_mapping {
   uint32_t flag;
   const char *alert_msg;
};

static tapealert_mapping tapealert_mappings[] = {
   { 0x01, (const char *)"Having problems reading (slowing down)" },
   { 0x02, (const char *)"Having problems writing (losing capacity)" },
   { 0x03, (const char *)"Uncorrectable read/write error" },
   { 0x04, (const char *)"Media Performance Degraded, Data Is At Risk" },
   { 0x05, (const char *)"Read Failure" },
   { 0x06, (const char *)"Write Failure" },
   { 0x07, (const char *)"Media has reached the end of its useful life" },
   { 0x08, (const char *)"Cartridge not data grade" },
   { 0x09, (const char *)"Cartridge write protected" },
   { 0x0A, (const char *)"Initiator is preventing media removal" },
   { 0x0B, (const char *)"Cleaning media found instead of data media" },
   { 0x0C, (const char *)"Cartridge contains data in an unsupported format" },
   { 0x0D, (const char *)"Recoverable mechanical cartridge failure (broken tape?)" },
   { 0x0E, (const char *)"Unrecoverable mechanical cartridge failure (broken tape?)" },
   { 0x0F, (const char *)"Failure of cartridge memory chip" },
   { 0x10, (const char *)"Forced eject of media" },
   { 0x11, (const char *)"Read only media loaded" },
   { 0x12, (const char *)"Tape directory corrupted on load" },
   { 0x13, (const char *)"Media nearing end of life" },
   { 0x14, (const char *)"Tape drive neads cleaning NOW" },
   { 0x15, (const char *)"Tape drive needs to be cleaned soon" },
   { 0x16, (const char *)"Cleaning cartridge used up" },
   { 0x17, (const char *)"Invalid cleaning cartidge used" },
   { 0x18, (const char *)"Retension requested" },
   { 0x19, (const char *)"Dual port interface failed" },
   { 0x1A, (const char *)"Cooling fan in drive has failed" },
   { 0x1B, (const char *)"Power supply failure in drive" },
   { 0x1C, (const char *)"Power consumption outside specified range" },
   { 0x1D, (const char *)"Preventative maintenance needed on drive" },
   { 0x1E, (const char *)"Hardware A: Tape drive has a problem not read/write related" },
   { 0x1F, (const char *)"Hardware B: Tape drive has a problem not read/write related" },
   { 0x20, (const char *)"Problem with SCSI interface between tape drive and initiator" },
   { 0x21, (const char *)"The current operation has failed. Eject and reload media" },
   { 0x22, (const char *)"Attempt to download new firmware failed" },
   { 0x23, (const char *)"Drive humidity outside operational range" },
   { 0x24, (const char *)"Drive temperature outside operational range" },
   { 0x25, (const char *)"Drive voltage outside operational range" },
   { 0x26, (const char *)"Failure of drive predicted soon" },
   { 0x27, (const char *)"Diagnostics required" },
   // 0x28 to 0x2E are obsolete, but some vendors still have them listed.
   { 0x28, (const char *)"Loader Hardware A: Changer having problems communicating with tape drive" },
   { 0x29, (const char *)"Loader Stray Tape: Stray tape left in drive from prior error" },
   { 0x2A, (const char *)"Loader Hardware B: Autoloader mechanism has a fault" },
   { 0x2B, (const char *)"Loader Door: Loader door is open, please close it" },
   // 0x2F to 0x31 are reserved
   { 0x32, (const char *)"Media statistics lost" },
   { 0x33, (const char *)"Tape directory invalid at unload (reread all to rebuild)" },
   { 0x34, (const char *)"Tape system area write failure" },
   { 0x35, (const char *)"Tape system area read failure" },
   { 0x36, (const char *)"Start of data not found" },
   { 0x37, (const char *)"Media could not be loaded/threaded" },
   { 0x38, (const char *)"Unrecoverable unload failed" },
   { 0x39, (const char *)"Automation interface failure" },
   { 0x3A, (const char *)"Firmware failure" },
   // 0x3B to 0x40 are reserved
   { 0, NULL }
};


/*
 * Store a value as 2 bytes MSB/LSB
 */
static inline void set_2_byte_value(unsigned char *field, int value)
{
   field[0] = (unsigned char)((value & 0xff00) >> 8);
   field[1] = (unsigned char)(value & 0x00ff);
}

bool GetTapealertFlags(int fd, const char *device_name, uint64_t *flags)
{
   LOG_SCSI_CDB cdb;
   TAPEALERT_PAGE_BUFFER cmd_page;
   int cmd_page_len, cdb_len;
   int tapealert_length, cnt;

   *flags = 0;
   cmd_page_len = sizeof(cmd_page);
   memset(&cmd_page, 0, cmd_page_len);

   /*
    * Fill the SCSI CDB.
    */
   cdb_len = sizeof(cdb);
   memset(&cdb, 0, cdb_len);
   cdb.opcode = SCSI_LOG_OPCODE;
   cdb.pagecode = SCSI_TAPE_ALERT_FLAGS;
   set_2_byte_value(cdb.allocation_length, cmd_page_len);

   /*
    * Retrieve the tapealert status.
    */
   if (!RecvScsiCmdPage(fd, device_name,
                          (void *)&cdb, cdb_len,
                          (void *)&cmd_page, cmd_page_len)) {
      return false;
   }

   /*
    * See if we got TapeAlert info back.
    */
   if ((cmd_page.pagecode & 0x3f) != SCSI_TAPE_ALERT_FLAGS) {
      return false;
   }

   tapealert_length = (cmd_page.page_length[0] << 8) + cmd_page.page_length[1];
   if (!tapealert_length) {
      return true;
   }

   /*
    * Walk over all tape alert parameters.
    */
   cnt = 0;
   while (cnt < tapealert_length) {
      uint16_t result_index;
      TAPEALERT_PARAMETER *ta_param;

      ta_param = (TAPEALERT_PARAMETER *)&cmd_page.log_parameters[cnt];
      result_index = (ta_param->parameter_code[0] << 8) + ta_param->parameter_code[1];
      if (result_index > 0 && result_index < MAX_TAPE_ALERTS) {
         /*
          * See if the TapeAlert is raised.
          */
         if (ta_param->parameter_value) {
            for (int j = 0; tapealert_mappings[j].alert_msg; j++) {
               if (result_index == tapealert_mappings[j].flag) {
                  Dmsg2(100, "TapeAlert [%d] set ==> %s\n", result_index, tapealert_mappings[j].alert_msg);
                  SetBit(result_index, (char *)flags);
               }
            }
         }
      }

      cnt += ((sizeof(TAPEALERT_PARAMETER) - 1) + ta_param->parameter_length);
   }

   return false;
}
#else
bool GetTapealertFlags(int fd, const char *device_name, uint64_t *flags)
{
   *flags = 0;
   return false;
}
#endif /* HAVE_LOWLEVEL_SCSI_INTERFACE */
