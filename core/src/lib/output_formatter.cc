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
 * Output Formatter routines.
 *
 * The idea behind these routines is that output to the user (user inferfaces, UIs)
 * is handled centrally with this class.
 *
 * Joerg Steffens, April 2015
 */

#define NEED_JANSSON_NAMESPACE 1
#include "include/bareos.h"
#include "lib/json.h"

const char *json_error_message_template = "{ "
  "\"jsonrpc\": \"2.0\", "
  "\"id\": null, "
  "\"error\": { "
    "\"message\": \"%s\", "
    "\"code\": 1 "
  "} "
"}\n";

OutputFormatter::OutputFormatter(SEND_HANDLER *send_func_arg,
                                   void *send_ctx_arg,
                                   FILTER_HANDLER *filter_func_arg,
                                   void *filter_ctx_arg,
                                   int api_mode)
{
   InitializeJson();

   send_func = send_func_arg;
   filter_func = filter_func_arg;
   send_ctx = send_ctx_arg;
   filter_ctx = filter_ctx_arg;
   api = api_mode;
   compact = false;
   num_rows_filtered = 0;

   filters = NULL;
   hidden_columns = NULL;
   result_message_plain = new PoolMem(PM_MESSAGE);
#if HAVE_JANSSON
   result_json = json_object();
   result_stack_json = New(alist(10, false));
   result_stack_json->push(result_json);
   message_object_json = json_object();
#endif
}

OutputFormatter::~OutputFormatter()
{
   if (hidden_columns) {
      free(hidden_columns);
   }
   if (filters) {
      delete filters;
   }
   delete result_message_plain;
#if HAVE_JANSSON
   json_object_clear(result_json);
   json_decref(result_json);
   delete result_stack_json;
   json_object_clear(message_object_json);
   json_decref(message_object_json);
#endif
}

void OutputFormatter::ObjectStart(const char *name)
{
#if HAVE_JANSSON
   json_t *json_object_current = NULL;
   json_t *json_object_existing = NULL;
   json_t *json_object_new = NULL;
#endif

   Dmsg1(800, "obj start: %s\n", name);
   switch (api) {
#if HAVE_JANSSON
   case API_MODE_JSON:
      json_object_current = (json_t *)result_stack_json->last();
      if (json_object_current == NULL) {
         Emsg0(M_ERROR, 0, "Failed to retrieve current JSON reference from stack.\n"
                           "This should not happen. Giving up.\n");
         return;
      }
      if (name == NULL) {
         /*
          * Add nameless object.
          */
         if (json_is_array(json_object_current)) {
            json_object_new = json_object();
            json_array_append_new(json_object_current, json_object_new);
            result_stack_json->push(json_object_new);
         } else {
            /*
             * nameless objects only are indented to be added to arrays.
             * We do a workaround here, but this will only keep the last added entry (others will be overwritten).
             */
            Dmsg0(800, "Warning: requested to add a nameless object to another object. This does not match.\n");
            result_stack_json->push(json_object_current);
         }
      } else {
         json_object_existing = json_object_get(json_object_current, name);
         if (json_object_existing) {
            Emsg2(M_ERROR, 0, "Failed to add JSON reference %s (stack size: %d) already exists.\n"
                              "This should not happen. Ignoring.\n", name, result_stack_json->size());
            return;
         } else {
            Dmsg2(800, "create new json object %s (stack size: %d)\n",
                  name, result_stack_json->size());
            json_object_new = json_object();
            json_object_set_new(json_object_current, name, json_object_new);
         }
         result_stack_json->push(json_object_new);
      }
      Dmsg1(800, "result stack: %d\n", result_stack_json->size());
      break;
#endif
   default:
      break;
   }
}

void OutputFormatter::ObjectEnd(const char *name)
{
   Dmsg1(800, "obj end:   %s\n", name);
   switch (api) {
#if HAVE_JANSSON
   case API_MODE_JSON:
      result_stack_json->pop();
      Dmsg1(800, "result stack: %d\n", result_stack_json->size());
      break;
#endif
   default:
      ProcessTextBuffer();
      break;
   }
}

