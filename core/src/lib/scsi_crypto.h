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
 * Marco van Wieringen, March 2012
 */

#ifndef BAREOS_LIB_SCSI_CRYPTO_H_
#define BAREOS_LIB_SCSI_CRYPTO_H_ 1

/*
 * Include the SCSI Low Level Interface functions and definitions.
 */
#include "scsi_lli.h"

#define SPP_SP_PROTOCOL_TDE       0x20

#define SPP_KEY_LENGTH            0x20 /* 32 bytes */
#define SPP_DESCRIPTOR_LENGTH     1024
#define SPP_PAGE_DES_LENGTH       24
#define SPP_PAGE_NBES_LENGTH      16
#define SPP_KAD_HEAD_LENGTH       4
#define SPP_PAGE_ALLOCATION       8192
#define SPP_UKAD_LENGTH           0x1e

/*
 * SCSI CDB opcodes
 */
enum {
   SCSI_SPIN_OPCODE = 0xa2,
   SCSI_SPOUT_OPCODE = 0xb5
};

/*
 * SCSI SPIN pagecodes.
 */
enum {
   SPIN_TAPE_DATA_ENCR_IN_SUP_PAGE = 0x00,     /* Tape Data Encryption In Support page */
   SPIN_TAPE_DATE_ENCR_OUT_SUP_PAGE = 0x01,    /* Tape Data Encryption Out Support page */
   SPIN_DATA_ENCR_CAP_PAGE = 0x10,             /* Data Encryption Capabilities page */
   SPIN_SUP_KEY_FORMATS_PAGE = 0x11,           /* Supported Key Formats page */
   SPIN_DATA_ENCR_MGMT_CAP_PAGE = 0x12,        /* Data Encryption Management Capabilities page */
   SPIN_DATA_ENCR_STATUS_PAGE = 0x20,          /* Data Encryption Status page */
   SPIN_NEXT_BLOCK_ENCR_STATUS_PAGE = 0x21,    /* Next Block Encryption Status page */
   SPIN_RANDOM_NUM_PAGE = 0x30,                /* Random Number page */
   SPIN_DEV_SVR_KEY_WRAP_PUB_KEY_PAGE = 0x31   /* Device Server Key Wrapping Public Key page */
};

/*
 * SCSI SPOUT pagecodes.
 */
enum {
   SPOUT_SET_DATA_ENCRYPTION_PAGE = 0x10,      /* Set Data Encryption page */
   SPOUT_SA_ENCAP_PAGE = 0x11                  /* SA Encapsulation page */
};

/*
 * SPP SCSI Control Descriptor Block
 */
typedef struct {
   uint8_t opcode;                             /* Operation Code See SCSI_*_OPCODE */
   uint8_t scp;                                /* Security Protocol */
   uint8_t scp_specific[2];                    /* Security Protocol Specific, 2 bytes MSB/LSB */
   uint8_t res_bits_1[2];                      /* Reserved, 2 bytes */
   uint8_t allocation_length[4];               /* Allocation Length, 4 bytes, 2 bytes MSB and 2 bytes LSB */
   uint8_t res_bits_2;                         /* Reserved, 1 byte */
   uint8_t control_byte;                       /* Control Byte */
} SPP_SCSI_CDB;

/*
 * Generic SPP Page Buffer
 */
typedef struct {
   uint8_t pageCode[2];
   uint8_t length[2];
   uint8_t buffer[SPP_PAGE_ALLOCATION];
} SPP_PAGE_BUFFER;

/*
 * Nexus Scopes
 */
enum {
   SPP_NEXUS_SC_PUBLIC = 0,                    /* All fields other than the scope field and LOCK bit shall be ignored.
                                                  The I_T nexus shall use data encryption parameters that are shared
                                                  by other I_T nexuses. If no I_T nexuses are sharing data encryption
                                                  parameters, the device server shall use default data encryption parameters. */
   SPP_NEXUS_SC_LOCAL = 1,                     /* The data encryption parameters are unique to the I_T nexus associated
                                                  with the SECURITY PROTOCOL OUT command and shall not be shared
                                                  with other I_T nexuses.*/
   SPP_NEXUS_SC_ALL_I_T_NEXUS = 2              /* The data encryption parameters shall be shared with all I_T nexuses. */
};

/*
 * Check External Encryption Mode
 */
