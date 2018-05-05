/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2005-2012 Free Software Foundation Europe e.V.

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
 * Copyright Patrick Powell 1995
 *
 * This code is based on code written by Patrick Powell
 * (papowell@astart.com) It may be used for any purpose as long
 * as this notice remains intact on all source code distributions.
 *
 * Adapted for BAREOS -- note there were lots of bugs in
 * the original code: %lld and %s were seriously broken, and
 * with FP turned off %f seg faulted.
 *
 * Kern Sibbald, November MMV
 */

#include "include/bareos.h"
#include <wchar.h>

#define FP_OUTPUT 1 /* BAREOS uses floating point */

/* Define the following if you want all the features of
 *  normal printf, but with all the security problems.
 *  For BAREOS we turn this off, and it silently ignores
 *  formats that could pose a security problem.
 */
#undef SECURITY_PROBLEM

#ifdef USE_BSNPRINTF

#ifdef HAVE_LONG_DOUBLE
#define LDOUBLE long double
#else
#define LDOUBLE double
#endif

int Bvsnprintf(char *buffer, int32_t maxlen, const char *format, va_list args);
static int32_t fmtstr(char *buffer, int32_t currlen, int32_t maxlen,
                   const char *value, int flags, int min, int max);
static int32_t fmtwstr(char *buffer, int32_t currlen, int32_t maxlen,
                   const wchar_t *value, int flags, int min, int max);
static int32_t fmtint(char *buffer, int32_t currlen, int32_t maxlen,
                   int64_t value, int base, int min, int max, int flags);

#ifdef FP_OUTPUT
# ifdef HAVE_FCVTL
#  define fcvt fcvtl
# endif
static int32_t fmtfp(char *buffer, int32_t currlen, int32_t maxlen,
                  LDOUBLE fvalue, int min, int max, int flags);
#else
#define fmtfp(b, c, m, f, min, max, fl) currlen
#endif

/*
 *  NOTE!!!! do not use this #define with a construct such
 *    as outch(--place);.  It just will NOT work, because the
 *    decrement of place is done ONLY if there is room in the
 *    output buffer.
 */
#define outch(c) {int len=currlen; if (currlen < maxlen) \
        { buffer[len] = (c); currlen++; }}

/* format read states */
#define DP_S_DEFAULT 0
#define DP_S_FLAGS   1
#define DP_S_MIN     2
#define DP_S_DOT     3
#define DP_S_MAX     4
#define DP_S_MOD     5
#define DP_S_CONV    6
#define DP_S_DONE    7

/* format flags - Bits */
#define DP_F_MINUS      (1 << 0)
#define DP_F_PLUS       (1 << 1)
#define DP_F_SPACE      (1 << 2)
#define DP_F_NUM        (1 << 3)
#define DP_F_ZERO       (1 << 4)
#define DP_F_UP         (1 << 5)
#define DP_F_UNSIGNED   (1 << 6)
#define DP_F_DOT        (1 << 7)

/* Conversion Flags */
#define DP_C_INT16    1
#define DP_C_INT32    2
#define DP_C_LDOUBLE  3
#define DP_C_INT64    4
#define DP_C_WCHAR    5      /* wide characters */
#define DP_C_SIZE_T   6

#define char_to_int(p) ((p)- '0')
#undef MAX
#define MAX(p,q) (((p) >= (q)) ? (p) : (q))

/*
  You might ask why does BAREOS have it's own printf routine? Well,
  There are two reasons: 1. Here (as opposed to library routines), we
  define %d and %ld to be 32 bit; %lld and %q to be 64 bit.  2. We
  disable %n for security reasons.
 */

int Bsnprintf(char *str, int32_t size, const char *fmt,  ...)
{
   va_list   arg_ptr;
   int len;

   va_start(arg_ptr, fmt);
   len = Bvsnprintf(str, size, fmt, arg_ptr);
   va_end(arg_ptr);
   return len;
}


