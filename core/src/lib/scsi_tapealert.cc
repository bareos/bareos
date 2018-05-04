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
