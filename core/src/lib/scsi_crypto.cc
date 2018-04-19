/*
   BAREOS® - Backup Archiving REcovery Open Sourced

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
 * Low level SCSI interface for SCSI crypto services
 *
 * Marco van Wieringen, March 2012
 */

#include "include/bareos.h"

/* Forward referenced functions */

static void indent_status_msg(POOLMEM *&status, const char *msg, int indent);

#include "lib/scsi_crypto.h"

#ifdef HAVE_LOWLEVEL_SCSI_INTERFACE

/*
 * Store a value as 2 bytes MSB/LSB
 */
static inline void set_2_byte_value(unsigned char *field, int value)
{
   field[0] = (unsigned char)((value & 0xff00) >> 8);
   field[1] = (unsigned char)(value & 0x00ff);
}

/*
 * Store a value as 4 bytes MSB/LSB
 */
static inline void set_4_byte_value(unsigned char *field, int value)
{
   field[0] = (unsigned char)((value & 0xff000000) >> 24);
   field[1] = (unsigned char)((value & 0x00ff0000) >> 16);
   field[2] = (unsigned char)((value & 0x0000ff00) >> 8);
   field[3] = (unsigned char)(value & 0x000000ff);
}

/*
 * Clear an encryption key used by a tape drive
 * using a lowlevel SCSI interface.
 *
 * The device is send a:
 * - SPOUT Security Protocol OUT SCSI CDB. (0xB5)
 * - SPOUT Send Encryption Key Page. (0x10)
 *
 * The Send Encryption Key page has the encryption
 * and decryption set to disabled and the key is empty.
 */
bool clear_scsi_encryption_key(int fd, const char *device_name)
{
   /*
    * Create a SPOUT Set Encryption Key CDB and
    *        a SPOUT Clear Encryption Mode Page
    */
   SPP_SCSI_CDB cdb;
   SPP_PAGE_BUFFER cmd_page;
   SPP_PAGE_SDE *sps;
   int cmd_page_len, cdb_len;

   /*
    * Put a SPOUT Set Data Encryption page into the start of
    * the generic cmd_page structure.
    */
   memset(&cmd_page, 0, sizeof(cmd_page));
   sps = (SPP_PAGE_SDE *)&cmd_page;
   set_2_byte_value(sps->pageCode, SPOUT_SET_DATA_ENCRYPTION_PAGE);
   sps->nexusScope = SPP_NEXUS_SC_ALL_I_T_NEXUS;
   sps->encryptionMode = SPP_ENCR_MODE_DISABLE;
   sps->decryptionMode = SPP_DECR_MODE_DISABLE;
   sps->algorithmIndex = 0x01;
   sps->kadFormat = SPP_KAD_KEY_FORMAT_NORMAL;
   set_2_byte_value(sps->keyLength, SPP_KEY_LENGTH);

   /*
    * Set the length to the size of the SPP_PAGE_SDE we just filled.
    */
   cmd_page_len = sizeof(SPP_PAGE_SDE);

   /*
    * Set the actual size of the cmd_page - 4 into the cmd_page length field
    * (without the pageCode and pageLength)
    */
   set_2_byte_value(cmd_page.length, cmd_page_len - 4);

   /*
    * Fill the SCSI CDB.
    */
   cdb_len = sizeof(cdb);
   memset(&cdb, 0, cdb_len);
   cdb.opcode = SCSI_SPOUT_OPCODE;
   cdb.scp = SPP_SP_PROTOCOL_TDE;
   set_2_byte_value(cdb.scp_specific, SPOUT_SET_DATA_ENCRYPTION_PAGE);
   set_4_byte_value(cdb.allocation_length, cmd_page_len);

   /*
    * Clear the encryption key.
    */
   return send_scsi_cmd_page(fd, device_name,
                            (void *)&cdb, cdb_len,
                            (void *)&cmd_page, cmd_page_len);
}

/*
 * Set an encryption key used by a tape drive
 * using a lowlevel SCSI interface.
 *
 * The device is send a:
 * - SPOUT Security Protocol OUT SCSI CDB. (0xB5)
 * - SPOUT Send Encryption Key Page. (0x10)
 *
 * The Send Encryption Key page has the encryption
 * and decryption set to not disabled and the key is filled.
 */
