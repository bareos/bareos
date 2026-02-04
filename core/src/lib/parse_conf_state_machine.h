/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2000-2012 Free Software Foundation Europe e.V.
   Copyright (C) 2011-2012 Planets Communications B.V.
   Copyright (C) 2013-2026 Bareos GmbH & Co. KG

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

#ifndef BAREOS_LIB_PARSE_CONF_STATE_MACHINE_H_
#define BAREOS_LIB_PARSE_CONF_STATE_MACHINE_H_

#include "lib/lex.h"

class ConfigurationParser;
class BareosResource;
struct ResourceTable;
struct ResourceItem;

class ConfigParserStateMachine {
 public:
  ConfigParserStateMachine(const char* config_file_name,
                           void* caller_ctx,
                           lexer::error_handler* ScanError,
                           ConfigurationParser& my_config);
  ~ConfigParserStateMachine();
  ConfigParserStateMachine(ConfigParserStateMachine& ohter) = delete;
  ConfigParserStateMachine(ConfigParserStateMachine&& ohter) = delete;
  ConfigParserStateMachine& operator=(ConfigParserStateMachine& rhs) = delete;
  ConfigParserStateMachine& operator=(ConfigParserStateMachine&& rhs) = delete;

  enum class ParserError
  {
    kNoError,
    kResourceIncomplete,
    kParserError
  };

  ParserError GetParseError() const;

  bool InitParserPass();
  bool Finished() const { return parser_pass_number_ == 2; }

  bool ParseAllTokens();
  void DumpResourcesAfterSecondPass();

 public:
  lexer* lexical_parser_ = nullptr;

 private:
  enum class ParseInternalReturnCode
  {
    kGetNextToken,
    kNextState,
    kError
  };

  ParseInternalReturnCode ParserInitResource(int token);
  ParseInternalReturnCode ScanResource(int token);
  void FreeUnusedMemoryFromPass2();

  int config_level_ = 0;
  int parser_pass_number_ = 0;
  std::string config_file_name_;
  void* caller_ctx_ = nullptr;
  lexer::error_handler* scan_error_ = nullptr;
  ConfigurationParser& my_config_;

  struct {
    int rcode_ = 0;
    const ResourceItem* resource_items_ = nullptr;
    BareosResource* allocated_resource_ = nullptr;
  } currently_parsed_resource_;

  enum class ParseState
  {
    kInit,
    kResource
  };

  ParseState state = ParseState::kInit;
};

#endif  // BAREOS_LIB_PARSE_CONF_STATE_MACHINE_H_
