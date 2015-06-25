/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2002-2011 Free Software Foundation Europe e.V.
   Copyright (C) 2011-2012 Planets Communications B.V.
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
 * edit.c  edit string to ascii, and ascii to internal
 *
 * Kern Sibbald, December MMII
 */

#include "bareos.h"
#include <math.h>

/* We assume ASCII input and don't worry about overflow */
uint64_t str_to_uint64(const char *str)
{
   const char *p = str;
   uint64_t value = 0;

   if (!p) {
      return 0;
   }
   while (B_ISSPACE(*p)) {
      p++;
   }
   if (*p == '+') {
      p++;
   }
   while (B_ISDIGIT(*p)) {
      value = B_TIMES10(value) + *p - '0';
      p++;
   }
   return value;
}

int64_t str_to_int64(const char *str)
{
   const char *p = str;
   int64_t value;
   bool negative = false;

   if (!p) {
      return 0;
   }
   while (B_ISSPACE(*p)) {
      p++;
   }
   if (*p == '+') {
      p++;
   } else if (*p == '-') {
      negative = true;
      p++;
   }
   value = str_to_uint64(p);
   if (negative) {
      value = -value;
   }
   return value;
}

/*
 * Edit an integer number with commas, the supplied buffer
 * must be at least 27 bytes long.  The incoming number
 * is always widened to 64 bits.
 */
char *edit_uint64_with_commas(uint64_t val, char *buf)
{
   edit_uint64(val, buf);
   return add_commas(buf, buf);
}

/*
 * Edit an integer into "human-readable" format with four or fewer
 * significant digits followed by a suffix that indicates the scale
 * factor.  The buf array inherits a 27 byte minimim length
 * requirement from edit_unit64_with_commas(), although the output
 * string is limited to eight characters.
 */
char *edit_uint64_with_suffix(uint64_t val, char *buf)
{
  int commas = 0;
  char *c, mbuf[50];
  const char *suffix[] =
    { "", "K", "M", "G", "T", "P", "E", "Z", "Y", "FIX ME" };
  int suffixes = sizeof(suffix) / sizeof(*suffix);

  edit_uint64_with_commas(val, mbuf);

  if ((c = strchr(mbuf, ',')) != NULL) {
    commas++;
    *c++ = '.';
    while  ((c = strchr(c, ',')) != NULL) {
      commas++;
      *c++ = '\0';
    }
    mbuf[5] = '\0'; // drop this to get '123.456 TB' rather than '123.4 TB'
  }

  if (commas >= suffixes)
    commas = suffixes - 1;
  bsnprintf(buf, 27, "%s %s", mbuf, suffix[commas]);
  return buf;
}

/*
 * Edit an integer number, the supplied buffer
 * must be at least 27 bytes long.  The incoming number
 * is always widened to 64 bits.
 */
char *edit_uint64(uint64_t val, char *buf)
{
   /*
    * Replacement for sprintf(buf, "%" llu, val)
    */
   char mbuf[50];
   mbuf[sizeof(mbuf)-1] = 0;
   int i = sizeof(mbuf)-2;                 /* edit backward */
   if (val == 0) {
      mbuf[i--] = '0';
   } else {
      while (val != 0) {
         mbuf[i--] = "0123456789"[val%10];
         val /= 10;
      }
   }
   bstrncpy(buf, &mbuf[i+1], 27);
   return buf;
}

char *edit_int64(int64_t val, char *buf)
{
   /*
    * Replacement for sprintf(buf, "%" llu, val)
    */
   char mbuf[50];
   bool negative = false;
   mbuf[sizeof(mbuf)-1] = 0;
   int i = sizeof(mbuf)-2;                 /* edit backward */
   if (val == 0) {
      mbuf[i--] = '0';
   } else {
      if (val < 0) {
         negative = true;
         val = -val;
      }
      while (val != 0) {
         mbuf[i--] = "0123456789"[val%10];
         val /= 10;
      }
   }
   if (negative) {
      mbuf[i--] = '-';
   }
   bstrncpy(buf, &mbuf[i+1], 27);
   return buf;
}

/*
 * Edit an integer number with commas, the supplied buffer
 * must be at least 27 bytes long.  The incoming number
 * is always widened to 64 bits.
 */
char *edit_int64_with_commas(int64_t val, char *buf)
{
   edit_int64(val, buf);
   return add_commas(buf, buf);
}

/*
 * Given a string "str", separate the numeric part into
 *   str, and the modifier into mod.
 */
