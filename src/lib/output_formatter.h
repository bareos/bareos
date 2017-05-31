/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2016-2016 Planets Communications B.V.
   Copyright (C) 2015-2017 Bareos GmbH & Co. KG

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
 * Joerg Steffens, April 2015
 */
/**
 * @file
 * Output Formatter prototypes
 */

#ifndef __OUTPUT_FORMATTER_H_
#define __OUTPUT_FORMATTER_H_

#define MSG_TYPE_INFO    "info"
#define MSG_TYPE_WARNING "warning"
#define MSG_TYPE_ERROR   "error"

#define OF_MAX_NR_HIDDEN_COLUMNS 64

#if HAVE_JANSSON
#define UA_JSON_FLAGS_NORMAL JSON_INDENT(2)
#define UA_JSON_FLAGS_COMPACT JSON_COMPACT

/**
 * See if the source file needs the full JANSSON namespace or that we can
 * get away with using a forward declaration of the json_t struct.
 */
#ifndef NEED_JANSSON_NAMESPACE
typedef struct json_t json_t;
#else
#include <jansson.h>
#endif
#endif

/**
 * Filtering states.
 */
typedef enum of_filter_state {
   OF_FILTER_STATE_SHOW,
   OF_FILTER_STATE_SUPPRESS,
   OF_FILTER_STATE_UNKNOWN
} of_filter_state;

/**
 * Filtering types.
 */
typedef enum of_filter_type {
   OF_FILTER_LIMIT,
   OF_FILTER_ACL,
   OF_FILTER_RESOURCE,
   OF_FILTER_ENABLED,
   OF_FILTER_DISABLED
} of_filter_type;

typedef struct of_limit_filter_tuple {
   int limit;                         /* Filter output to a maximum of limit entries */
} of_limit_filter_tuple;

typedef struct of_acl_filter_tuple {
   int column;                        /* Filter resource is located in this column */
   int acltype;                       /* Filter resource based on this ACL type */
} of_acl_filter_tuple;

typedef struct of_res_filter_tuple {
   int column;                        /* Filter resource is located in this column */
   int restype;                       /* Filter resource based on this resource type */
} of_res_filter_tuple;

typedef struct of_filter_tuple {
   of_filter_type type;
   union {
      of_limit_filter_tuple limit_filter;
      of_acl_filter_tuple acl_filter;
      of_res_filter_tuple res_filter;
   } u;
} of_filter_tuple;

/**
 * Actual output formatter class.
 */
class OUTPUT_FORMATTER : public SMARTALLOC {
public:
   /*
    * Typedefs.
    */
   typedef bool (SEND_HANDLER)(void *ctx, const char *msg);
   typedef of_filter_state (FILTER_HANDLER)(void *ctx, void *data, of_filter_tuple *tuple);

private:
   /*
    * Members
    */
   int api;
   bool compact;
   SEND_HANDLER *send_func;
   FILTER_HANDLER *filter_func;
   void *send_ctx;
   void *filter_ctx;
   alist *filters;
   char *hidden_columns;
   POOL_MEM *result_message_plain;
   static const unsigned int max_message_length_shown_in_error = 1024;
#if HAVE_JANSSON
   json_t *result_json;
   alist *result_stack_json;
   json_t *message_object_json;
#endif

private:
   /*
    * Methods
    */
   void create_new_res_filter(of_filter_type type, int column, int restype);
   bool process_text_buffer();

   /*
    * reformat string.
    * remove newlines and replace tabs with a single space.
    * wrap < 0: no modification
    * wrap = 0: reformat to single line
    * wrap > 0: if api==0: wrap after x characters, else no modifications
    */
   void rewrap(POOL_MEM &string, int wrap);

#if HAVE_JANSSON
   bool json_send_error_message(const char *message);
#endif

public:
   /*
    * Methods
    */
   OUTPUT_FORMATTER(SEND_HANDLER *send_func,
                    void *send_ctx,
                    FILTER_HANDLER *filter_func,
                    void *filter_ctx,
                    int api_mode = API_MODE_OFF);
   ~OUTPUT_FORMATTER();

   void set_mode(int mode) { api = mode; };
   int get_mode() { return api; };

   /*
    * Allow to set compact output mode. Only used for json api mode.
    * There it can reduce the size of message by 1/3.
    */
   void set_compact(bool value) { compact = value; };
   bool get_compact() { return compact; };

   void object_start(const char *name = NULL);
   void object_end(const char *name = NULL);
   void array_start(const char *name);
   void array_end(const char *name);
   void decoration(const char *fmt, ...);
   /*
    * boolean and integer can not be used to distinguish overloading functions,
    * therefore the bool function have the postfix _bool.
    * The boolean value is given a string ("true" or "false") to the value_fmt string.
    * The format string must therefore match "%s".
    */
   void object_key_value_bool(const char *key, bool value);
   void object_key_value_bool(const char *key, bool value, const char *value_fmt);
   void object_key_value_bool(const char *key, const char *key_fmt, bool value, const char *value_fmt);
   void object_key_value(const char *key, uint64_t value);
   void object_key_value(const char *key, uint64_t value, const char *value_fmt);
   void object_key_value(const char *key, const char *key_fmt, uint64_t value, const char *value_fmt);
   void object_key_value(const char *key, const char *value, int wrap = -1);
   void object_key_value(const char *key, const char *value, const char *value_fmt, int wrap = -1);
   void object_key_value(const char *key, const char *key_fmt, const char *value, const char *value_fmt, int wrap = -1);

   /*
    * some programs (BAT in api mode 1) parses data message by message,
    * instead of using a separator.
    * An example for this is BAT with the ".defaults job" command in API mode 1.
    * In this cases, the send_buffer function must be called at between two messages.
    * In API mode 2 this function has no effect.
    * This function should only be used, when there is a specific need for it.
    */
   void send_buffer();

   /*
    * Filtering.
    */
   void add_limit_filter_tuple(int limit);
   void add_acl_filter_tuple(int column, int acltype);
   void add_res_filter_tuple(int column, int restype);
   void add_enabled_filter_tuple(int column, int restype);
   void add_disabled_filter_tuple(int column, int restype);
   void clear_filters();
   bool has_filters() { return filters && !filters->empty(); };
   bool has_acl_filters();
   bool filter_data(void *data);

   /*
    * Hidden columns.
    */
   void add_hidden_column(int column);
   bool is_hidden_column(int column);
   void clear_hidden_columns();

   void message(const char *type, POOL_MEM &message);

   void finalize_result(bool result);

#if HAVE_JANSSON
   void json_add_result(json_t *json);
   bool json_key_value_add_bool(const char *key, bool value);
   bool json_key_value_add(const char *key, uint64_t value);
   bool json_key_value_add(const char *key, const char *value);
   void json_add_message(const char *type, POOL_MEM &message);
   bool json_has_error_message();
   void json_finalize_result(bool result);
#endif
};

#endif
