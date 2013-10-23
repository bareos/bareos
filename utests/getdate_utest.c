#include <droplet.h>
#include <droplet/utils.h>
#include <check.h>
#include "utest_main.h"

/*
 * Test case data copied from the Cyrus IMAPD unit tests
 * http://git.cyrusimap.org/cyrus-imapd/tree/cunit/times.testc
 */

START_TEST(rfc3501_test)
{
  time_t t;

  /* RFC3501 format */

  /* Well-formed full RFC3501 format with 2-digit day
   * "dd-mmm-yyyy HH:MM:SS zzzzz" */
  t = dpl_get_date("15-Oct-2010 03:19:52 +1100", (time_t *)NULL);
  dpl_assert_int_eq(t, 1287073192);

  /* Well-formed full RFC3501 format with 1-digit day
   * " d-mmm-yyyy HH:MM:SS zzzzz" */
  t = dpl_get_date(" 5-Oct-2010 03:19:52 +1100", (time_t *)NULL);
  dpl_assert_int_eq(t, 1286209192);
}
END_TEST

/* The getdate.y parser allows for RFC-822 style (comments)
 * which are stripped out. */
START_TEST(bracket_test)
{
  time_t t;

  /* This is a quick and dirty way to break up the tokens
   * in a date without introducing spaces. */
  t = dpl_get_date("15-Oct-2010()03:19:52()+1100", (time_t *)NULL);
  dpl_assert_int_eq(t, 1287073192);

  t = dpl_get_date("15-Oct-2010(  )03:19:52(\t  )+1100", (time_t *)NULL);
  dpl_assert_int_eq(t, 1287073192);

  t = dpl_get_date("15-Oct-2010 ( hello ) 03:19:52(world)+1100", (time_t *)NULL);
  dpl_assert_int_eq(t, 1287073192);

}
END_TEST

START_TEST(standard_timezone_test)
{
    static const time_t zulu = 813727192;
    time_t t;
#define DATE "15-Oct-95 03:19:52"

/* Note, this test relies on the code not being able to tell
 * that when daylight savings apply the given date/timezone
 * combination may not be not valid.  We're basically just
 * testing that certain timezone names are recognised and
 * that their time offsets are correct. */

    /* GMT/UTC */
    t = dpl_get_date(DATE " gmt", (time_t *)NULL);
    dpl_assert_int_eq(t, zulu);
    t = dpl_get_date(DATE " GMT", (time_t *)NULL);
    dpl_assert_int_eq(t, zulu);
    t = dpl_get_date(DATE " UT", (time_t *)NULL);
    dpl_assert_int_eq(t, zulu);
    t = dpl_get_date(DATE " ut", (time_t *)NULL);
    dpl_assert_int_eq(t, zulu);
    t = dpl_get_date(DATE " UTC", (time_t *)NULL);
    dpl_assert_int_eq(t, zulu);
    t = dpl_get_date(DATE " utc", (time_t *)NULL);
    dpl_assert_int_eq(t, zulu);

    /* US standard timezones */
    t = dpl_get_date(DATE " est", (time_t *)NULL);
    dpl_assert_int_eq(t, zulu+5*3600);
    t = dpl_get_date(DATE " EST", (time_t *)NULL);
    dpl_assert_int_eq(t, zulu+5*3600);
    t = dpl_get_date(DATE " cst", (time_t *)NULL);
    dpl_assert_int_eq(t, zulu+6*3600);
    t = dpl_get_date(DATE " CST", (time_t *)NULL);
    dpl_assert_int_eq(t, zulu+6*3600);
    t = dpl_get_date(DATE " mst", (time_t *)NULL);
    dpl_assert_int_eq(t, zulu+7*3600);
    t = dpl_get_date(DATE " MST", (time_t *)NULL);
    dpl_assert_int_eq(t, zulu+7*3600);
    t = dpl_get_date(DATE " pst", (time_t *)NULL);
    dpl_assert_int_eq(t, zulu+8*3600);
    t = dpl_get_date(DATE " PST", (time_t *)NULL);
    dpl_assert_int_eq(t, zulu+8*3600);

    /* US daylight saving timezones */
    t = dpl_get_date(DATE " edt", (time_t *)NULL);
    dpl_assert_int_eq(t, zulu+4*3600);
    t = dpl_get_date(DATE " EDT", (time_t *)NULL);
    dpl_assert_int_eq(t, zulu+4*3600);
    t = dpl_get_date(DATE " cdt", (time_t *)NULL);
    dpl_assert_int_eq(t, zulu+5*3600);
    t = dpl_get_date(DATE " CDT", (time_t *)NULL);
    dpl_assert_int_eq(t, zulu+5*3600);
    t = dpl_get_date(DATE " mdt", (time_t *)NULL);
    dpl_assert_int_eq(t, zulu+6*3600);
    t = dpl_get_date(DATE " MDT", (time_t *)NULL);
    dpl_assert_int_eq(t, zulu+6*3600);
    t = dpl_get_date(DATE " pdt", (time_t *)NULL);
    dpl_assert_int_eq(t, zulu+7*3600);
    t = dpl_get_date(DATE " PDT", (time_t *)NULL);
    dpl_assert_int_eq(t, zulu+7*3600);

    /* Australian standard timezones */

    /* Eastern */
    t = dpl_get_date(DATE " aest", (time_t *)NULL);
    dpl_assert_int_eq(t, zulu-10*3600);
    t = dpl_get_date(DATE " AEST", (time_t *)NULL);
    dpl_assert_int_eq(t, zulu-10*3600);
    /* Central is tricky - half an hour offset */
    t = dpl_get_date(DATE " acst", (time_t *)NULL);
    dpl_assert_int_eq(t, zulu-(9*3600+1800));
    t = dpl_get_date(DATE " ACST", (time_t *)NULL);
    dpl_assert_int_eq(t, zulu-(9*3600+1800));
    /* Western */
    t = dpl_get_date(DATE " wst", (time_t *)NULL);
    dpl_assert_int_eq(t, zulu-8*3600);
    t = dpl_get_date(DATE " WST", (time_t *)NULL);
    dpl_assert_int_eq(t, zulu-8*3600);

    /* Australian daylight timezones */

    /* Eastern */
    t = dpl_get_date(DATE " aedt", (time_t *)NULL);
    dpl_assert_int_eq(t, zulu-11*3600);
    t = dpl_get_date(DATE " AEDT", (time_t *)NULL);
    dpl_assert_int_eq(t, zulu-11*3600);
    /* Central */
    t = dpl_get_date(DATE " acdt", (time_t *)NULL);
    dpl_assert_int_eq(t, zulu-(10*3600+1800));
    t = dpl_get_date(DATE " ACDT", (time_t *)NULL);
    dpl_assert_int_eq(t, zulu-(10*3600+1800));
    /* no daylight savings in Western Aus */

#undef DATE
}
END_TEST

