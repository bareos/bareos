/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2007-2010 Free Software Foundation Europe e.V.
   Copyright (C) 2016-2016 Bareos GmbH & Co. KG

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
 *  Kern Sibbald, August MMVII
 */
/**
 * @file
 * Attempt to save the current working directory by various means so that
 *  we can optimize code by doing a cwd and then restore the cwd.
 */

#include "bareos.h"
#include "savecwd.h"

#ifdef HAVE_FCHDIR
static bool fchdir_failed = false;          /* set if we get a fchdir failure */
#else
static bool fchdir_failed = true;           /* set if we get a fchdir failure */
#endif

/**
 * Save current working directory.
 * Returns: true if OK
 *          false if failed
 */
bool saveCWD::save(JCR *jcr)
{
   release();                                /* clean up */
   if (!fchdir_failed) {
      m_fd = open(".", O_RDONLY);
      if (m_fd < 0) {
         berrno be;
         Jmsg1(jcr, M_ERROR, 0, _("Cannot open current directory: ERR=%s\n"), be.bstrerror());
         m_saved = false;
         return false;
      }
   }

   if (fchdir_failed) {
      POOLMEM *buf = get_memory(5000);
      m_cwd = (POOLMEM *)getcwd(buf, sizeof_pool_memory(buf));
      if (m_cwd == NULL) {
         berrno be;
         Jmsg1(jcr, M_ERROR, 0, _("Cannot get current directory: ERR=%s\n"), be.bstrerror());
         free_pool_memory(buf);
         m_saved = false;
         return false;
      }
   }
   m_saved = true;
   return true;
}

/**
 * Restore previous working directory.
 * Returns: true if OK
 *          false if failed
 */
bool saveCWD::restore(JCR *jcr)
{
   if (!m_saved) {
      return true;
   }
   m_saved = false;
   if (m_fd >= 0) {
      if (fchdir(m_fd) != 0) {
         berrno be;
         Jmsg1(jcr, M_ERROR, 0, _("Cannot reset current directory: ERR=%s\n"), be.bstrerror());
         close(m_fd);
         m_fd = -1;
         fchdir_failed = true;
         chdir("/");                  /* punt */
         return false;
      }
      return true;
   }
   if (chdir(m_cwd) < 0) {
      berrno be;
      Jmsg1(jcr, M_ERROR, 0, _("Cannot reset current directory: ERR=%s\n"), be.bstrerror());
      chdir("/");
      free_pool_memory(m_cwd);
      m_cwd = NULL;
      return false;
   }
   return true;
}

void saveCWD::release()
{
   if (!m_saved) {
      return;
   }
   m_saved = false;
   if (m_fd >= 0) {
      close(m_fd);
      m_fd = -1;
   }
   if (m_cwd) {
      free_pool_memory(m_cwd);
      m_cwd = NULL;
   }
}
