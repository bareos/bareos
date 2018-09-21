/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2000-2006 Free Software Foundation Europe e.V.

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

#ifndef BAREOS_LIB_BTIME_H_
#define BAREOS_LIB_BTIME_H_

/* New btime definition -- use this */
btime_t GetCurrentBtime(void);
time_t BtimeToUnix(btime_t bt);   /* bareos time to epoch time */
utime_t BtimeToUtime(btime_t bt); /* bareos time to utime_t */

int tm_wom(int mday, int wday);
int TmWoy(time_t stime);

void Blocaltime(const time_t *time, struct tm *tm);
char *bstrutime(char *dt, int maxlen, utime_t tim);
char *bstrftime(char *dt, int maxlen, utime_t tim, const char *fmt = NULL);
char *bstrftimes(char *dt, int maxlen, utime_t tim);
char *bstrftime_ny(char *dt, int maxlen, utime_t tim);
char *bstrftime_nc(char *dt, int maxlen, utime_t tim);
char *bstrftime_wd(char *dt, int maxlen, utime_t tim);
utime_t StrToUtime(const char *str);


/* =========================================================== */
/*        old code deprecated below. Do not use.               */

typedef float64_t fdate_t;             /* Date type */
typedef float64_t ftime_t;             /* Time type */

struct date_time {
    fdate_t julian_day_number;         /* Julian day number */
    ftime_t julian_day_fraction;       /* Julian day fraction */
};

/*  In arguments and results of the following functions,
    quantities are expressed as follows.

        year    Year in the Common Era.  The canonical
                date of adoption of the Gregorian calendar
                (October 5, 1582 in the Julian calendar)
                is assumed.

        month   Month index with January 0, December 11.

        day     Day number of month, 1 to 31.

*/


extern fdate_t DateEncode(uint32_t year, uint8_t month, uint8_t day);
extern ftime_t TimeEncode(uint8_t hour, uint8_t minute, uint8_t second,
                          float32_t second_fraction);
extern void date_time_encode(struct date_time *dt,
                             uint32_t year, uint8_t month, uint8_t day,
                             uint8_t hour, uint8_t minute, uint8_t second,
                             float32_t second_fraction);

extern void DateDecode(fdate_t date, uint32_t *year, uint8_t *month,
                        uint8_t *day);
extern void TimeDecode(ftime_t time, uint8_t *hour, uint8_t *minute,
                        uint8_t *second, float32_t *second_fraction);
extern void date_time_decode(struct date_time *dt,
                             uint32_t *year, uint8_t *month, uint8_t *day,
                             uint8_t *hour, uint8_t *minute, uint8_t *second,
                             float32_t *second_fraction);

extern int DateTimeCompare(struct date_time *dt1, struct date_time *dt2);

extern void TmEncode(struct date_time *dt, struct tm *tm);
extern void TmDecode(struct date_time *dt, struct tm *tm);
extern void get_current_time(struct date_time *dt);


#endif /* BAREOS_LIB_BTIME_H_ */
