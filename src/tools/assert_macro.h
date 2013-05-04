/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2000-2010 Free Software Foundation Europe e.V.

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

/**
 * Assertion definitions
 */

#ifndef _ASSERT_MACRO_H
#define _ASSERT_MACRO_H 1

/* Assertions definitions */

/* check valid pointer if not return */
#ifndef ASSERT_NVAL_RET
#define ASSERT_NVAL_RET(value) \
   if ( ! value ){ \
      return; \
   }
#endif

/* check an error if true return */
#ifndef ASSERT_VAL_RET
#define ASSERT_VAL_RET(value) \
   if ( value ){ \
      return; \
   }
#endif

/* check valid pointer with Null return */
#ifndef ASSERT_NVAL_RET_NULL
#define ASSERT_NVAL_RET_NULL(value) \
   if ( ! value ) \
   { \
      return NULL; \
   }
#endif

/* if value then Null return */
#ifndef ASSERT_VAL_RET_NULL
#define ASSERT_VAL_RET_NULL(value) \
   if ( value ) \
   { \
      return NULL; \
   }
#endif

/* check valid pointer with int/err return */
#ifndef ASSERT_NVAL_RET_ONE
#define ASSERT_NVAL_RET_ONE(value) \
   if ( ! value ) \
   { \
      return 1; \
   }
#endif

/* check valid pointer with int/err return */
#ifndef ASSERT_NVAL_RET_NONE
#define ASSERT_NVAL_RET_NONE(value) \
   if ( ! value ) \
   { \
      return -1; \
   }
#endif

/* check error if not exit with error */
#ifndef ASSERT_NVAL_EXIT_ONE
#define ASSERT_NVAL_EXIT_ONE(value) \
   if ( ! value ){ \
      exit ( 1 ); \
   }
#endif

/* check error if not exit with error */
#ifndef ASSERT_NVAL_EXIT_E
#define ASSERT_NVAL_EXIT_E(value,ev) \
   if ( ! value ){ \
      exit ( ev ); \
   }
#endif

/* check error if not return zero */
#ifndef ASSERT_NVAL_RET_ZERO
#define ASSERT_NVAL_RET_ZERO(value) \
   if ( ! value ){ \
      return 0; \
   }
#endif

/* check error if not return value */
#ifndef ASSERT_NVAL_RET_V
#define ASSERT_NVAL_RET_V(value,rv) \
   if ( ! value ){ \
      return rv; \
   }
#endif

/* checks error value then int/err return */
#ifndef ASSERT_VAL_RET_ONE
#define ASSERT_VAL_RET_ONE(value) \
   if ( value ) \
   { \
      return 1; \
   }
#endif

/* checks error value then int/err return */
#ifndef ASSERT_VAL_RET_NONE
#define ASSERT_VAL_RET_NONE(value) \
   if ( value ) \
   { \
      return -1; \
   }
#endif

/* checks error value then exit one */
#ifndef ASSERT_VAL_EXIT_ONE
#define ASSERT_VAL_EXIT_ONE(value) \
   if ( value ) \
   { \
      exit (1); \
   }
#endif

/* check error if not return zero */
#ifndef ASSERT_VAL_RET_ZERO
#define ASSERT_VAL_RET_ZERO(value) \
   if ( value ){ \
      return 0; \
   }
#endif

/* check error if not return value */
#ifndef ASSERT_VAL_RET_V
#define ASSERT_VAL_RET_V(value,rv) \
   if ( value ){ \
      return rv; \
   }
#endif

#endif /* _ASSERT_MACRO_H */
