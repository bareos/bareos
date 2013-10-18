%{
/*
**  Originally written by Steven M. Bellovin <smb@research.att.com> while
**  at the University of North Carolina at Chapel Hill.  Later tweaked by
**  a couple of people on Usenet.  Completely overhauled by Rich $alz
**  <rsalz@bbn.com> and Jim Berets <jberets@bbn.com> in August, 1990;
**
**  This grammar has 13 shift/reduce conflicts.
**
**  This code is in the public domain and has no copyright.
*/

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

/* Since the code of getdate.y is not included in the Emacs executable
   itself, there is no need to #define static in this file.  Even if
   the code were included in the Emacs executable, it probably
   wouldn't do any harm to #undef it here; this will only cause
   problems if we try to write to a static variable, which I don't
   think this code needs to do.  */
#ifdef emacs
# undef static
#endif

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <time.h>
#include <string.h>

#define ISDIGIT(c) ((unsigned) (c) - '0' <= 9)

#define EPOCH		1970
#define HOUR(x)		((x) * 60)

#define MAX_BUFF_LEN    128   /* size of buffer to read the date into */

/*
**  An entry in the lexical lookup table.
*/
typedef struct _TABLE {
    const char	*name;
    int		type;
    int		value;
} TABLE;


/*
**  Meridian:  am, pm, or 24-hour style.
*/
typedef enum _MERIDIAN {
    MERam, MERpm, MER24
} MERIDIAN;

struct global {
  int	DayOrdinal;
  int	DayNumber;
  int	HaveDate;
  int	HaveDay;
  int	HaveRel;
  int	HaveTime;
  int	HaveZone;
  int	Timezone;
  int	Day;
  int	Hour;
  int	Minutes;
  int	Month;
  int	Seconds;
  int	Year;
  MERIDIAN	Meridian;
  int	RelDay;
  int	RelHour;
  int	RelMinutes;
  int	RelMonth;
  int	RelSeconds;
  int	RelYear;
};

union YYSTYPE;
static int yylex (union YYSTYPE *lvalp, const char **yyInput);
static int yyerror (const char **yyInput, struct global *yy, char *s);
static int yyparse (const char **yyInput, struct global *yy);

#define YYENABLE_NLS 0
#define YYLTYPE_IS_TRIVIAL 0

%}

%name-prefix "lu_gd"
%define api.pure

%parse-param { const char **yyInput }
%lex-param { const char **yyInput }
%parse-param { struct global *yy }

%union {
    int			Number;
    enum _MERIDIAN	Meridian;
}

%token	tAGO tDAY tDAY_UNIT tDAYZONE tDST tHOUR_UNIT tID
%token	tMERIDIAN tMINUTE_UNIT tMONTH tMONTH_UNIT
%token	tSEC_UNIT tSNUMBER tUNUMBER tYEAR_UNIT tZONE

%type	<Number>	tDAY tDAY_UNIT tDAYZONE tHOUR_UNIT tMINUTE_UNIT
%type	<Number>	tMONTH tMONTH_UNIT
%type	<Number>	tSEC_UNIT tSNUMBER tUNUMBER tYEAR_UNIT tZONE
%type	<Meridian>	tMERIDIAN o_merid

%expect 13

%%

spec	: /* NULL */
	| spec item
	;

item	: time {
	    yy->HaveTime++;
	}
	| zone {
	    yy->HaveZone++;
	}
	| date {
	    yy->HaveDate++;
	}
	| day {
	    yy->HaveDay++;
	}
	| rel {
	    yy->HaveRel++;
	}
	| number
	;

time	: tUNUMBER tMERIDIAN {
	    yy->Hour = $1;
	    yy->Minutes = 0;
	    yy->Seconds = 0;
	    yy->Meridian = $2;
	}
	| tUNUMBER ':' tUNUMBER o_merid {
	    yy->Hour = $1;
	    yy->Minutes = $3;
	    yy->Seconds = 0;
	    yy->Meridian = $4;
	}
	| tUNUMBER ':' tUNUMBER tSNUMBER {
	    yy->Hour = $1;
	    yy->Minutes = $3;
	    yy->Meridian = MER24;
	    yy->HaveZone++;
	    yy->Timezone = ($4 < 0
			  ? -$4 % 100 + (-$4 / 100) * 60
			  : - ($4 % 100 + ($4 / 100) * 60));
	}
	| tUNUMBER ':' tUNUMBER ':' tUNUMBER o_merid {
	    yy->Hour = $1;
	    yy->Minutes = $3;
	    yy->Seconds = $5;
	    yy->Meridian = $6;
	}
	| tUNUMBER ':' tUNUMBER ':' tUNUMBER tSNUMBER {
	    yy->Hour = $1;
	    yy->Minutes = $3;
	    yy->Seconds = $5;
	    yy->Meridian = MER24;
	    yy->HaveZone++;
	    yy->Timezone = ($6 < 0
			  ? -$6 % 100 + (-$6 / 100) * 60
			  : - ($6 % 100 + ($6 / 100) * 60));
	}
	;

