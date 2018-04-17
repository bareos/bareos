/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2011-2011 Free Software Foundation Europe e.V.
   Copyright (C) 2011-2012 Planets Communications B.V.
   Copyright (C) 2013-2016 Bareos GmbH & Co. KG

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
 * Written by Marco van Wieringen, November 2011
 */
/**
 * @file
 * Detect fileset shadowing e.g. when an include entry pulls in data
 * which is already being backuped by another include pattern. Currently
 * we support both local and global shadowing. Where local shadowing is
 * when the shadowing occurs within one include block and global when
 * between multiple include blocks.
 */

#include "bareos.h"
#include "find.h"

/**
 * Check if a certain fileset include pattern shadows another pattern.
 */
static inline bool check_include_pattern_shadowing(JobControlRecord *jcr,
                                                   const char *pattern1,
                                                   const char *pattern2,
                                                   bool recursive)
{
   int len1, len2;
   bool retval = false;
   struct stat st1, st2;

   /*
    * See if one directory shadows the other or if two
    * files are hardlinked.
    */
   if (lstat(pattern1, &st1) != 0) {
      berrno be;
      Jmsg(jcr, M_WARNING, 0,
           _("Cannot stat file %s: ERR=%s\n"),
           pattern1, be.bstrerror());
      goto bail_out;
   }

   if (lstat(pattern2, &st2) != 0) {
      berrno be;
      Jmsg(jcr, M_WARNING, 0,
           _("Cannot stat file %s: ERR=%s\n"),
           pattern2, be.bstrerror());
      goto bail_out;
   }

   if (S_ISDIR(st1.st_mode) && S_ISDIR(st2.st_mode)) {
      /*
       * Only check shadowing of directories when recursion is turned on.
       */
      if (recursive ) {
         len1 = strlen(pattern1);
         len2 = strlen(pattern2);

         /*
          * See if one pattern shadows the other.
          */
         if (((len1 < len2 && pattern1[len1] == '\0' && IsPathSeparator(pattern2[len1])) ||
              (len1 > len2 && IsPathSeparator(pattern1[len2]) && pattern1[len1] == '\0')) &&
             bstrncmp(pattern1, pattern2, MIN(len1, len2))) {
            /*
             * If both directories have the same st_dev they shadow
             * each other e.g. are not on separate filesystems.
             */
            if (st1.st_dev == st2.st_dev) {
               retval = true;
            }
         }
      }
   } else {
      /*
       * See if the two files are hardlinked.
       */
      if (st1.st_dev == st2.st_dev &&
          st1.st_ino == st2.st_ino) {
         retval = true;
      }
   }

bail_out:
   return retval;
}

/**
 * See if recursion is on or off for a specific include block.
 * We use the data from the default options block.
 * e.g. the last option block in the include block.
 */
static inline bool include_block_is_recursive(findIncludeExcludeItem *incexe)
{
   int i;
   findFOPTS *fo;
   bool recursive = true;

   for (i = 0; i < incexe->opts_list.size(); i++) {
      fo = (findFOPTS *)incexe->opts_list.get(i);

      recursive = !bit_is_set(FO_NO_RECURSION, fo->flags);
   }

   return recursive;
}

/**
 * See if an options block of an include block has any wildcard
 * or regex settings which are not used for excluding.
 */
static inline bool include_block_has_patterns(findIncludeExcludeItem *incexe)
{
   int i;
   bool has_find_patterns = false;
   findFOPTS *fo;

   for (i = 0; i < incexe->opts_list.size(); i++) {
      fo = (findFOPTS *)incexe->opts_list.get(i);

      /*
       * See if this is an exclude block.
       * e.g. exclude = yes is set then we
       * should still check for shadowing.
       */
      if (bit_is_set(FO_EXCLUDE, fo->flags)) {
         continue;
      }

      /*
       * See if the include block has any interesting
       * wildcard matching options. We consider the following
       * as making the shadowing match to difficult:
       * - regex = entries
       * - regexdir = entries
       * - wild = entries
       * - wildir = entries
       *
       * For filename shadowing we only check if files are
       * hardlinked so we don't take into consideration
       * - regexfile = entries
       * - wildfile = entries
       */
      if (fo->regex.size() > 0 ||
          fo->regexdir.size() > 0 ||
          fo->wild.size() > 0 ||
          fo->wilddir.size() > 0) {
         has_find_patterns = true;
      }
   }

   return has_find_patterns;
}