int Bvsnprintf(char *buffer, int32_t maxlen, const char *format, va_list args)
{
   char ch;
   int64_t value;
   char *strvalue;
   wchar_t *wstrvalue;
   int min;
   int max;
   int state;
   int flags;
   int cflags;
   int32_t currlen;
   int base;
#ifdef FP_OUTPUT
   LDOUBLE fvalue;
#endif

   state = DP_S_DEFAULT;
   currlen = flags = cflags = min = 0;
   max = -1;
   ch = *format++;
   *buffer = 0;

   while (state != DP_S_DONE) {
      if ((ch == '\0') || (currlen >= maxlen)) {
         state = DP_S_DONE;
      }
      switch (state) {
      case DP_S_DEFAULT:
         if (ch == '%') {
            state = DP_S_FLAGS;
         } else {
            outch(ch);
         }
         ch = *format++;
         break;
      case DP_S_FLAGS:
         switch (ch) {
         case '-':
            flags |= DP_F_MINUS;
            ch = *format++;
            break;
         case '+':
            flags |= DP_F_PLUS;
            ch = *format++;
            break;
         case ' ':
            flags |= DP_F_SPACE;
            ch = *format++;
            break;
         case '#':
            flags |= DP_F_NUM;
            ch = *format++;
            break;
         case '0':
            flags |= DP_F_ZERO;
            ch = *format++;
            break;
         default:
            state = DP_S_MIN;
            break;
         }
         break;
      case DP_S_MIN:
         if (isdigit((unsigned char)ch)) {
            min = 10 * min + char_to_int(ch);
            ch = *format++;
         } else if (ch == '*') {
            min = va_arg(args, int);
            ch = *format++;
            state = DP_S_DOT;
         } else
            state = DP_S_DOT;
         break;
      case DP_S_DOT:
         if (ch == '.') {
            state = DP_S_MAX;
            flags |= DP_F_DOT;
            ch = *format++;
         } else
            state = DP_S_MOD;
         break;
      case DP_S_MAX:
         if (isdigit((unsigned char)ch)) {
            if (max < 0)
               max = 0;
            max = 10 * max + char_to_int(ch);
            ch = *format++;
         } else if (ch == '*') {
            max = va_arg(args, int);
            ch = *format++;
            state = DP_S_MOD;
         } else
            state = DP_S_MOD;
         break;
      case DP_S_MOD:
         switch (ch) {
         case 'h':
            cflags = DP_C_INT16;
            ch = *format++;
            break;
         case 'l':
            cflags = DP_C_INT32;
            ch = *format++;
            if (ch == 's') {
               cflags = DP_C_WCHAR;
            } else if (ch == 'l') {       /* It's a long long */
               cflags = DP_C_INT64;
               ch = *format++;
            }
            break;
         case 'z':
            cflags = DP_C_SIZE_T;
            ch = *format++;
            break;
         case 'L':
            cflags = DP_C_LDOUBLE;
            ch = *format++;
            break;
         case 'q':                 /* same as long long */
            cflags = DP_C_INT64;
            ch = *format++;
            break;
         default:
            break;
         }
         state = DP_S_CONV;
         break;
      case DP_S_CONV:
         switch (ch) {
         case 'd':
         case 'i':
            if (cflags == DP_C_INT16) {
               value = va_arg(args, int32_t);
            } else if (cflags == DP_C_INT32) {
               value = va_arg(args, int32_t);
            } else if (cflags == DP_C_INT64) {
               value = va_arg(args, int64_t);
            } else if (cflags == DP_C_SIZE_T) {
               value = va_arg(args, ssize_t);
            } else {
               value = va_arg(args, int);
            }
            currlen = fmtint(buffer, currlen, maxlen, value, 10, min, max, flags);
            break;
         case 'X':
         case 'x':
         case 'o':
         case 'u':
            if (ch == 'o') {
               base = 8;
            } else if (ch == 'x') {
               base = 16;
            } else if (ch == 'X') {
               base = 16;
               flags |= DP_F_UP;
            } else {
               base = 10;
            }
            flags |= DP_F_UNSIGNED;
            if (cflags == DP_C_INT16) {
               value = va_arg(args, uint32_t);
            } else if (cflags == DP_C_INT32) {
               value = va_arg(args, uint32_t);
            } else if (cflags == DP_C_INT64) {
               value = va_arg(args, uint64_t);
            } else if (cflags == DP_C_SIZE_T) {
               value = va_arg(args, size_t);
            } else {
               value = va_arg(args, unsigned int);
            }
            currlen = fmtint(buffer, currlen, maxlen, value, base, min, max, flags);
            break;
         case 'f':
            if (cflags == DP_C_LDOUBLE) {
               fvalue = va_arg(args, LDOUBLE);
            } else {
               fvalue = va_arg(args, double);
            }
            currlen = fmtfp(buffer, currlen, maxlen, fvalue, min, max, flags);
            break;
         case 'E':
            flags |= DP_F_UP;
         case 'e':
            if (cflags == DP_C_LDOUBLE) {
               fvalue = va_arg(args, LDOUBLE);
            } else {
               fvalue = va_arg(args, double);
            }
            currlen = fmtfp(buffer, currlen, maxlen, fvalue, min, max, flags);
            break;
         case 'G':
            flags |= DP_F_UP;
         case 'g':
            if (cflags == DP_C_LDOUBLE) {
               fvalue = va_arg(args, LDOUBLE);
            } else {
               fvalue = va_arg(args, double);
            }
            currlen = fmtfp(buffer, currlen, maxlen, fvalue, min, max, flags);
            break;
         case 'c':
            ch = va_arg(args, int);
            outch(ch);
            break;
         case 's':
            if (cflags != DP_C_WCHAR) {
              strvalue = va_arg(args, char *);
              if (!strvalue) {
                 strvalue = (char *)"<NULL>";
              }
              currlen = fmtstr(buffer, currlen, maxlen, strvalue, flags, min, max);
            } else {
              /* %ls means to edit wide characters */
              wstrvalue = va_arg(args, wchar_t *);
              if (!wstrvalue) {
                 wstrvalue = (wchar_t *)L"<NULL>";
              }
              currlen = fmtwstr(buffer, currlen, maxlen, wstrvalue, flags, min, max);
            }
            break;
         case 'p':
            flags |= DP_F_UNSIGNED;
            if (sizeof(char *) == 4) {
               value = va_arg(args, uint32_t);
            } else if (sizeof(char *) == 8) {
               value = va_arg(args, uint64_t);
            } else {
               value = 0;             /* we have a problem */
            }
            currlen = fmtint(buffer, currlen, maxlen, value, 16, min, max, flags);
            break;

#ifdef SECURITY_PROBLEM
         case 'n':
            if (cflags == DP_C_INT16) {
               int16_t *num;
               num = va_arg(args, int16_t *);
               *num = currlen;
            } else if (cflags == DP_C_INT32) {
               int32_t *num;
               num = va_arg(args, int32_t *);
               *num = (int32_t)currlen;
            } else if (cflags == DP_C_INT64) {
               int64_t *num;
               num = va_arg(args, int64_t *);
               *num = (int64_t)currlen;
            } else {
               int32_t *num;
               num = va_arg(args, int32_t *);
               *num = (int32_t)currlen;
            }
            break;
#endif
         case '%':
            outch(ch);
            break;
         case 'w':
            /* not supported yet, skip char */
            format++;
            break;
         default:
            /* Unknown, skip */
            break;
         }
         ch = *format++;
         state = DP_S_DEFAULT;
         flags = cflags = min = 0;
         max = -1;
         break;
      case DP_S_DONE:
         break;
      default:
         /* hmm? */
         break;                    /* some picky compilers need this */
      }
   }
   if (currlen < maxlen - 1) {
      buffer[currlen] = '\0';
   } else {
      buffer[maxlen - 1] = '\0';
   }
   return currlen;
}

