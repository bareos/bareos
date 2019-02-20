/**************************************************************
 * Original:
 * Patrick Powell Tue Apr 11 09:48:21 PDT 1995
 * A bombproof version of doprnt(dopr) included.
 * Sigh.  This sort of thing is always nasty do deal with.  Note that
 * the version here does not include floating point...
 *
 * snprintf() is used instead of sprintf() as it does limit checks
 * for string length.  This covers a nasty loophole.
 *
 * The other functions are there to prevent NULL pointers from
 * causing nast effects.
 *
 * More Recently:
 *  Brandon Long <blong@fiction.net> 9/15/96 for mutt 0.43
 *  This was ugly.  It is still ugly.  I opted out of floating point
 *  numbers, but the formatter understands just about everything
 *  from the normal C string format, at least as far as I can tell from
 *  the Solaris 2.5 printf(3S) man page.
 *
 *  Brandon Long <blong@fiction.net> 10/22/97 for mutt 0.87.1
 *    Ok, added some minimal floating point support, which means this
 *    probably requires libm on most operating systems.  Don't yet
 *    support the exponent(e, E) and sigfig(g, G).  Also, fmtint()
 *    was pretty badly broken, it just wasn't being exercised in ways
 *    which showed it, so that's been fixed.  Also, formated the code
 *    to mutt conventions, and removed dead code left over from the
 *    original.  Also, there is now a builtin-test, just compile with:
 *           gcc -DTEST_SNPRINTF -o snprintf snprintf.c -lm
 *    and run snprintf for results.
 *
 *  Thomas Roessler <roessler@guug.de> 01/27/98 for mutt 0.89i
 *    The PGP code was using unsigned hexadecimal formats.
 *    Unfortunately, unsigned formats simply didn't work.
 *
 *  Michael Elkins <me@cs.hmc.edu> 03/05/98 for mutt 0.90.8
 *    The original code assumed that both snprintf() and vsnprintf() were
 *    missing.  Some systems only have snprintf() but not vsnprintf(), so
 *    the code is now broken down under HAVE_SNPRINTF and HAVE_VSNPRINTF.
 *
 *  Ben Lindstrom <mouring@eviladmin.org> 09/27/00 for OpenSSH
 *    Welcome to the world of %lld and %qd support.  With other
 *    long long support.  This is needed for sftp-server to work
 *    right.
 *
 *  Ben Lindstrom <mouring@eviladmin.org> 02/12/01 for OpenSSH
 *    Removed all hint of VARARGS stuff and banished it to the void,
 *    and did a bit of KNF style work to make things a bit more
 *    acceptable.  Consider stealing from mutt or enlightenment.
 **************************************************************/

#include "include/bareos.h"

typedef void(prfun)(char*, size_t*, size_t, int);

int dopr(char* buffer, size_t maxlen, const char* format, va_list args, prfun);


static void fmtstr(char* buffer,
                   size_t* currlen,
                   size_t maxlen,
                   char* value,
                   int flags,
                   int min,
                   int max,
                   prfun);

static void fmtint(char* buffer,
                   size_t* currlen,
                   size_t maxlen,
                   INT64 value,
                   int base,
                   int min,
                   int max,
                   int flags,
                   prfun);

static void fmtfp(char* buffer,
                  size_t* currlen,
                  size_t maxlen,
                  long double fvalue,
                  int min,
                  int max,
                  int flags,
                  prfun);

static void dopr_outch(char* buffer, size_t* currlen, size_t maxlen, int c);

/*
 * dopr(): poor man's version of doprintf
 */

#ifndef MAX
#define MAX(a, b) ((a) > (b) ? (a) : (b))
#endif

/* format read states */
#define DP_S_DEFAULT 0
#define DP_S_FLAGS 1
#define DP_S_MIN 2
#define DP_S_DOT 3
#define DP_S_MAX 4
#define DP_S_MOD 5
#define DP_S_CONV 6
#define DP_S_DONE 7

/* format flags - Bits */
#define DP_F_MINUS (1 << 0)
#define DP_F_PLUS (1 << 1)
#define DP_F_SPACE (1 << 2)
#define DP_F_NUM (1 << 3)
#define DP_F_ZERO (1 << 4)
#define DP_F_UP (1 << 5)
#define DP_F_UNSIGNED (1 << 6)

/* Conversion Flags */
#define DP_C_SHORT 1
#define DP_C_LONG 2
#define DP_C_LDOUBLE 3
#define DP_C_LONG_LONG 4

#define char_to_int(p) (p - '0')
#define abs_val(p) (p < 0 ? -p : p)

static const char digitval[] = "0123456789abcdef0123456789ABCDEF";