void OutputFormatter::ArrayStart(const char *name)
{
#if HAVE_JANSSON
   json_t *json_object_current = NULL;
   json_t *json_object_existing = NULL;
   json_t *json_new = NULL;
#endif

   Dmsg1(800, "array start: %s\n", name);
   switch (api) {
#if HAVE_JANSSON
   case API_MODE_JSON:
      json_object_current = (json_t *)result_stack_json->last();
      if (json_object_current == NULL) {
         Emsg0(M_ERROR, 0, "Failed to retrieve current JSON reference from stack.\n"
                           "This should not happen. Giving up.\n");
         return;
      }

      if (!json_is_object(json_object_current)) {
         Emsg0(M_ERROR, 0, "Failed to retrieve object from JSON stack.\n"
                           "This should not happen. Giving up.\n");
         return;
      }

      json_object_existing = json_object_get(json_object_current, name);
      if (json_object_existing) {
         Emsg2(M_ERROR, 0, "Failed to add JSON reference %s (stack size: %d) already exists.\n"
                           "This should not happen. Ignoring.\n", name, result_stack_json->size());
         return;
      }
      json_new = json_array();
      json_object_set_new(json_object_current, name, json_new);
      result_stack_json->push(json_new);
      Dmsg1(800, "result stack: %d\n", result_stack_json->size());
      break;
#endif
   default:
      break;
   }
}

void OutputFormatter::ArrayEnd(const char *name)
{
   Dmsg1(800, "array end:   %s\n", name);
   switch (api) {
#if HAVE_JANSSON
   case API_MODE_JSON:
      result_stack_json->pop();
      Dmsg1(800, "result stack: %d\n", result_stack_json->size());
      break;
#endif
   default:
      //process_text_buffer();
      break;
   }
}

void OutputFormatter::Decoration(const char *fmt, ...)
{
   PoolMem string;
   va_list arg_ptr;

   switch (api) {
   case API_MODE_ON:
   case API_MODE_JSON:
      break;
   default:
      va_start(arg_ptr, fmt);
      string.Bvsprintf(fmt, arg_ptr);
      result_message_plain->strcat(string);
      va_end(arg_ptr);
      break;
   }
}

void OutputFormatter::ObjectKeyValueBool(const char *key, bool value)
{
   ObjectKeyValueBool(key, NULL, value, NULL);
}

void OutputFormatter::ObjectKeyValueBool(const char *key, bool value,
                                             const char *value_fmt)
{
   ObjectKeyValueBool(key, NULL, value, value_fmt);
}

void OutputFormatter::ObjectKeyValueBool(const char *key, const char *key_fmt,
                                             bool value, const char *value_fmt)
{
   PoolMem string;

   switch (api) {
#if HAVE_JANSSON
   case API_MODE_JSON:
      JsonKeyValueAddBool(key, value);
      break;
#endif
   default:
      if (key_fmt) {
         string.bsprintf(key_fmt, key);
         result_message_plain->strcat(string);
      }
      if (value_fmt) {
         if (value) {
            string.bsprintf(value_fmt, "true");
         } else {
            string.bsprintf(value_fmt, "false");
         }
         result_message_plain->strcat(string);
      }
      break;
   }
}

void OutputFormatter::ObjectKeyValue(const char *key, uint64_t value)
{
   ObjectKeyValue(key, NULL, value, NULL);
}

void OutputFormatter::ObjectKeyValue(const char *key, uint64_t value,
                                        const char *value_fmt)
{
   ObjectKeyValue(key, NULL, value, value_fmt);
}

void OutputFormatter::ObjectKeyValue(const char *key, const char *key_fmt,
                                        uint64_t value, const char *value_fmt)
{
   PoolMem string;

   switch (api) {
#if HAVE_JANSSON
   case API_MODE_JSON:
      JsonKeyValueAdd(key, value);
      break;
#endif
   default:
      if (key_fmt) {
         string.bsprintf(key_fmt, key);
         result_message_plain->strcat(string);
      }
      if (value_fmt) {
         string.bsprintf(value_fmt, value);
         result_message_plain->strcat(string);
      }
      break;
   }
}

