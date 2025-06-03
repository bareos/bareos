/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2002-2011 Free Software Foundation Europe e.V.
   Copyright (C) 2013-2025 Bareos GmbH & Co. KG

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
 * bpipe.c bi-directional pipe
 *
 * Kern Sibbald, November MMII
 */

#include <sys/wait.h>
#if !defined(HAVE_MSVC)
#  include <unistd.h>
#endif
#include "include/bareos.h"
#include "jcr.h"
#include "lib/berrno.h"
#include "lib/bsys.h"
#include "lib/btimers.h"
#include "lib/util.h"
#include "lib/bpipe.h"

/*
 * Run an external program. Optionally wait a specified number
 * of seconds. Program killed if wait exceeded. Optionally
 * return the output from the program (normally a single line).
 *
 * If the watchdog kills the program, fgets returns, and ferror is set
 * to 1 (=>SUCCESS), so we check if the watchdog killed the program.
 *
 * Contrary to my normal calling conventions, this program
 *
 * Returns: 0 on success
 *          non-zero on error == BErrNo status
 */
int RunProgram(char* prog, int wait, POOLMEM*& results)
{
  Bpipe* bpipe;
  int stat1, stat2;
  char* mode;

  mode = (char*)"r";
  bpipe = OpenBpipe(prog, wait, mode);
  if (!bpipe) { return ENOENT; }

  results[0] = 0;
  int len = SizeofPoolMemory(results) - 1;
  fgets(results, len, bpipe->rfd);
  results[len] = 0;

  if (feof(bpipe->rfd)) {
    stat1 = 0;
  } else {
    stat1 = ferror(bpipe->rfd);
  }

  if (stat1 < 0) {
    BErrNo be;
    Dmsg2(150, "Run program fgets stat=%d ERR=%s\n", stat1,
          be.bstrerror(errno));
  } else if (stat1 != 0) {
    Dmsg1(150, "Run program fgets stat=%d\n", stat1);
    if (bpipe->timer_id) {
      Dmsg1(150, "Run program fgets killed=%d\n", bpipe->timer_id->killed);
      /* NB: I'm not sure it is really useful for RunProgram. Without the
       * following lines RunProgram would not detect if the program was killed
       * by the watchdog. */
      if (bpipe->timer_id->killed) {
        stat1 = ETIME;
        PmStrcpy(results, T_("Program killed by BAREOS (timeout)\n"));
      }
    }
  }
  stat2 = CloseBpipe(bpipe);
  stat1 = stat2 != 0 ? stat2 : stat1;
  Dmsg1(150, "Run program returning %d\n", stat1);

  return stat1;
}

/*
 * Run an external program. Optionally wait a specified number
 * of seconds. Program killed if wait exceeded (it is done by the
 * watchdog, as fgets is a blocking function).
 *
 * If the watchdog kills the program, fgets returns, and ferror is set
 * to 1 (=>SUCCESS), so we check if the watchdog killed the program.
 *
 * Return the full output from the program (not only the first line).
 *
 * Contrary to my normal calling conventions, this program
 *
 * Returns: 0 on success
 *          non-zero on error == BErrNo status
 */
int RunProgramFullOutput(char* prog, int wait, POOLMEM*& results)
{
  Bpipe* bpipe;
  int stat1, stat2;
  char* mode;
  POOLMEM* tmp;
  char* buf;
  const int bufsize = 32000;


  tmp = GetPoolMemory(PM_MESSAGE);
  buf = (char*)malloc(bufsize + 1);

  results[0] = 0;
  mode = (char*)"r";
  bpipe = OpenBpipe(prog, wait, mode);
  if (!bpipe) {
    stat1 = ENOENT;
    goto bail_out;
  }

  tmp[0] = 0;
  while (1) {
    buf[0] = 0;
    fgets(buf, bufsize, bpipe->rfd);
    buf[bufsize] = 0;
    PmStrcat(tmp, buf);
    if (feof(bpipe->rfd)) {
      stat1 = 0;
      Dmsg1(900, "Run program fgets stat=%d\n", stat1);
      break;
    } else {
      stat1 = ferror(bpipe->rfd);
    }
    if (stat1 < 0) {
      BErrNo be;
      Dmsg2(200, "Run program fgets stat=%d ERR=%s\n", stat1, be.bstrerror());
      break;
    } else if (stat1 != 0) {
      Dmsg1(900, "Run program fgets stat=%d\n", stat1);
      if (bpipe->timer_id && bpipe->timer_id->killed) {
        Dmsg1(250, "Run program saw fgets killed=%d\n",
              bpipe->timer_id->killed);
        break;
      }
    }
  }

  /* We always check whether the timer killed the program. We would see
   * an eof even when it does so we just have to trust the killed flag
   * and set the timer values to avoid edge cases where the program ends
   * just as the timer kills it. */
  if (bpipe->timer_id && bpipe->timer_id->killed) {
    Dmsg1(150, "Run program fgets killed=%d\n", bpipe->timer_id->killed);
    PmStrcpy(tmp, T_("Program killed by BAREOS (timeout)\n"));
    stat1 = ETIME;
  }

  PmStrcpy(results, tmp);

  Dmsg3(1900, "resadr=%p reslen=%" PRIuz " res=%s\n", results, strlen(results),
        results);
  stat2 = CloseBpipe(bpipe);
  stat1 = stat2 != 0 ? stat2 : stat1;

  Dmsg1(900, "Run program returning %d\n", stat1);
bail_out:
  FreePoolMemory(tmp);
  free(buf);
  return stat1;
}
