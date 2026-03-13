/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2016-2016 Planets Communications B.V.
   Copyright (C) 2015-2026 Bareos GmbH & Co. KG

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
// Joerg Steffens, April 2015
/**
 * @file
 * Output Formatter prototypes
 */

#ifndef BAREOS_LIB_OUTPUT_FORMATTER_H_
#define BAREOS_LIB_OUTPUT_FORMATTER_H_

#define MSG_TYPE_INFO "info"
#define MSG_TYPE_WARNING "warning"
#define MSG_TYPE_ERROR "error"

#define OF_MAX_NR_HIDDEN_COLUMNS 64

#if HAVE_JANSSON
/**
 * See if the source file needs the full JANSSON namespace or that we can
 * get away with using a forward declaration of the json_t struct.
 */
#  ifndef NEED_JANSSON_NAMESPACE
typedef struct json_t json_t;
#  else
#    include <jansson.h>
#  endif

#  define UA_JSON_FLAGS_NORMAL JSON_INDENT(2)
#  define UA_JSON_FLAGS_COMPACT JSON_COMPACT

#endif /* HAVE_JANSSON */

#include "lib/alist.h"
#include "lib/api_mode.h"
#include "include/compiler_macro.h"
#include <stdint.h>

class PoolMem;

// Filtering states.
typedef enum of_filter_state
{
  OF_FILTER_STATE_SHOW,
  OF_FILTER_STATE_SUPPRESS,
  OF_FILTER_STATE_UNKNOWN
} of_filter_state;

// Filtering types.
typedef enum of_filter_type
{
  OF_FILTER_LIMIT,
  OF_FILTER_OFFSET,
  OF_FILTER_ACL,
  OF_FILTER_RESOURCE,
  OF_FILTER_ENABLED,
  OF_FILTER_DISABLED
} of_filter_type;

typedef struct of_limit_filter_tuple {
  uint64_t limit = 0; /* Filter output to a maximum of limit entries */
} of_limit_filter_tuple;

typedef struct of_offset_filter_tuple {
  uint64_t offset = 0;
} of_offset_filter_tuple;

typedef struct of_acl_filter_tuple {
  int column = 0;  /* Filter resource is located in this column */
  int acltype = 0; /* Filter resource based on this ACL type */
} of_acl_filter_tuple;

typedef struct of_res_filter_tuple {
  int column = 0;  /* Filter resource is located in this column */
  int restype = 0; /* Filter resource based on this resource type */
} of_res_filter_tuple;

typedef struct of_filter_tuple {
  of_filter_type type;
  union {
    of_limit_filter_tuple limit_filter;
    of_offset_filter_tuple offset_filter;
    of_acl_filter_tuple acl_filter;
    of_res_filter_tuple res_filter;
  } u;
} of_filter_tuple;

// Actual output formatter class.
class OutputFormatter {
 public:
  // Typedefs.
  typedef bool(SEND_HANDLER)(void* ctx, const char* fmt, ...) PRINTF_LIKE(2, 3);

  typedef of_filter_state(FILTER_HANDLER)(void* ctx,
                                          void* data,
                                          of_filter_tuple* tuple);

 private:
  // Members
  int api = 0;
  bool compact = false;
  SEND_HANDLER* send_func = nullptr;
  FILTER_HANDLER* filter_func = nullptr;
  void* send_ctx = nullptr;
  void* filter_ctx = nullptr;
  alist<of_filter_tuple*>* filters = nullptr;
  char* hidden_columns = nullptr;
  PoolMem* result_message_plain = nullptr;
  static const unsigned int max_message_length_shown_in_error = 1024;
  int num_rows_filtered = 0;
#if HAVE_JANSSON
  json_t* result_json = nullptr;
  alist<json_t*>* result_stack_json = nullptr;
  json_t* message_object_json = nullptr;
#endif

 private:
  // Methods
  int get_num_rows_filtered() { return num_rows_filtered; }
  void SetNumRowsFiltered(int value) { num_rows_filtered = value; }
  void ClearNumRowsFiltered() { SetNumRowsFiltered(0); }

  void CreateNewResFilter(of_filter_type type, int column, int restype);
  bool ProcessTextBuffer();

  /* reformat string.
   * remove newlines and replace tabs with a single space.
   * wrap < 0: no modification
   * wrap = 0: reformat to single line
   * wrap > 0: if api==0: wrap after x characters, else no modifications */
  void rewrap(PoolMem& string, int wrap);

#if HAVE_JANSSON
  bool JsonSendErrorMessage(const char* message);
#endif