static int32_t fmtstr(char *buffer, int32_t currlen, int32_t maxlen,
                   const char *value, int flags, int min, int max)
{
   int padlen, strln;              /* amount to pad */
   int cnt = 0;
   char ch;


   if (flags & DP_F_DOT && max < 0) {   /* Max not specified */
      max = 0;
   } else if (max < 0) {
      max = maxlen;
   }
   strln = strlen(value);
   if (strln > max) {
      strln = max;                /* truncate to max */
   }
   padlen = min - strln;
   if (padlen < 0) {
      padlen = 0;
   }
   if (flags & DP_F_MINUS) {
      padlen = -padlen;            /* Left Justify */
   }

   while (padlen > 0) {
      outch(' ');
      --padlen;
   }
   while (*value && (cnt < max)) {
      ch = *value++;
      outch(ch);
      ++cnt;
   }
   while (padlen < 0) {
      outch(' ');
      ++padlen;
   }
   return currlen;
}

static int32_t fmtwstr(char *buffer, int32_t currlen, int32_t maxlen,
                   const wchar_t *value, int flags, int min, int max)
{
   int padlen, strln;              /* amount to pad */
   int cnt = 0;
   char ch;


   if (flags & DP_F_DOT && max < 0) {   /* Max not specified */
      max = 0;
   } else if (max < 0) {
      max = maxlen;
   }
   strln = wcslen(value);
   if (strln > max) {
      strln = max;                /* truncate to max */
   }
   padlen = min - strln;
   if (padlen < 0) {
      padlen = 0;
   }
   if (flags & DP_F_MINUS) {
      padlen = -padlen;            /* Left Justify */
   }

   while (padlen > 0) {
      outch(' ');
      --padlen;
   }
   while (*value && (cnt < max)) {

      ch = (*value++) & 0xff;
      outch(ch);
      ++cnt;
   }
   while (padlen < 0) {
      outch(' ');
      ++padlen;
   }
   return currlen;
}

