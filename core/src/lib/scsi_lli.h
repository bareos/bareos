/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2011-2012 Planets Communications B.V.
   Copyright (C) 2013-2018 Bareos GmbH & Co. KG

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
 * Marco van Wieringen, March 2012
 */

#ifndef BAREOS_LIB_SCSI_LLI_H_
#define BAREOS_LIB_SCSI_LLI_H_ 1

/*
 * Device Inquiry Response
 */
typedef struct {
#if HAVE_BIG_ENDIAN
  uint8_t peripheralQualifier : 3;
  uint8_t periphrealDeviceType : 5;
  uint8_t RMB : 1;
  uint8_t res_bits_1 : 7;
#else
  uint8_t periphrealDeviceType : 5;
  uint8_t peripheralQualifier : 3;
  uint8_t res_bits_1 : 7;
  uint8_t RMB : 1;
#endif
  uint8_t Version[1];
#if HAVE_BIG_ENDIAN
  uint8_t obs_bits_1 : 2;
  uint8_t NORMACA : 1;
  uint8_t HISUP : 1;
  uint8_t responseDataFormat : 4;
#else
  uint8_t responseDataFormat : 4;
  uint8_t HISUP : 1;
  uint8_t NORMACA : 1;
  uint8_t obs_bits_1 : 2;
#endif
  uint8_t additionalLength[1];
#if HAVE_BIG_ENDIAN
  uint8_t SCCS : 1;
  uint8_t ACC : 1;
  uint8_t TPGS : 2;
  uint8_t threePC : 1;
  uint8_t res_bits_2 : 2;
  uint8_t protect : 1;
  uint8_t obs_bits_2 : 1;
  uint8_t ENCSERV : 1;
  uint8_t VS : 1;
  uint8_t MULTIP : 1;
  uint8_t MCHNGR : 1;
  uint8_t obs_bits_3 : 2;
  uint8_t ADDR16 : 1;
  uint8_t obs_bits_4 : 2;
  uint8_t WBUS16 : 1;
  uint8_t SYNC : 1;
  uint8_t obs_bits_5 : 2;
  uint8_t CMDQUE : 1;
  uint8_t VS2 : 1;
#else
  uint8_t protect : 1;
  uint8_t res_bits_2 : 2;
  uint8_t threePC : 1;
  uint8_t TPGS : 2;
  uint8_t ACC : 1;
  uint8_t SCCS : 1;
  uint8_t ADDR16 : 1;
  uint8_t obs_bits_3 : 2;
  uint8_t MCHNGR : 1;
  uint8_t MULTIP : 1;
  uint8_t VS : 1;
  uint8_t ENCSERV : 1;
  uint8_t obs_bits_2 : 1;
  uint8_t VS2 : 1;
  uint8_t CMDQUE : 1;
  uint8_t obs_bits_5 : 2;
  uint8_t SYNC : 1;
  uint8_t WBUS16 : 1;
  uint8_t obs_bits_4 : 2;
#endif
  uint8_t vendor[8];
  uint8_t productID[16];
  uint8_t productRev[4];
  uint8_t SN[7];
  uint8_t venderUnique[12];
#if HAVE_BIG_ENDIAN
  uint8_t res_bits_3 : 4;
  uint8_t CLOCKING : 2;
  uint8_t QAS : 1;
  uint8_t IUS : 1;
#else
  uint8_t IUS : 1;
  uint8_t QAS : 1;
  uint8_t CLOCKING : 2;
  uint8_t res_bits_3 : 4;
#endif
  uint8_t res_bits_4[1];
  uint8_t versionDescriptor[16];
  uint8_t res_bits_5[22];
  uint8_t copyright[1];
} SCSI_PAGE_INQ;

/*
 * Sense Data Response
 */
typedef struct {
#if HAVE_BIG_ENDIAN
  uint8_t valid : 1;
  uint8_t responseCode : 7;
#else
  uint8_t responseCode : 7;
  uint8_t valid : 1;
#endif
  uint8_t res_bits_1;
#if HAVE_BIG_ENDIAN
  uint8_t filemark : 1;
  uint8_t EOM : 1;
  uint8_t ILI : 1;
  uint8_t res_bits_2 : 1;
  uint8_t senseKey : 4;
#else
  uint8_t senseKey : 4;
  uint8_t res_bits_2 : 1;
  uint8_t ILI : 1;
  uint8_t EOM : 1;
  uint8_t filemark : 1;
#endif
  uint8_t information[4];
  uint8_t addSenseLen;
  uint8_t cmdSpecificInfo[4];
  uint8_t addSenseCode;
  uint8_t addSenseCodeQual;
  uint8_t fieldRepUnitCode;
#if HAVE_BIG_ENDIAN
  uint8_t sim : 3; /* system information message */
  uint8_t bpv : 1; /* bit pointer valid */
  uint8_t resvd2 : 2;
  uint8_t cd : 1; /* control/data */
  uint8_t SKSV : 1;
#else
  uint8_t SKSV : 1;
  uint8_t cd : 1; /* control/data */
  uint8_t resvd2 : 2;
  uint8_t bpv : 1; /* bit pointer valid */
  uint8_t sim : 3; /* system information message */
#endif
  uint8_t field[2]; /* field pointer */
  uint8_t addSenseData[109];
} SCSI_PAGE_SENSE;

bool RecvScsiCmdPage(int fd,
                     const char* device_name,
                     void* cdb,
                     unsigned int cdb_len,
                     void* cmd_page,
                     unsigned int cmd_page_len);
bool send_scsi_cmd_page(int fd,
                        const char* device_name,
                        void* cdb,
                        unsigned int cdb_len,
                        void* cmd_page,
                        unsigned int cmd_page_len);
bool CheckScsiAtEod(int fd);
#endif /* BAREOS_LIB_SCSI_LLI_H_ */
