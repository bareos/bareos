/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2020-2020 Bareos GmbH & Co. KG

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
 * Output Formatter for Bareos Resources.
 */


#include "include/bareos.h"
#include "lib/util.h"
#include "lib/output_formatter_resource.h"

const char* GetAsCString(void* item) { return (const char*)item; }

OutputFormatterResource::OutputFormatterResource(OutputFormatter* send,
                                                 int indent_level)
{
  send_ = send;
  indent_level_ = indent_level;
}

OutputFormatterResource::~OutputFormatterResource() {}


bool OutputFormatterResource::requiresEscaping(const char* o)
{
  bool esc = false;
  while (*o) {
    switch (*o) {
      case '\\':
        if (esc) {
          esc = false;
        } else {
          esc = true;
        }
        o++;
        break;
      case '"':
        if (!esc) {
          /* found an unescaped double quote ("). Escaping required. */
          return true;
        }
        o++;
        esc = false;
        break;
      default:
        esc = false;
        o++;
        break;
    }
  }
  return false;
}


std::string OutputFormatterResource::GetKeyFormatString(bool inherited,
                                                        std::string baseformat)
{
  std::string format;
  if (baseformat.size() == 0) { return ""; }
  for (int i = 0; i < indent_level_; i++) { format += "  "; }
  if (inherited) { format += "# "; }
  format += baseformat;
  return format;
}

void OutputFormatterResource::ResourceStart(const char* resource_type_groupname,
                                            const char* resource_type_name,
                                            const char* resource_name,
                                            bool as_comment)
{
  const bool case_sensitive_name = true;
  /*
   * Use resource_type_groupname as structure key (JSON),
   * but use resource_type_name when writing config resources.
   */
  std::string format = std::string(resource_type_name) + std::string(" {\n");
  send_->ObjectStart(resource_type_groupname,
                     GetKeyFormatString(as_comment, format).c_str());
  indent_level_++;
  send_->ObjectStart(resource_name, nullptr, case_sensitive_name);
}

void OutputFormatterResource::ResourceEnd(const char* resource_type_groupname,
                                          const char* resource_type_name,
                                          const char* resource_name,
                                          bool as_comment)
{
  send_->ObjectEnd(resource_name);
  indent_level_--;
  send_->ObjectEnd(resource_type_groupname,
                   GetKeyFormatString(as_comment, "}\n\n").c_str());
}

void OutputFormatterResource::SubResourceStart(const char* name,
                                               bool as_comment,
                                               std::string baseformat)
{
  /* Note: if baseformat is empty here, it also has to be empty in
   * SubResourceEnd. */
  send_->ObjectStart(name, GetKeyFormatString(as_comment, baseformat).c_str());
  if (!baseformat.empty()) { indent_level_++; }
}

void OutputFormatterResource::SubResourceEnd(const char* name,
                                             bool as_comment,
                                             std::string baseformat)
{
  /* Note: if baseformat is empty here, it also has to be empty in
   * SubResourceStart. */
  if (baseformat.empty()) {
    send_->ObjectEnd(name);
  } else {
    indent_level_--;
    send_->ObjectEnd(name, GetKeyFormatString(as_comment, baseformat).c_str());
  }
}


void OutputFormatterResource::KeyBool(const char* name,
                                      bool value,
                                      bool as_comment)
{
  send_->ObjectKeyValueBool(name, GetKeyFormatString(as_comment).c_str(), value,
                            value ? "Yes\n" : "No\n");
}

void OutputFormatterResource::KeySignedInt(const char* name,
                                           int64_t value,
                                           bool as_comment)
{
  send_->ObjectKeyValue(name, GetKeyFormatString(as_comment).c_str(), value,
                        "%d\n");
}
void OutputFormatterResource::KeyUnsignedInt(const char* name,
                                             int64_t value,
                                             bool as_comment)
{
  send_->ObjectKeyValue(name, GetKeyFormatString(as_comment).c_str(), value,
                        "%u\n");
}

void OutputFormatterResource::KeyQuotedString(const char* name,
                                              const char* value,
                                              bool as_comment)
{
  if (value == nullptr) {
    KeyUnquotedString(name, value, as_comment);
    return;
  }
  send_->ObjectKeyValue(name, GetKeyFormatString(as_comment).c_str(), value,
                        "\"%s\"\n");
}

void OutputFormatterResource::KeyQuotedString(const char* name,
                                              const std::string value,
                                              bool as_comment)
{
  KeyQuotedString(name, value.c_str(), as_comment);
}