static bool get_modifier(char *str, char *num, int num_len, char *mod, int mod_len)
{
   int i, len, num_begin, num_end, mod_begin, mod_end;

   strip_trailing_junk(str);
   len = strlen(str);

   for (i=0; i<len; i++) {
      if (!B_ISSPACE(str[i])) {
         break;
      }
   }
   num_begin = i;

   /* Walk through integer part */
   for ( ; i<len; i++) {
      if (!B_ISDIGIT(str[i]) && str[i] != '.') {
         break;
      }
   }
   num_end = i;
   if (num_len > (num_end - num_begin + 1)) {
      num_len = num_end - num_begin + 1;
   }
   if (num_len == 0) {
      return false;
   }
   /* Eat any spaces in front of modifier */
   for ( ; i<len; i++) {
      if (!B_ISSPACE(str[i])) {
         break;
      }
   }
   mod_begin = i;
   for ( ; i<len; i++) {
      if (!B_ISALPHA(str[i])) {
         break;
      }
   }
   mod_end = i;
   if (mod_len > (mod_end - mod_begin + 1)) {
      mod_len = mod_end - mod_begin + 1;
   }
   Dmsg5(900, "str=%s: num_beg=%d num_end=%d mod_beg=%d mod_end=%d\n",
      str, num_begin, num_end, mod_begin, mod_end);
   bstrncpy(num, &str[num_begin], num_len);
   bstrncpy(mod, &str[mod_begin], mod_len);
   if (!is_a_number(num)) {
      return false;
   }
   bstrncpy(str, &str[mod_end], len);
   Dmsg2(900, "num=%s mod=%s\n", num, mod);

   return true;
}

/*
 * Convert a string duration to utime_t (64 bit seconds)
 * Returns false: if error
           true:  if OK, and value stored in value
 */
bool duration_to_utime(char *str, utime_t *value)
{
   int i, mod_len;
   double val, total = 0.0;
   char mod_str[20];
   char num_str[50];
   /*
    * The "n" = mins and months appears before minutes so that m maps to months.
    */
   static const char *mod[] = {
      "n",
      "seconds",
      "months",
      "minutes",
      "mins",
      "hours",
      "days",
      "weeks",
      "quarters",
      "years",
      (char *)NULL
   };
   static const int32_t mult[] = {
      60,
      1,
      60 * 60 * 24 * 30,
      60,
      60,
      3600,
      3600 * 24,
      3600 * 24 * 7,
      3600 * 24 * 91,
      3600 * 24 * 365,
      0
   };

   while (*str) {
      if (!get_modifier(str, num_str, sizeof(num_str), mod_str, sizeof(mod_str))) {
         return false;
      }
      /* Now find the multiplier corresponding to the modifier */
      mod_len = strlen(mod_str);
      if (mod_len == 0) {
         i = 1;                          /* default to seconds */
      } else {
         for (i=0; mod[i]; i++) {
            if (bstrncasecmp(mod_str, mod[i], mod_len)) {
               break;
            }
         }
         if (mod[i] == NULL) {
            return false;
         }
      }
      Dmsg2(900, "str=%s: mult=%d\n", num_str, mult[i]);
      errno = 0;
      val = strtod(num_str, NULL);
      if (errno != 0 || val < 0) {
         return false;
      }
      total += val * mult[i];
   }
   *value = (utime_t)total;
   return true;
}

/*
 * Edit a utime "duration" into ASCII
 */
char *edit_utime(utime_t val, char *buf, int buf_len)
{
   char mybuf[200];
   static const int32_t mult[] = {60*60*24*365, 60*60*24*30, 60*60*24, 60*60, 60};
   static const char *mod[]  = {"year",  "month",  "day", "hour", "min"};
   int i;
   uint32_t times;

   *buf = 0;
   for (i=0; i<5; i++) {
      times = (uint32_t)(val / mult[i]);
      if (times > 0) {
         val = val - (utime_t)times * mult[i];
         bsnprintf(mybuf, sizeof(mybuf), "%d %s%s ", times, mod[i], times>1?"s":"");
         bstrncat(buf, mybuf, buf_len);
      }
   }
   if (val == 0 && strlen(buf) == 0) {
      bstrncat(buf, "0 secs", buf_len);
   } else if (val != 0) {
      bsnprintf(mybuf, sizeof(mybuf), "%d sec%s", (uint32_t)val, val>1?"s":"");
      bstrncat(buf, mybuf, buf_len);
   }
   return buf;
}

static bool strunit_to_uint64(char *str, uint64_t *value, const char **mod)
{
   int i, mod_len;
   double val;
   char mod_str[20];
   char num_str[50];
   const int64_t mult[] = {1,             /* byte */
                           1024,          /* kilobyte */
                           1000,          /* kb kilobyte */
                           1048576,       /* megabyte */
                           1000000,       /* mb megabyte */
                           1073741824,    /* gigabyte */
                           1000000000};   /* gb gigabyte */

   if (!get_modifier(str, num_str, sizeof(num_str), mod_str, sizeof(mod_str))) {
      return 0;
   }
   /* Now find the multiplier corresponding to the modifier */
   mod_len = strlen(mod_str);
   if (mod_len == 0) {
      i = 0;                          /* default with no modifier = 1 */
   } else {
      for (i=0; mod[i]; i++) {
         if (bstrncasecmp(mod_str, mod[i], mod_len)) {
            break;
         }
      }
      if (mod[i] == NULL) {
         return false;
      }
   }
   Dmsg2(900, "str=%s: mult=%d\n", str, mult[i]);
   errno = 0;
   val = strtod(num_str, NULL);
   if (errno != 0 || val < 0) {
      return false;
   }
   *value = (utime_t)(val * mult[i]);
   return true;
}