zone	: tZONE {
	    yy->Timezone = $1;
	}
	| tDAYZONE {
	    yy->Timezone = $1 - 60;
	}
	|
	  tZONE tDST {
	    yy->Timezone = $1 - 60;
	}
	;

day	: tDAY {
	    yy->DayOrdinal = 1;
	    yy->DayNumber = $1;
	}
	| tDAY ',' {
	    yy->DayOrdinal = 1;
	    yy->DayNumber = $1;
	}
	| tUNUMBER tDAY {
	    yy->DayOrdinal = $1;
	    yy->DayNumber = $2;
	}
	;

date	: tUNUMBER '/' tUNUMBER {
	    yy->Month = $1;
	    yy->Day = $3;
	}
	| tUNUMBER '/' tUNUMBER '/' tUNUMBER {
	  /* Interpret as YY->YY/MM/DD if $1 >= 1000, otherwise as MM/DD/YY.
	     The goal in recognizing YY->YY/MM/DD is solely to support legacy
	     machine-generated dates like those in an RCS log listing.  If
	     you want portability, use the ISO 8601 format.  */
	  if ($1 >= 1000)
	    {
	      yy->Year = $1;
	      yy->Month = $3;
	      yy->Day = $5;
	    }
	  else
	    {
	      yy->Month = $1;
	      yy->Day = $3;
	      yy->Year = $5;
	    }
	}
	| tUNUMBER tSNUMBER tSNUMBER {
	    /* ISO 8601 format.  yy->yy-mm-dd.  */
	    yy->Year = $1;
	    yy->Month = -$2;
	    yy->Day = -$3;
	}
	| tUNUMBER tMONTH tSNUMBER {
	    /* e.g. 17-JUN-1992.  */
	    yy->Day = $1;
	    yy->Month = $2;
	    yy->Year = -$3;
	}
	| tMONTH tUNUMBER {
	    yy->Month = $1;
	    yy->Day = $2;
	}
	| tMONTH tUNUMBER ',' tUNUMBER {
	    yy->Month = $1;
	    yy->Day = $2;
	    yy->Year = $4;
	}
	| tUNUMBER tMONTH {
	    yy->Month = $2;
	    yy->Day = $1;
	}
	| tUNUMBER tMONTH tUNUMBER {
	    yy->Month = $2;
	    yy->Day = $1;
	    yy->Year = $3;
	}
	;

rel	: relunit tAGO {
	    yy->RelSeconds = -yy->RelSeconds;
	    yy->RelMinutes = -yy->RelMinutes;
	    yy->RelHour = -yy->RelHour;
	    yy->RelDay = -yy->RelDay;
	    yy->RelMonth = -yy->RelMonth;
	    yy->RelYear = -yy->RelYear;
	}
	| relunit
	;

relunit	: tUNUMBER tYEAR_UNIT {
	    yy->RelYear += $1 * $2;
	}
	| tSNUMBER tYEAR_UNIT {
	    yy->RelYear += $1 * $2;
	}
	| tYEAR_UNIT {
	    yy->RelYear++;
	}
	| tUNUMBER tMONTH_UNIT {
	    yy->RelMonth += $1 * $2;
	}
	| tSNUMBER tMONTH_UNIT {
	    yy->RelMonth += $1 * $2;
	}
	| tMONTH_UNIT {
	    yy->RelMonth++;
	}
	| tUNUMBER tDAY_UNIT {
	    yy->RelDay += $1 * $2;
	}
	| tSNUMBER tDAY_UNIT {
	    yy->RelDay += $1 * $2;
	}
	| tDAY_UNIT {
	    yy->RelDay++;
	}
	| tUNUMBER tHOUR_UNIT {
	    yy->RelHour += $1 * $2;
	}
	| tSNUMBER tHOUR_UNIT {
	    yy->RelHour += $1 * $2;
	}
	| tHOUR_UNIT {
	    yy->RelHour++;
	}
	| tUNUMBER tMINUTE_UNIT {
	    yy->RelMinutes += $1 * $2;
	}
	| tSNUMBER tMINUTE_UNIT {
	    yy->RelMinutes += $1 * $2;
	}
	| tMINUTE_UNIT {
	    yy->RelMinutes++;
	}
	| tUNUMBER tSEC_UNIT {
	    yy->RelSeconds += $1 * $2;
	}
	| tSNUMBER tSEC_UNIT {
	    yy->RelSeconds += $1 * $2;
	}
	| tSEC_UNIT {
	    yy->RelSeconds++;
	}
	;

