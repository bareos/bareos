/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2003-2009 Free Software Foundation Europe e.V.
   Copyright (C) 2016-2019 Bareos GmbH & Co. KG

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
 *  Copyright (c) 2001-2002 Ralf S. Engelschall <rse@engelschall.com>
 *  Copyright (c) 2001-2002 The OSSP Project (http://www.ossp.org/)
 *  Copyright (c) 2001-2002 Cable & Wireless Deutschland (http://www.cw.com/de/)
 *
 *  This file is part of OSSP var, a variable expansion
 *  library which can be found at http://www.ossp.org/pkg/lib/var/.
 *
 *  Permission to use, copy, modify, and distribute this software for
 *  any purpose with or without fee is hereby granted, provided that
 *  the above copyright notice and this permission notice appear in all
 *  copies.
 *
 *  For disclaimer see below.
 */
/*
 * Modified for use with BACULA by Kern Sibbald, June 2003
 */
/**
 * @file
 * OSSP var - Variable Expansion
 */

#ifndef BAREOS_LIB_VAR_H_
#define BAREOS_LIB_VAR_H_

/* Error codes */
typedef enum
{
  VAR_ERR_CALLBACK = -64,
  VAR_ERR_FORMATTING_FAILURE = -45,
  VAR_ERR_UNDEFINED_OPERATION = -44,
  VAR_ERR_MALFORMED_OPERATION_ARGUMENTS = -43,
  VAR_ERR_INVALID_CHAR_IN_LOOP_LIMITS = -42,
  VAR_ERR_UNTERMINATED_LOOP_CONSTRUCT = -41,
  VAR_ERR_DIVISION_BY_ZERO_IN_INDEX = -40,
  VAR_ERR_UNCLOSED_BRACKET_IN_INDEX = -39,
  VAR_ERR_INCOMPLETE_INDEX_SPEC = -37,
  VAR_ERR_INVALID_CHAR_IN_INDEX_SPEC = -36,
  VAR_ERR_ARRAY_LOOKUPS_ARE_UNSUPPORTED = -35,
  VAR_ERR_INCOMPLETE_QUOTED_PAIR = -34,
  VAR_ERR_INVALID_ARGUMENT = -34,
  VAR_ERR_SUBMATCH_OUT_OF_RANGE = -33,
  VAR_ERR_UNKNOWN_QUOTED_PAIR_IN_REPLACE = -32,
  VAR_ERR_EMPTY_PADDING_FILL_STRING = -31,
  VAR_ERR_MISSING_PADDING_WIDTH = -30,
  VAR_ERR_MALFORMATTED_PADDING = -29,
  VAR_ERR_INCORRECT_TRANSPOSE_CLASS_SPEC = -28,
  VAR_ERR_EMPTY_TRANSPOSE_CLASS = -27,
  VAR_ERR_TRANSPOSE_CLASSES_MISMATCH = -26,
  VAR_ERR_MALFORMATTED_TRANSPOSE = -25,
  VAR_ERR_OFFSET_LOGIC = -24,
  VAR_ERR_OFFSET_OUT_OF_BOUNDS = -23,
  VAR_ERR_RANGE_OUT_OF_BOUNDS = -22,
  VAR_ERR_INVALID_OFFSET_DELIMITER = -21,
  VAR_ERR_MISSING_START_OFFSET = -20,
  VAR_ERR_EMPTY_SEARCH_STRING = -19,
  VAR_ERR_MISSING_PARAMETER_IN_COMMAND = -18,
  VAR_ERR_INVALID_REGEX_IN_REPLACE = -17,
  VAR_ERR_UNKNOWN_REPLACE_FLAG = -16,
  VAR_ERR_MALFORMATTED_REPLACE = -15,
  VAR_ERR_UNKNOWN_COMMAND_CHAR = -14,
  VAR_ERR_INPUT_ISNT_TEXT_NOR_VARIABLE = -13,
  VAR_ERR_UNDEFINED_VARIABLE = -12,
  VAR_ERR_INCOMPLETE_VARIABLE_SPEC = -11,
  VAR_ERR_OUT_OF_MEMORY = -10,
  VAR_ERR_INVALID_CONFIGURATION = -9,
  VAR_ERR_INCORRECT_CLASS_SPEC = -8,
  VAR_ERR_INCOMPLETE_GROUPED_HEX = -7,
  VAR_ERR_INCOMPLETE_OCTAL = -6,
  VAR_ERR_INVALID_OCTAL = -5,
  VAR_ERR_OCTAL_TOO_LARGE = -4,
  VAR_ERR_INVALID_HEX = -3,
  VAR_ERR_INCOMPLETE_HEX = -2,
  VAR_ERR_INCOMPLETE_NAMED_CHARACTER = -1,
  VAR_OK = 0
} var_rc_t;

struct var_st;
typedef struct var_st var_t;

enum class var_config_t
{
  VAR_CONFIG_SYNTAX,
  VAR_CONFIG_CB_VALUE,
  VAR_CONFIG_CB_OPERATION
};

typedef struct {
  char escape;            /* default: '\' */
  char delim_init;        /* default: '$' */
  char delim_open;        /* default: '{' */
  char delim_close;       /* default: '}' */
  char index_open;        /* default: '[' */
  char index_close;       /* default: ']' */
  char index_mark;        /* default: '#' */
  const char* name_chars; /* default: "a-zA-Z0-9_" */
} var_syntax_t;

typedef var_rc_t (*var_cb_value_t)(var_t* var,
                                   void* ctx,
                                   const char* var_ptr,
                                   int var_len,
                                   int var_inc,
                                   int var_idx,
                                   const char** val_ptr,
                                   int* val_len,
                                   int* val_size);

typedef var_rc_t (*var_cb_operation_t)(var_t* var,
                                       void* ctx,
                                       const char* op_ptr,
                                       int op_len,
                                       const char* arg_ptr,
                                       int arg_len,
                                       const char* val_ptr,
                                       int val_len,
                                       const char** out_ptr,
                                       int* out_len,
                                       int* out_size);


var_rc_t var_create(var_t** var);
var_rc_t var_destroy(var_t* var);
var_rc_t var_config(var_t* var, var_config_t mode, ...);
var_rc_t var_unescape(var_t* var,
                      const char* src_ptr,
                      int src_len,
                      char* dst_ptr,
                      int dst_len,
                      int all);
var_rc_t var_expand(var_t* var,
                    const char* src_ptr,
                    int src_len,
                    char** dst_ptr,
                    int* dst_len,
                    int force_expand);
var_rc_t var_formatv(var_t* var,
                     char** dst_ptr,
                     int force_expand,
                     const char* fmt,
                     va_list ap);
var_rc_t var_format(var_t* var,
                    char** dst_ptr,
                    int force_expand,
                    const char* fmt,
                    ...);
const char* var_strerror(var_t* var, var_rc_t rc);

#endif /* BAREOS_LIB_VAR_H_ */