enum {
   SPP_CEEM_VENDOR_SPECIFIC = 0,               /* Vendor specific */
   SPP_CEEM_NO_ENCR_CHECK = 1,                 /* Do not check the encryption mode that was in use when the block
                                                  was written to the medium.*/
   SPP_CEEM_CHECK_EXTERNAL = 2,                /* On read and verify commands, check the encryption mode that
                                                  was in use when the block was written to the medium. Report an
                                                  error if the block was written in EXTERNAL mode */
   SPP_CEEM_CHECK_ENCR = 3                     /* On read and verify commands, check the encryption mode that
                                                  was in use when the block was written to the medium. Report
                                                  an error if the block was written in ENCRYPT mode */
};

/*
 * Raw Decryption Mode Control
 */
enum {
   SPP_RDMC_DEFAULT = 0,                       /* The device server shall mark each encrypted block per the default
                                                  setting for the algorithm */
   SPP_RDMC_UNPROTECT = 2,                     /* The device server shall mark each encrypted block written to the
                                                  medium in a format specific manner as enabled for raw decryption
                                                  mode operations. */
   SPP_RDMC_PROTECT = 3                        /* The device server shall mark each encrypted block written to the
                                                  medium in a format specific manner as disabled for raw
                                                  decryption mode operations. */
};

/*
 * Encryption Modes.
 */
enum {
   SPP_ENCR_MODE_DISABLE = 0,                  /* Data encryption is disabled. */
   SPP_ENCR_MODE_EXTERNAL = 1,                 /* The data associated with the WRITE(6) and WRITE(16) commands has been
                                                  encrypted by a system that is compatible with the algorithm specified
                                                  by the ALGORITHM INDEX field. */
   SPP_ENCR_MODE_ENCRYPT = 2                   /* The device server shall encrypt all data that it receives for a
                                                  WRITE(6) or WRITE(16) command using the algorithm specified in the
                                                  ALGORITHM INDEX field and the key specified in the KEY field. */
};

/*
 * Decryption Modes.
 */
enum {
   SPP_DECR_MODE_DISABLE = 0,                  /* Data decryption is disabled. If the device server encounters an
                                                  encrypted logical block while reading, it shall not allow access
                                                  to the data. */
   SPP_DECR_MODE_RAW = 1,                      /* Data decryption is disabled. If the device server encounters an
                                                  encrypted logical block while reading, it shall pass the encrypted
                                                  block to the host without decrypting it. The encrypted block
                                                  may contain data that is not user data. */
   SPP_DECR_MODE_DECRYPT = 2,                  /* The device server shall decrypt all data that is read from the medium
                                                  when processing a READ(6), READ(16), READ REVERSE(6), READ REVERSE(16),
                                                  or RECOVER BUFFERED DATA command or verified when processing a
                                                  VERIFY(6) or VERIFY(16) command. The data shall be decrypted
                                                  using the algorithm specified in the ALGORITHM INDEX field and
                                                  the key specified in the KEY field */
   SPP_DECR_MODE_MIXED = 3                     /* The device server shall decrypt all data that is read from the
                                                  medium that the device server determines was encrypted when processing
                                                  a READ(6), READ(16), READ REVERSE(6), READ REVERSE(16), or
                                                  RECOVER BUFFERED DATA command or verified when processing a
                                                  VERIFY(6) or VERIFY(16) command. The data shall be decrypted
                                                  using the algorithm specified in the ALGORITHM INDEX
                                                  field and the key specified in the KEY field. If the device
                                                  server encounters unencrypted data when processing a READ(6),
                                                  READ(16), READ REVERSE(6), READ REVERSE(16), RECOVER BUFFERED DATA,
                                                  VERIFY(6), or VERIFY(16) command, the data shall be processed
                                                  without decrypting */
};

/*
 * Key Format Types.
 */
enum {
   SPP_KAD_KEY_FORMAT_NORMAL = 0,              /* The KEY field contains the key to be used to encrypt or decrypt data. */
   SPP_KAD_KEY_FORMAT_REFERENCE = 1,           /* The KEY field contains a vendor-specific key reference. */
   SPP_KAD_KEY_FORMAT_WRAPPED = 2,             /* The KEY field contains the key wrapped by the device server public key. */
   SPP_KAD_KEY_FORMAT_ESP_SCSI = 3             /* The KEY field contains a key that is encrypted using ESP-SCSI. */
};