bool set_scsi_encryption_key(int fd, const char *device_name, char *encryption_key)
{
   /*
    * Create a SPOUT Set Encryption Key CDB and
    *        a SPOUT Send Encryption Key Page
    */
   SPP_SCSI_CDB cdb;
   SPP_PAGE_BUFFER cmd_page;
   SPP_PAGE_SDE *sps;
   int cmd_page_len, cdb_len;

   /*
    * Put a SPOUT Set Data Encryption page into the start of
    * the generic cmd_page structure.
    */
   memset(&cmd_page, 0, sizeof(cmd_page));
   sps = (SPP_PAGE_SDE *)&cmd_page;
   set_2_byte_value(sps->pageCode, SPOUT_SET_DATA_ENCRYPTION_PAGE);
   sps->nexusScope = SPP_NEXUS_SC_ALL_I_T_NEXUS;
   sps->encryptionMode = SPP_ENCR_MODE_ENCRYPT;
   sps->decryptionMode = SPP_DECR_MODE_MIXED;
   sps->algorithmIndex = 0x01;
   sps->kadFormat = SPP_KAD_KEY_FORMAT_NORMAL;
   set_2_byte_value(sps->keyLength, SPP_KEY_LENGTH);
   bstrncpy((char *)sps->keyData, encryption_key, SPP_KEY_LENGTH);

   /*
    * Set the length to the size of the SPP_PAGE_SDE we just filled.
    */
   cmd_page_len = sizeof(SPP_PAGE_SDE);

   /*
    * Set the actual size of the cmd_page - 4 into the cmd_page length field
    * (without the pageCode and pageLength)
    */
   set_2_byte_value(cmd_page.length, cmd_page_len - 4);

   /*
    * Fill the SCSI CDB.
    */
   cdb_len = sizeof(cdb);
   memset(&cdb, 0, cdb_len);
   cdb.opcode = SCSI_SPOUT_OPCODE;
   cdb.scp = SPP_SP_PROTOCOL_TDE;
   set_2_byte_value(cdb.scp_specific, SPOUT_SET_DATA_ENCRYPTION_PAGE);
   set_4_byte_value(cdb.allocation_length, cmd_page_len);

   /*
    * Set the new encryption key.
    */
   return send_scsi_cmd_page(fd, device_name,
                            (void *)&cdb, cdb_len,
                            (void *)&cmd_page, cmd_page_len);
}

/*
 * Request the encryption state of a drive
 * using a lowlevel SCSI interface.
 *
 * The device is send a:
 * - SPIN Security Protocol IN SCSI CDB. (0xA2)
 * - SPIN Get Data Encryption Status page. (0x20)
 *
 * The return data is interpreted and a status report is build.
 */