number	: tUNUMBER
          {
	    if (yy->HaveTime && yy->HaveDate && !yy->HaveRel)
	      yy->Year = $1;
	    else
	      {
		if ($1>10000)
		  {
		    yy->HaveDate++;
		    yy->Day= ($1)%100;
		    yy->Month= ($1/100)%100;
		    yy->Year = $1/10000;
		  }
		else
		  {
		    yy->HaveTime++;
		    if ($1 < 100)
		      {
			yy->Hour = $1;
			yy->Minutes = 0;
		      }
		    else
		      {
		    	yy->Hour = $1 / 100;
		    	yy->Minutes = $1 % 100;
		      }
		    yy->Seconds = 0;
		    yy->Meridian = MER24;
		  }
	      }
	  }
	;

o_merid	: /* NULL */
	  {
	    $$ = MER24;
	  }
	| tMERIDIAN
	  {
	    $$ = $1;
	  }
	;

%%

/* Month and day table. */
static TABLE const MonthDayTable[] = {
    { "january",	tMONTH,  1 },
    { "february",	tMONTH,  2 },
    { "march",		tMONTH,  3 },
    { "april",		tMONTH,  4 },
    { "may",		tMONTH,  5 },
    { "june",		tMONTH,  6 },
    { "july",		tMONTH,  7 },
    { "august",		tMONTH,  8 },
    { "september",	tMONTH,  9 },
    { "sept",		tMONTH,  9 },
    { "october",	tMONTH, 10 },
    { "november",	tMONTH, 11 },
    { "december",	tMONTH, 12 },
    { "sunday",		tDAY, 0 },
    { "monday",		tDAY, 1 },
    { "tuesday",	tDAY, 2 },
    { "tues",		tDAY, 2 },
    { "wednesday",	tDAY, 3 },
    { "wednes",		tDAY, 3 },
    { "thursday",	tDAY, 4 },
    { "thur",		tDAY, 4 },
    { "thurs",		tDAY, 4 },
    { "friday",		tDAY, 5 },
    { "saturday",	tDAY, 6 },
    { NULL, 0, 0 }
};

/* Time units table. */
static TABLE const UnitsTable[] = {
    { "year",		tYEAR_UNIT,	1 },
    { "month",		tMONTH_UNIT,	1 },
    { "fortnight",	tDAY_UNIT,	14 },
    { "week",		tDAY_UNIT,	7 },
    { "day",		tDAY_UNIT,	1 },
    { "hour",		tHOUR_UNIT,	1 },
    { "minute",		tMINUTE_UNIT,	1 },
    { "min",		tMINUTE_UNIT,	1 },
    { "second",		tSEC_UNIT,	1 },
    { "sec",		tSEC_UNIT,	1 },
    { NULL, 0, 0 }
};

/* Assorted relative-time words. */
static TABLE const OtherTable[] = {
    { "tomorrow",	tMINUTE_UNIT,	1 * 24 * 60 },
    { "yesterday",	tMINUTE_UNIT,	-1 * 24 * 60 },
    { "today",		tMINUTE_UNIT,	0 },
    { "now",		tMINUTE_UNIT,	0 },
    { "last",		tUNUMBER,	-1 },
    { "this",		tMINUTE_UNIT,	0 },
    { "next",		tUNUMBER,	2 },
    { "first",		tUNUMBER,	1 },
/*  { "second",		tUNUMBER,	2 }, */
    { "third",		tUNUMBER,	3 },
    { "fourth",		tUNUMBER,	4 },
    { "fifth",		tUNUMBER,	5 },
    { "sixth",		tUNUMBER,	6 },
    { "seventh",	tUNUMBER,	7 },
    { "eighth",		tUNUMBER,	8 },
    { "ninth",		tUNUMBER,	9 },
    { "tenth",		tUNUMBER,	10 },
    { "eleventh",	tUNUMBER,	11 },
    { "twelfth",	tUNUMBER,	12 },
    { "ago",		tAGO,	1 },
    { NULL, 0, 0 }
};

