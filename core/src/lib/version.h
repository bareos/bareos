/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2000-2013 Free Software Foundation Europe e.V.
   Copyright (C) 2010-2017 Planets Communications B.V.
   Copyright (C) 2013-2020 Bareos GmbH & Co. KG

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

#ifndef BAREOS_LIB_VERSION_H_
#define BAREOS_LIB_VERSION_H_

#ifdef __cplusplus
extern "C" {
#endif

struct BareosVersionStrings {
  const char* Full;
  const char* Date;
  const char* ShortDate;
  const char* ProgDateTime;
  const char* FullWithDate;
  const char* Year;
  const char* BinaryInfo;
  const char* ServicesMessage;
  const char* JoblogMessage;
  void (*FormatCopyrightWithFsfAndPlanets)(char* out, size_t len, int FsfYear);
  void (*PrintCopyrightWithFsfAndPlanets)(FILE* fh, int FsfYear);
  void (*FormatCopyright)(char* out, size_t len, int StartYear);
  void (*PrintCopyright)(FILE* fh, int StartYear);
};

extern const struct BareosVersionStrings kBareosVersionStrings;

#ifdef __cplusplus
}
#endif

/* If this is set stdout will not be closed on startup */
/* #define DEVELOPER 1 */

/*
 * SMCHECK does orphaned buffer checking (memory leaks)
 *  it can always be turned on, but has some minor performance
 *  penalties.
 */
#ifdef DEVELOPER
#define SMCHECK
#endif

/*
 * for fastest speed but you must have a UPS to avoid unwanted shutdowns
 */
//#define SQLITE3_INIT_QUERY "PRAGMA synchronous = OFF"

/*
 * for more safety, but is 30 times slower than above
 */
#define SQLITE3_INIT_QUERY "PRAGMA synchronous = NORMAL"

/* Debug flags not normally turned on */

/* #define TRACE_JCR_CHAIN 1 */
/* #define TRACE_RES 1 */
/* #define DEBUG_BLOCK_CHECKSUM 1 */

/*
 * Set SMALLOC_SANITY_CHECK to zero to turn off, otherwise
 *  it is the maximum memory malloced before Bareos will
 *  abort.  Except for debug situations, this should be zero
 */
#define SMALLOC_SANITY_CHECK 0 /* 500000000  0.5 GB max */


/* Check if header of tape block is zero before writing */
/* #define DEBUG_BLOCK_ZEROING 1 */

/* The following are turned on for performance testing */
/*
 * If you turn on the NO_ATTRIBUTES_TEST and rebuild, the SD
 *  will receive the attributes from the FD, will write them
 *  to disk, then when the data is written to tape, it will
 *  read back the attributes, but they will not be sent to
 *  the Director. So this will eliminate: 1. the comm time
 *  to send the attributes to the Director. 2. the time it
 *  takes the Director to put them in the catalog database.
 */
/* #define NO_ATTRIBUTES_TEST 1 */

/*
 * If you turn on NO_TAPE_WRITE_TEST and rebuild, the SD
 *  will do all normal actions, but will not write to the
 *  Volume.  Note, this means a lot of functions such as
 *  labeling will not work, so you must use it only when
 *  Bareos is going to append to a Volume. This will eliminate
 *  the time it takes to write to the Volume (not the time
 *  it takes to do any positioning).
 */
/* #define NO_TAPE_WRITE_TEST 1 */

/*
 * If you turn on FD_NO_SEND_TEST and rebuild, the FD will
 *  not send any attributes or data to the SD. This will
 *  eliminate the comm time sending to the SD.
 */
/* #define FD_NO_SEND_TEST 1 */
#endif /* BAREOS_LIB_VERSION_H_ */