int get_scsi_drive_encryption_status(int fd, const char *device_name,
                                     POOLMEM *&status, int indent)
{
   SPP_SCSI_CDB cdb;
   SPP_PAGE_BUFFER cmd_page;
   SPP_PAGE_DES *spd;
   int cmd_page_len, cdb_len;

   cmd_page_len = sizeof(cmd_page);
   memset(&cmd_page, 0, cmd_page_len);

   /*
    * Fill the SCSI CDB.
    */
   cdb_len = sizeof(cdb);
   memset(&cdb, 0, cdb_len);
   cdb.opcode = SCSI_SPIN_OPCODE;
   cdb.scp = SPP_SP_PROTOCOL_TDE;
   set_2_byte_value(cdb.scp_specific, SPIN_DATA_ENCR_STATUS_PAGE);
   set_4_byte_value(cdb.allocation_length, cmd_page_len);

   /*
    * Retrieve the drive encryption status.
    */
   if (!recv_scsi_cmd_page(fd, device_name,
                          (void *)&cdb, cdb_len,
                          (void *)&cmd_page, cmd_page_len)) {
      return 0;
   }

   /*
    * We got a response which should contain a SPP_PAGE_DES.
    * Create a pointer to the beginning of the generic
    * cmd_page structure.
    */
   spd = (SPP_PAGE_DES *)&cmd_page;

   pm_strcpy(status, "");
   indent_status_msg(status, _("Drive encryption status:\n"), indent);

   /*
    * See what encrption mode is enabled.
    */
   switch (spd->encryptionMode) {
   case SPP_ENCR_MODE_DISABLE:
      indent_status_msg(status,
                        _("Encryption Mode: Disabled\n"),
                        indent + 3);
      break;
   case SPP_ENCR_MODE_EXTERNAL:
      indent_status_msg(status,
                        _("Encryption Mode: External\n"),
                        indent + 3);
      break;
   case SPP_ENCR_MODE_ENCRYPT:
      indent_status_msg(status,
                        _("Encryption Mode: Encrypt\n"),
                        indent + 3);
      break;
   default:
      break;
   }

   /*
    * See what decryption mode is enabled.
    */
   switch (spd->decryptionMode) {
   case SPP_DECR_MODE_DISABLE:
      indent_status_msg(status,
                        _("Decryption Mode: Disabled\n"),
                        indent + 3);
      break;
   case SPP_DECR_MODE_RAW:
      indent_status_msg(status,
                        _("Decryption Mode: Raw\n"),
                        indent + 3);
      break;
   case SPP_DECR_MODE_DECRYPT:
      indent_status_msg(status,
                        _("Decryption Mode: Decrypt\n"),
                        indent + 3);
      break;
   case SPP_DECR_MODE_MIXED:
      indent_status_msg(status,
                        _("Decryption Mode: Mixed\n"),
                        indent + 3);
      break;
   default:
      break;
   }

   /*
    * See if RDMD is enabled.
    */
   if (spd->RDMD) {
      indent_status_msg(status,
                        _("Raw Decryption Mode Disabled (RDMD): Enabled\n"),
                        indent + 3);
   } else {
      indent_status_msg(status,
                        _("Raw Decryption Mode Disabled (RDMD): Disabled\n"),
                        indent + 3);
   }

   /*
    * See what Check External Encryption Mode Status is set.
    */
   switch (spd->CEEMS) {
   case SPP_CEEM_NO_ENCR_CHECK:
      indent_status_msg(status,
                        _("Check External Encryption Mode Status (CEEMS) : No\n"),
                        indent + 3);
      break;
   case SPP_CEEM_CHECK_EXTERNAL:
      indent_status_msg(status,
                        _("Check External Encryption Mode Status (CEEMS) : External\n"),
                        indent + 3);
      break;
   case SPP_CEEM_CHECK_ENCR:
      indent_status_msg(status,
                        _("Check External Encryption Mode Status (CEEMS) : Encrypt\n"),
                        indent + 3);
      break;
   default:
      break;
   }

   /*
    * See if VCELB is enabled.
    */
   if (spd->VCELB) {
      indent_status_msg(status,
                        _("Volume Contains Encrypted Logical Blocks (VCELB): Enabled\n"),
                        indent + 3);
   } else {
      indent_status_msg(status,
                        _("Volume Contains Encrypted Logical Blocks (VCELB): Disabled\n"),
                        indent + 3);
   }

   /*
    * See what is providing the encryption keys.
    */
   switch (spd->parametersControl) {
   case SPP_PARM_LOG_BLOCK_ENCR_NONE:
      indent_status_msg(status,
                        _("Logical Block encryption parameters: No report\n"),
                        indent + 3);
      break;
   case SPP_PARM_LOG_BLOCK_ENCR_AME:
      indent_status_msg(status,
                        _("Logical Block encryption parameters: Application Managed\n"),
                        indent + 3);
      break;
   case SPP_PARM_LOG_BLOCK_ENCR_DRIVE:
      indent_status_msg(status,
                        _("Logical Block encryption parameters: Drive Managed\n"),
                        indent + 3);
      break;
   case SPP_PARM_LOG_BLOCK_LME_ADC:
      indent_status_msg(status,
                        _("Logical Block encryption parameters: Library/Key Management Appliance Managed\n"),
                        indent + 3);
      break;
   case SPP_PARM_LOG_BLOCK_UNSUP:
      indent_status_msg(status,
                        _("Logical Block encryption parameters: Unsupported\n"),
                        indent + 3);
      break;
   default:
      break;
   }

   /*
    * Only when both encryption and decryption are disabled skip the KAD Format field.
    */
   if (spd->encryptionMode != SPP_ENCR_MODE_DISABLE &&
       spd->decryptionMode != SPP_DECR_MODE_DISABLE) {
      switch (spd->kadFormat) {
      case SPP_KAD_KEY_FORMAT_NORMAL:
         indent_status_msg(status,
                           _("Key Associated Data (KAD) Descriptor: Normal key\n"),
                           indent + 3);
         break;
      case SPP_KAD_KEY_FORMAT_REFERENCE:
         indent_status_msg(status,
                           _("Key Associated Data (KAD) Descriptor: Vendor-specific reference\n"),
                           indent + 3);
         break;
      case SPP_KAD_KEY_FORMAT_WRAPPED:
         indent_status_msg(status,
                           _("Key Associated Data (KAD) Descriptor: Wrapped public key\n"),
                           indent + 3);
         break;
      case SPP_KAD_KEY_FORMAT_ESP_SCSI:
         indent_status_msg(status,
                           _("Key Associated Data (KAD) Descriptor: Key using ESP-SCSI\n"),
                           indent + 3);
         break;
      default:
         break;
      }
   }

   return strlen(status);
}