int dopr(char* buffer,
         size_t maxlen,
         const char* format,
         va_list args,
         prfun outch)
{
  char* strvalue;
  char ch;
  INT64 value;
  long double fvalue;
  int min = 0;
  int max = -1;
  int state = DP_S_DEFAULT;
  int flags = 0;
  int cflags = 0;
  size_t currlen = 0;

  ch = *format++;

  while (state != DP_S_DONE) {
    if ((ch == '\0') || (currlen >= maxlen)) state = DP_S_DONE;

    switch (state) {
      case DP_S_DEFAULT:
        if (ch == '%')
          state = DP_S_FLAGS;
        else
          outch(buffer, &currlen, maxlen, ch);
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
          ch = *format++;
        } else
          state = DP_S_MOD;
        break;
      case DP_S_MAX:
        if (isdigit((unsigned char)ch)) {
          if (max < 0) max = 0;
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
            cflags = DP_C_SHORT;
            ch = *format++;
            break;
          case 'l':
            cflags = DP_C_LONG;
            ch = *format++;
            if (ch == 'l') {
              cflags = DP_C_LONG_LONG;
              ch = *format++;
            }
            break;
          case 'q':
            cflags = DP_C_LONG_LONG;
            ch = *format++;
            break;
          case 'L':
            cflags = DP_C_LDOUBLE;
            ch = *format++;
            break;
          default:
            break;
        }
        state = DP_S_CONV;
        break;
      case DP_S_CONV:
        switch (ch) {
          case 'b':
            flags |= DP_F_UNSIGNED;
            if (cflags == DP_C_SHORT)
              value = va_arg(args, unsigned int);
            else if (cflags == DP_C_LONG)
              value = va_arg(args, unsigned long int);
            else if (cflags == DP_C_LONG_LONG)
              value = va_arg(args, UINT64);
            else
              value = va_arg(args, unsigned int);
            fmtint(buffer, &currlen, maxlen, value, 2, min, max, flags, outch);
            break;
          case 'd':
          case 'i':
            if (cflags == DP_C_SHORT)
              value = va_arg(args, int);
            else if (cflags == DP_C_LONG)
              value = va_arg(args, long int);
            else if (cflags == DP_C_LONG_LONG)
              value = va_arg(args, INT64);
            else
              value = va_arg(args, int);
            fmtint(buffer, &currlen, maxlen, value, 10, min, max, flags, outch);
            break;
          case 'o':
            flags |= DP_F_UNSIGNED;
            if (cflags == DP_C_SHORT)
              value = va_arg(args, unsigned int);
            else if (cflags == DP_C_LONG)
              value = va_arg(args, unsigned long int);
            else if (cflags == DP_C_LONG_LONG)
              value = va_arg(args, UINT64);
            else
              value = va_arg(args, unsigned int);
            fmtint(buffer, &currlen, maxlen, value, 8, min, max, flags, outch);
            break;
          case 'u':
            flags |= DP_F_UNSIGNED;
            if (cflags == DP_C_SHORT)
              value = va_arg(args, unsigned int);
            else if (cflags == DP_C_LONG)
              value = va_arg(args, unsigned long int);
            else if (cflags == DP_C_LONG_LONG)
              value = va_arg(args, UINT64);
            else
              value = va_arg(args, unsigned int);
            fmtint(buffer, &currlen, maxlen, value, 10, min, max, flags, outch);
            break;
          case 'X':
            flags |= DP_F_UP;
          case 'x':
            flags |= DP_F_UNSIGNED;
            if (cflags == DP_C_SHORT)
              value = va_arg(args, unsigned int);
            else if (cflags == DP_C_LONG)
              value = va_arg(args, unsigned long int);
            else if (cflags == DP_C_LONG_LONG)
              value = va_arg(args, UINT64);
            else
              value = va_arg(args, unsigned int);
            fmtint(buffer, &currlen, maxlen, value, 16, min, max, flags, outch);
            break;
          case 'f':
            if (cflags == DP_C_LDOUBLE)
              fvalue = va_arg(args, long double);
            else
              fvalue = va_arg(args, double);
            /* um, floating point? */
            fmtfp(buffer, &currlen, maxlen, fvalue, min, max, flags, outch);
            break;
          case 'E':
            flags |= DP_F_UP;
          case 'e':
            if (cflags == DP_C_LDOUBLE)
              fvalue = va_arg(args, long double);
            else
              fvalue = va_arg(args, double);
            break;
          case 'G':
            flags |= DP_F_UP;
          case 'g':
            if (cflags == DP_C_LDOUBLE)
              fvalue = va_arg(args, long double);
            else
              fvalue = va_arg(args, double);
            break;
          case 'c':
            outch(buffer, &currlen, maxlen, va_arg(args, int));
            break;
          case 's':
            strvalue = va_arg(args, char*);
            if (max < 0) { max = maxlen; /* ie, no max */ }
            fmtstr(buffer, &currlen, maxlen, strvalue, flags, min, max, outch);
            break;
          case 'p':
            flags |= DP_F_UNSIGNED;
            if (sizeof(char*) == 4) {
              value = va_arg(args, uint32_t);
            } else if (sizeof(char*) == 8) {
              value = va_arg(args, uint64_t);
            } else {
              value = 0; /* we have a problem */
            }
            fmtint(buffer, &currlen, maxlen, value, 16, min, max, flags, outch);
            break;
          case 'n':
            if (cflags == DP_C_SHORT) {
              short int* num;
              num = va_arg(args, short int*);
              *num = currlen;
            } else if (cflags == DP_C_LONG) {
              long int* num;
              num = va_arg(args, long int*);
              *num = currlen;
            } else if (cflags == DP_C_LONG_LONG) {
              INT64* num;
              num = va_arg(args, INT64*);
              *num = currlen;
            } else {
              int* num;
              num = va_arg(args, int*);
              *num = currlen;
            }
            break;
          case '%':
            outch(buffer, &currlen, maxlen, ch);
            break;
          case 'w': /* not supported yet, treat as next char */
            ch = *format++;
            break;
          default: /* Unknown, skip */
            break;
        }
        ch = *format++;
        state = DP_S_DEFAULT;
        flags = cflags = min = 0;
        max = -1;
        break;
      case DP_S_DONE:
        break;
      default: /* hmm? */
        break; /* some picky compilers need this */
    }
  }
  outch(buffer, &currlen, maxlen, -1);
  return currlen;
}