/*
 * Convert a size in bytes to uint64_t
 * Returns false: if error
           true:  if OK, and value stored in value
 */
bool size_to_uint64(char *str, uint64_t *value)
{
   /* first item * not used */
   static const char *mod[]  = {"*", "k", "kb", "m", "mb",  "g", "gb",  NULL};
   return strunit_to_uint64(str, value, mod);
}

/*
 * Convert a speed in bytes/s to uint64_t
 * Returns false: if error
           true:  if OK, and value stored in value
 */
bool speed_to_uint64(char *str, uint64_t *value)
{
   /* first item * not used */
   static const char *mod[]  = {"*", "k/s", "kb/s", "m/s", "mb/s",  NULL};
   return strunit_to_uint64(str, value, mod);
}

/*
 * Check if specified string is a number or not.
 *  Taken from SQLite, cool, thanks.
 */
bool is_a_number(const char *n)
{
   bool digit_seen = false;

   if( *n == '-' || *n == '+' ) {
      n++;
   }
   while (B_ISDIGIT(*n)) {
      digit_seen = true;
      n++;
   }
   if (digit_seen && *n == '.') {
      n++;
      while (B_ISDIGIT(*n)) { n++; }
   }
   if (digit_seen && (*n == 'e' || *n == 'E')
       && (B_ISDIGIT(n[1]) || ((n[1]=='-' || n[1] == '+') && B_ISDIGIT(n[2])))) {
      n += 2;                         /* skip e- or e+ or e digit */
      while (B_ISDIGIT(*n)) { n++; }
   }
   return digit_seen && *n==0;
}

/*
 * Check if specified string is a list of numbers or not
 */
bool is_a_number_list(const char *n)
{
   bool previous_digit = false;
   bool digit_seen = false;
   while (*n) {
      if (B_ISDIGIT(*n)) {
         previous_digit=true;
         digit_seen = true;
      } else if (*n == ',' && previous_digit) {
         previous_digit = false;
      } else {
         return false;
      }
      n++;
   }
   return digit_seen && *n==0;
}

/*
 * Check if the specified string is an integer
 */
bool is_an_integer(const char *n)
{
   bool digit_seen = false;
   while (B_ISDIGIT(*n)) {
      digit_seen = true;
      n++;
   }
   return digit_seen && *n==0;
}

/*
 * Check if BAREOS Resoure Name is valid
 */
/*
 * Check if the Volume name has legal characters
 * If ua is non-NULL send the message
 */
bool is_name_valid(const char *name, POOLMEM **msg)
{
   int len;
   const char *p;
   /* Special characters to accept */
   const char *accept = ":.-_/ ";

   /* No name is invalid */
   if (!name) {
      if (msg) {
         Mmsg(msg, _("Empty name not allowed.\n"));
      }
      return false;
   }
   /* Restrict the characters permitted in the Volume name */
   for (p=name; *p; p++) {
      if (B_ISALPHA(*p) || B_ISDIGIT(*p) || strchr(accept, (int)(*p))) {
         continue;
      }
      if (msg) {
         Mmsg(msg, _("Illegal character \"%c\" in name.\n"), *p);
      }
      return false;
   }
   len = p - name;
   if (len >= MAX_NAME_LENGTH) {
      if (msg) {
         Mmsg(msg, _("Name too long.\n"));
      }
      return false;
   }
   if (len == 0) {
      if (msg) {
         Mmsg(msg,  _("Volume name must be at least one character long.\n"));
      }
      return false;
   }
   return true;
}



/*
 * Add commas to a string, which is presumably
 * a number.
 */
char *add_commas(char *val, char *buf)
{
   int len, nc;
   char *p, *q;
   int i;

   if (val != buf) {
      strcpy(buf, val);
   }
   len = strlen(buf);
   if (len < 1) {
      len = 1;
   }
   nc = (len - 1) / 3;
   p = buf+len;
   q = p + nc;
   *q-- = *p--;
   for ( ; nc; nc--) {
      for (i=0; i < 3; i++) {
          *q-- = *p--;
      }
      *q-- = ',';
   }
   return buf;
}

#ifdef TEST_PROGRAM
void d_msg(const char*, int, int, const char*, ...)
{}
int main(int argc, char *argv[])
{
   char *str[] = {"3", "3n", "3 hours", "3.5 day", "3 week", "3 m", "3 q", "3 years"};
   utime_t val;
   char buf[100];
   char outval[100];

   for (int i=0; i<8; i++) {
      strcpy(buf, str[i]);
      if (!duration_to_utime(buf, &val)) {
         printf("Error return from duration_to_utime for in=%s\n", str[i]);
         continue;
      }
      edit_utime(val, outval);
      printf("in=%s val=%" lld " outval=%s\n", str[i], val, outval);
   }
}
#endif