/* The timezone table. */
static TABLE const TimezoneTable[] = {
    { "gmt",	tZONE,     HOUR ( 0) },	/* Greenwich Mean */
    { "ut",	tZONE,     HOUR ( 0) },	/* Universal (Coordinated) */
    { "utc",	tZONE,     HOUR ( 0) },
    { "wet",	tZONE,     HOUR ( 0) },	/* Western European */
    { "bst",	tDAYZONE,  HOUR ( 0) },	/* British Summer */
    { "wat",	tZONE,     HOUR ( 1) },	/* West Africa */
    { "at",	tZONE,     HOUR ( 2) },	/* Azores */
    { "ast",	tZONE,     HOUR ( 4) },	/* Atlantic Standard */
    { "adt",	tDAYZONE,  HOUR ( 4) },	/* Atlantic Daylight */
    { "est",	tZONE,     HOUR ( 5) },	/* Eastern Standard */
    { "edt",	tDAYZONE,  HOUR ( 5) },	/* Eastern Daylight */
    { "cst",	tZONE,     HOUR ( 6) },	/* Central Standard */
    { "cdt",	tDAYZONE,  HOUR ( 6) },	/* Central Daylight */
    { "mst",	tZONE,     HOUR ( 7) },	/* Mountain Standard */
    { "mdt",	tDAYZONE,  HOUR ( 7) },	/* Mountain Daylight */
    { "pst",	tZONE,     HOUR ( 8) },	/* Pacific Standard */
    { "pdt",	tDAYZONE,  HOUR ( 8) },	/* Pacific Daylight */
    { "yst",	tZONE,     HOUR ( 9) },	/* Yukon Standard */
    { "ydt",	tDAYZONE,  HOUR ( 9) },	/* Yukon Daylight */
    { "hst",	tZONE,     HOUR (10) },	/* Hawaii Standard */
    { "hdt",	tDAYZONE,  HOUR (10) },	/* Hawaii Daylight */
    { "cat",	tZONE,     HOUR (10) },	/* Central Alaska */
    { "ahst",	tZONE,     HOUR (10) },	/* Alaska-Hawaii Standard */
    { "nt",	tZONE,     HOUR (11) },	/* Nome */
    { "idlw",	tZONE,     HOUR (12) },	/* International Date Line West */
    { "cet",	tZONE,     -HOUR (1) },	/* Central European */
    { "met",	tZONE,     -HOUR (1) },	/* Middle European */
    { "mewt",	tZONE,     -HOUR (1) },	/* Middle European Winter */
    { "mest",	tDAYZONE,  -HOUR (1) },	/* Middle European Summer */
    { "mesz",	tDAYZONE,  -HOUR (1) },	/* Middle European Summer */
    { "swt",	tZONE,     -HOUR (1) },	/* Swedish Winter */
    { "sst",	tDAYZONE,  -HOUR (1) },	/* Swedish Summer */
    { "fwt",	tZONE,     -HOUR (1) },	/* French Winter */
    { "fst",	tDAYZONE,  -HOUR (1) },	/* French Summer */
    { "eet",	tZONE,     -HOUR (2) },	/* Eastern Europe, USSR Zone 1 */
    { "bt",	tZONE,     -HOUR (3) },	/* Baghdad, USSR Zone 2 */
    { "zp4",	tZONE,     -HOUR (4) },	/* USSR Zone 3 */
    { "zp5",	tZONE,     -HOUR (5) },	/* USSR Zone 4 */
    { "zp6",	tZONE,     -HOUR (6) },	/* USSR Zone 5 */
    { "wst",	tZONE,     -HOUR (8) },	/* West Australian Standard */
    { "cct",	tZONE,     -HOUR (8) },	/* China Coast, USSR Zone 7 */
    { "jst",	tZONE,     -HOUR (9) },	/* Japan Standard, USSR Zone 8 */
    { "acst",	tZONE,     -(HOUR (9) + 30)},	/* Australian Central Standard */
    { "acdt",	tDAYZONE,  -(HOUR (9) + 30)},	/* Australian Central Daylight */
    { "aest",	tZONE,     -HOUR (10) },	/* Australian Eastern Standard */
    { "aedt",	tDAYZONE,  -HOUR (10) },	/* Australian Eastern Daylight */
    { "gst",	tZONE,     -HOUR (10) },	/* Guam Standard, USSR Zone 9 */
    { "nzt",	tZONE,     -HOUR (12) },	/* New Zealand */
    { "nzst",	tZONE,     -HOUR (12) },	/* New Zealand Standard */
    { "nzdt",	tDAYZONE,  -HOUR (12) },	/* New Zealand Daylight */
    { "idle",	tZONE,     -HOUR (12) },	/* International Date Line East */
    {  NULL, 0, 0  }
};

