#ifndef __UTEST_MAIN_H__
#define __UTEST_MAIN_H__

#include <check.h>

extern Suite *  dict_suite      (void);
extern Suite *  vec_suite       (void);
extern Suite *  sbuf_suite      (void);
extern Suite *  dbuf_suite      (void);
extern Suite *  ntinydb_suite   (void);
extern Suite *  taskpool_suite  (void);

/* Redefine the <check.h> comparison assert macros to not expand their
 * arguments twice.  This allows arguments to have side effects, which
 * can make the test code more compact and readable.  The string
 * comparison asserts also allow for either of the arguments to be NULL.
 */

/* Integer comparsion macros with improved output compared to fail_unless(). */
/* O may be any comparion operator. */
#undef _ck_assert_int
#undef ck_assert_int_eq
#undef ck_assert_int_ne
#define _ck_assert_int(X, O, Y) \
    do { \
	unsigned long _x=(X); \
	unsigned long _y=(Y); \
	ck_assert_msg(_x O _y, "Assertion '"#X#O#Y"' failed: "#X"==%lld, "#Y"==%lld", _x, _y); \
    } while(0)
#define ck_assert_int_eq(X, Y) _ck_assert_int(X, ==, Y)
#define ck_assert_int_ne(X, Y) _ck_assert_int(X, !=, Y)

/* String comparsion macros with improved output compared to fail_unless() */
#undef _ck_assert_str
#undef ck_assert_str_eq
#undef ck_assert_str_ne
#define _ck_assert_str(X, O, Y) \
    do { \
	const char *_x = (X); \
	const char *_y = (Y); \
	ck_assert_msg(1 O !!(_x == _y || (_x && _y && !strcmp(_x, _y))), \
		      "Assertion '"#X#O#Y"' failed: "#X"==%s%s%s, "#Y"==%s%s%s", \
		      (_x ? "\"" : ""), \
		      (_x ? _x : "(null)"), \
		      (_x ? "\"" : ""), \
		      (_y ? "\"" : ""), \
		      (_y ? _y : "(null)"), \
		      (_y ? "\"" : "")); \
    } while(0)
#define ck_assert_str_eq(X, Y) _ck_assert_str(X, ==, Y)
#define ck_assert_str_ne(X, Y) _ck_assert_str(X, !=, Y)

#endif