/**
 * For this include block lookup the shadow checking type requested.
 * We use the data from the default options block.
 * e.g. the last option block in the include block.
 */
static inline b_fileset_shadow_type include_block_get_shadow_type(findIncludeExcludeItem *incexe)
{
   int i;
   findFOPTS *fo;
   b_fileset_shadow_type shadow_type = check_shadow_none;

   for (i = 0; i < incexe->opts_list.size(); i++) {
      fo = (findFOPTS *)incexe->opts_list.get(i);

      shadow_type = fo->shadow_type;
   }
   return shadow_type;
}

/**
 * See if there is any local shadowing within an include block.
 */
static void check_local_fileset_shadowing(JobControlRecord *jcr,
                                          findIncludeExcludeItem *incexe,
                                          bool remove)
{
   dlistString *str1, *str2, *next;
   bool recursive;

   /*
    * See if this is a recursive include block.
    */
   recursive = include_block_is_recursive(incexe);

   /*
    * Loop over all entries in the name_list
    * and compare them against all next entries
    * after the one we are currently examining.
    * This way we only check shadowing only once.
    */
   str1 = (dlistString *)incexe->name_list.first();
   while (str1) {
      str2 = (dlistString *)incexe->name_list.next(str1);
      while (str1 && str2) {
         if (check_include_pattern_shadowing(jcr,
                                             str1->c_str(),
                                             str2->c_str(),
                                             recursive)) {
            /*
             * See what entry shadows the other, the longest entry
             * shadow the shorter one.
             */
            if (strlen(str1->c_str()) < strlen(str2->c_str())) {
               if (remove) {
                  /*
                   * Pattern2 is longer then Pattern1 e.g. the include block patterns
                   * are probably sorted right. This is the easiest case where we just
                   * remove the entry from the list and continue.
                   */
                  Jmsg(jcr, M_WARNING, 0,
                       _("Fileset include block entry %s shadows %s removing it from fileset\n"),
                       str2->c_str(), str1->c_str());
                  next = (dlistString *)incexe->name_list.next(str2);
                  incexe->name_list.remove(str2);
                  str2 = next;
                  continue;
               } else {
                  Jmsg(jcr, M_WARNING, 0,
                       _("Fileset include block entry %s shadows %s\n"),
                       str2->c_str(), str1->c_str());
               }
            } else {
               if (remove) {
                  /*
                   * Pattern1 is longer then Pattern2 e.g. the include block patterns
                   * are not sorted right and probably reverse. This is a bit more difficult.
                   * We remove the first pattern from the list and restart the shadow scan.
                   * By setting str1 to NULL we force a rescan as the next method of the dlist
                   * will start at the first entry of the dlist again.
                   */
                  Jmsg(jcr, M_WARNING, 0,
                       _("Fileset include block entry %s shadows %s removing it from fileset\n"),
                       str1->c_str(), str2->c_str());
                  incexe->name_list.remove(str1);
                  str1 = NULL;
                  continue;
               } else {
                  Jmsg(jcr, M_WARNING, 0,
                       _("Fileset include block entry %s shadows %s\n"),
                       str1->c_str(), str2->c_str());
               }
            }
         }
         str2 = (dlistString *)incexe->name_list.next(str2);
      }
      str1 = (dlistString *)incexe->name_list.next(str1);
   }
}

/**
 * See if there is any local shadowing within an include block or
 * any global shadowing between include blocks.
 */
