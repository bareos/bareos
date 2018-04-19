/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2006-2011 Free Software Foundation Europe e.V.

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
 * Manipulation routines for BareosRegex list
 *
 * Eric Bollengier, March 2007
 */

#include "include/bareos.h"

#include "breg.h"
#include "mem_pool.h"

BareosRegex *new_bregexp(const char *motif)
{
   Dmsg0(500, "bregexp: creating new bregexp object\n");
   BareosRegex *self = (BareosRegex *)bmalloc(sizeof(BareosRegex));
   memset(self, 0, sizeof(BareosRegex));

   if (!self->extract_regexp(motif)) {
      Dmsg0(100, "bregexp: extract_regexp error\n");
      free_bregexp(self);
      return NULL;
   }

   self->result = get_pool_memory(PM_FNAME);
   self->result[0] = '\0';

   return self;
}

void free_bregexp(BareosRegex *self)
{
   Dmsg0(500, "bregexp: freeing BareosRegex object\n");

   if (!self) {
      return;
   }

   if (self->expr) {
      bfree(self->expr);
   }
   if (self->result) {
      free_pool_memory(self->result);
   }
   regfree(&self->preg);
   bfree(self);
}

/* Free a bregexps alist
 */
void free_bregexps(alist *bregexps)
{
   Dmsg0(500, "bregexp: freeing all BareosRegex object\n");

   BareosRegex *elt;
   foreach_alist(elt, bregexps) {
      free_bregexp(elt);
   }
}

/* Apply all regexps to fname
 */
bool apply_bregexps(const char *fname, alist *bregexps, char **result)
{
   BareosRegex *elt;
   bool ok=false;

   char *ret = (char *) fname;
   foreach_alist(elt, bregexps) {
      ret = elt->replace(ret);
      ok = ok || elt->success;
   }
   Dmsg2(500, "bregexp: fname=%s ret=%s\n", fname, ret);

   *result = ret;
   return ok;
}

/* return an alist of BareosRegex or return NULL if it's not a
 * where=!tmp!opt!ig,!temp!opt!i
 */
alist *get_bregexps(const char *where)
{
   char *p = (char *)where;
   alist *list = New(alist(10, not_owned_by_alist));
   BareosRegex *reg;

   reg = new_bregexp(p);

   while(reg) {
      p = reg->eor;
      list->append(reg);
      reg = new_bregexp(p);
   }

   if (list->size()) {
      return list;
   } else {
      delete list;
      return NULL;
   }
}

bool BareosRegex::extract_regexp(const char *motif)
{
   if ( !motif ) {
      return false;
   }

   char sep = motif[0];

   if (!(sep == '!' ||
         sep == ':' ||
         sep == ';' ||
         sep == '|' ||
         sep == ',' ||
         sep == '&' ||
         sep == '%' ||
         sep == '=' ||
         sep == '~' ||
         sep == '/' ||
         sep == '#'   ))
   {
      return false;
   }

   char *search = (char *) motif + 1;
   int options = REG_EXTENDED | REG_NEWLINE;
   bool ok = false;

   /* extract 1st part */
   char *dest = expr = bstrdup(motif);

   while (*search && !ok) {
      if (search[0] == '\\' && search[1] == sep) {
         *dest++ = *++search;       /* we skip separator */

      } else if (search[0] == '\\' && search[1] == '\\') {
         *dest++ = *++search;       /* we skip the second \ */

      } else if (*search == sep) {  /* we found end of expression */
         *dest++ = '\0';

         if (subst) {           /* already have found motif */
            ok = true;

         } else {
            *dest++ = *++search; /* we skip separator */
            subst = dest;        /* get replaced string */
         }

      } else {
         *dest++ = *search++;
      }
   }
   *dest = '\0';                /* in case of */

   if (!ok || !subst) {
      /* bad regexp */
      return false;
   }

   ok = false;
   /* find options */
   while (*search && !ok) {
      if (*search == 'i') {
         options |= REG_ICASE;

      } else if (*search == 'g') {
              /* recherche multiple*/

      } else if (*search == sep) {
         /* skip separator */

      } else {                  /* end of options */
         ok = true;
      }
      search++;
   }

   int rc = regcomp(&preg, expr, options);
   if (rc != 0) {
      char prbuf[500];
      regerror(rc, &preg, prbuf, sizeof(prbuf));
      Dmsg1(100, "bregexp: compile error: %s\n", prbuf);
      return false;
   }

   eor = search;                /* useful to find the next regexp in where */

   return true;
}

/* return regexp->result */
char *BareosRegex::replace(const char *fname)
{
   success = false;             /* use this.success to known if it's ok */
   int flen = strlen(fname);
   int rc = regexec(&preg, fname, BREG_NREGS, regs, 0);

   if (rc == REG_NOMATCH) {
      Dmsg0(500, "bregexp: regex mismatch\n");
      return return_fname(fname, flen);
   }

   int len = compute_dest_len(fname, regs);

   if (len) {
      result = check_pool_memory_size(result, len);
      edit_subst(fname, regs);
      success = true;
      Dmsg2(500, "bregexp: len = %i, result_len = %i\n", len, strlen(result));

   } else {                     /* error in substitution */
      Dmsg0(100, "bregexp: error in substitution\n");
      return return_fname(fname, flen);
   }

   return result;
}