/*
 * Request the encryption state of the next block
 * using a lowlevel SCSI interface.
 *
 * The device is send a:
 * - SPIN Security Protocol IN SCSI CDB. (0xA2)
 * - SPIN Get Data Encryption Status page. (0x21)
 *
 * The return data is interpreted and a status report is build.
 */
int get_scsi_volume_encryption_status(int fd, const char *device_name,
                                      POOLMEM *&status, int indent)
{
   SPP_SCSI_CDB cdb;
   SPP_PAGE_BUFFER cmd_page;
   SPP_PAGE_NBES *spnb;
   int cmd_page_len, cdb_len;

   cmd_page_len = sizeof(cmd_page);
   memset(&cmd_page, 0, cmd_page_len);

   /*
    * Fill the SCSI CDB.
    */
   cdb_len = sizeof(cdb);
   memset(&cdb, 0, cdb_len);
   cdb.opcode = SCSI_SPIN_OPCODE;
   cdb.scp = SPP_SP_PROTOCOL_TDE;
   set_2_byte_value(cdb.scp_specific, SPIN_NEXT_BLOCK_ENCR_STATUS_PAGE);
   set_4_byte_value(cdb.allocation_length, cmd_page_len);

   /*
    * Retrieve the volume encryption status.
    */
   if (!recv_scsi_cmd_page(fd, device_name,
                          (void *)&cdb, cdb_len,
                          (void *)&cmd_page, cmd_page_len)) {
      return 0;
   }

   /*
    * We got a response which should contain a SPP_PAGE_NBES.
    * Create a pointer to the beginning of the generic
    * cmd_page structure.
    */
   spnb = (SPP_PAGE_NBES *)&cmd_page;

   pm_strcpy(status, "");
   indent_status_msg(status, _("Volume encryption status:\n"), indent);

   switch (spnb->compressionStatus) {
   case SPP_COMP_STATUS_UNKNOWN:
      indent_status_msg(status,
                        _("Compression Status: Unknown\n"),
                        indent + 3);
      break;
   case SPP_COMP_STATUS_UNAVAIL:
      indent_status_msg(status,
                        _("Compression Status: Unavailable\n"),
                        indent + 3);
      break;
   case SPP_COMP_STATUS_ILLEGAL:
      indent_status_msg(status,
                        _("Compression Status: Illegal logical block\n"),
                        indent + 3);
      break;
   case SPP_COMP_STATUS_UNCOMPRESSED:
      indent_status_msg(status,
                        _("Compression Status: Compression Disabled\n"),
                        indent + 3);
      break;
   case SPP_COMP_STATUS_COMPRESSED:
      indent_status_msg(status,
                        _("Compression Status: Compression Enabled\n"),
                        indent + 3);
      break;
   default:
      break;
   }

   switch (spnb->encryptionStatus) {
   case SPP_ENCR_STATUS_UNKNOWN:
      indent_status_msg(status,
                        _("Encryption Status: Unknown\n"),
                        indent + 3);
      break;
   case SPP_ENCR_STATUS_UNAVAIL:
      indent_status_msg(status,
                        _("Encryption Status: Unavailable\n"),
                        indent + 3);
      break;
   case SPP_ENCR_STATUS_ILLEGAL:
      indent_status_msg(status,
                        _("Encryption Status: Illegal logical block\n"),
                        indent + 3);
      break;
   case SPP_ENCR_STATUS_NOT_ENCRYPTED:
      indent_status_msg(status,
                        _("Encryption Status: Encryption Disabled\n"),
                        indent + 3);
      break;
   case SPP_ENCR_STATUS_ENCR_ALG_NOT_SUPP:
      indent_status_msg(status,
                        _("Encryption Status: Encryption Enabled but with non supported algorithm\n"),
                        indent + 3);
      break;
   case SPP_ENCR_STATUS_ENCRYPTED:
      indent_status_msg(status,
                        _("Encryption Status: Encryption Enabled\n"),
                        indent + 3);
      break;
   case SPP_ENCR_STATUS_ENCR_NOT_AVAIL:
      indent_status_msg(status,
                        _("Encryption Status: Encryption Enabled but no valid key available for decryption\n"),
                        indent + 3);
      break;
   default:
      break;
   }

   if (spnb->RDMDS) {
      indent_status_msg(status,
                        _("Raw Decryption Mode Disabled Status (RDMDS): Enabled\n"),
                        indent + 3);
   } else {
      indent_status_msg(status,
                        _("Raw Decryption Mode Disabled Status (RDMDS): Disabled\n"),
                        indent + 3);
   }

   if (spnb->EMES) {
      indent_status_msg(status,
                        _("Encryption Mode External Status (EMES): Enabled\n"),
                        indent + 3);
   } else {
      indent_status_msg(status,
                        _("Encryption Mode External Status (EMES): Disabled\n"),
                        indent + 3);
   }

   /*
    * Only when the encryption status is set to SPP_ENCR_STATUS_ENCRYPTED we
    * can use the nextBlockKADFormat otherwise that value is bogus.
    */
   if (spnb->encryptionStatus == SPP_ENCR_STATUS_ENCRYPTED) {
      switch (spnb->nextBlockKADFormat) {
      case SPP_KAD_KEY_FORMAT_NORMAL:
         indent_status_msg(status,
                           _("Next Block Key Associated Data (KAD) Descriptor: Normal key\n"),
                           indent + 3);
         break;
      case SPP_KAD_KEY_FORMAT_REFERENCE:
         indent_status_msg(status,
                           _("Next Block Key Associated Data (KAD) Descriptor: Vendor-specific reference\n"),
                           indent + 3);
         break;
      case SPP_KAD_KEY_FORMAT_WRAPPED:
         indent_status_msg(status,
                           _("Next Block Key Associated Data (KAD) Descriptor: Wrapped public key\n"),
                           indent + 3);
         break;
      case SPP_KAD_KEY_FORMAT_ESP_SCSI:
         indent_status_msg(status,
                           _("Next Block Key Associated Data (KAD) Descriptor: Key using ESP-SCSI\n"),
                           indent + 3);
         break;
      default:
         break;
      }
   }

   return strlen(status);
}