void OutputFormatter::ObjectKeyValue(const char *key, const char *value, int wrap)
{
   ObjectKeyValue(key, NULL, value, NULL, wrap);
}

void OutputFormatter::ObjectKeyValue(const char *key, const char *value,
                                        const char *value_fmt, int wrap)
{
   ObjectKeyValue(key, NULL, value, value_fmt, wrap);
}

void OutputFormatter::ObjectKeyValue(const char *key, const char *key_fmt,
                                        const char *value, const char *value_fmt,
                                        int wrap)
{
   PoolMem string;
   PoolMem wvalue(value);
   rewrap(wvalue, wrap);

   switch (api) {
#if HAVE_JANSSON
   case API_MODE_JSON:
      JsonKeyValueAdd(key, wvalue.c_str());
      break;
#endif
   default:
      if (key_fmt) {
         string.bsprintf(key_fmt, key);
         result_message_plain->strcat(string);
      }
      if (value_fmt) {
         string.bsprintf(value_fmt, wvalue.c_str());
         result_message_plain->strcat(string);
      }
      Dmsg2(800, "obj: %s:%s\n", key, wvalue.c_str());
      break;
   }
}

void OutputFormatter::rewrap(PoolMem &string, int wrap)
{
   char *p, *q;
   int open = 0;
   int charsinline = 0;
   PoolMem rewrap_string(PM_MESSAGE);

   /*
    * wrap < 0: no modification
    * wrap = 0: single line
    * wrap > 0: wrap line after x characters (if api==0)
    */
   if (wrap < 0) {
      return;
   }

   /*
    * Allocate a wrap buffer that is big enough to hold the original string and the wrap chars.
    * Using strlen * 2 means, we could add a wrap character after each character,
    * which should be enough.
    */
   rewrap_string.check_size(string.strlen() * 2);

   /*
    * Walk the input buffer and copy the data to the wrap buffer
    * inserting line breaks as needed.
    */
   q = rewrap_string.c_str();
   p = string.c_str();
   while (*p) {
      charsinline++;
      switch (*p) {
      case ' ':
         if (api == 0 && wrap > 0 && charsinline >= wrap && open <= 0 && *(p + 1) != '|') {
            *q++ = '\n';
            *q++ = '\t';
            charsinline = 0;
         } else {
            if (charsinline > 1) {
               *q++ = ' ';
            }
         }
         break;
      case '|':
         *q++ = '|';
         if (api == 0 && wrap > 0 && open <= 0) {
            *q++ = '\n';
            *q++ = '\t';
            charsinline = 0;
         }
         break;
      case '[':
      case '<':
         open++;
         *q++ = *p;
         break;
      case ']':
      case '>':
         open--;
         *q++ = *p;
         break;
      case '\n':
      case '\t':
         if (charsinline > 1) {
            if (*(p + 1) != '\n' && *(p + 1) != '\t' && *(p + 1) != ' ') {
               *q++ = ' ';
            }
         }
         break;
      default:
         *q++ = *p;
         break;
      }
      p++;
   }
   *q = '\0';

   string.strcpy(rewrap_string);
}

void OutputFormatter::CreateNewResFilter(of_filter_type type, int column, int restype)
{
   of_filter_tuple *tuple;

   if (!filters) {
      filters = New(alist(10, true));
   }

   tuple = (of_filter_tuple *)malloc(sizeof(of_filter_tuple));
   tuple->type = type;
   tuple->u.res_filter.column = column;
   tuple->u.res_filter.restype = restype;

   filters->append(tuple);
}

void OutputFormatter::AddLimitFilterTuple(int limit)
{
   of_filter_tuple *tuple;

   if (!filters) {
      filters = New(alist(10, true));
   }

   tuple = (of_filter_tuple *)malloc(sizeof(of_filter_tuple));
   tuple->type = OF_FILTER_LIMIT;
   tuple->u.limit_filter.limit = limit;

   filters->append(tuple);
}

void OutputFormatter::AddOffsetFilterTuple(int offset)
{
   of_filter_tuple *tuple;

   if (!filters) {
      filters = New(alist(10, true));
   }

   tuple = (of_filter_tuple *)malloc(sizeof(of_filter_tuple));
   tuple->type = OF_FILTER_OFFSET;
   tuple->u.offset_filter.offset = offset;

   filters->append(tuple);
}