/*
 * Key Descriptor Types
 */
enum {
   SPP_KAD_KEY_DESC_UKAD = 0,                  /* Unauthenticated key-associated data */
   SPP_KAD_KEY_DESC_AKAD = 1,                  /* Authenticated key-associated data */
   SPP_KAD_KEY_DESC_NONCE = 2,                 /* Nonce value */
   SPP_KAD_KEY_DESC_META = 3                   /* Metadata key-associated data */
};

/*
 * SPOUT Page Set Data Encryption (0x10)
 */
typedef struct {
   uint8_t pageCode[2];                        /* Page Code, 2 bytes MSB/LSB */
   uint8_t length[2];                          /* Page Length, 2 bytes MSB/LSB */
#if HAVE_BIG_ENDIAN
   uint8_t nexusScope:3;                       /* Scope, See SPP_NEXUS_SC_* */
   uint8_t res_bits_1:4;                       /* Reserved, 4 bits */
   uint8_t lock:1;                             /* Lock bit */
   uint8_t CEEM:2;                             /* Check External Encryption Mode, See SPP_CEEM_* */
   uint8_t RDMC:2;                             /* Raw Decryption Mode Control, See SPP_RDMC_* */
   uint8_t SDK:1;                              /* Supplemental Decryption Key */
   uint8_t CKOD:1;                             /* Clear Key On Demount */
   uint8_t CKORP:1;                            /* Clear Key On Reservation Preempt */
   uint8_t CKORL:1;                            /* Clear Key On Reservation Lost */
#else
   uint8_t lock:1;                             /* Lock bit */
   uint8_t res_bits_1:4;                       /* Reserved, 4 bits */
   uint8_t nexusScope:3;                       /* Scope, See SPP_NEXUS_SC_* */
   uint8_t CKORL:1;                            /* Clear Key On Reservation Lost */
   uint8_t CKORP:1;                            /* Clear Key On Reservation Preempt */
   uint8_t CKOD:1;                             /* Clear Key On Demount */
   uint8_t SDK:1;                              /* Supplemental Decryption Key */
   uint8_t RDMC:2;                             /* Raw Decryption Mode Control, See SPP_RDMC_* */
   uint8_t CEEM:2;                             /* Check External Encryption Mode, See SPP_CEEM_* */
#endif
   uint8_t encryptionMode;                     /* Encryption Mode, See SPP_ENCR_MODE_* */
   uint8_t decryptionMode;                     /* Decryption Mode, See SPP_DECR_MODE_* */
   uint8_t algorithmIndex;                     /* Algorithm Index */
   uint8_t keyFormat;                          /* Logical Block Encryption Key Format */
   uint8_t kadFormat;                          /* KAD Format, See SPP_KAD_KEY_FORMAT_* */
   uint8_t res_bits_2[7];                      /* Reserved, 7 bytes */
   uint8_t keyLength[2];                       /* Logical Block Encryption Key Length, 2 bytes MSB/LSB */
   uint8_t keyData[SPP_KEY_LENGTH];
} SPP_PAGE_SDE;

enum {
   SPP_PARM_LOG_BLOCK_ENCR_NONE = 0,           /* Logical block encryption parameters control is not reported. */
   SPP_PARM_LOG_BLOCK_ENCR_AME = 1,            /* Logical Block Encryption Parameters are not exclusively
                                                  controlled by external data encryption control. */
   SPP_PARM_LOG_BLOCK_ENCR_DRIVE = 2,          /* Logical block encryption parameters are exclusively
                                                  controlled by the sequential-access device server. */
   SPP_PARM_LOG_BLOCK_LME_ADC = 3,             /* Logical block encryption parameters are exclusively
                                                  controlled by the automation/drive interface device server. */
   SPP_PARM_LOG_BLOCK_UNSUP = 4                /* Not supported. */
};

/*
 * Device Encryption Status Page (0x20)
 */