static void fmtstr(char* buffer,
                   size_t* currlen,
                   size_t maxlen,
                   char* value,
                   int flags,
                   int min,
                   int max,
                   prfun outch)
{
  int padlen, strln; /* amount to pad */
  int cnt = 0;

  if (value == NULL) value = (char*)"<NULL>";

  for (strln = 0; value[strln]; ++strln)
    ; /* strlen */
  padlen = min - strln;
  if (padlen < 0) padlen = 0;
  if (flags & DP_F_MINUS) padlen = -padlen; /* Left Justify */

  while ((padlen > 0) && (cnt < max)) {
    outch(buffer, currlen, maxlen, ' ');
    --padlen;
    ++cnt;
  }
  while (*value && (cnt < max)) {
    outch(buffer, currlen, maxlen, *value++);
    ++cnt;
  }
  while ((padlen < 0) && (cnt < max)) {
    outch(buffer, currlen, maxlen, ' ');
    ++padlen;
    ++cnt;
  }
}

/* Have to handle DP_F_NUM(ie 0x and 0 alternates) */

static void fmtint(char* buffer,
                   size_t* currlen,
                   size_t maxlen,
                   INT64 value,
                   int base,
                   int min,
                   int max,
                   int flags,
                   prfun outch)
{
  UINT64 uvalue;
  char convert[20];
  int signvalue = 0;
  int place = 0;
  int spadlen = 0; /* amount to space pad */
  int zpadlen = 0; /* amount to zero pad */
  int caps = 0;

  if (max < 0) max = 0;

  uvalue = value;

  if (!(flags & DP_F_UNSIGNED)) {
    if (value < 0) {
      signvalue = '-';
      uvalue = -value;
    } else if (flags & DP_F_PLUS) /* Do a sign(+/i) */
      signvalue = '+';
    else if (flags & DP_F_SPACE)
      signvalue = ' ';
  }

  if (flags & DP_F_UP) caps = 16; /* Should characters be upper case? */

  do {
    convert[place++] = digitval[(uvalue % base) + caps];
    uvalue = (uvalue / (unsigned)base);
  } while (uvalue && (place < 20));

  if (place == 20) place--;

  convert[place] = 0;

  zpadlen = max - place;
  spadlen = min - MAX(max, place) - (signvalue ? 1 : 0);

  if (zpadlen < 0) zpadlen = 0;
  if (spadlen < 0) spadlen = 0;
  if (flags & DP_F_ZERO) {
    zpadlen = MAX(zpadlen, spadlen);
    spadlen = 0;
  }
  if (flags & DP_F_MINUS) spadlen = -spadlen; /* Left Justifty */


  /* Spaces */
  while (spadlen > 0) {
    outch(buffer, currlen, maxlen, ' ');
    --spadlen;
  }

  /* Sign */
  if (signvalue) outch(buffer, currlen, maxlen, signvalue);

  /* Zeros */
  if (zpadlen > 0) {
    while (zpadlen > 0) {
      outch(buffer, currlen, maxlen, '0');
      --zpadlen;
    }
  }

  /* Digits */
  while (place > 0) outch(buffer, currlen, maxlen, convert[--place]);

  /* Left Justified spaces */
  while (spadlen < 0) {
    outch(buffer, currlen, maxlen, ' ');
    ++spadlen;
  }
}