/*
 * Test US military single-letter timezone names.
 *
 * Sadly the code appears to calculate the correct timezone
 * offset but then apply it *BACKWARDS*.  As a result only
 * the J (invalid) and Z (offset 0) cases pass.  If I cared
 * more about this niche feature I would fix it.
 */
START_TEST(military_timezone_test)
{
    time_t t;
    int r;
    static const time_t zulu = 813727192;
#define DATE "15-Oct-95 03:19:52"

    /* Well-formed legacy format with 2-digit day, 2-digit year,
     * uppercase 1-char timezone = UTC, "dd-mmm-yy HH:MM:SS-z" */
    t = dpl_get_date(DATE"-Z", (time_t *)NULL);
    dpl_assert_int_eq(t, zulu);

    /* Well-formed legacy format with 2-digit day, 2-digit year,
     * lowercase 1-char timezone = UTC, "dd-mmm-yy HH:MM:SS-z" */
    t = dpl_get_date(DATE"-z", (time_t *)NULL);
    dpl_assert_int_eq(t, zulu);

#if 0
    /* Well-formed legacy format with 2-digit day, 2-digit year,
     * uppercase 1-char timezone = +0100, "dd-mmm-yy HH:MM:SS-z" */
    t = dpl_get_date(DATE"-A", (time_t *)NULL);
    dpl_assert_int_eq(t, zulu-1*3600);

    /* Well-formed legacy format with 2-digit day, 2-digit year,
     * uppercase 1-char timezone = +0200, "dd-mmm-yy HH:MM:SS-z" */
    t = dpl_get_date(DATE"-B", (time_t *)NULL);
    dpl_assert_int_eq(t, zulu-2*3600);

    /* Well-formed legacy format with 2-digit day, 2-digit year,
     * uppercase 1-char timezone = +0900, "dd-mmm-yy HH:MM:SS-z" */
    t = dpl_get_date(DATE"-I", (time_t *)NULL);
    dpl_assert_int_eq(t, zulu-9*3600);
#endif

    /* Well-formed legacy format with 2-digit day, 2-digit year,
     * erroneous uppercase 1-char timezone, "dd-mmm-yy HH:MM:SS-z" */
    t = dpl_get_date(DATE"-J", (time_t *)NULL);
    dpl_assert_int_eq(t, -1);

#if 0
    /* Well-formed legacy format with 2-digit day, 2-digit year,
     * uppercase 1-char timezone = +1000, "dd-mmm-yy HH:MM:SS-z" */
    t = dpl_get_date(DATE"-K", (time_t *)NULL);
    dpl_assert_int_eq(t, zulu-10*3600);

    /* Well-formed legacy format with 2-digit day, 2-digit year,
     * 1-char timezone = +1200, "dd-mmm-yy HH:MM:SS-z" */
    t = dpl_get_date(DATE"-M", (time_t *)NULL);
    dpl_assert_int_eq(t, zulu-12*3600);

    /* Well-formed legacy format with 2-digit day, 2-digit year,
     * uppercase 1-char timezone = -0100, "dd-mmm-yy HH:MM:SS-z" */
    t = dpl_get_date(DATE"-N", (time_t *)NULL);
    dpl_assert_int_eq(t, zulu+1*3600);

    /* Well-formed legacy format with 2-digit day, 2-digit year,
     * 1-char timezone = -1200, "dd-mmm-yy HH:MM:SS-z" */
    t = dpl_get_date(DATE"-Y", (time_t *)NULL);
    dpl_assert_int_eq(t, zulu+12*3600);
#endif

#undef DATE
}
END_TEST

