/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2011-2012 Free Software Foundation Europe e.V.

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
 * Kern Sibbald, January  MMXII
 *
 * Selection list. A string of integers separated by commas
 * representing items selected. Ranges of the form nn-mm
 * are also permitted.
 */

#include "include/bareos.h"
#include "lib/edit.h"

/*
 * Returns next item
 *   error if returns -1 and errmsg set
 *   end of items if returns -1 and errmsg NULL
 */
int64_t sellist::next()
{
   errmsg = NULL;
   if (beg <= end) {
      return beg++;
   }
   if (e == NULL) {
      goto bail_out;
   }
   /*
    * As we walk the list, we set EOF in
    *   the end of the next item to ease scanning,
    *   but save and then restore the character.
    */
   for (p=e; p && *p; p=e) {
      /* Check for list */
      e = strchr(p, ',');
      if (e) {
         esave = *e;
         *e++ = 0;
      } else {
         esave = 0;
      }
      /* Check for range */
      h = strchr(p, '-');             /* range? */
      if (h == p) {
         errmsg = _("Negative numbers not permitted.\n");
         goto bail_out;
      }
      if (h) {
         hsave = *h;
         *h++ = 0;
         if (!IsAnInteger(h)) {
            errmsg = _("Range end is not integer.\n");
            goto bail_out;
         }
         SkipSpaces(&p);
         if (!IsAnInteger(p)) {
            errmsg = _("Range start is not an integer.\n");
            goto bail_out;
         }
         beg = str_to_int64(p);
         end = str_to_int64(h);
         if (end < beg) {
            errmsg = _("Range end not bigger than start.\n");
            goto bail_out;
         }
      } else {
         hsave = 0;
         SkipSpaces(&p);
         if (!IsAnInteger(p)) {
            errmsg = _("Input value is not an integer.\n");
            goto bail_out;
         }
         beg = end = str_to_int64(p);
      }
      if (esave) {
         *(e-1) = esave;
      }
      if (hsave) {
         *(h-1) = hsave;
      }
      if (beg <= 0 || end <= 0) {
         errmsg = _("Selection items must be be greater than zero.\n");
         goto bail_out;
      }
      if (end > max) {
         errmsg = _("Selection item too large.\n");
         goto bail_out;
      }
      if (beg <= end) {
         return beg++;
      }
   }
   /* End of items */
   errmsg = NULL;      /* No error */
   return -1;

bail_out:
   return -1;          /* Error, errmsg set */
}


/*
 * Set selection string and optionally scan it
 *   returns false on error in string
 *   returns true if OK
 */
bool sellist::SetString(char *string, bool scan=true)
{
   /*
    * Copy string, because we write into it,
    *  then scan through it once to find any
    *  errors.
    */
   if (str) {
      free(str);
   }
   e = str = bstrdup(string);
   end = 0;
   beg = 1;
   num_items = 0;
   if (scan) {
      while (next() >= 0) {
         num_items++;
      }
      if (get_errmsg()) {
         return false;
      }
      e = str;
      end = 0;
      beg = 1;
   }
   return true;
}