/* Military timezone table. */
static TABLE const MilitaryTable[] = {
    { "a",	tZONE,	HOUR (  1) },
    { "b",	tZONE,	HOUR (  2) },
    { "c",	tZONE,	HOUR (  3) },
    { "d",	tZONE,	HOUR (  4) },
    { "e",	tZONE,	HOUR (  5) },
    { "f",	tZONE,	HOUR (  6) },
    { "g",	tZONE,	HOUR (  7) },
    { "h",	tZONE,	HOUR (  8) },
    { "i",	tZONE,	HOUR (  9) },
    { "k",	tZONE,	HOUR ( 10) },
    { "l",	tZONE,	HOUR ( 11) },
    { "m",	tZONE,	HOUR ( 12) },
    { "n",	tZONE,	HOUR (- 1) },
    { "o",	tZONE,	HOUR (- 2) },
    { "p",	tZONE,	HOUR (- 3) },
    { "q",	tZONE,	HOUR (- 4) },
    { "r",	tZONE,	HOUR (- 5) },
    { "s",	tZONE,	HOUR (- 6) },
    { "t",	tZONE,	HOUR (- 7) },
    { "u",	tZONE,	HOUR (- 8) },
    { "v",	tZONE,	HOUR (- 9) },
    { "w",	tZONE,	HOUR (-10) },
    { "x",	tZONE,	HOUR (-11) },
    { "y",	tZONE,	HOUR (-12) },
    { "z",	tZONE,	HOUR (  0) },
    { NULL, 0, 0 }
};




/* ARGSUSED */
static int
yyerror (const char **yyInput, struct global *yy, char *s)
{
  (void)yyInput;
  (void)yy;
  (void)s;
  return 0;
}

static int
ToHour (int Hours, MERIDIAN Meridian)
{
  switch (Meridian)
    {
    case MER24:
      if (Hours < 0 || Hours > 23)
	return -1;
      return Hours;
    case MERam:
      if (Hours < 1 || Hours > 12)
	return -1;
      if (Hours == 12)
	Hours = 0;
      return Hours;
    case MERpm:
      if (Hours < 1 || Hours > 12)
	return -1;
      if (Hours == 12)
	Hours = 0;
      return Hours + 12;
    default:
      abort ();
    }
  /* NOTREACHED */
}

static int
ToYear (int Year)
{
  if (Year < 0)
    Year = -Year;

  /* XPG4 suggests that years 00-68 map to 2000-2068, and
     years 69-99 map to 1969-1999.  */
  if (Year < 69)
    Year += 2000;
  else if (Year < 100)
    Year += 1900;

  return Year;
}

