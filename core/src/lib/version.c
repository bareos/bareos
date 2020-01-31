/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2019-2019 Bareos GmbH & Co. KG

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
#include <stdio.h>
#include "include/host.h"
#include "version.h"

#if !defined BAREOS_VERSION
#warning should define BAREOS_VERSION when building version.c
#define BAREOS_VERSION "x.y.z"
#endif

#if !defined BAREOS_DATE
#warning should define BAREOS_DATE when building version.c
#define BAREOS_DATE "unknown date"
#endif

#if !defined BAREOS_SHORT_DATE
#warning should define BAREOS_SHORT_DATE when building version.c
#define BAREOS_SHORT_DATE "unknown date"
#endif

#if !defined BAREOS_PROG_DATE_TIME
#warning should define BAREOS_PROG_DATE_TIME when building version.c
#define BAREOS_PROG_DATE_TIME __DATE__ " " __TIME__
#endif

#if !defined BAREOS_YEAR
#warning should define BAREOS_YEAR when building version.c
#define BAREOS_YEAR "2019"
#endif

#if !defined BAREOS_BINARY_INFO
#define BAREOS_BINARY_INFO "self-compiled"
#endif

#if !defined BAREOS_SERVICES_MESSAGE
#define BAREOS_SERVICES_MESSAGE                             \
  "self-compiled binaries are UNSUPPORTED by bareos.com.\n" \
  "Get official binaries and vendor support on https://www.bareos.com"
#endif

#if !defined BAREOS_JOBLOG_MESSAGE
#define BAREOS_JOBLOG_MESSAGE \
  "self-compiled: Get official binaries and vendor support on bareos.com"
#endif

#define BAREOS_COPYRIGHT_MESSAGE_WITH_FSF_AND_PLANETS            \
  "\n%s"                                                         \
  "\n"                                                           \
  "Copyright (C) 2013-%s Bareos GmbH & Co. KG\n"                 \
  "Copyright (C) %d-2012 Free Software Foundation Europe e.V.\n" \
  "Copyright (C) 2010-2017 Planets Communications B.V.\n"        \
  "\n"                                                           \
  "Version: %s (%s) %s %s %s\n"                                  \
  "\n"


static void FormatCopyrightWithFsfAndPlanets(char* out, size_t len, int FsfYear)
{
  snprintf(out, len, BAREOS_COPYRIGHT_MESSAGE_WITH_FSF_AND_PLANETS,
           kBareosVersionStrings.ServicesMessage, kBareosVersionStrings.Year,
           FsfYear, kBareosVersionStrings.Full, kBareosVersionStrings.Date,
           HOST_OS, DISTNAME, DISTVER);
}
static void PrintCopyrightWithFsfAndPlanets(FILE* fh, int FsfYear)
{
  fprintf(fh, BAREOS_COPYRIGHT_MESSAGE_WITH_FSF_AND_PLANETS,
          kBareosVersionStrings.ServicesMessage, kBareosVersionStrings.Year,
          FsfYear, kBareosVersionStrings.Full, kBareosVersionStrings.Date,
          HOST_OS, DISTNAME, DISTVER);
}


#define BAREOS_COPYRIGHT_MESSAGE               \
  "\n%s"                                       \
  "\n"                                         \
  "Copyright (C) %d-%s Bareos GmbH & Co. KG\n" \
  "\n"                                         \
  "Version: %s (%s) %s %s %s\n"                \
  "\n"

static void FormatCopyright(char* out, size_t len, int StartYear)
{
  snprintf(out, len, BAREOS_COPYRIGHT_MESSAGE,
           kBareosVersionStrings.ServicesMessage, StartYear,
           kBareosVersionStrings.Year, kBareosVersionStrings.Full,
           kBareosVersionStrings.Date, HOST_OS, DISTNAME, DISTVER);
}
static void PrintCopyright(FILE* fh, int StartYear)
{
  fprintf(fh, BAREOS_COPYRIGHT_MESSAGE, kBareosVersionStrings.ServicesMessage,
          StartYear, kBareosVersionStrings.Year, kBareosVersionStrings.Full,
          kBareosVersionStrings.Date, HOST_OS, DISTNAME, DISTVER);
}

const struct BareosVersionStrings kBareosVersionStrings = {
    .Full = BAREOS_VERSION,
    .Date = BAREOS_DATE,
    .ShortDate = BAREOS_SHORT_DATE,
    .ProgDateTime = BAREOS_PROG_DATE_TIME,
    .FullWithDate = BAREOS_VERSION " (" BAREOS_DATE ")",
    .Year = BAREOS_YEAR,
    .BinaryInfo = BAREOS_BINARY_INFO,
    .ServicesMessage = BAREOS_SERVICES_MESSAGE,
    .JoblogMessage = BAREOS_JOBLOG_MESSAGE,
    .FormatCopyrightWithFsfAndPlanets = FormatCopyrightWithFsfAndPlanets,
    .PrintCopyrightWithFsfAndPlanets = PrintCopyrightWithFsfAndPlanets,
    .FormatCopyright = FormatCopyright,
    .PrintCopyright = PrintCopyright};