typedef struct {
   uint8_t pageCode[2];                        /* Page Code, 2 bytes MSB/LSB */
   uint8_t length[2];                          /* Page Length, 2 bytes MSB/LSB */
#if HAVE_BIG_ENDIAN
   uint8_t nexusScope:3;                       /* Scope, See SPP_NEXUS_SC_* */
   uint8_t res_bits_1:2;                       /* Reserved, 2 bits */
   uint8_t keyScope:3;                         /* Logical Block Encryption Scope */
#else
   uint8_t keyScope:3;                         /* Logical Block Encryption Scope */
   uint8_t res_bits_1:2;                       /* Reserved, 2 bits */
   uint8_t nexusScope:3;                       /* Scope, See SPP_NEXUS_SC_* */
#endif
   uint8_t encryptionMode;                     /* Encryption Mode, See SPP_ENCR_MODE_* */
   uint8_t decryptionMode;                     /* Decryption Mode, See SPP_DECR_MODE_* */
   uint8_t algorithmIndex;                     /* Algorithm Index */
   uint8_t keyInstance[4];                     /* Key Instance Counter MSB/LSB */
#if HAVE_BIG_ENDIAN
   uint8_t res_bits_2:1;                       /* Reserved, 1 bit */
   uint8_t parametersControl:3;                /* Logical Block encryption parameters, See SPP_PARM_LOG_BLOCK_* */
   uint8_t VCELB:1;                            /* Volume Contains Encrypted Logical Blocks */
   uint8_t CEEMS:2;                            /* Check External Encryption Mode Status, See SPP_CEEM_* */
   uint8_t RDMD:1;                             /* Raw Decryption Mode Disabled */
#else
   uint8_t RDMD:1;                             /* Raw Decryption Mode Disabled */
   uint8_t CEEMS:2;                            /* Check External Encryption Mode Status, See SPP_CEEM_* */
   uint8_t VCELB:1;                            /* Volume Contains Encrypted Logical Blocks */
   uint8_t parametersControl:3;                /* Logical Block encryption parameters, See SPP_PARM_LOG_BLOCK_* */
   uint8_t res_bits_2:1;                       /* Reserved, 1 bit */
#endif
   uint8_t kadFormat;                          /* KAD Format, See SPP_KAD_KEY_FORMAT_* */
   uint8_t ASDKCount[2];                       /* Available Supplemental Decryption Key MSB/LSB */
   uint8_t res_bits_4[8];                      /* Reserved, 8 bytes */
} SPP_PAGE_DES;

enum {
   SPP_COMP_STATUS_UNKNOWN = 0,                /* The device server is incapable of determining if the logical
                                                  object referenced by the LOGICAL OBJECT NUMBER field has been
                                                  compressed. */
   SPP_COMP_STATUS_UNAVAIL = 1,                /* The device server is capable of determining if the logical
                                                  object referenced by the LOGICAL OBJECT NUMBER field has
                                                  been compressed, but is not able to at this time.
                                                  Possible reasons are:
                                                     a) the next logical block has not yet been read into the buffer;
                                                     b) there was an error reading the next logical block; or
                                                     c) there are no more logical blocks (i.e., end-of-data). */
   SPP_COMP_STATUS_ILLEGAL = 2,                /* The device server has determined that the logical object referenced
                                                  by the LOGICAL OBJECT NUMBER field is not a logical block. */
   SPP_COMP_STATUS_UNCOMPRESSED = 3,           /* The device server has determined that the logical object referenced
                                                  by the LOGICAL OBJECT NUMBER field is not compressed. */
   SPP_COMP_STATUS_COMPRESSED = 4              /* The device server has determined that the logical object referenced
                                                  by the LOGICAL OBJECT NUMBER field is compressed. */
};