void OutputFormatter::AddAclFilterTuple(int column, int acltype)
{
   of_filter_tuple *tuple;

   if (!filters) {
      filters = New(alist(10, true));
   }

   tuple = (of_filter_tuple *)malloc(sizeof(of_filter_tuple));
   tuple->type = OF_FILTER_ACL;
   tuple->u.acl_filter.column = column;
   tuple->u.acl_filter.acltype = acltype;

   filters->append(tuple);
}

void OutputFormatter::AddResFilterTuple(int column, int restype)
{
   CreateNewResFilter(OF_FILTER_RESOURCE, column, restype);
}

void OutputFormatter::AddEnabledFilterTuple(int column, int restype)
{
   CreateNewResFilter(OF_FILTER_ENABLED, column, restype);
}

void OutputFormatter::AddDisabledFilterTuple(int column, int restype)
{
   CreateNewResFilter(OF_FILTER_DISABLED, column, restype);
}

void OutputFormatter::ClearFilters()
{
   if (filters) {
      if (!filters->empty()) {
         filters->destroy();
      }
      delete filters;
      filters = NULL;
   }
}

bool OutputFormatter::has_acl_filters()
{
   of_filter_tuple *tuple;

   if (filters) {
      foreach_alist(tuple, filters) {
         if (tuple->type == OF_FILTER_ACL) {
            return true;
         }
      }
   }

   return false;
}

bool OutputFormatter::FilterData(void *data)
{
   of_filter_state state;
   of_filter_tuple *tuple;
   int acl_filter_show = 0,
       acl_filter_unknown = 0;

   /*
    * See if a filtering function is registered.
    * See if there are any filters.
    */
   if (filter_func && filters && !filters->empty()) {
      foreach_alist(tuple, filters) {
         state = filter_func(filter_ctx, data, tuple);

         Dmsg1(800, "filter_state %d\n", state);
         switch (state) {
         case OF_FILTER_STATE_SHOW:
            if (tuple->type == OF_FILTER_ACL) {
               acl_filter_show++;
            }
            break;
         case OF_FILTER_STATE_SUPPRESS:
            num_rows_filtered++;
            return false;
         case OF_FILTER_STATE_UNKNOWN:
            if (tuple->type == OF_FILTER_ACL) {
               acl_filter_unknown++;
            }
            break;
         }
      }
   }

   /*
    * If we have multiple ACL filters and none gave an
    * explicit show state we suppress the entry just to
    * be sure we do not show to much information if we
    * are not sure if we should show the item.
    */
   if (acl_filter_unknown > 0 && acl_filter_show == 0) {
      Dmsg2(200, "tri-state filtering acl_filter_unknown %d, acl_filter_show %d\n",
            acl_filter_unknown, acl_filter_show);
      num_rows_filtered++;
      return false;
   }

   return true;
}

void OutputFormatter::AddHiddenColumn(int column)
{
   if (column >= 0 && column <= OF_MAX_NR_HIDDEN_COLUMNS) {
      /*
       * See if the bitmap was already allocated.
       */
      if (!hidden_columns) {
         hidden_columns = (char *)malloc(NbytesForBits(OF_MAX_NR_HIDDEN_COLUMNS));
         ClearAllBits(OF_MAX_NR_HIDDEN_COLUMNS, hidden_columns);
      }

      SetBit(column, hidden_columns);
   }
}

bool OutputFormatter::IsHiddenColumn(int column)
{
   if (!hidden_columns || column < 0 || column > OF_MAX_NR_HIDDEN_COLUMNS) {
      return false;
   }

   return BitIsSet(column, hidden_columns);
}

void OutputFormatter::ClearHiddenColumns()
{
   if (hidden_columns) {
      ClearAllBits(OF_MAX_NR_HIDDEN_COLUMNS, hidden_columns);
   }
}