/*
 * See if we need a decryption key to read the next block
 * using a lowlevel SCSI interface.
 *
 * The device is send a:
 * - SPIN Security Protocol IN SCSI CDB. (0xA2)
 * - SPIN Get Data Encryption Status page. (0x21)
 */
bool need_scsi_crypto_key(int fd, const char *device_name, bool use_drive_status)
{
   SPP_SCSI_CDB cdb;
   SPP_PAGE_BUFFER cmd_page;
   SPP_PAGE_DES *spd;
   SPP_PAGE_NBES *spnb;
   int cmd_page_len, cdb_len;

   cmd_page_len = sizeof(cmd_page);
   memset(&cmd_page, 0, cmd_page_len);

   /*
    * Fill the SCSI CDB.
    */
   cdb_len = sizeof(cdb);
   memset(&cdb, 0, cdb_len);
   cdb.opcode = SCSI_SPIN_OPCODE;
   cdb.scp = SPP_SP_PROTOCOL_TDE;
   if (use_drive_status) {
      set_2_byte_value(cdb.scp_specific, SPIN_DATA_ENCR_STATUS_PAGE);
   } else {
      set_2_byte_value(cdb.scp_specific, SPIN_NEXT_BLOCK_ENCR_STATUS_PAGE);
   }
   set_4_byte_value(cdb.allocation_length, cmd_page_len);

   /*
    * Retrieve the volume encryption status.
    */
   if (!recv_scsi_cmd_page(fd, device_name,
                          (void *)&cdb, cdb_len,
                          (void *)&cmd_page, cmd_page_len)) {
      return false;
   }

   if (use_drive_status) {
      /*
       * We got a response which should contain a SPP_PAGE_DES.
       * Create a pointer to the beginning of the generic
       * cmd_page structure.
       */
      spd = (SPP_PAGE_DES *)&cmd_page;

      /*
       * Return the status of the Volume Contains Encrypted Logical Blocks (VCELB) field.
       */
      return (spd->VCELB) ? true : false;
   } else {
      /*
       * We got a response which should contain a SPP_PAGE_NBES.
       * Create a pointer to the beginning of the generic
       * cmd_page structure.
       */
      spnb = (SPP_PAGE_NBES *)&cmd_page;

      /*
       * If the encryptionStatus is set to encrypted or encrypted but valid key not available
       * we know we need to load a key to decrypt the data.
       */
      switch (spnb->encryptionStatus) {
      case SPP_ENCR_STATUS_ENCRYPTED:
      case SPP_ENCR_STATUS_ENCR_NOT_AVAIL:
         return true;
      default:
         break;
      }
   }

   return false;
}