enum {
   SPP_ENCR_STATUS_UNKNOWN = 0,                /* The device server is incapable of determining if the logical object
                                                  referenced by the LOGICAL OBJECT NUMBER field has been encrypted. */
   SPP_ENCR_STATUS_UNAVAIL = 1,                /* The device server is capable of determining if the logical object
                                                  referenced by the LOGICAL OBJECT NUMBER field has been encrypted,
                                                  but is not able to at this time. Possible reasons are:
                                                     a) the next logical block has not yet been read into the buffer;
                                                     b) there was an error reading the next logical block; or
                                                     c) there are no more logical blocks (i.e., end-of-data). */
   SPP_ENCR_STATUS_ILLEGAL = 2,                /* The device server has determined that the logical object referenced
                                                  by the LOGICAL OBJECT NUMBER field is not a logical block. */
   SPP_ENCR_STATUS_NOT_ENCRYPTED = 3,          /* The device server has determined that the logical object referenced
                                                  by the LOGICAL OBJECT NUMBER field is not encrypted. */
   SPP_ENCR_STATUS_ENCR_ALG_NOT_SUPP = 4,      /* The device server has determined that the logical object referenced by
                                                  the LOGICAL OBJECT NUMBER field is encrypted by an algorithm that is
                                                  not supported by this device server. The values in the KEY-ASSOCIATED
                                                  DATA DESCRIPTORS field contain information pertaining to the encrypted block. */
   SPP_ENCR_STATUS_ENCRYPTED = 5,              /* The device server has determined that the logical object referenced by
                                                  the LOGICAL OBJECT NUMBER field is encrypted by an algorithm that is
                                                  supported by this device server. The values in the ALGORITHM INDEX and
                                                  KEY-ASSOCIATED DATA DESCRIPTORS fields contain information pertaining
                                                  to the encrypted block. */
   SPP_ENCR_STATUS_ENCR_NOT_AVAIL = 6          /* The device server has determined that the logical object referenced by
                                                  the LOGICAL OBJECT NUMBER field is encrypted by an algorithm that is
                                                  supported by this device server, but the device server is either not
                                                  enabled to decrypt or does not have the correct key or nonce value to
                                                  decrypt the encrypted block. */
};

/*
 * Next Block Encryption Status Page (0x21)
 */
typedef struct {
   uint8_t pageCode[2];                        /* Page Code, 2 bytes MSB/LSB */
   uint8_t length[2];                          /* Page Length, 2 bytes MSB/LSB */
   uint8_t log_obj_num[8];                     /* Logical Object Number */
#if HAVE_BIG_ENDIAN
   uint8_t compressionStatus:4;                /* Compression Status, See SPP_COMPRESS_STATUS_* */
   uint8_t encryptionStatus:4;                 /* Encryption Status, See SPP_ENCR_STATUS_* */
#else
   uint8_t encryptionStatus:4;                 /* Encryption Status, See SPP_ENCR_STATUS_* */
   uint8_t compressionStatus:4;                /* Compression Status, See SPP_COMPRESS_STATUS_* */
#endif
   uint8_t algorithmIndex;                     /* Algorithm Index */
#if HAVE_BIG_ENDIAN
   uint8_t res_bits_1:6;                       /* Reserved, 6 bits */
   uint8_t EMES:1;                             /* Encryption Mode External Status */
   uint8_t RDMDS:1;                            /* Raw Decryption Mode Disabled Status */
#else
   uint8_t RDMDS:1;                            /* Raw Decryption Mode Disabled Status */
   uint8_t EMES:1;                             /* Encryption Mode External Status */
   uint8_t res_bits_1:6;                       /* Reserved, 6 bits */
#endif
   uint8_t nextBlockKADFormat;                 /* Next Block KAD Format, See SPP_KAD_KEY_FORMAT_* */
} SPP_PAGE_NBES;

/*
 * Key Associated Data (KAD) Descriptors
 */
typedef struct {
   uint8_t type;                               /* Key Descriptor Type, See SPP_KAD_KEY_DESC_* */
#if HAVE_BIG_ENDIAN
   uint8_t res_bits_1:5;                       /* Reserved, 5 bits */
   uint8_t authenticated:3;
#else
   uint8_t authenticated:3;
   uint8_t res_bits_1:5;                       /* Reserved, 5 bits */
#endif
   uint8_t descriptorLength[2];                /* Key Descriptor Length MSB/LSB */
   uint8_t descriptor[SPP_DESCRIPTOR_LENGTH];
} SPP_KAD;

DLL_IMP_EXP bool clear_scsi_encryption_key(int fd, const char *device);
DLL_IMP_EXP bool set_scsi_encryption_key(int fd, const char *device, char *encryption_key);
DLL_IMP_EXP int get_scsi_drive_encryption_status(int fd, const char *device_name,
                                     POOLMEM *&status, int indent);
DLL_IMP_EXP int get_scsi_volume_encryption_status(int fd, const char *device_name,
                                      POOLMEM *&status, int indent);
DLL_IMP_EXP bool need_scsi_crypto_key(int fd, const char *device_name, bool use_drive_status);
DLL_IMP_EXP bool is_scsi_encryption_enabled(int fd, const char *device_name);

#endif /* BAREOS_LIB_SCSI_CRYPTO_H_ */