bool OutputFormatter::ProcessTextBuffer()
{
   bool retval = false;
   PoolMem ErrorMsg;
   size_t string_length = 0;

   string_length = result_message_plain->strlen();
   if (string_length > 0) {
      retval = send_func(send_ctx, result_message_plain->c_str());
      if (!retval) {
         /*
          * If send failed, include short messages in error messages.
          * As messages can get quite long, don't show long messages.
          */
         ErrorMsg.bsprintf("Failed to send message (length=%lld). ", string_length);
         if (string_length < max_message_length_shown_in_error) {
            ErrorMsg.strcat("Message: ");
            ErrorMsg.strcat(result_message_plain->c_str());
            ErrorMsg.strcat("\n");
         } else {
            ErrorMsg.strcat("Maybe result message to long?\n");
         }
         Dmsg0(100, ErrorMsg.c_str());
      }
      result_message_plain->strcpy("");
   }
   return retval;
}

void OutputFormatter::message(const char *type, PoolMem &message)
{
   switch (api) {
#if HAVE_JANSSON
   case API_MODE_JSON:
      /*
       * currently, only error message influence the JSON result.
       * Other messages are not visible.
       */
      JsonAddMessage(type, message);
      break;
#endif
   default:
      /*
       * Send message immediately.
       * Type is not relevant here (handled before).
       */
      send_func(send_ctx, message.c_str());
      break;
   }
}

void OutputFormatter::SendBuffer()
{
   switch (api) {
#if HAVE_JANSSON
   case API_MODE_JSON:
      break;
#endif
   default:
      ProcessTextBuffer();
      break;
   }
}

void OutputFormatter::FinalizeResult(bool result)
{
   switch (api) {
#if HAVE_JANSSON
   case API_MODE_JSON:
      JsonFinalizeResult(result);
      break;
#endif
   default:
      ProcessTextBuffer();
      break;
   }

   /*
    * Clear any pending filters.
    */
   ClearFilters();

   /*
    * Clear any pending hidden columns.
    */
   ClearHiddenColumns();

   /*
    * Clear num_rows_filtered
    */
   ClearNumRowsFiltered();

}

#if HAVE_JANSSON
bool OutputFormatter::JsonKeyValueAddBool(const char *key, bool value)
{
   json_t *json_obj = NULL;
#if JANSSON_VERSION_HEX < 0x020400
   json_t *json_bool = NULL;
#endif
   PoolMem lkey(key);

   lkey.toLower();
   json_obj = (json_t *)result_stack_json->last();
   if (json_obj == NULL) {
      Emsg2(M_ERROR, 0, "No json object defined to add %s: %llu", key, value);
   }

#if JANSSON_VERSION_HEX >= 0x020400
   json_object_set_new(json_obj, lkey.c_str(), json_boolean(value));
#else
   /*
    * The function json_boolean(bool) requires jansson >= 2.4,
    * which is not available on all platform, so assign the value manually.
    */
   if (value) {
      json_bool = json_true();
   } else {
      json_bool = json_false();
   }
   json_object_set_new(json_obj, lkey.c_str(), json_bool);
#endif

   return true;
}

bool OutputFormatter::JsonKeyValueAdd(const char *key, uint64_t value)
{
   json_t *json_obj = NULL;
   PoolMem lkey(key);

   lkey.toLower();
   json_obj = (json_t *)result_stack_json->last();
   if (json_obj == NULL) {
      Emsg2(M_ERROR, 0, "No json object defined to add %s: %llu", key, value);
   }
   json_object_set_new(json_obj, lkey.c_str(), json_integer(value));

   return true;
}

bool OutputFormatter::JsonKeyValueAdd(const char *key, const char *value)
{
   json_t *json_obj = NULL;
   PoolMem lkey(key);

   lkey.toLower();
   json_obj = (json_t *)result_stack_json->last();
   if (json_obj == NULL) {
      Emsg2(M_ERROR, 0, "No json object defined to add %s: %s", key, value);
      return false;
   }
   json_object_set_new(json_obj, lkey.c_str(), json_string(value));

   return true;
}

