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

DLL_IMP_EXP bool GetTapealertFlags(int fd, const char *device_name, uint64_t *flags);


#endif /* BAREOS_LIB_SCSI_TAPEALERT_H_ */
