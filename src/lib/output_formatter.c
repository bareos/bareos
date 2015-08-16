/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2015-2015 Bareos GmbH & Co. KG

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
#include "bareos.h"

/*
 * Extra bytes to allocate for the wrap buffer.
 */
#define WRAP_EXTRA_BYTES 50

OUTPUT_FORMATTER::OUTPUT_FORMATTER(SEND_HANDLER *send_func_arg,
                                   void *send_ctx_arg, int api_mode)
{
   initialize_json();

   send_func = send_func_arg;
   send_ctx = send_ctx_arg;
   api = api_mode;

   result_message_plain = new POOL_MEM(PM_MESSAGE);
#if HAVE_JANSSON
   result_json = json_object();
   result_stack_json = New(alist(10, false));
   result_stack_json->push(result_json);
   message_object_json = json_object();
#endif
}

OUTPUT_FORMATTER::~OUTPUT_FORMATTER()
{
   delete result_message_plain;
#if HAVE_JANSSON
   json_object_clear(result_json);
   json_decref(result_json);
   delete result_stack_json;
#endif
}

void OUTPUT_FORMATTER::object_start(const char *name)
{
#if HAVE_JANSSON
   json_t *json_object_current = NULL;
   json_t *json_object_existing = NULL;
   json_t *json_object_new = json_object();
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
            json_object_set(json_object_current, name, json_object_new);
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

void OUTPUT_FORMATTER::object_end(const char *name)
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
      process_text_buffer();
      break;
   }
}

void OUTPUT_FORMATTER::array_start(const char *name)
{
#if HAVE_JANSSON
   json_t *json_object_current = NULL;
   json_t *json_object_existing = NULL;
   json_t *json_new = json_array();
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
      json_object_set(json_object_current, name, json_new);
      result_stack_json->push(json_new);
      Dmsg1(800, "result stack: %d\n", result_stack_json->size());
      break;
#endif
   default:
      break;
   }
}

void OUTPUT_FORMATTER::array_end(const char *name)
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

void OUTPUT_FORMATTER::decoration(const char *fmt, ...)
{
   POOL_MEM string;
   va_list arg_ptr;

   switch (api) {
   case API_MODE_ON:
   case API_MODE_JSON:
      break;
   default:
      va_start(arg_ptr, fmt);
      string.bvsprintf(fmt, arg_ptr);
      result_message_plain->strcat(string);
      va_end(arg_ptr);
      break;
   }
}

void OUTPUT_FORMATTER::object_key_value(const char *key, uint64_t value)
{
   object_key_value(key, NULL, value, NULL);
}

void OUTPUT_FORMATTER::object_key_value(const char *key, uint64_t value,
                                        const char *value_fmt)
{
   object_key_value(key, NULL, value, value_fmt);
}