void OutputFormatter::JsonAddMessage(const char *type, PoolMem &message)
{
   json_t *message_type_array;
   json_t *message_json=json_string(message.c_str());

   if (type != NULL) {
      message_type_array = json_object_get(message_object_json, type);
   } else {
      message_type_array = json_object_get(message_object_json, "normal");
   }
   if (message_type_array==NULL) {
      message_type_array=json_array();
      json_object_set_new(message_object_json, type, message_type_array);
   }
   json_array_append_new(message_type_array, message_json);
}

bool OutputFormatter::JsonHasErrorMessage()
{
   bool retval = false;

   if (json_object_get(message_object_json, MSG_TYPE_ERROR)) {
      retval = true;
   }

   return retval;
}

bool OutputFormatter::JsonSendErrorMessage(const char *message)
{
   PoolMem json_error_message;

   json_error_message.bsprintf(json_error_message_template, message);
   return send_func(send_ctx, json_error_message.c_str());
}

void OutputFormatter::JsonFinalizeResult(bool result)
{
   json_t *msg_obj = json_object();
   json_t *error_obj = NULL;
   json_t *data_obj = NULL;
   json_t *meta_obj = NULL;
   json_t *range_obj = NULL;
   PoolMem ErrorMsg;
   char *string;
   size_t string_length = 0;

   /*
    * We mimic json-rpc result and error messages,
    * To make it easier to implement real json-rpc later on.
    */
   json_object_set_new(msg_obj, "jsonrpc", json_string("2.0"));
   json_object_set_new(msg_obj, "id", json_null());

   if (!result || JsonHasErrorMessage()) {
      error_obj = json_object();
      json_object_set_new(error_obj, "code", json_integer(1));
      json_object_set_new(error_obj, "message", json_string("failed"));
      data_obj = json_object();
      json_object_set(data_obj, "result", result_json);
      json_object_set(data_obj, "messages", message_object_json);
      json_object_set_new(error_obj, "data", data_obj);
      json_object_set_new(msg_obj, "error", error_obj);
   } else {
      json_object_set(msg_obj, "result", result_json);
      if (HasFilters()) {
         meta_obj = json_object();
         json_object_set_new(result_json, "meta", meta_obj);

         range_obj = json_object();

         of_filter_tuple *tuple;
         foreach_alist(tuple, filters) {
            if (tuple->type == OF_FILTER_LIMIT) {
               json_object_set_new(range_obj, "limit", json_integer(tuple->u.limit_filter.limit));
            }
            if (tuple->type == OF_FILTER_OFFSET) {
               json_object_set_new(range_obj, "offset", json_integer(tuple->u.offset_filter.offset));
            }
         }
         json_object_set_new(range_obj, "filtered", json_integer(get_num_rows_filtered()));
         json_object_set_new(meta_obj, "range", range_obj);
      }
   }

   if (compact) {
      string = json_dumps(msg_obj, UA_JSON_FLAGS_COMPACT);
   } else {
      string = json_dumps(msg_obj, UA_JSON_FLAGS_NORMAL);
   }
   string_length = strlen(string);
   Dmsg1(800, "message length (json): %lld\n", string_length);
   if (string == NULL) {
      /*
       * json_dumps return NULL on failure (this should not happen).
       */
      Emsg0(M_ERROR, 0, "Failed to generate json string.\n");
   } else {
      /*
       * send json string, on failure, send json error message
       */
      if (!send_func(send_ctx, string)) {
         /*
          * If send failed, include short messages in error messages.
          * As messages can get quite long, don't show long messages.
          */
         ErrorMsg.bsprintf("Failed to send json message (length=%lld). ", string_length);
         if (string_length < max_message_length_shown_in_error) {
            ErrorMsg.strcat("Message: ");
            ErrorMsg.strcat(string);
            ErrorMsg.strcat("\n");
         } else {
            ErrorMsg.strcat("Maybe result message to long?\n");
         }
         Dmsg0(100, ErrorMsg.c_str());
         JsonSendErrorMessage(ErrorMsg.c_str());
      }
      free(string);
   }

   /*
    * empty result stack
    */
   while (result_stack_json->pop()) {}
   result_stack_json->push(result_json);

   json_object_clear(result_json);
   json_object_clear(message_object_json);
   json_object_clear(msg_obj);
}
#endif