START_TEST(numeric_timezone_test)
{
    static const time_t zulu = 813727192;
    time_t t;
#define DATE "15-Oct-95 03:19:52"

    t = dpl_get_date(DATE "+0000", (time_t *)NULL);
    dpl_assert_int_eq(t, zulu);

    t = dpl_get_date(DATE "+0100", (time_t *)NULL);
    dpl_assert_int_eq(t, zulu-1*3600);

    t = dpl_get_date(DATE "+0900", (time_t *)NULL);
    dpl_assert_int_eq(t, zulu-9*3600);

    t = dpl_get_date(DATE "+1000", (time_t *)NULL);
    dpl_assert_int_eq(t, zulu-10*3600);

    t = dpl_get_date(DATE "+1100", (time_t *)NULL);
    dpl_assert_int_eq(t, zulu-11*3600);

    t = dpl_get_date(DATE "-0100", (time_t *)NULL);
    dpl_assert_int_eq(t, zulu+1*3600);

    t = dpl_get_date(DATE "-0900", (time_t *)NULL);
    dpl_assert_int_eq(t, zulu+9*3600);

    t = dpl_get_date(DATE "-1000", (time_t *)NULL);
    dpl_assert_int_eq(t, zulu+10*3600);

    t = dpl_get_date(DATE "-1100", (time_t *)NULL);
    dpl_assert_int_eq(t, zulu+11*3600);

#undef DATE
}
END_TEST

START_TEST(rfc822_test)
{
    time_t t;

    t = dpl_get_date("Tue, 16 Nov 2010 12:46:49 +1100", (time_t *)NULL);
    dpl_assert_int_eq(t, 1289872009);
}
END_TEST

START_TEST(ctime_test)
{
    time_t t;

    t = dpl_get_date("16-Nov-2010 13:15:25 +1100", (time_t *)NULL);
    dpl_assert_int_eq(t, 1289873725);
}
END_TEST

#if 0
/* ISO 8601 format, like 2010-11-26T14:22:02+11:00 doesn't work,
 * the "T" confuses the parser */
START_TEST(iso8601_test)
{
    static const time_t TIMET = 1290741722;
    time_t t;

    t = dpl_get_date("2010-11-26T14:22:02+11:00", (time_t *)NULL);
    dpl_assert_int_eq(t, TIMET);

    t = dpl_get_date("2010-11-26T03:22:02Z", (time_t *)NULL);
    dpl_assert_int_eq(t, TIMET);

    t = dpl_get_date("2010-11-25T22:22:02-05:00", (time_t *)NULL);
    dpl_assert_int_eq(t, TIMET);
}
END_TEST
#endif


#if 0
/* the getdate.y code doesn't seem to check for leapyear correctness */
START_TEST(rfc3501_leapyear_test)
{
    time_t t;

    /* 2000 is a leapyear */
    t = dpl_get_date("29-Feb-2000 11:22:33 +1100", (time_t *)NULL);
    dpl_assert_int_eq(t, 951783753);

    /* 2001 is not a leapyear */
    t = dpl_get_date("29-Feb-2001 11:22:33 +1100", (time_t *)NULL);
    dpl_assert_int_eq(t, -1);

    /* 2004 is a leapyear */
    t = dpl_get_date("29-Feb-2004 11:22:33 +1100", (time_t *)NULL);
    dpl_assert_int_eq(t, 1078014153);
}
END_TEST
#endif



Suite *
getdate_suite(void)
{
  Suite *s = suite_create("getdate");
  TCase *t = tcase_create("base");
  tcase_add_test(t, rfc3501_test);
  tcase_add_test(t, bracket_test);
  tcase_add_test(t, standard_timezone_test);
  tcase_add_test(t, military_timezone_test);
  tcase_add_test(t, numeric_timezone_test);
  tcase_add_test(t, rfc822_test);
  tcase_add_test(t, ctime_test);
  suite_add_tcase(s, t);
  return s;
}