void OUTPUT_FORMATTER::object_key_value(const char *key, const char *key_fmt,
                                        uint64_t value, const char *value_fmt)
{
   POOL_MEM string;

   switch (api) {
#if HAVE_JANSSON
   case API_MODE_JSON:
      json_key_value_add(key, value);
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

void OUTPUT_FORMATTER::object_key_value(const char *key, const char *value, int wrap)
{
   object_key_value(key, NULL, value, NULL, wrap);
}

void OUTPUT_FORMATTER::object_key_value(const char *key, const char *value,
                                        const char *value_fmt, int wrap)
{
   object_key_value(key, NULL, value, value_fmt, wrap);
}

void OUTPUT_FORMATTER::object_key_value(const char *key, const char *key_fmt,
                                        const char *value, const char *value_fmt,
                                        int wrap)
{
   POOL_MEM string;
   POOL_MEM wvalue(value);
   rewrap(wvalue, wrap);

   switch (api) {
#if HAVE_JANSSON
   case API_MODE_JSON:
      json_key_value_add(key, wvalue.c_str());
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

void OUTPUT_FORMATTER::rewrap(POOL_MEM &string, int wrap)
{
   char *p, *q;
   int open = 0;
   int charsinline = 0;
   POOL_MEM rewrap_string(PM_MESSAGE);

   /*
    * wrap < 0: no modification
    * wrap = 0: single line
    * wrap > 0: wrap line after x characters (if api==0)
    */
   if (wrap < 0) {
      return;
   }

   /*
    * Allocate a wrap buffer that is big enough to hold the original
    * string + WRAP_EXTRA_BYTES. This means we can accommodate enough
    * line breaks and spaces to wrap the original string.
    */
   rewrap_string.check_size(string.max_size() + WRAP_EXTRA_BYTES);

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

void OUTPUT_FORMATTER::process_text_buffer()
{
   if (result_message_plain->strlen() > 0) {
      send_func(send_ctx, result_message_plain->c_str());
      result_message_plain->strcpy("");
   }
}

void OUTPUT_FORMATTER::message(const char *type, POOL_MEM &message)
{
   switch (api) {
#if HAVE_JANSSON
   case API_MODE_JSON:
      /*
       * currently, only error message influence the JSON result.
       * Other messages are not visible.
       */
      json_add_message(type, message);
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

void OUTPUT_FORMATTER::send_buffer()
{
   switch (api) {
#if HAVE_JANSSON
   case API_MODE_JSON:
      break;
#endif
   default:
      process_text_buffer();
      break;
   }
}



void OUTPUT_FORMATTER::finalize_result(bool result)
{
   switch (api) {
#if HAVE_JANSSON
   case API_MODE_JSON:
      json_finalize_result(result);
      break;
#endif
   default:
      process_text_buffer();
      break;
   }
}

#if HAVE_JANSSON
bool OUTPUT_FORMATTER::json_key_value_add(const char *key, uint64_t value)
{
   json_t *json_obj = NULL;
   POOL_MEM lkey(key);

   lkey.toLower();
   json_obj = (json_t *)result_stack_json->last();
   if (json_obj == NULL) {
      Emsg2(M_ERROR, 0, "No json object defined to add %s: %llu", key, value);
   }
   json_object_set(json_obj, lkey.c_str(), json_integer(value));

   return true;
}

bool OUTPUT_FORMATTER::json_key_value_add(const char *key, const char *value)
{
   json_t *json_obj = NULL;
   POOL_MEM lkey(key);

   lkey.toLower();
   json_obj = (json_t *)result_stack_json->last();
   if (json_obj == NULL) {
      Emsg2(M_ERROR, 0, "No json object defined to add %s: %s", key, value);
      return false;
   }
   json_object_set(json_obj, lkey.c_str(), json_string(value));

   return true;
}

void OUTPUT_FORMATTER::json_add_message(const char *type, POOL_MEM &message)
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
      json_object_set(message_object_json, type, message_type_array);
   }
   json_array_append_new(message_type_array,message_json);
}

bool OUTPUT_FORMATTER::json_has_error_message()
{
   bool retval = false;

   if (json_object_get(message_object_json, MSG_TYPE_ERROR)) {
      retval = true;
   }

   return retval;
}

void OUTPUT_FORMATTER::json_finalize_result(bool result)
{
   POOL_MEM string;
   json_t *msg_obj = json_object();
   json_t *error_obj;
   json_t *data_obj;

   /*
    * We mimic json-rpc result and error messages,
    * To make it easier to implement real json-rpc later on.
    */
   json_object_set(msg_obj, "jsonrpc", json_string("2.0"));
   json_object_set(msg_obj, "id", json_null());

   if (!result || json_has_error_message()) {
      error_obj = json_object();
      json_object_set_new(error_obj, "code", json_integer(1));
      json_object_set_new(error_obj, "message", json_string("failed"));
      data_obj = json_object();
      json_object_set(data_obj, "result", result_json);
      json_object_set(data_obj, "messages", message_object_json);
      json_object_set(error_obj, "data", data_obj);
      json_object_set_new(msg_obj, "error", error_obj);
   } else {
      json_object_set(msg_obj, "result", result_json);
   }

   string.bsprintf("%s\n", json_dumps(msg_obj, UA_JSON_FLAGS));
   send_func(send_ctx, string.c_str());

   while (result_stack_json->pop()) {};

   json_object_clear(result_json);
   result_stack_json->push(result_json);
   json_object_clear(message_object_json);
   json_object_clear(msg_obj);
}
#endif
