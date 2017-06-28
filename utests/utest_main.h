#ifndef __UTEST_MAIN_H__
#define __UTEST_MAIN_H__

#include <check.h>

extern Suite    *dict_suite(void);
extern Suite    *droplet_suite(void);
extern Suite    *vec_suite(void);
extern Suite    *sbuf_suite(void);
extern Suite    *dbuf_suite(void);
extern Suite    *ntinydb_suite(void);
extern Suite    *taskpool_suite(void);
extern Suite    *utest_suite(void);
extern Suite    *addrlist_suite(void);
extern Suite    *getdate_suite(void);
extern Suite    *util_suite(void);
extern Suite    *profile_suite(void);
extern Suite    *sproxyd_suite(void);

/* S3 backend tests */
extern Suite    *s3_auth_v2_suite(void);
extern Suite    *s3_auth_v4_suite(void);

/* Provide versions of the <check.h> comparison assert macros which do
 * not expand their arguments twice.  This allows arguments to have side
 * effects, which can make the test code more compact and readable.  The
 * string comparison asserts also allow for either of the arguments to
 * be NULL.
 */

/* Bit comparison macros with improved output compared to fail_unless(). */
/* O may be any comparison operator. */
#define _dpl_assert_bit(X, O, Y) \
    do { \
	struct x { \
	    int a:1; \
	}; \
	struct x _x; \
	_x.a=(X); \
	struct x _y; \
	_y.a=(Y); \
	ck_assert_msg(_x.a O _y.a, "Assertion '"#X#O#Y"' failed: "#X"==%d, "#Y"==%d", _x.a, _y.a); \
    } while(0)
#define dpl_assert_bit_eq(X, Y) _dpl_assert_bit(X, ==, Y)
#define dpl_assert_bit_ne(X, Y) _dpl_assert_bit(X, !=, Y)

/* Integer comparison macros with improved output compared to fail_unless(). */
/* O may be any comparison operator. */
#define _dpl_assert_int(X, O, Y) \
    do { \
	unsigned long _x=(X); \
	unsigned long _y=(Y); \
	ck_assert_msg(_x O _y, "Assertion '"#X#O#Y"' failed: "#X"==%lld, "#Y"==%lld", _x, _y); \
    } while(0)
#define dpl_assert_int_eq(X, Y) _dpl_assert_int(X, ==, Y)
#define dpl_assert_int_ne(X, Y) _dpl_assert_int(X, !=, Y)

/* String comparison macros with improved output compared to fail_unless() */
/* O may be either == or != */
#define _dpl_assert_str(X, O, Y) \
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
#define dpl_assert_str_eq(X, Y) _dpl_assert_str(X, ==, Y)
#define dpl_assert_str_ne(X, Y) _dpl_assert_str(X, !=, Y)

/* hopefully this will compile and silently truncate
 * the literal when used on 32b platforms */
#define BADPOINTER ((void *)(unsigned long)0xdeadbeefcafebabeULL)

/* Pointer comparison macros with improved output compared to fail_unless() */
/* O may be either == or != */
#define _dpl_assert_ptr(X, O, Y) \
    do { \
	const void *_x = (X); \
	const void *_y = (Y); \
	ck_assert_msg(_x O _y, \
		      "Assertion '"#X#O#Y"' failed: "#X"==%p, "#Y"==%p", _x, _y); \
    } while(0)
#define dpl_assert_ptr_eq(X, Y) _dpl_assert_ptr(X, ==, Y)
#define dpl_assert_ptr_ne(X, Y) _dpl_assert_ptr(X, !=, Y)
#define dpl_assert_ptr_null(X) _dpl_assert_ptr(X, ==, NULL)
#define dpl_assert_ptr_not_null(X) _dpl_assert_ptr(X, !=, NULL)

#endif