/*
 * See if encryption is enabled on a device using a lowlevel SCSI interface.
 *
 * The device is send a:
 * - SPIN Security Protocol IN SCSI CDB. (0xA2)
 * - SPIN Get Data Encryption Status page. (0x20)
 */
bool is_scsi_encryption_enabled(int fd, const char *device_name)
{
   SPP_SCSI_CDB cdb;
   SPP_PAGE_BUFFER cmd_page;
   SPP_PAGE_DES *spd;
   int cmd_page_len, cdb_len;

   cmd_page_len = sizeof(cmd_page);
   memset(&cmd_page, 0, cmd_page_len);

   /*
    * Fill the SCSI CDB.
    */
   cdb_len = sizeof(cdb);
   memset(&cdb, 0, cdb_len);
   cdb.opcode = SCSI_SPIN_OPCODE;
   cdb.scp = SPP_SP_PROTOCOL_TDE;
   set_2_byte_value(cdb.scp_specific, SPIN_DATA_ENCR_STATUS_PAGE);
   set_4_byte_value(cdb.allocation_length, cmd_page_len);

   /*
    * Retrieve the drive encryption status.
    */
   if (!recv_scsi_cmd_page(fd, device_name,
                          (void *)&cdb, cdb_len,
                          (void *)&cmd_page, cmd_page_len)) {
      return false;
   }

   /*
    * We got a response which should contain a SPP_PAGE_DES.
    * Create a pointer to the beginning of the generic
    * cmd_page structure.
    */
   spd = (SPP_PAGE_DES *)&cmd_page;

   /*
    * When either encryptionMode or decryptionMode are not disabled we return true
    */
   return (spd->encryptionMode != SPP_ENCR_MODE_DISABLE) ||
          (spd->decryptionMode != SPP_DECR_MODE_DISABLE);
}

#else

bool clear_scsi_encryption_key(int fd, const char *device_name)
{
   return false;
}

bool set_scsi_encryption_key(int fd, const char *device_name, char *encryption_key)
{
   return false;
}

int get_scsi_drive_encryption_status(int fd, const char *device_name,
                                     POOLMEM *&status, int indent)
{
   pm_strcpy(status, "");
   indent_status_msg(status, _("Drive encryption status: Unknown\n"), indent);
   return strlen(status);
}

int get_scsi_volume_encryption_status(int fd, const char *device_name,
                                      POOLMEM *&status, int indent)
{
   pm_strcpy(status, "");
   indent_status_msg(status, _("Volume encryption status: Unknown\n"), indent);
   return strlen(status);
}

bool need_scsi_crypto_key(int fd, const char *device_name, bool use_drive_status)
{
   return false;
}

bool get_scsi_encryption_enabled(int fd, const char *device_name)
{
   return false;
}
#endif /* HAVE_LOWLEVEL_SCSI_INTERFACE */

static void indent_status_msg(POOLMEM *&status, const char *msg, int indent)
{
   int cnt;
   char indent_level[17];

   /*
    * See if we need to indent the line.
    */
   if (indent > 0) {
      for (cnt = 0; cnt < indent && cnt < 16; cnt++) {
         indent_level[cnt] = ' ';
      }
      indent_level[cnt] = '\0';
      pm_strcat(status, indent_level);
   }

   pm_strcat(status, msg);
}
