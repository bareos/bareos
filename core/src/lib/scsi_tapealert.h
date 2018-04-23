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
 * Marco van Wieringen, November 2013
 */

#ifndef BAREOS_LIB_SCSI_TAPEALERT_H_
#define BAREOS_LIB_SCSI_TAPEALERT_H_ 1

/*
 * Include the SCSI Low Level Interface functions and definitions.
 */
#include "scsi_lli.h"

#define MAX_TAPE_ALERTS 64

/*
 * SCSI CDB opcodes
 */
enum {
   SCSI_LOG_OPCODE = 0x4d
};

enum {
   SCSI_TAPE_ALERT_FLAGS = 0x2e
};

/*
 * SCSI Control Descriptor Block
 */
typedef struct {
   uint8_t opcode;                             /* Operation Code See SCSI_*_OPCODE */
   uint8_t res_bits_1[1];                      /* Reserved, 1 byte */
   uint8_t pagecode;                           /* Page Code, 1 byte */
   uint8_t res_bits_2[2];                      /* Reserved, 2 bytes */
   uint8_t parameter_pointer[2];               /* Parameter Pointer, 2 bytes, 1 bytes MSB and 1 bytes LSB */
   uint8_t allocation_length[2];               /* Allocation Length, 2 bytes, 1 bytes MSB and 1 bytes LSB */
   uint8_t control_byte;                       /* Control Byte */
} LOG_SCSI_CDB;

typedef struct {
   uint8_t pagecode;                           /* Page Code, 1 byte */
   uint8_t res_bits_1[1];                      /* Reserved, 1 byte */
   uint8_t page_length[2];                     /* Page Length, 2 bytes, 1 bytes MSB and 1 bytes LSB */
   uint8_t log_parameters[2044];               /* Log parameters (2048 bytes - 4 bytes header) */
} TAPEALERT_PAGE_BUFFER;

typedef struct {
   uint8_t parameter_code[2];                  /* Parameter Code, 2 bytes */
#if HAVE_BIG_ENDIAN
   uint8_t parameter_length;                   /* Parameter Length, 1 byte */
   uint8_t disable_update:1;                   /* DU: Will be set to 0. The library will always update values reflected by the log parameters. */
   uint8_t disable_save:1;                     /* DS: Will be set to 1. The library does not support saving of log parameters. */
   uint8_t target_save_disabled:1;             /* TSD: Will be set to 0. The library provides a self-defined method for saving log parameters. */
   uint8_t enable_threshold_comparison:1;      /* ETC: Will be set to 0. No comparison to threshold values is made. */
   uint8_t threshold_met_criteria:2;           /* TMC: Will be set to 0. Comparison to threshold values is not supported. */
   uint8_t list_parameter_binary:1;            /* LBIN: This field is only valid if LP is set to 1. When LBIN is set to 0, the list
                                                * parameter is ASCII. When LBIN is set to 1, the list parameter is a binary value. */
   uint8_t list_parameter:1;                   /* LP: This field will be set to 0 for data counters and set to 1 for list parameters */
#else
   uint8_t list_parameter:1;                   /* LP: This field will be set to 0 for data counters and set to 1 for list parameters */
   uint8_t list_parameter_binary:1;            /* LBIN: This field is only valid if LP is set to 1. When LBIN is set to 0, the list
                                                * parameter is ASCII. When LBIN is set to 1, the list parameter is a binary value. */
   uint8_t threshold_met_criteria:2;           /* TMC: Will be set to 0. Comparison to threshold values is not supported. */
   uint8_t enable_threshold_comparison:1;      /* ETC: Will be set to 0. No comparison to threshold values is made. */
   uint8_t target_save_disabled:1;             /* TSD: Will be set to 0. The library provides a self-defined method for saving log parameters. */
   uint8_t disable_save:1;                     /* DS: Will be set to 1. The library does not support saving of log parameters. */
   uint8_t disable_update:1;                   /* DU: Will be set to 0. The library will always update values reflected by the log parameters. */
   uint8_t parameter_length;                   /* Parameter Length, 1 byte */
#endif
   uint8_t parameter_value;                    /* Parameter Value, n bytes */
} TAPEALERT_PARAMETER;

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
   /*
    * 0x28 to 0x2E are obsolete, but some vendors still have them listed.
    */
   { 0x28, (const char *)"Loader Hardware A: Changer having problems communicating with tape drive" },
   { 0x29, (const char *)"Loader Stray Tape: Stray tape left in drive from prior error" },
   { 0x2A, (const char *)"Loader Hardware B: Autoloader mechanism has a fault" },
   { 0x2B, (const char *)"Loader Door: Loader door is open, please close it" },
   /*
    * 0x2F to 0x31 are reserved
    */
   { 0x32, (const char *)"Media statistics lost" },
   { 0x33, (const char *)"Tape directory invalid at unload (reread all to rebuild)" },
   { 0x34, (const char *)"Tape system area write failure" },
   { 0x35, (const char *)"Tape system area read failure" },
   { 0x36, (const char *)"Start of data not found" },
   { 0x37, (const char *)"Media could not be loaded/threaded" },
   { 0x38, (const char *)"Unrecoverable unload failed" },
   { 0x39, (const char *)"Automation interface failure" },
   { 0x3A, (const char *)"Firmware failure" },
     /*
      * 0x3B to 0x40 are reserved
      */
   { 0, NULL }
};


DLL_IMP_EXP bool get_tapealert_flags(int fd, const char *device_name, uint64_t *flags);


#endif /* BAREOS_LIB_SCSI_TAPEALERT_H_ */
