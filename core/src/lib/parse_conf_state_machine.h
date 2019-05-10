/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2000-2012 Free Software Foundation Europe e.V.
   Copyright (C) 2011-2012 Planets Communications B.V.
   Copyright (C) 2013-2019 Bareos GmbH & Co. KG

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

struct ConfigParserStateMachine {
  ConfigParserStateMachine(ConfigurationParser& my_config);
  ~ConfigParserStateMachine();
  ConfigParserStateMachine(ConfigParserStateMachine&& ohter) = delete;
  ConfigParserStateMachine(ConfigParserStateMachine& ohter) = delete;
  ConfigParserStateMachine& operator=(ConfigParserStateMachine& ohter) = delete;
  bool InitParserPass(const char* cf,
                      void* caller_ctx,
                      LEX*& lexical_parser_,
                      LEX_ERROR_HANDLER* ScanError,
                      LEX_WARNING_HANDLER* scan_warning,
                      int pass);
  void DumpResourcesAfterSecondPass();
  bool ParseSuccess() const { return state == ParseState::kInit; }
  int ParseAllTokens();

 public:
  LEX* lexical_parser_ = nullptr;
  ResourceTable* resource_table_ = nullptr;
  ResourceItem* resource_items_ = nullptr;
  int rcode_ = 0;
  int parser_pass_number_ = 1;
  BareosResource* static_resource_ = nullptr;
  int config_level_ = 0;
  ConfigurationParser& my_config_;

 private:
  int ParserStateNone(int token);
  int ScanResource(int token);
  enum
  {
    GET_NEXT_TOKEN,
    ERROR,
    NEXT_STATE
  };

  enum class ParseState
  {
    kInit,
    kResource
  };

  ParseState state = ParseState::kInit;
};

#endif  // BAREOS_LIB_PARSE_CONF_STATE_MACHINE_H_