static int
LookupWord (YYSTYPE *lvalp, char *buff)
{
  register char *p;
  register char *q;
  register const TABLE *tp;
  int i;
  int abbrev;

  /* Make it lowercase. */
  for (p = buff; *p; p++)
    if (isupper ((unsigned char)*p))
      *p = tolower (*p);

  if (strcmp (buff, "am") == 0 || strcmp (buff, "a.m.") == 0)
    {
      lvalp->Meridian = MERam;
      return tMERIDIAN;
    }
  if (strcmp (buff, "pm") == 0 || strcmp (buff, "p.m.") == 0)
    {
      lvalp->Meridian = MERpm;
      return tMERIDIAN;
    }

  /* See if we have an abbreviation for a month. */
  if (strlen (buff) == 3)
    abbrev = 1;
  else if (strlen (buff) == 4 && buff[3] == '.')
    {
      abbrev = 1;
      buff[3] = '\0';
    }
  else
    abbrev = 0;

  for (tp = MonthDayTable; tp->name; tp++)
    {
      if (abbrev)
	{
	  if (strncmp (buff, tp->name, 3) == 0)
	    {
	      lvalp->Number = tp->value;
	      return tp->type;
	    }
	}
      else if (strcmp (buff, tp->name) == 0)
	{
	  lvalp->Number = tp->value;
	  return tp->type;
	}
    }

  for (tp = TimezoneTable; tp->name; tp++)
    if (strcmp (buff, tp->name) == 0)
      {
	lvalp->Number = tp->value;
	return tp->type;
      }

  if (strcmp (buff, "dst") == 0)
    return tDST;

  for (tp = UnitsTable; tp->name; tp++)
    if (strcmp (buff, tp->name) == 0)
      {
	lvalp->Number = tp->value;
	return tp->type;
      }

  /* Strip off any plural and try the units table again. */
  i = strlen (buff) - 1;
  if (buff[i] == 's')
    {
      buff[i] = '\0';
      for (tp = UnitsTable; tp->name; tp++)
	if (strcmp (buff, tp->name) == 0)
	  {
	    lvalp->Number = tp->value;
	    return tp->type;
	  }
      buff[i] = 's';		/* Put back for "this" in OtherTable. */
    }

  for (tp = OtherTable; tp->name; tp++)
    if (strcmp (buff, tp->name) == 0)
      {
	lvalp->Number = tp->value;
	return tp->type;
      }

  /* Military timezones. */
  if (buff[1] == '\0' && isalpha ((unsigned char)*buff))
    {
      for (tp = MilitaryTable; tp->name; tp++)
	if (strcmp (buff, tp->name) == 0)
	  {
	    lvalp->Number = tp->value;
	    return tp->type;
	  }
    }

  /* Drop out any periods and try the timezone table again. */
  for (i = 0, p = q = buff; *q; q++)
    if (*q != '.')
      *p++ = *q;
    else
      i++;
  *p = '\0';
  if (i)
    for (tp = TimezoneTable; tp->name; tp++)
      if (strcmp (buff, tp->name) == 0)
	{
	  lvalp->Number = tp->value;
	  return tp->type;
	}

  return tID;
}

static int
yylex (YYSTYPE *lvalp, const char **yyInput)
{
  register char c;
  register char *p;
  char buff[20];
  int Count;
  int sign;

  for (;;)
    {
      while (isspace ((unsigned char)**yyInput))
	(*yyInput)++;

      if (ISDIGIT (c = **yyInput) || c == '-' || c == '+')
	{
	  if (c == '-' || c == '+')
	    {
	      sign = c == '-' ? -1 : 1;
	      if (!ISDIGIT (*++*yyInput))
		/* skip the '-' sign */
		continue;
	    }
	  else
	    sign = 0;
	  for (lvalp->Number = 0; ISDIGIT (c = *(*yyInput)++);)
	    lvalp->Number = 10 * lvalp->Number + c - '0';
	  (*yyInput)--;
	  if (sign < 0)
	    lvalp->Number = -lvalp->Number;
	  return sign ? tSNUMBER : tUNUMBER;
	}
      if (isalpha ((unsigned char)c))
	{
	  for (p = buff; (c = *(*yyInput)++, isalpha ((unsigned char)c))
		 || c == '.';)
	    if (p < &buff[sizeof buff - 1])
	      *p++ = c;
	  *p = '\0';
	  (*yyInput)--;
	  return LookupWord (lvalp, buff);
	}
      if (c != '(')
	return *(*yyInput)++;
      Count = 0;
      do
	{
	  c = *(*yyInput)++;
	  if (c == '\0')
	    return c;
	  if (c == '(')
	    Count++;
	  else if (c == ')')
	    Count--;
	}
      while (Count > 0);
    }
}

#define TM_YEAR_ORIGIN 1900

