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
 * Kern Sibbald, August MMVII
 */
/**
 * @file
 * save current working dir header file
 */

#ifndef _SAVECWD_H
#define _SAVECWD_H 1

class saveCWD {
   bool m_saved;                   /* set if we should do chdir i.e. save_cwd worked */
   int m_fd;                       /* fd of current dir before chdir */
   char *m_cwd;                    /* cwd before chdir if fd fchdir() works */

public:
   saveCWD() { m_saved=false; m_fd=-1; m_cwd=NULL; };
   ~saveCWD() { release(); };
   bool save(JCR *jcr);
   bool restore(JCR *jcr);
   void release();
   bool is_saved() { return m_saved; };
};

#endif /* _SAVECWD_H */