static inline void check_global_fileset_shadowing(JobControlRecord *jcr,
                                                  findFILESET *fileset,
                                                  bool remove)
{
   int i, j;
   bool local_recursive, global_recursive;
   findIncludeExcludeItem *current, *compare_against;
   dlistString *str1, *str2, *next;

   /*
    * Walk all the include blocks and see if there
    * is any shadowing between the different sets.
    */
   for (i = 0; i < fileset->include_list.size(); i++) {
      current = (findIncludeExcludeItem *)fileset->include_list.get(i);

      /*
       * See if there is any local shadowing.
       */
      check_local_fileset_shadowing(jcr, current, remove);

      /*
       * Only check global shadowing against this include block
       * when it doesn't have any patterns. Testing if a fileset
       * shadows the other with patterns is next to impossible
       * without comparing the matching criteria which can be
       * in so many forms we forget it all together. When you
       * are smart enough to create include/exclude patterns
       * we also don't provide you with basic stop gap measures.
       */
      if (include_block_has_patterns(current)) {
         continue;
      }

      /*
       * Now compare this block against any include block after this one.
       * We can shortcut as we don't have to start at the beginning of
       * the list again because we compare all sets against each other
       * this way anyhow. e.g. we start with set 1 against 2 .. x and
       * then 2 against 3 .. x (No need to compare 2 against 1 again
       * as we did that in the first run already.
       *
       * See if this is a recursive include block.
       */
      local_recursive = include_block_is_recursive(current);
      for (j = i + 1; j < fileset->include_list.size(); j++) {
         compare_against = (findIncludeExcludeItem *)fileset->include_list.get(j);

         /*
          * Only check global shadowing against this include block
          * when it doesn't have any patterns.
          */
         if (include_block_has_patterns(compare_against)) {
            continue;
         }

         /*
          * See if both include blocks are recursive.
          */
         global_recursive = (local_recursive &&
                             include_block_is_recursive(compare_against));

         /*
          * Walk over the filename list and compare it
          * against the other entry from the other list.
          */
         str1 = (dlistString *)current->name_list.first();
         while (str1) {
            str2 = (dlistString *)compare_against->name_list.first();
            while (str1 && str2) {
               if (check_include_pattern_shadowing(jcr,
                                                   str1->c_str(),
                                                   str2->c_str(),
                                                   global_recursive)) {
                  /*
                   * See what entry shadows the other, the longest entry
                   * shadow the shorter one.
                   */
                  if (strlen(str1->c_str()) < strlen(str2->c_str())) {
                     if (remove) {
                        /*
                         * Pattern2 is longer then Pattern1 e.g. the include block patterns
                         * are probably sorted right. This is the easiest case where we just
                         * remove the entry from the list and continue.
                         */
                        Jmsg(jcr, M_WARNING, 0,
                             _("Fileset include block entry %s shadows %s removing it from fileset\n"),
                             str2->c_str(), str1->c_str());
                        next = (dlistString *)compare_against->name_list.next(str2);
                        compare_against->name_list.remove(str2);
                        str2 = next;
                        continue;
                     } else {
                        Jmsg(jcr, M_WARNING, 0,
                             _("Fileset include block entry %s shadows %s\n"),
                             str2->c_str(), str1->c_str());
                     }
                  } else {
                     if (remove) {
                        /*
                         * Pattern1 is longer then Pattern2 e.g. the include block patterns
                         * are not sorted right and probably reverse. This is a bit more
                         * difficult. We remove the first pattern from the list and restart
                         * the shadow scan. By setting str1 to NULL we force a rescan as the
                         * next method of the dlist will start at the first entry of the
                         * dlist again.
                         */
                        Jmsg(jcr, M_WARNING, 0,
                             _("Fileset include block entry %s shadows %s removing it from fileset\n"),
                             str1->c_str(), str2->c_str());
                        current->name_list.remove(str1);
                        str1 = NULL;
                        continue;
                     } else {
                        Jmsg(jcr, M_WARNING, 0,
                             _("Fileset include block entry %s shadows %s\n"),
                             str1->c_str(), str2->c_str());
                     }
                  }
               }
               str2 = (dlistString *)compare_against->name_list.next(str2);
            }
            str1 = (dlistString *)current->name_list.next(str1);
         }
      }
   }
}

void check_include_list_shadowing(JobControlRecord *jcr, findFILESET *fileset)
{
   int i;
   findIncludeExcludeItem *incexe;
   b_fileset_shadow_type shadow_type;

   /*
    * Walk the list of include blocks.
    */
   for (i = 0; i < fileset->include_list.size(); i++) {
      incexe = (findIncludeExcludeItem *)fileset->include_list.get(i);

      /*
       * See if the shadow check option is enabled for this
       * include block. If not just continue with the next include block.
       */
      shadow_type = include_block_get_shadow_type(incexe);
      switch (shadow_type) {
      case check_shadow_none:
         continue;
      case check_shadow_local_warn:
      case check_shadow_local_remove:
         /*
          * Check only for local shadowing within the same include block.
          */
         check_local_fileset_shadowing(jcr, incexe,
                                       shadow_type == check_shadow_local_remove);
         break;
      case check_shadow_global_warn:
      case check_shadow_global_remove:
         /*
          * Check global shadowing over more then one include block.
          * We only need to perform the global check once because we
          * visit all entries in that scan so return after returning
          * from the function.
          */
         check_global_fileset_shadowing(jcr, fileset,
                                        shadow_type == check_shadow_global_remove);
         return;
      }
   }
}