static long double pow10(int exp)
{
  long double result = 1;

  while (exp) {
    result *= 10;
    exp--;
  }

  return result;
}

static long round(long double value)
{
  long intpart = (long)value;

  value -= intpart;
  if (value >= 0.5) intpart++;

  return intpart;
}

static void fmtfp(char* buffer,
                  size_t* currlen,
                  size_t maxlen,
                  long double fvalue,
                  int min,
                  int max,
                  int flags,
                  prfun outch)
{
  char iconvert[20];
  char fconvert[20];
  int signvalue = 0;
  int iplace = 0;
  int fplace = 0;
  int padlen = 0; /* amount to pad */
  int zpadlen = 0;
  long intpart;
  long fracpart;
  long double ufvalue;

  /*
   * AIX manpage says the default is 0, but Solaris says the default
   * is 6, and sprintf on AIX defaults to 6
   */
  if (max < 0) max = 6;

  ufvalue = abs_val(fvalue);

  if (fvalue < 0)
    signvalue = '-';
  else if (flags & DP_F_PLUS) /* Do a sign(+/i) */
    signvalue = '+';
  else if (flags & DP_F_SPACE)
    signvalue = ' ';

  intpart = (long)ufvalue;

  /*
   * Sorry, we only support 9 digits past the decimal because of our
   * conversion method
   */
  if (max > 9) max = 9;

  /* We "cheat" by converting the fractional part to integer by
   * multiplying by a factor of 10
   */
  fracpart = round((pow10(max)) * (ufvalue - intpart));

  if (fracpart >= pow10(max)) {
    intpart++;
    fracpart -= (long)pow10(max);
  }

  /* Convert integer part */
  do {
    iconvert[iplace++] = digitval[intpart % 10];
    intpart = (intpart / 10);
  } while (intpart && (iplace < 20));

  if (iplace == 20) iplace--;

  iconvert[iplace] = 0;

  /* Convert fractional part */
  do {
    fconvert[fplace++] = digitval[fracpart % 10];
    fracpart = (fracpart / 10);
  } while (fracpart && (fplace < 20));

  if (fplace == 20) fplace--;

  fconvert[fplace] = 0;

  /* -1 for decimal point, another -1 if we are printing a sign */
  padlen = min - iplace - max - 1 - ((signvalue) ? 1 : 0);
  zpadlen = max - fplace;
  if (zpadlen < 0) zpadlen = 0;
  if (padlen < 0) padlen = 0;
  if (flags & DP_F_MINUS) padlen = -padlen; /* Left Justifty */

  if ((flags & DP_F_ZERO) && (padlen > 0)) {
    if (signvalue) {
      outch(buffer, currlen, maxlen, signvalue);
      --padlen;
      signvalue = 0;
    }
    while (padlen > 0) {
      outch(buffer, currlen, maxlen, '0');
      --padlen;
    }
  }

  while (padlen > 0) {
    outch(buffer, currlen, maxlen, ' ');
    --padlen;
  }

  if (signvalue) outch(buffer, currlen, maxlen, signvalue);

  while (iplace > 0) outch(buffer, currlen, maxlen, iconvert[--iplace]);

  /*
   * Decimal point.  This should probably use locale to find the correct
   * char to print out.
   */
  outch(buffer, currlen, maxlen, '.');

  while (fplace > 0) outch(buffer, currlen, maxlen, fconvert[--fplace]);

  while (zpadlen > 0) {
    outch(buffer, currlen, maxlen, '0');
    --zpadlen;
  }

  while (padlen < 0) {
    outch(buffer, currlen, maxlen, ' ');
    ++padlen;
  }
}

static void dopr_outch(char* buffer, size_t* currlen, size_t maxlen, int c)
{
  if (c == -1) {
    if (*currlen < maxlen - 1)
      buffer[*currlen] = '\0';
    else
      buffer[maxlen - 1] = '\0';
  } else if (*currlen < maxlen)
    buffer[(*currlen)++] = c;
}

int __sprintf(char* str, const char* fmt, ...)
{
  int rval;
  va_list ap;
  va_start(ap, fmt);
  rval = vsnprintf(str, 128 * 1024, fmt, ap);
  va_end(ap);
  return rval;
}


int __snprintf(char* str, size_t count, const char* fmt, ...)
{
  int rval;
  va_list ap;

  va_start(ap, fmt);
  rval = vsnprintf(str, count, fmt, ap);
  va_end(ap);

  return rval;
}

int __vsprintf(char* s, const char* format, va_list args)
{
  s[0] = 0;
  return dopr(s, 0xfffff, format, args, dopr_outch);
}

int __vsnprintf(char* s, size_t count, const char* format, va_list args)
{
  s[0] = 0;
  return dopr(s, count, format, args, dopr_outch);
}