/* Yield A - B, measured in seconds.  */
static long
difftm (struct tm *a, struct tm *b)
{
  int ay = a->tm_year + (TM_YEAR_ORIGIN - 1);
  int by = b->tm_year + (TM_YEAR_ORIGIN - 1);
  long days = (
  /* difference in day of year */
		a->tm_yday - b->tm_yday
  /* + intervening leap days */
		+ ((ay >> 2) - (by >> 2))
		- (ay / 100 - by / 100)
		+ ((ay / 100 >> 2) - (by / 100 >> 2))
  /* + difference in years * 365 */
		+ (long) (ay - by) * 365
  );
  return (60 * (60 * (24 * days + (a->tm_hour - b->tm_hour))
		+ (a->tm_min - b->tm_min))
	  + (a->tm_sec - b->tm_sec));
}

time_t
dpl_get_date (const char *p, const time_t *now)
{
  struct tm tm, tm0, *tmp;
  struct global yy;
  time_t Start;
  struct tm res;

  memset(&res, 0, sizeof (res));
  Start = now ? *now : time ((time_t *) NULL);
  localtime_r(&Start, &res);
  tmp = &res;

  memset (&yy, 0, sizeof(yy));
  yy.Year = tmp->tm_year + TM_YEAR_ORIGIN;
  yy.Month = tmp->tm_mon + 1;
  yy.Day = tmp->tm_mday;
  yy.Hour = tmp->tm_hour;
  yy.Minutes = tmp->tm_min;
  yy.Seconds = tmp->tm_sec;
  yy.Meridian = MER24;
  yy.RelSeconds = 0;
  yy.RelMinutes = 0;
  yy.RelHour = 0;
  yy.RelDay = 0;
  yy.RelMonth = 0;
  yy.RelYear = 0;
  yy.HaveDate = 0;
  yy.HaveDay = 0;
  yy.HaveRel = 0;
  yy.HaveTime = 0;
  yy.HaveZone = 0;

  if (yyparse (&p, &yy)
      || yy.HaveTime > 1 || yy.HaveZone > 1 || yy.HaveDate > 1 || yy.HaveDay > 1)
    return -1;

  tm.tm_year = ToYear (yy.Year) - TM_YEAR_ORIGIN + yy.RelYear;
  tm.tm_mon = yy.Month - 1 + yy.RelMonth;
  tm.tm_mday = yy.Day + yy.RelDay;
  if (yy.HaveTime || (yy.HaveRel && !yy.HaveDate && !yy.HaveDay))
    {
      tm.tm_hour = ToHour (yy.Hour, yy.Meridian);
      if (tm.tm_hour < 0)
	return -1;
      tm.tm_min = yy.Minutes;
      tm.tm_sec = yy.Seconds;
    }
  else
    {
      tm.tm_hour = tm.tm_min = tm.tm_sec = 0;
    }
  tm.tm_hour += yy.RelHour;
  tm.tm_min += yy.RelMinutes;
  tm.tm_sec += yy.RelSeconds;
  tm.tm_isdst = -1;
  tm0 = tm;

  Start = mktime (&tm);

  if (Start == (time_t) -1)
    {

      /* Guard against falsely reporting errors near the time_t boundaries
         when parsing times in other time zones.  For example, if the min
         time_t value is 1970-01-01 00:00:00 UTC and we are 8 hours ahead
         of UTC, then the min localtime value is 1970-01-01 08:00:00; if
         we apply mktime to 1970-01-01 00:00:00 we will get an error, so
         we apply mktime to 1970-01-02 08:00:00 instead and adjust the time
         zone by 24 hours to compensate.  This algorithm assumes that
         there is no DST transition within a day of the time_t boundaries.  */
      if (yy.HaveZone)
	{
	  tm = tm0;
	  if (tm.tm_year <= EPOCH - TM_YEAR_ORIGIN)
	    {
	      tm.tm_mday++;
	      yy.Timezone -= 24 * 60;
	    }
	  else
	    {
	      tm.tm_mday--;
	      yy.Timezone += 24 * 60;
	    }
	  Start = mktime (&tm);
	}

      if (Start == (time_t) -1)
	return Start;
    }

  if (yy.HaveDay && !yy.HaveDate)
    {
      tm.tm_mday += ((yy.DayNumber - tm.tm_wday + 7) % 7
		     + 7 * (yy.DayOrdinal - (0 < yy.DayOrdinal)));
      Start = mktime (&tm);
      if (Start == (time_t) -1)
	return Start;
    }

  if (yy.HaveZone)
    {
      long delta = yy.Timezone * 60L + difftm (&tm, gmtime (&Start));
      if ((Start + delta < Start) != (delta < 0))
	return -1;		/* time_t overflow */
      Start += delta;
    }

  return Start;
}