void OutputFormatterResource::KeyUnquotedString(const char* name,
                                                const char* value,
                                                bool as_comment)
{
  if (value == nullptr) {
    if (!as_comment) { return; }
  }
  send_->ObjectKeyValue(name, GetKeyFormatString(as_comment).c_str(), value,
                        "%s\n");
}


void OutputFormatterResource::KeyUnquotedString(const char* name,
                                                const std::string value,
                                                bool as_comment)
{
  KeyUnquotedString(name, value.c_str(), as_comment);
}


void OutputFormatterResource::KeyMultipleStringsInOneLine(
    const char* key,
    alist* list,
    std::function<const char*(void* item)> GetValue,
    bool as_comment,
    bool quoted_strings)
{
  /*
   * Each member of the list is comma-separated
   */
  int cnt = 0;
  char* item = nullptr;
  std::string format = "%s";
  if (quoted_strings) { format = "\"%s\""; }

  send_->ArrayStart(key, GetKeyFormatString(as_comment).c_str());
  if (list != NULL) {
    foreach_alist (item, list) {
      send_->ArrayItem(GetValue(item), format.c_str());
      if (cnt == 0) { format.insert(0, ", "); }
      cnt++;
    }
  }
  send_->ArrayEnd(key, "\n");
}

void OutputFormatterResource::KeyMultipleStringsInOneLine(const char* key,
                                                          alist* list,
                                                          bool as_comment,
                                                          bool quoted_strings)
{
  KeyMultipleStringsInOneLine(key, list, GetAsCString, as_comment,
                              quoted_strings);
}


void OutputFormatterResource::KeyMultipleStringsOnePerLineAddItem(
    const char* key,
    const char* item,
    bool as_comment,
    bool quoted_strings)
{
  PoolMem lineformat;
  std::string escItem;
  const char* value = item;
  std::string format = GetKeyFormatString(as_comment) + "%s\n";
  if (quoted_strings) { format = GetKeyFormatString(as_comment) + "\"%s\"\n"; }
  if (requiresEscaping(item)) {
    escItem = EscapeString(item);
    value = escItem.c_str();
  }
  lineformat.bsprintf(format.c_str(), key, value);
  send_->ArrayItem(item, lineformat.c_str(), false);
}


void OutputFormatterResource::KeyMultipleStringsOnePerLine(
    const char* key,
    alist* list,
    std::function<const char*(void* item)> GetValue,
    bool as_comment,
    bool quoted_strings)
{
  /*
   * One line for each member of the list
   */
  char* item = nullptr;

  if ((list == NULL) or (list->empty())) {
    if (as_comment) {
      std::string format = GetKeyFormatString(as_comment) + "\n";
      send_->ArrayStart(key, format.c_str());
      send_->ArrayEnd(key);
    }
  } else {
    send_->ArrayStart(key);
    foreach_alist (item, list) {
      KeyMultipleStringsOnePerLineAddItem(key, GetValue(item), as_comment,
                                          quoted_strings);
    }
    send_->ArrayEnd(key);
  }
}


void OutputFormatterResource::KeyMultipleStringsOnePerLine(const char* key,
                                                           alist* list,
                                                           bool as_comment,
                                                           bool quoted_strings)
{
  KeyMultipleStringsOnePerLine(key, list, GetAsCString, as_comment,
                               quoted_strings);
}


void OutputFormatterResource::KeyMultipleStringsOnePerLine(
    const char* key,
    const std::vector<std::string>& list,
    bool as_comment,
    bool quoted_strings)
{
  if (list.empty()) {
    if (as_comment) {
      std::string format = GetKeyFormatString(as_comment) + "\n";
      send_->ArrayStart(key, format.c_str());
      send_->ArrayEnd(key);
    }
  } else {
    send_->ArrayStart(key);
    for (const std::string& item : list) {
      KeyMultipleStringsOnePerLineAddItem(key, item.c_str(), as_comment,
                                          quoted_strings);
    }
    send_->ArrayEnd(key);
  }
}

void OutputFormatterResource::ArrayStart(const char* key,
                                         bool as_comment,
                                         std::string baseformat)
{
  /* Note: if baseformat is empty here, it also has to be empty in ArrayEnd. */
  send_->ArrayStart(key, GetKeyFormatString(as_comment, baseformat).c_str());
  if (!baseformat.empty()) { indent_level_++; }
}

void OutputFormatterResource::ArrayEnd(const char* key,
                                       bool as_comment,
                                       std::string baseformat)
{
  /* Note: if baseformat is empty here, it also has to be empty in ArrayStart.
   */
  if (!baseformat.empty()) { indent_level_--; }
  send_->ArrayEnd(key, GetKeyFormatString(as_comment, baseformat).c_str());
}