 public:
  // Methods
  OutputFormatter(SEND_HANDLER* send_func,
                  void* send_ctx,
                  FILTER_HANDLER* filter_func,
                  void* filter_ctx,
                  int api_mode = API_MODE_OFF);

  ~OutputFormatter();

  void SetMode(int mode) { api = mode; }
  int GetMode() { return api; }

  /* Allow to set compact output mode. Only used for json api mode.
   * There it can reduce the size of message by 1/3. */
  void SetCompact(bool value) { compact = value; }
  bool GetCompact() { return compact; }

  void Decoration(const char* fmt, ...) PRINTF_LIKE(2, 3);

  void ArrayStart(const char* name, const char* fmt = NULL);
  void ArrayEnd(const char* name, const char* fmt = NULL);

  void ArrayItem(bool value, const char* value_fmt = NULL);
  void ArrayItem(uint64_t value, const char* value_fmt = NULL);
  void ArrayItem(const char* value,
                 const char* value_fmt = NULL,
                 bool format = true);

  void ObjectStart(const char* name = NULL,
                   const char* fmt = NULL,
                   bool case_sensitiv_name = false);
  void ObjectEnd(const char* name = NULL, const char* fmt = NULL);

  /* boolean and integer can not be used to distinguish overloading functions,
   * therefore the bool function have the postfix _bool.
   * The boolean value is given a string ("true" or "false") to the value_fmt
   * string. The format string must therefore match "%s". */
  void ObjectKeyValueBool(const char* key, bool value);
  void ObjectKeyValueBool(const char* key, bool value, const char* value_fmt);
  void ObjectKeyValueBool(const char* key,
                          const char* key_fmt,
                          bool value,
                          const char* value_fmt);
  void ObjectKeyValue(const char* key, uint64_t value);
  void ObjectKeyValue(const char* key, uint64_t value, const char* value_fmt);
  void ObjectKeyValue(const char* key,
                      const char* key_fmt,
                      uint64_t value,
                      const char* value_fmt);
  void ObjectKeyValueSignedInt(const char* key, int64_t value);
  void ObjectKeyValueSignedInt(const char* key,
                               int64_t value,
                               const char* value_fmt);
  void ObjectKeyValueSignedInt(const char* key,
                               const char* key_fmt,
                               int64_t value,
                               const char* value_fmt);
  void ObjectKeyValue(const char* key, const char* value, int wrap = -1);
  void ObjectKeyValue(const char* key,
                      const char* value,
                      const char* value_fmt,
                      int wrap = -1);
  void ObjectKeyValue(const char* key,
                      const char* key_fmt,
                      const char* value,
                      const char* value_fmt,
                      int wrap = -1);

  /* some programs (BAT in api mode 1) parses data message by message,
   * instead of using a separator.
   * An example for this is BAT with the ".defaults job" command in API mode 1.
   * In this cases, the SendBuffer function must be called at between two
   * messages. In API mode 2 this function has no effect. This function should
   * only be used, when there is a specific need for it. */
  void SendBuffer();

  // Filtering.
  void AddLimitFilterTuple(uint64_t limit);
  void AddOffsetFilterTuple(uint64_t offset);
  void AddAclFilterTuple(int column, int acltype);
  void AddResFilterTuple(int column, int restype);
  void AddEnabledFilterTuple(int column, int restype);
  void AddDisabledFilterTuple(int column, int restype);
  void ClearFilters();
  bool HasFilters() { return filters && !filters->empty(); }
  bool has_acl_filters();
  bool FilterData(void* data);

  // Hidden columns.
  void AddHiddenColumn(int column);
  bool IsHiddenColumn(int column);
  void ClearHiddenColumns();

  void message(const char* type, PoolMem& message);

  void FinalizeResult(bool result);

#if HAVE_JANSSON
  bool JsonArrayItemAdd(json_t* value);
  bool JsonKeyValueAddBool(const char* key, bool value);
  bool JsonKeyValueAdd(const char* key, uint64_t value);
  bool JsonKeyValueAdd(const char* key, const char* value);
  void JsonAddMessage(const char* type, PoolMem& message);
  bool JsonHasErrorMessage();
  void JsonFinalizeResult(bool result);
#endif
};

#ifdef HAVE_JANSSON
// JSON output helper functions
struct s_kw;
struct ResourceItem;

json_t* json_item(const s_kw* item);
json_t* json_item(const ResourceItem* item, bool is_alias = false);
json_t* json_items(const ResourceItem items[]);
#endif

#endif  // BAREOS_LIB_OUTPUT_FORMATTER_H_