/* Have to handle DP_F_NUM (ie 0x and 0 alternates) */

static int32_t fmtint(char *buffer, int32_t currlen, int32_t maxlen,
                   int64_t value, int base, int min, int max, int flags)
{
   int signvalue = 0;
   uint64_t uvalue;
   char convert[25];
   int place = 0;
   int spadlen = 0;                /* amount to space pad */
   int zpadlen = 0;                /* amount to zero pad */
   int caps = 0;
   const char *cvt_string;

   if (max < 0) {
      max = 0;
   }

   uvalue = value;

   if (!(flags & DP_F_UNSIGNED)) {
      if (value < 0) {
         signvalue = '-';
         uvalue = -value;
      } else if (flags & DP_F_PLUS) {  /* Do a sign (+/i) */
         signvalue = '+';
      } else if (flags & DP_F_SPACE) {
         signvalue = ' ';
      }
   }

   if (flags & DP_F_UP) {
      caps = 1;                    /* Should characters be upper case? */
   }

   cvt_string = caps ? "0123456789ABCDEF" : "0123456789abcdef";
   do {
      convert[place++] = cvt_string[uvalue % (unsigned)base];
      uvalue = (uvalue / (unsigned)base);
   } while (uvalue && (place < (int)sizeof(convert)));
   if (place == (int)sizeof(convert)) {
      place--;
   }
   convert[place] = 0;

   zpadlen = max - place;
   spadlen = min - MAX(max, place) - (signvalue ? 1 : 0);
   if (zpadlen < 0)
      zpadlen = 0;
   if (spadlen < 0)
      spadlen = 0;
   if (flags & DP_F_ZERO) {
      zpadlen = MAX(zpadlen, spadlen);
      spadlen = 0;
   }
   if (flags & DP_F_MINUS)
      spadlen = -spadlen;          /* Left Justifty */

#ifdef DEBUG_SNPRINTF
   printf("zpad: %d, spad: %d, min: %d, max: %d, place: %d\n",
          zpadlen, spadlen, min, max, place);
#endif

   /* Spaces */
   while (spadlen > 0) {
      outch(' ');
      --spadlen;
   }

   /* Sign */
   if (signvalue) {
      outch(signvalue);
   }

   /* Zeros */
   if (zpadlen > 0) {
      while (zpadlen > 0) {
         outch('0');
         --zpadlen;
      }
   }

   /* Output digits backward giving correct order */
   while (place > 0) {
      place--;
      outch(convert[place]);
   }

   /* Left Justified spaces */
   while (spadlen < 0) {
      outch(' ');
      ++spadlen;
   }
   return currlen;
}

#ifdef FP_OUTPUT

static LDOUBLE abs_val(LDOUBLE value)
{
   LDOUBLE result = value;

   if (value < 0)
      result = -value;

   return result;
}

static LDOUBLE pow10(int exp)
{
   LDOUBLE result = 1;

   while (exp) {
      result *= 10;
      exp--;
   }

   return result;
}

static int64_t round(LDOUBLE value)
{
   int64_t intpart;

   intpart = (int64_t)value;
   value = value - intpart;
   if (value >= 0.5)
      intpart++;

   return intpart;
}

