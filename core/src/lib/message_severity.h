/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2000-2011 Free Software Foundation Europe e.V.
   Copyright (C) 2011-2012 Planets Communications B.V.
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

#ifndef BAREOS_LIB_MESSAGE_LEVEL_H_
#define BAREOS_LIB_MESSAGE_LEVEL_H_

#undef M_DEBUG
#undef M_ABORT
#undef M_FATAL
#undef M_ERROR
#undef M_WARNING
#undef M_INFO
#undef M_MOUNT
#undef M_ERROR_TERM
#undef M_TERM
#undef M_RESTORED
#undef M_SECURITY
#undef M_ALERT
#undef M_VOLMGMT
#undef M_AUDIT

/**
 * Most of these message levels are more or less obvious.
 * They have evolved somewhat during the development of BAREOS,
 * and here are some of the details of where I am trying to
 * head (in the process of changing the code) as of 15 June 2002.
 *
 * M_ABORT       BAREOS immediately aborts and tries to produce a traceback
 *               This is for really serious errors like segmentation fault.
 * M_ERROR_TERM  BAREOS immediately terminates but no dump. This is for
 *               "obvious" serious errors like daemon already running or
 *               cannot open critical file, ... where a dump is not wanted.
 * M_TERM        BAREOS daemon shutting down because of request (SIGTERM).
 * M_DEBUG       Debug Messages
 *
 * The remaining apply to Jobs rather than the daemon.
 *
 * M_FATAL       BAREOS detected a fatal Job error. The Job will be killed,
 *               but BAREOS continues running.
 * M_ERROR       BAREOS detected a Job error. The Job will continue running
 *               but the termination status will be error.
 * M_WARNING     Job warning message.
 * M_INFO        Job information message.
 * M_SAVED       Info on saved file
 * M_NOTSAVED    Info on not saved file
 * M_RESTORED    An ls -l of each restored file.
 * M_SKIPPED     File skipped during backup by option setting
 * M_SECURITY    For security viloations. This is equivalent to FATAL.
 * M_ALERT       For Tape Alert messages.
 * M_VOLMGMT     Volume Management message
 * M_AUDIT       Auditing message
 * M_MOUNT       Mount requests
 */
enum
{
  /*
   * Keep M_ABORT=1 for dlist.h
   */
  M_ABORT = 1,
  M_DEBUG,
  M_FATAL,
  M_ERROR,
  M_WARNING,
  M_INFO,
  M_SAVED,
  M_NOTSAVED,
  M_SKIPPED,
  M_MOUNT,
  M_ERROR_TERM,
  M_TERM,
  M_RESTORED,
  M_SECURITY,
  M_ALERT,
  M_VOLMGMT,
  M_AUDIT
};

#define M_MAX M_AUDIT /* keep this updated ! */
#define NR_MSG_TYPES NbytesForBits(M_MAX + 1)

#endif  // BAREOS_LIB_MESSAGE_LEVEL_H_