char *BareosRegex::return_fname(const char *fname, int len)
{
   result = check_pool_memory_size(result, len+1);
   strcpy(result,fname);
   return result;
}

int BareosRegex::compute_dest_len(const char *fname, regmatch_t pmatch[])
{
   int len=0;
   char *p;
   char *psubst = subst;
   int no;

   if (!fname || !pmatch) {
      return 0;
   }

   /* match failed ? */
   if (pmatch[0].rm_so < 0) {
      return 0;
   }

   for (p = psubst++; *p ; p = psubst++) {
      /* match $1 \1 back references */
      if ((*p == '$' || *p == '\\') && ('0' <= *psubst && *psubst <= '9')) {
         no = *psubst++ - '0';

         /* we check if the back reference exists */
         /* references can not match if we are using (..)? */

         if (pmatch[no].rm_so >= 0 && pmatch[no].rm_eo >= 0) {
            len += pmatch[no].rm_eo - pmatch[no].rm_so;
         }

      } else {
         len++;
      }
   }

   /* $0 is replaced by subst */
   len -= pmatch[0].rm_eo - pmatch[0].rm_so;
   len += strlen(fname) + 1;

   return len;
}

char *BareosRegex::edit_subst(const char *fname, regmatch_t pmatch[])
{
   int i;
   char *p;
   char *psubst = subst;
   int no;
   int len;

   /* il faut recopier fname dans dest
    *  on recopie le debut fname -> pmatch->start[0]
    */

   for (i = 0; i < pmatch[0].rm_so ; i++) {
      result[i] = fname[i];
   }

   /* on recopie le motif de remplacement (avec tous les $x) */

   for (p = psubst++; *p ; p = psubst++) {
      /* match $1 \1 back references */
      if ((*p == '$' || *p == '\\') && ('0' <= *psubst && *psubst <= '9')) {
         no = *psubst++ - '0';

         /* have a back reference ? */
         if (pmatch[no].rm_so >= 0 && pmatch[no].rm_eo >= 0) {
            len = pmatch[no].rm_eo - pmatch[no].rm_so;
            bstrncpy(result + i, fname + pmatch[no].rm_so, len + 1);
            i += len ;
         }

      } else {
         result[i++] = *p;
      }
   }

   /* we copy what is out of the match */
   strcpy(result + i, fname + pmatch[0].rm_eo);

   return result;
}

/* escape sep char and \
 * dest must be long enough (src*2+1)
 * return end of the string */
char *bregexp_escape_string(char *dest, const char *src, const char sep)
{
   char *ret = dest;
   while (*src)
   {
      if (*src == sep) {
         *dest++ = '\\';
      } else if (*src == '\\') {
         *dest++ = '\\';
      }
      *dest++ = *src++;
   }
   *dest = '\0';

   return ret;
}

static const char regexp_sep = '!';
static const char *str_strip_prefix = "!%s!!i";
static const char *str_add_prefix   = "!^!%s!";
static const char *str_add_suffix   = "!([^/])$!$1%s!";

int bregexp_get_build_where_size(char *strip_prefix,
                                 char *add_prefix,
                                 char *add_suffix)
{
   int str_size = ((strip_prefix?strlen(strip_prefix)+strlen(str_strip_prefix):0) +
                   (add_prefix?strlen(add_prefix)+strlen(str_add_prefix)      :0) +
                   (add_suffix?strlen(add_suffix)+strlen(str_add_suffix)      :0) )
         /* escape + 3*, + \0 */
            * 2    + 3   + 1;

   Dmsg1(200, "bregexp_get_build_where_size = %i\n", str_size);
   return str_size;
}

/* build a regexp string with user arguments
 * Usage :
 *
 * int len = bregexp_get_build_where_size(a,b,c) ;
 * char *dest = (char *) bmalloc (len * sizeof(char));
 * bregexp_build_where(dest, len, a, b, c);
 * bfree(dest);
 *
 */
char *bregexp_build_where(char *dest, int str_size,
                          char *strip_prefix,
                          char *add_prefix,
                          char *add_suffix)
{
   int len=0;

   POOLMEM *str_tmp = get_memory(str_size);

   *str_tmp = *dest = '\0';

   if (strip_prefix) {
      len += bsnprintf(dest, str_size - len, str_strip_prefix,
                       bregexp_escape_string(str_tmp, strip_prefix, regexp_sep));
   }

   if (add_suffix) {
      if (len) dest[len++] = ',';

      len += bsnprintf(dest + len,  str_size - len, str_add_suffix,
                       bregexp_escape_string(str_tmp, add_suffix, regexp_sep));
   }

   if (add_prefix) {
      if (len) dest[len++] = ',';

      len += bsnprintf(dest + len, str_size - len, str_add_prefix,
                       bregexp_escape_string(str_tmp, add_prefix, regexp_sep));
   }

   free_pool_memory(str_tmp);

   return dest;
}


void BareosRegex::debug()
{
   printf("expr=[%s]\n", expr);
   printf("subst=[%s]\n", subst);
   printf("result=%s\n", NPRT(result));
}