static int32_t fmtfp(char *buffer, int32_t currlen, int32_t maxlen,
                  LDOUBLE fvalue, int min, int max, int flags)
{
   int signvalue = 0;
   LDOUBLE ufvalue;
#ifndef HAVE_FCVT
   char iconvert[311];
   char fconvert[311];
#else
   char iconvert[311];
   char fconvert[311];
   char *result;
   char dummy[10];
   int dec_pt, sig;
   int r_length;
   extern char *fcvt(double value, int ndigit, int *decpt, int *sign);
#endif
   int fiter;
   int iplace = 0;
   int fplace = 0;
   int padlen = 0;                 /* amount to pad */
   int zpadlen = 0;
   int64_t intpart;
   int64_t fracpart;
   const char *cvt_str;

   /*
    * AIX manpage says the default is 0, but Solaris says the default
    * is 6, and sprintf on AIX defaults to 6
    */
   if (max < 0)
      max = 6;

   ufvalue = abs_val(fvalue);

   if (fvalue < 0)
      signvalue = '-';
   else if (flags & DP_F_PLUS)     /* Do a sign (+/i) */
      signvalue = '+';
   else if (flags & DP_F_SPACE)
      signvalue = ' ';

#ifndef HAVE_FCVT
   intpart = (int64_t)ufvalue;

   /*
    * Sorry, we only support 9 digits past the decimal because of our
    * conversion method
    */
   if (max > 9)
      max = 9;

   /* We "cheat" by converting the fractional part to integer by
    * multiplying by a factor of 10
    */
   fracpart = round((pow10(max)) * (ufvalue - intpart));

   if (fracpart >= pow10(max)) {
      intpart++;
      fracpart -= (int64_t)pow10(max);
   }

#ifdef DEBUG_SNPRINTF
   printf("fmtfp: %g %lld.%lld min=%d max=%d\n",
          (double)fvalue, intpart, fracpart, min, max);
#endif

   /* Convert integer part */
   cvt_str = "0123456789";
   do {
      iconvert[iplace++] = cvt_str[(int)(intpart % 10)];
      intpart = (intpart / 10);
   } while (intpart && (iplace < (int)sizeof(iconvert)));

   if (iplace == (int)sizeof(fconvert)) {
      iplace--;
   }
   iconvert[iplace] = 0;

   /* Convert fractional part */
   cvt_str = "0123456789";
   fiter = max;
   do {
      fconvert[fplace++] = cvt_str[fracpart % 10];
      fracpart = (fracpart / 10);
   } while (--fiter);

   if (fplace == (int)sizeof(fconvert)) {
      fplace--;
   }
   fconvert[fplace] = 0;
#else                              /* use fcvt() */
   if (max > 310) {
      max = 310;
   }
# ifdef HAVE_FCVTL
   result = fcvtl(ufvalue, max, &dec_pt, &sig);
# else
   result = fcvt(ufvalue, max, &dec_pt, &sig);
# endif

   if (!result) {
      r_length = 0;
      dummy[0] = 0;
      result = dummy;
   } else {
      r_length = strlen(result);
   }

   /*
    * Fix broken fcvt implementation returns..
    */

   if (r_length == 0) {
      result[0] = '0';
      result[1] = '\0';
      r_length = 1;
   }

   if (r_length < dec_pt)
      dec_pt = r_length;

   if (dec_pt <= 0) {
      iplace = 1;
      iconvert[0] = '0';
      iconvert[1] = '\0';

      fplace = 0;

      while (r_length) {
         fconvert[fplace++] = result[--r_length];
      }

      while ((dec_pt < 0) && (fplace < max)) {
         fconvert[fplace++] = '0';
         dec_pt++;
      }
   } else {
      int c;

      iplace = 0;
      for (c = dec_pt; c; iconvert[iplace++] = result[--c]);
      iconvert[iplace] = '\0';

      result += dec_pt;
      fplace = 0;

      for (c = (r_length - dec_pt); c; fconvert[fplace++] = result[--c]);
   }
#endif  /* HAVE_FCVT */

   /* -1 for decimal point, another -1 if we are printing a sign */
   padlen = min - iplace - max - 1 - ((signvalue) ? 1 : 0);
   zpadlen = max - fplace;
   if (zpadlen < 0) {
      zpadlen = 0;
   }
   if (padlen < 0) {
      padlen = 0;
   }
   if (flags & DP_F_MINUS) {
      padlen = -padlen;            /* Left Justifty */
   }

   if ((flags & DP_F_ZERO) && (padlen > 0)) {
      if (signvalue) {
         outch(signvalue);
         --padlen;
         signvalue = 0;
      }
      while (padlen > 0) {
         outch('0');
         --padlen;
      }
   }
   while (padlen > 0) {
      outch(' ');
      --padlen;
   }
   if (signvalue) {
      outch(signvalue);
   }

   while (iplace > 0) {
      iplace--;
      outch(iconvert[iplace]);
   }


#ifdef DEBUG_SNPRINTF
   printf("fmtfp: fplace=%d zpadlen=%d\n", fplace, zpadlen);
#endif

   /*
    * Decimal point.  This should probably use locale to find the correct
    * char to print out.
    */
   if (max > 0) {
      outch('.');
      while (fplace > 0) {
         fplace--;
         outch(fconvert[fplace]);
      }
   }

   while (zpadlen > 0) {
      outch('0');
      --zpadlen;
   }

   while (padlen < 0) {
      outch(' ');
      ++padlen;
   }
   return currlen;
}
#endif  /* FP_OUTPUT */
#endif /* USE_BSNPRINTF */
