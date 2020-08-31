/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2000-2011 Free Software Foundation Europe e.V.
   Copyright (C) 2013-2020 Bareos GmbH & Co. KG

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
 * This file handles most things related to generic resources.
 *
 * Kern Sibbald, January MM
 * Split from parse_conf.c April MMV
 */

#define NEED_JANSSON_NAMESPACE 1
#include "include/bareos.h"
#include "generic_res.h"
#include "lib/edit.h"
#include "include/jcr.h"
#include "qualified_resource_name_type_converter.h"
#include "lib/parse_conf.h"
#include "lib/messages_resource.h"
#include "lib/output_formatter_resource.h"
#include "lib/resource_item.h"
#include "lib/util.h"
#include "lib/address_conf.h"
#include "lib/output_formatter.h"

/*
 * Set default indention e.g. 2 spaces.
 */
#define DEFAULT_INDENT_STRING "  "

static int res_locked = 0; /* resource chain lock count -- for debug */

// #define TRACE_RES

void ConfigurationParser::b_LockRes(const char* file, int line) const
{
  int errstat;

#ifdef TRACE_RES
  char ed1[50];

  Pmsg4(000, "LockRes  locked=%d w_active=%d at %s:%d\n", res_locked,
        res_lock_.w_active, file, line);

  if (res_locked) {
    Pmsg2(000, "LockRes writerid=%lu myid=%s\n", res_lock_.writer_id,
          edit_pthread(pthread_self(), ed1, sizeof(ed1)));
  }
#endif

  if ((errstat = RwlWritelock(&res_lock_)) != 0) {
    Emsg3(M_ABORT, 0, _("RwlWritelock failure at %s:%d:  ERR=%s\n"), file, line,
          strerror(errstat));
  }

  res_locked++;
}

void ConfigurationParser::b_UnlockRes(const char* file, int line) const
{
  int errstat;

  if ((errstat = RwlWriteunlock(&res_lock_)) != 0) {
    Emsg3(M_ABORT, 0, _("RwlWriteunlock failure at %s:%d:. ERR=%s\n"), file,
          line, strerror(errstat));
  }
  res_locked--;
#ifdef TRACE_RES
  Pmsg4(000, "UnLockRes locked=%d wactive=%d at %s:%d\n", res_locked,
        res_lock_.w_active, file, line);
#endif
}

/*
 * Return resource of type rcode that matches name
 */
BareosResource* ConfigurationParser::GetResWithName(int rcode,
                                                    const char* name,
                                                    bool lock) const
{
  BareosResource* res;
  int rindex = rcode - r_first_;

  if (lock) { LockRes(this); }

  res = res_head_[rindex];
  while (res) {
    if (bstrcmp(res->resource_name_, name)) { break; }
    res = res->next_;
  }

  if (lock) { UnlockRes(this); }

  return res;
}

/*
 * Return next resource of type rcode. On first
 * call second arg (res) is NULL, on subsequent
 * calls, it is called with previous value.
 */
BareosResource* ConfigurationParser::GetNextRes(int rcode,
                                                BareosResource* res) const
{
  BareosResource* nres;
  int rindex = rcode - r_first_;

  if (res == NULL) {
    nres = res_head_[rindex];
  } else {
    nres = res->next_;
  }

  return nres;
}

const char* ConfigurationParser::ResToStr(int rcode) const
{
  if (rcode < r_first_ || rcode > r_last_) {
    return _("***UNKNOWN***");
  } else {
    return resources_[rcode - r_first_].name;
  }
}

const char* ConfigurationParser::ResGroupToStr(int rcode) const
{
  if (rcode < r_first_ || rcode > r_last_) {
    return _("***UNKNOWN***");
  } else {
    return resources_[rcode - r_first_].groupname;
  }
}

bool ConfigurationParser::GetTlsPskByFullyQualifiedResourceName(
    ConfigurationParser* config,
    const char* fq_name_in,
    std::string& psk)
{
  char* fq_name_buffer = strdup(fq_name_in);
  UnbashSpaces(fq_name_buffer);
  std::string fq_name(fq_name_buffer);
  free(fq_name_buffer);

  QualifiedResourceNameTypeConverter* c =
      config->GetQualifiedResourceNameTypeConverter();
  if (!c) { return false; }

  int r_type;
  std::string name; /* either unique job name or client name */

  bool ok = c->StringToResource(name, r_type, fq_name_in);
  if (!ok) { return false; }

  if (fq_name.find("R_JOB") != std::string::npos) {
    const char* psk_cstr = JcrGetAuthenticateKey(name.c_str());
    if (psk_cstr) {
      psk = psk_cstr;
      return true;
    }
  } else {
    TlsResource* tls = dynamic_cast<TlsResource*>(
        config->GetResWithName(r_type, name.c_str()));
    if (tls) {
      psk = tls->password_.value;
      return true;
    } else {
      Dmsg1(100, "Could not get tls resource for %d.\n", r_type);
    }
  }
  return false;
}

/*
 * Scan for message types and add them to the message
 * destination. The basic job here is to connect message types
 * (WARNING, ERROR, FATAL, INFO, ...) with an appropriate
 * destination (MAIL, FILE, OPERATOR, ...)
 */
void ConfigurationParser::ScanTypes(LEX* lc,
                                    MessagesResource* msg,
                                    MessageDestinationCode dest_code,
                                    const std::string& where,
                                    const std::string& cmd,
                                    const std::string& timestamp_format)
{
  int i;
  bool found, is_not;
  int msg_type = 0;
  char* str;

  for (;;) {
    LexGetToken(lc, BCT_NAME); /* expect at least one type */
    found = false;
    if (lc->str[0] == '!') {
      is_not = true;
      str = &lc->str[1];
    } else {
      is_not = false;
      str = &lc->str[0];
    }
    for (i = 0; msg_types[i].name; i++) {
      if (Bstrcasecmp(str, msg_types[i].name)) {
        msg_type = msg_types[i].token;
        found = true;
        break;
      }
    }
    if (!found) {
      scan_err1(lc, _("message type: %s not found"), str);
      return;
    }

    if (msg_type == M_MAX + 1) {     /* all? */
      for (i = 1; i <= M_MAX; i++) { /* yes set all types */
        msg->AddMessageDestination(dest_code, i, where, cmd, timestamp_format);
      }
    } else if (is_not) {
      msg->RemoveMessageDestination(dest_code, msg_type, where);
    } else {
      msg->AddMessageDestination(dest_code, msg_type, where, cmd,
                                 timestamp_format);
    }
    if (lc->ch != ',') { break; }
    Dmsg0(900, "call LexGetToken() to eat comma\n");
    LexGetToken(lc, BCT_ALL); /* eat comma */
  }
  Dmsg0(900, "Done ScanTypes()\n");
}

/*
 * Store Messages Destination information
 */
void ConfigurationParser::StoreMsgs(LEX* lc,
                                    ResourceItem* item,
                                    int index,
                                    int pass)
{
  int token;
  const char* cmd = nullptr;
  POOLMEM* dest;
  int dest_len;

  Dmsg2(900, "StoreMsgs pass=%d code=%d\n", pass, item->code);

  MessagesResource* message_resource =
      dynamic_cast<MessagesResource*>(*item->allocated_resource);

  if (!message_resource) {
    Dmsg0(900, "Could not dynamic_cast to MessageResource\n");
    abort();
    return;
  }

  if (pass == 1) {
    switch (static_cast<MessageDestinationCode>(item->code)) {
      case MessageDestinationCode::kStdout:
      case MessageDestinationCode::kStderr:
      case MessageDestinationCode::kConsole:
      case MessageDestinationCode::kCatalog:
        ScanTypes(lc, message_resource,
                  static_cast<MessageDestinationCode>(item->code),
                  std::string(), std::string(),
                  message_resource->timestamp_format_);
        break;
      case MessageDestinationCode::kSyslog: { /* syslog */
        char* p;
        int cnt = 0;
        bool done = false;

        /*
         * See if this is an old style syslog definition.
         * We count the number of = signs in the current config line.
         */
        p = lc->line;
        while (!done && *p) {
          switch (*p) {
            case '=':
              cnt++;
              break;
            case ',':
            case ';':
              /*
               * No need to continue scanning when we encounter a ',' or ';'
               */
              done = true;
              break;
            default:
              break;
          }
          p++;
        }

        /*
         * If there is more then one = its the new format e.g.
         * syslog = facility = filter
         */
        if (cnt > 1) {
          dest = GetPoolMemory(PM_MESSAGE);
          /*
           * Pick up a single facility.
           */
          token = LexGetToken(lc, BCT_NAME); /* Scan destination */
          PmStrcpy(dest, lc->str);
          dest_len = lc->str_len;
          token = LexGetToken(lc, BCT_SKIP_EOL);

          ScanTypes(lc, message_resource,
                    static_cast<MessageDestinationCode>(item->code), dest,
                    std::string(), std::string());
          FreePoolMemory(dest);
          Dmsg0(900, "done with dest codes\n");
        } else {
          ScanTypes(lc, message_resource,
                    static_cast<MessageDestinationCode>(item->code),
                    std::string(), std::string(), std::string());
        }
        break;
      }
      case MessageDestinationCode::kOperator:
      case MessageDestinationCode::kDirector:
      case MessageDestinationCode::kMail:
      case MessageDestinationCode::KMailOnError:
      case MessageDestinationCode::kMailOnSuccess:
        if (static_cast<MessageDestinationCode>(item->code) ==
            MessageDestinationCode::kOperator) {
          cmd = message_resource->operator_cmd_.c_str();
        } else {
          cmd = message_resource->mail_cmd_.c_str();
        }
        dest = GetPoolMemory(PM_MESSAGE);
        dest[0] = 0;
        dest_len = 0;

        /*
         * Pick up comma separated list of destinations.
         */
        for (;;) {
          token = LexGetToken(lc, BCT_NAME); /* Scan destination */
          dest = CheckPoolMemorySize(dest, dest_len + lc->str_len + 2);
          if (dest[0] != 0) {
            PmStrcat(dest, " "); /* Separate multiple destinations with space */
            dest_len++;
          }
          PmStrcat(dest, lc->str);
          dest_len += lc->str_len;
          Dmsg2(900, "StoreMsgs newdest=%s: dest=%s:\n", lc->str, NPRT(dest));
          token = LexGetToken(lc, BCT_SKIP_EOL);
          if (token == BCT_COMMA) { continue; /* Get another destination */ }
          if (token != BCT_EQUALS) {
            scan_err1(lc, _("expected an =, got: %s"), lc->str);
            return;
          }
          break;
        }
        Dmsg1(900, "mail_cmd=%s\n", NPRT(cmd));
        ScanTypes(lc, message_resource,
                  static_cast<MessageDestinationCode>(item->code), dest, cmd,
                  message_resource->timestamp_format_);
        FreePoolMemory(dest);
        Dmsg0(900, "done with dest codes\n");
        break;
      case MessageDestinationCode::kFile:
      case MessageDestinationCode::kAppend: {
        /*
         * Pick up a single destination.
         */
        token = LexGetToken(lc, BCT_STRING); /* Scan destination */
        std::string dest_file_path(lc->str);
        dest_len = lc->str_len;
        token = LexGetToken(lc, BCT_SKIP_EOL);
        Dmsg1(900, "StoreMsgs dest=%s:\n", dest_file_path.c_str());
        if (token != BCT_EQUALS) {
          scan_err1(lc, _("expected an =, got: %s"), lc->str);
          return;
        }
        ScanTypes(lc, message_resource,
                  static_cast<MessageDestinationCode>(item->code),
                  dest_file_path, std::string(),
                  message_resource->timestamp_format_);
        Dmsg0(900, "done with dest codes\n");
        break;
      }
      default:
        scan_err1(lc, _("Unknown item code: %d\n"), item->code);
        return;
    }
  }
  ScanToEol(lc);
  SetBit(index, message_resource->item_present_);
  ClearBit(index, message_resource->inherit_content_);
  Dmsg0(900, "Done StoreMsgs\n");
}

/*
 * This routine is ONLY for resource names
 * Store a name at specified address.
 */
void ConfigurationParser::StoreName(LEX* lc,
                                    ResourceItem* item,
                                    int index,
                                    int pass)
{
  POOLMEM* msg = GetPoolMemory(PM_EMSG);

  LexGetToken(lc, BCT_NAME);
  if (!IsNameValid(lc->str, msg)) {
    scan_err1(lc, "%s\n", msg);
    return;
  }
  FreePoolMemory(msg);
  /*
   * Store the name both in pass 1 and pass 2
   */
  char** p = GetItemVariablePointer<char**>(*item);

  if (*p) {
    scan_err2(lc, _("Attempt to redefine name \"%s\" to \"%s\"."), *p, lc->str);
    return;
  }
  *p = strdup(lc->str);
  ScanToEol(lc);
  SetBit(index, (*item->allocated_resource)->item_present_);
  ClearBit(index, (*item->allocated_resource)->inherit_content_);
}

/*
 * Store a name string at specified address
 * A name string is limited to MAX_RES_NAME_LENGTH
 */
void ConfigurationParser::StoreStrname(LEX* lc,
                                       ResourceItem* item,
                                       int index,
                                       int pass)
{
  LexGetToken(lc, BCT_NAME);
  if (pass == 1) {
    char** p = GetItemVariablePointer<char**>(*item);
    if (*p) { free(*p); }
    *p = strdup(lc->str);
  }
  ScanToEol(lc);
  SetBit(index, (*item->allocated_resource)->item_present_);
  ClearBit(index, (*item->allocated_resource)->inherit_content_);
}

/*
 * Store a string at specified address
 */
void ConfigurationParser::StoreStr(LEX* lc,
                                   ResourceItem* item,
                                   int index,
                                   int pass)
{
  LexGetToken(lc, BCT_STRING);
  if (pass == 1) { SetItemVariableFreeMemory<char*>(*item, strdup(lc->str)); }
  ScanToEol(lc);
  SetBit(index, (*item->allocated_resource)->item_present_);
  ClearBit(index, (*item->allocated_resource)->inherit_content_);
}

/*
 * Store a string at specified address
 */
void ConfigurationParser::StoreStdstr(LEX* lc,
                                      ResourceItem* item,
                                      int index,
                                      int pass)
{
  LexGetToken(lc, BCT_STRING);
  if (pass == 1) { SetItemVariable<std::string>(*item, lc->str); }
  ScanToEol(lc);
  SetBit(index, (*item->allocated_resource)->item_present_);
  ClearBit(index, (*item->allocated_resource)->inherit_content_);
}

/*
 * Store a directory name at specified address. Note, we do
 * shell expansion except if the string begins with a vertical
 * bar (i.e. it will likely be passed to the shell later).
 */
void ConfigurationParser::StoreDir(LEX* lc,
                                   ResourceItem* item,
                                   int index,
                                   int pass)
{
  LexGetToken(lc, BCT_STRING);
  if (pass == 1) {
    char** p = GetItemVariablePointer<char**>(*item);
    if (*p) { free(*p); }
    if (lc->str[0] != '|') {
      DoShellExpansion(lc->str, SizeofPoolMemory(lc->str));
    }
    *p = strdup(lc->str);
  }
  ScanToEol(lc);
  SetBit(index, (*item->allocated_resource)->item_present_);
  ClearBit(index, (*item->allocated_resource)->inherit_content_);
}

void ConfigurationParser::StoreStdstrdir(LEX* lc,
                                         ResourceItem* item,
                                         int index,
                                         int pass)
{
  LexGetToken(lc, BCT_STRING);
  if (pass == 1) {
    if (lc->str[0] != '|') {
      DoShellExpansion(lc->str, SizeofPoolMemory(lc->str));
    }
    SetItemVariable<std::string>(*item, lc->str);
  }
  ScanToEol(lc);
  SetBit(index, (*item->allocated_resource)->item_present_);
  ClearBit(index, (*item->allocated_resource)->inherit_content_);
}

/*
 * Store a password at specified address in MD5 coding
 */
void ConfigurationParser::StoreMd5Password(LEX* lc,
                                           ResourceItem* item,
                                           int index,
                                           int pass)
{
  LexGetToken(lc, BCT_STRING);
  if (pass == 1) { /* free old item */
    s_password* pwd = GetItemVariablePointer<s_password*>(*item);

    if (pwd->value) { free(pwd->value); }

    /*
     * See if we are parsing an MD5 encoded password already.
     */
    if (bstrncmp(lc->str, "[md5]", 5)) {
      if ((item->code & CFG_ITEM_REQUIRED) == CFG_ITEM_REQUIRED) {
        static const char* empty_password_md5_hash =
            "d41d8cd98f00b204e9800998ecf8427e";
        if (strncmp(lc->str + 5, empty_password_md5_hash,
                    strlen(empty_password_md5_hash)) == 0) {
          Emsg1(M_ERROR_TERM, 0, "No Password for Resource \"%s\" given\n",
                (*item->allocated_resource)->resource_name_);
        }
      }
      pwd->encoding = p_encoding_md5;
      pwd->value = strdup(lc->str + 5);
    } else {
      unsigned int i, j;
      MD5_CTX md5c;
      unsigned char digest[CRYPTO_DIGEST_MD5_SIZE];
      char sig[100];

      if ((item->code & CFG_ITEM_REQUIRED) == CFG_ITEM_REQUIRED) {
        if (strnlen(lc->str, MAX_NAME_LENGTH) == 0) {
          Emsg1(M_ERROR_TERM, 0, "No Password for Resource \"%s\" given\n",
                (*item->allocated_resource)->resource_name_);
        }
      }

      MD5_Init(&md5c);
      MD5_Update(&md5c, (unsigned char*)(lc->str), lc->str_len);
      MD5_Final(digest, &md5c);
      for (i = j = 0; i < sizeof(digest); i++) {
        sprintf(&sig[j], "%02x", digest[i]);
        j += 2;
      }
      pwd->encoding = p_encoding_md5;
      pwd->value = strdup(sig);
    }
  }
  ScanToEol(lc);
  SetBit(index, (*item->allocated_resource)->item_present_);
  ClearBit(index, (*item->allocated_resource)->inherit_content_);
}

/*
 * Store a password at specified address in MD5 coding
 */
void ConfigurationParser::StoreClearpassword(LEX* lc,
                                             ResourceItem* item,
                                             int index,
                                             int pass)
{
  LexGetToken(lc, BCT_STRING);
  if (pass == 1) {
    s_password* pwd = GetItemVariablePointer<s_password*>(*item);


    if (pwd->value) { free(pwd->value); }

    if ((item->code & CFG_ITEM_REQUIRED) == CFG_ITEM_REQUIRED) {
      if (strnlen(lc->str, MAX_NAME_LENGTH) == 0) {
        Emsg1(M_ERROR_TERM, 0, "No Password for Resource \"%s\" given\n",
              (*item->allocated_resource)->resource_name_);
      }
    }

    pwd->encoding = p_encoding_clear;
    pwd->value = strdup(lc->str);
  }
  ScanToEol(lc);
  SetBit(index, (*item->allocated_resource)->item_present_);
  ClearBit(index, (*item->allocated_resource)->inherit_content_);
}

/*
 * Store a resource at specified address.
 * If we are in pass 2, do a lookup of the
 * resource.
 */
void ConfigurationParser::StoreRes(LEX* lc,
                                   ResourceItem* item,
                                   int index,
                                   int pass)
{
  LexGetToken(lc, BCT_NAME);
  if (pass == 2) {
    BareosResource* res = GetResWithName(item->code, lc->str);
    if (res == NULL) {
      scan_err3(
          lc,
          _("Could not find config resource \"%s\" referenced on line %d: %s"),
          lc->str, lc->line_no, lc->line);
      return;
    }
    BareosResource** p = GetItemVariablePointer<BareosResource**>(*item);
    if (*p) {
      scan_err3(
          lc,
          _("Attempt to redefine resource \"%s\" referenced on line %d: %s"),
          item->name, lc->line_no, lc->line);
      return;
    }
    *p = res;
  }
  ScanToEol(lc);
  SetBit(index, (*item->allocated_resource)->item_present_);
  ClearBit(index, (*item->allocated_resource)->inherit_content_);
}

/*
 * Store a resource pointer in an alist. default_value indicates how many
 * times this routine can be called -- i.e. how many alists there are.
 *
 * If we are in pass 2, do a lookup of the resource.
 */
void ConfigurationParser::StoreAlistRes(LEX* lc,
                                        ResourceItem* item,
                                        int index,
                                        int pass)
{
  int i = 0;
  alist* list;
  int count = str_to_int32(item->default_value);

  if (pass == 2) {
    alist** alistvalue = GetItemVariablePointer<alist**>(*item);

    if (count == 0) { /* always store in item->value */
      i = 0;
      if (!alistvalue[i]) { alistvalue[i] = new alist(10, not_owned_by_alist); }
    } else {
      /*
       * Find empty place to store this directive
       */
      while ((alistvalue)[i] != NULL && i++ < count) {}
      if (i >= count) {
        scan_err4(lc, _("Too many %s directives. Max. is %d. line %d: %s\n"),
                  lc->str, count, lc->line_no, lc->line);
        return;
      }
      alistvalue[i] = new alist(10, not_owned_by_alist);
    }
    list = alistvalue[i];

    for (;;) {
      LexGetToken(lc, BCT_NAME); /* scan next item */
      BareosResource* res = GetResWithName(item->code, lc->str);
      if (res == NULL) {
        scan_err3(lc,
                  _("Could not find config Resource \"%s\" referenced on line "
                    "%d : %s\n"),
                  item->name, lc->line_no, lc->line);
        return;
      }
      Dmsg5(900, "Append %p to alist %p size=%d i=%d %s\n", res, list,
            list->size(), i, item->name);
      list->append(res);
      if (lc->ch != ',') { /* if no other item follows */
        break;             /* get out */
      }
      LexGetToken(lc, BCT_ALL); /* eat comma */
    }
  }
  ScanToEol(lc);
  SetBit(index, (*item->allocated_resource)->item_present_);
  ClearBit(index, (*item->allocated_resource)->inherit_content_);
}

/*
 * Store a std::string in an std::vector<std::string>.
 */
void ConfigurationParser::StoreStdVectorStr(LEX* lc,
                                            ResourceItem* item,
                                            int index,
                                            int pass)
{
  if (pass == 2) {
    std::vector<std::string>* list =
        GetItemVariablePointer<std::vector<std::string>*>(*item);
    LexGetToken(lc, BCT_STRING); /* scan next item */
    Dmsg4(900, "Append %s to vector %p size=%d %s\n", lc->str, list,
          list->size(), item->name);

    /*
     * See if we need to drop the default value from the alist.
     *
     * We first check to see if the config item has the CFG_ITEM_DEFAULT
     * flag set and currently has exactly one entry.
     */
    if ((item->flags & CFG_ITEM_DEFAULT) && list->size() == 1) {
      if (list->at(0) == item->default_value) { list->clear(); }
    }
    list->push_back(lc->str);
  }
  ScanToEol(lc);
  SetBit(index, (*item->allocated_resource)->item_present_);
  ClearBit(index, (*item->allocated_resource)->inherit_content_);
}

/*
 * Store a string in an alist.
 */
void ConfigurationParser::StoreAlistStr(LEX* lc,
                                        ResourceItem* item,
                                        int index,
                                        int pass)
{
  if (pass == 2) {
    alist** alistvalue = GetItemVariablePointer<alist**>(*item);
    if (!*alistvalue) { *alistvalue = new alist(10, owned_by_alist); }
    alist* list = *alistvalue;

    LexGetToken(lc, BCT_STRING); /* scan next item */
    Dmsg4(900, "Append %s to alist %p size=%d %s\n", lc->str, list,
          list->size(), item->name);

    /*
     * See if we need to drop the default value from the alist.
     *
     * We first check to see if the config item has the CFG_ITEM_DEFAULT
     * flag set and currently has exactly one entry.
     */
    if ((item->flags & CFG_ITEM_DEFAULT) && list->size() == 1) {
      char* entry;

      entry = (char*)list->first();
      if (bstrcmp(entry, item->default_value)) {
        list->destroy();
        list->init(10, owned_by_alist);
      }
    }

    list->append(strdup(lc->str));
  }
  ScanToEol(lc);
  SetBit(index, (*item->allocated_resource)->item_present_);
  ClearBit(index, (*item->allocated_resource)->inherit_content_);
}

/*
 * Store a directory name at specified address in an alist.
 * Note, we do shell expansion except if the string begins
 * with a vertical bar (i.e. it will likely be passed to the
 * shell later).
 */
void ConfigurationParser::StoreAlistDir(LEX* lc,
                                        ResourceItem* item,
                                        int index,
                                        int pass)
{
  if (pass == 2) {
    alist** alistvalue = GetItemVariablePointer<alist**>(*item);
    if (!*alistvalue) { *alistvalue = new alist(10, owned_by_alist); }
    alist* list = *alistvalue;

    LexGetToken(lc, BCT_STRING); /* scan next item */
    Dmsg4(900, "Append %s to alist %p size=%d %s\n", lc->str, list,
          list->size(), item->name);

    if (lc->str[0] != '|') {
      DoShellExpansion(lc->str, SizeofPoolMemory(lc->str));
    }

    /*
     * See if we need to drop the default value from the alist.
     *
     * We first check to see if the config item has the CFG_ITEM_DEFAULT
     * flag set and currently has exactly one entry.
     */
    if ((item->flags & CFG_ITEM_DEFAULT) && list->size() == 1) {
      char* entry;

      entry = (char*)list->first();
      if (bstrcmp(entry, item->default_value)) {
        list->destroy();
        list->init(10, owned_by_alist);
      }
    }

    list->append(strdup(lc->str));
  }
  ScanToEol(lc);
  SetBit(index, (*item->allocated_resource)->item_present_);
  ClearBit(index, (*item->allocated_resource)->inherit_content_);
}

/*
 * Store a list of plugin names to load by the daemon on startup.
 */
void ConfigurationParser::StorePluginNames(LEX* lc,
                                           ResourceItem* item,
                                           int index,
                                           int pass)
{
  if (pass == 2) {
    char *p, *plugin_name, *plugin_names;
    LexGetToken(lc, BCT_STRING); /* scan next item */

    alist** alistvalue = GetItemVariablePointer<alist**>(*item);
    if (!*alistvalue) { *alistvalue = new alist(10, owned_by_alist); }
    alist* list = *alistvalue;

    plugin_names = strdup(lc->str);
    plugin_name = plugin_names;
    while (plugin_name) {
      if ((p = strchr(plugin_name, ':'))) { /* split string at ':' */
        *p++ = '\0';
      }

      list->append(strdup(plugin_name));
      plugin_name = p;
    }
    free(plugin_names);
  }
  ScanToEol(lc);
  SetBit(index, (*item->allocated_resource)->item_present_);
  ClearBit(index, (*item->allocated_resource)->inherit_content_);
}

/*
 * Store default values for Resource from xxxDefs
 * If we are in pass 2, do a lookup of the
 * resource and store everything not explicitly set
 * in main resource.
 *
 * Note, here item points to the main resource (e.g. Job, not
 *  the jobdefs, which we look up).
 */
void ConfigurationParser::StoreDefs(LEX* lc,
                                    ResourceItem* item,
                                    int index,
                                    int pass)
{
  BareosResource* res;

  LexGetToken(lc, BCT_NAME);
  if (pass == 2) {
    Dmsg2(900, "Code=%d name=%s\n", item->code, lc->str);
    res = GetResWithName(item->code, lc->str);
    if (res == NULL) {
      scan_err3(
          lc, _("Missing config Resource \"%s\" referenced on line %d : %s\n"),
          lc->str, lc->line_no, lc->line);
      return;
    }
  }
  ScanToEol(lc);
}

/*
 * Store an integer at specified address
 */
void ConfigurationParser::store_int16(LEX* lc,
                                      ResourceItem* item,
                                      int index,
                                      int pass)
{
  LexGetToken(lc, BCT_INT16);
  SetItemVariable<int16_t>(*item, lc->u.int16_val);
  ScanToEol(lc);
  SetBit(index, (*item->allocated_resource)->item_present_);
  ClearBit(index, (*item->allocated_resource)->inherit_content_);
}

void ConfigurationParser::store_int32(LEX* lc,
                                      ResourceItem* item,
                                      int index,
                                      int pass)
{
  LexGetToken(lc, BCT_INT32);
  SetItemVariable<int32_t>(*item, lc->u.int32_val);
  ScanToEol(lc);
  SetBit(index, (*item->allocated_resource)->item_present_);
  ClearBit(index, (*item->allocated_resource)->inherit_content_);
}

/*
 * Store a positive integer at specified address
 */
void ConfigurationParser::store_pint16(LEX* lc,
                                       ResourceItem* item,
                                       int index,
                                       int pass)
{
  LexGetToken(lc, BCT_PINT16);
  SetItemVariable<uint16_t>(*item, lc->u.pint16_val);
  ScanToEol(lc);
  SetBit(index, (*item->allocated_resource)->item_present_);
  ClearBit(index, (*item->allocated_resource)->inherit_content_);
}

void ConfigurationParser::store_pint32(LEX* lc,
                                       ResourceItem* item,
                                       int index,
                                       int pass)
{
  LexGetToken(lc, BCT_PINT32);
  SetItemVariable<uint32_t>(*item, lc->u.pint32_val);
  ScanToEol(lc);
  SetBit(index, (*item->allocated_resource)->item_present_);
  ClearBit(index, (*item->allocated_resource)->inherit_content_);
}

/*
 * Store an 64 bit integer at specified address
 */
void ConfigurationParser::store_int64(LEX* lc,
                                      ResourceItem* item,
                                      int index,
                                      int pass)
{
  LexGetToken(lc, BCT_INT64);
  SetItemVariable<int64_t>(*item, lc->u.int64_val);
  ScanToEol(lc);
  SetBit(index, (*item->allocated_resource)->item_present_);
  ClearBit(index, (*item->allocated_resource)->inherit_content_);
}

/*
 * Store a size in bytes
 */
void ConfigurationParser::store_int_unit(LEX* lc,
                                         ResourceItem* item,
                                         int index,
                                         int pass,
                                         bool size32,
                                         enum unit_type type)
{
  uint64_t uvalue;
  char bsize[500];

  Dmsg0(900, "Enter store_unit\n");
  int token = LexGetToken(lc, BCT_SKIP_EOL);
  errno = 0;
  switch (token) {
    case BCT_NUMBER:
    case BCT_IDENTIFIER:
    case BCT_UNQUOTED_STRING:
      bstrncpy(bsize, lc->str, sizeof(bsize)); /* save first part */
      /*
       * If terminated by space, scan and get modifier
       */
      while (lc->ch == ' ') {
        token = LexGetToken(lc, BCT_ALL);
        switch (token) {
          case BCT_NUMBER:
          case BCT_IDENTIFIER:
          case BCT_UNQUOTED_STRING:
            bstrncat(bsize, lc->str, sizeof(bsize));
            break;
        }
      }

      switch (type) {
        case STORE_SIZE:
          if (!size_to_uint64(bsize, &uvalue)) {
            scan_err1(lc, _("expected a size number, got: %s"), lc->str);
            return;
          }
          break;
        case STORE_SPEED:
          if (!speed_to_uint64(bsize, &uvalue)) {
            scan_err1(lc, _("expected a speed number, got: %s"), lc->str);
            return;
          }
          break;
        default:
          scan_err0(lc, _("unknown unit type encountered"));
          return;
      }

      if (size32) {
        SetItemVariable<uint32_t>(*item, uvalue);
      } else {
        switch (type) {
          case STORE_SIZE:
            SetItemVariable<int64_t>(*item, uvalue);
            break;
          case STORE_SPEED:
            SetItemVariable<uint64_t>(*item, uvalue);
            break;
        }
      }
      break;
    default:
      scan_err2(lc, _("expected a %s, got: %s"),
                (type == STORE_SIZE) ? _("size") : _("speed"), lc->str);
      return;
  }
  if (token != BCT_EOL) { ScanToEol(lc); }
  SetBit(index, (*item->allocated_resource)->item_present_);
  ClearBit(index, (*item->allocated_resource)->inherit_content_);
  Dmsg0(900, "Leave store_unit\n");
}

/*
 * Store a size in bytes
 */
void ConfigurationParser::store_size32(LEX* lc,
                                       ResourceItem* item,
                                       int index,
                                       int pass)
{
  store_int_unit(lc, item, index, pass, true /* 32 bit */, STORE_SIZE);
}

/*
 * Store a size in bytes
 */
void ConfigurationParser::store_size64(LEX* lc,
                                       ResourceItem* item,
                                       int index,
                                       int pass)
{
  store_int_unit(lc, item, index, pass, false /* not 32 bit */, STORE_SIZE);
}

/*
 * Store a speed in bytes/s
 */
void ConfigurationParser::StoreSpeed(LEX* lc,
                                     ResourceItem* item,
                                     int index,
                                     int pass)
{
  store_int_unit(lc, item, index, pass, false /* 64 bit */, STORE_SPEED);
}

/*
 * Store a time period in seconds
 */
void ConfigurationParser::StoreTime(LEX* lc,
                                    ResourceItem* item,
                                    int index,
                                    int pass)
{
  utime_t utime;
  char period[500];

  int token = LexGetToken(lc, BCT_SKIP_EOL);
  errno = 0;
  switch (token) {
    case BCT_NUMBER:
    case BCT_IDENTIFIER:
    case BCT_UNQUOTED_STRING:
      bstrncpy(period, lc->str, sizeof(period)); /* get first part */
      /*
       * If terminated by space, scan and get modifier
       */
      while (lc->ch == ' ') {
        token = LexGetToken(lc, BCT_ALL);
        switch (token) {
          case BCT_NUMBER:
          case BCT_IDENTIFIER:
          case BCT_UNQUOTED_STRING:
            bstrncat(period, lc->str, sizeof(period));
            break;
        }
      }
      if (!DurationToUtime(period, &utime)) {
        scan_err1(lc, _("expected a time period, got: %s"), period);
        return;
      }
      SetItemVariable<utime_t>(*item, utime);
      break;
    default:
      scan_err1(lc, _("expected a time period, got: %s"), lc->str);
      return;
  }
  if (token != BCT_EOL) { ScanToEol(lc); }
  SetBit(index, (*item->allocated_resource)->item_present_);
  ClearBit(index, (*item->allocated_resource)->inherit_content_);
}

/*
 * Store a yes/no in a bit field
 */
void ConfigurationParser::StoreBit(LEX* lc,
                                   ResourceItem* item,
                                   int index,
                                   int pass)
{
  LexGetToken(lc, BCT_NAME);
  char* bitvalue = GetItemVariablePointer<char*>(*item);
  if (Bstrcasecmp(lc->str, "yes") || Bstrcasecmp(lc->str, "true")) {
    SetBit(item->code, bitvalue);
  } else if (Bstrcasecmp(lc->str, "no") || Bstrcasecmp(lc->str, "false")) {
    ClearBit(item->code, bitvalue);
  } else {
    scan_err2(lc, _("Expect %s, got: %s"), "YES, NO, TRUE, or FALSE",
              lc->str); /* YES and NO must not be translated */
    return;
  }
  ScanToEol(lc);
  SetBit(index, (*item->allocated_resource)->item_present_);
  ClearBit(index, (*item->allocated_resource)->inherit_content_);
}

/*
 * Store a bool in a bit field
 */
void ConfigurationParser::StoreBool(LEX* lc,
                                    ResourceItem* item,
                                    int index,
                                    int pass)
{
  LexGetToken(lc, BCT_NAME);
  if (Bstrcasecmp(lc->str, "yes") || Bstrcasecmp(lc->str, "true")) {
    SetItemVariable<bool>(*item, true);
  } else if (Bstrcasecmp(lc->str, "no") || Bstrcasecmp(lc->str, "false")) {
    SetItemVariable<bool>(*item, false);
  } else {
    scan_err2(lc, _("Expect %s, got: %s"), "YES, NO, TRUE, or FALSE",
              lc->str); /* YES and NO must not be translated */
    return;
  }
  ScanToEol(lc);
  SetBit(index, (*item->allocated_resource)->item_present_);
  ClearBit(index, (*item->allocated_resource)->inherit_content_);
}

/*
 * Store Tape Label Type (BAREOS, ANSI, IBM)
 */
void ConfigurationParser::StoreLabel(LEX* lc,
                                     ResourceItem* item,
                                     int index,
                                     int pass)
{
  LexGetToken(lc, BCT_NAME);
  /*
   * Store the label pass 2 so that type is defined
   */
  int i;
  for (i = 0; tapelabels[i].name; i++) {
    if (Bstrcasecmp(lc->str, tapelabels[i].name)) {
      SetItemVariable<uint32_t>(*item, tapelabels[i].token);
      i = 0;
      break;
    }
  }
  if (i != 0) {
    scan_err1(lc, _("Expected a Tape Label keyword, got: %s"), lc->str);
    return;
  }
  ScanToEol(lc);
  SetBit(index, (*item->allocated_resource)->item_present_);
  ClearBit(index, (*item->allocated_resource)->inherit_content_);
}

/*
 * Store network addresses.
 *
 *   my tests
 *   positiv
 *   = { ip = { addr = 1.2.3.4; port = 1205; } ipv4 = { addr = 1.2.3.4; port =
 * http; } } = { ip = { addr = 1.2.3.4; port = 1205; } ipv4 = { addr = 1.2.3.4;
 * port = http; } ipv6 = { addr = 1.2.3.4; port = 1205;
 *     }
 *     ip = {
 *       addr = 1.2.3.4
 *       port = 1205
 *     }
 *     ip = {
 *       addr = 1.2.3.4
 *     }
 *     ip = {
 *       addr = 2001:220:222::2
 *     }
 *     ip = {
 *       addr = bluedot.thun.net
 *     }
 *   }
 *   negativ
 *   = { ip = { } }
 *   = { ipv4 { addr = doof.nowaytoheavenxyz.uhu; } }
 *   = { ipv4 { port = 4711 } }
 */
void ConfigurationParser::StoreAddresses(LEX* lc,
                                         ResourceItem* item,
                                         int index,
                                         int pass)
{
  int token;
  int exist;
  int family = 0;
  char errmsg[1024];
  char port_str[128];
  char hostname_str[1024];
  enum
  {
    EMPTYLINE = 0x0,
    PORTLINE = 0x1,
    ADDRLINE = 0x2
  } next_line = EMPTYLINE;
  int port = str_to_int32(item->default_value);

  token = LexGetToken(lc, BCT_SKIP_EOL);
  if (token != BCT_BOB) {
    scan_err1(lc, _("Expected a block begin { , got: %s"), lc->str);
  }
  token = LexGetToken(lc, BCT_SKIP_EOL);
  if (token == BCT_EOB) { scan_err0(lc, _("Empty addr block is not allowed")); }
  do {
    if (!(token == BCT_UNQUOTED_STRING || token == BCT_IDENTIFIER)) {
      scan_err1(lc, _("Expected a string, got: %s"), lc->str);
    }
    if (Bstrcasecmp("ip", lc->str) || Bstrcasecmp("ipv4", lc->str)) {
      family = AF_INET;
#ifdef HAVE_IPV6
    } else if (Bstrcasecmp("ipv6", lc->str)) {
      family = AF_INET6;
    } else {
      scan_err1(lc, _("Expected a string [ip|ipv4|ipv6], got: %s"), lc->str);
    }
#else
    } else {
      scan_err1(lc, _("Expected a string [ip|ipv4], got: %s"), lc->str);
    }
#endif
    token = LexGetToken(lc, BCT_SKIP_EOL);
    if (token != BCT_EQUALS) {
      scan_err1(lc, _("Expected a equal =, got: %s"), lc->str);
    }
    token = LexGetToken(lc, BCT_SKIP_EOL);
    if (token != BCT_BOB) {
      scan_err1(lc, _("Expected a block begin { , got: %s"), lc->str);
    }
    token = LexGetToken(lc, BCT_SKIP_EOL);
    exist = EMPTYLINE;
    port_str[0] = hostname_str[0] = '\0';
    do {
      if (token != BCT_IDENTIFIER) {
        scan_err1(lc, _("Expected a identifier [addr|port], got: %s"), lc->str);
      }
      if (Bstrcasecmp("port", lc->str)) {
        next_line = PORTLINE;
        if (exist & PORTLINE) {
          scan_err0(lc, _("Only one port per address block"));
        }
        exist |= PORTLINE;
      } else if (Bstrcasecmp("addr", lc->str)) {
        next_line = ADDRLINE;
        if (exist & ADDRLINE) {
          scan_err0(lc, _("Only one addr per address block"));
        }
        exist |= ADDRLINE;
      } else {
        scan_err1(lc, _("Expected a identifier [addr|port], got: %s"), lc->str);
      }
      token = LexGetToken(lc, BCT_SKIP_EOL);
      if (token != BCT_EQUALS) {
        scan_err1(lc, _("Expected a equal =, got: %s"), lc->str);
      }
      token = LexGetToken(lc, BCT_SKIP_EOL);
      switch (next_line) {
        case PORTLINE:
          if (!(token == BCT_UNQUOTED_STRING || token == BCT_NUMBER ||
                token == BCT_IDENTIFIER)) {
            scan_err1(lc, _("Expected a number or a string, got: %s"), lc->str);
          }
          bstrncpy(port_str, lc->str, sizeof(port_str));
          break;
        case ADDRLINE:
          if (!(token == BCT_UNQUOTED_STRING || token == BCT_IDENTIFIER)) {
            scan_err1(lc, _("Expected an IP number or a hostname, got: %s"),
                      lc->str);
          }
          bstrncpy(hostname_str, lc->str, sizeof(hostname_str));
          break;
        case EMPTYLINE:
          scan_err0(lc, _("State machine mismatch"));
          break;
      }
      token = LexGetToken(lc, BCT_SKIP_EOL);
    } while (token == BCT_IDENTIFIER);
    if (token != BCT_EOB) {
      scan_err1(lc, _("Expected a end of block }, got: %s"), lc->str);
    }
    if (pass == 1 &&
        !AddAddress(GetItemVariablePointer<dlist**>(*item), IPADDR::R_MULTIPLE,
                    htons(port), family, hostname_str, port_str, errmsg,
                    sizeof(errmsg))) {
      scan_err3(lc, _("Can't add hostname(%s) and port(%s) to addrlist (%s)"),
                hostname_str, port_str, errmsg);
    }
    token = ScanToNextNotEol(lc);
  } while ((token == BCT_IDENTIFIER || token == BCT_UNQUOTED_STRING));
  if (token != BCT_EOB) {
    scan_err1(lc, _("Expected a end of block }, got: %s"), lc->str);
  }
  SetBit(index, (*item->allocated_resource)->item_present_);
  ClearBit(index, (*item->allocated_resource)->inherit_content_);
}

void ConfigurationParser::StoreAddressesAddress(LEX* lc,
                                                ResourceItem* item,
                                                int index,
                                                int pass)
{
  int token;
  char errmsg[1024];
  int port = str_to_int32(item->default_value);

  token = LexGetToken(lc, BCT_SKIP_EOL);
  if (!(token == BCT_UNQUOTED_STRING || token == BCT_NUMBER ||
        token == BCT_IDENTIFIER)) {
    scan_err1(lc, _("Expected an IP number or a hostname, got: %s"), lc->str);
  }
  if (pass == 1 &&
      !AddAddress(GetItemVariablePointer<dlist**>(*item), IPADDR::R_SINGLE_ADDR,
                  htons(port), AF_INET, lc->str, 0, errmsg, sizeof(errmsg))) {
    scan_err2(lc, _("can't add port (%s) to (%s)"), lc->str, errmsg);
  }
}

void ConfigurationParser::StoreAddressesPort(LEX* lc,
                                             ResourceItem* item,
                                             int index,
                                             int pass)
{
  int token;
  char errmsg[1024];
  int port = str_to_int32(item->default_value);

  token = LexGetToken(lc, BCT_SKIP_EOL);
  if (!(token == BCT_UNQUOTED_STRING || token == BCT_NUMBER ||
        token == BCT_IDENTIFIER)) {
    scan_err1(lc, _("Expected a port number or string, got: %s"), lc->str);
  }
  if (pass == 1 &&
      !AddAddress(GetItemVariablePointer<dlist**>(*item), IPADDR::R_SINGLE_PORT,
                  htons(port), AF_INET, 0, lc->str, errmsg, sizeof(errmsg))) {
    scan_err2(lc, _("can't add port (%s) to (%s)"), lc->str, errmsg);
  }
}

/*
 * Generic store resource dispatcher.
 */
bool ConfigurationParser::StoreResource(int type,
                                        LEX* lc,
                                        ResourceItem* item,
                                        int index,
                                        int pass)
{
  switch (type) {
    case CFG_TYPE_STR:
      StoreStr(lc, item, index, pass);
      break;
    case CFG_TYPE_DIR:
      StoreDir(lc, item, index, pass);
      break;
    case CFG_TYPE_STDSTR:
      StoreStdstr(lc, item, index, pass);
      break;
    case CFG_TYPE_STDSTRDIR:
      StoreStdstrdir(lc, item, index, pass);
      break;
    case CFG_TYPE_MD5PASSWORD:
      StoreMd5Password(lc, item, index, pass);
      break;
    case CFG_TYPE_CLEARPASSWORD:
      StoreClearpassword(lc, item, index, pass);
      break;
    case CFG_TYPE_NAME:
      StoreName(lc, item, index, pass);
      break;
    case CFG_TYPE_STRNAME:
      StoreStrname(lc, item, index, pass);
      break;
    case CFG_TYPE_RES:
      StoreRes(lc, item, index, pass);
      break;
    case CFG_TYPE_ALIST_RES:
      StoreAlistRes(lc, item, index, pass);
      break;
    case CFG_TYPE_ALIST_STR:
      StoreAlistStr(lc, item, index, pass);
      break;
    case CFG_TYPE_STR_VECTOR:
    case CFG_TYPE_STR_VECTOR_OF_DIRS:
      StoreStdVectorStr(lc, item, index, pass);
      break;
    case CFG_TYPE_ALIST_DIR:
      StoreAlistDir(lc, item, index, pass);
      break;
    case CFG_TYPE_INT16:
      store_int16(lc, item, index, pass);
      break;
    case CFG_TYPE_PINT16:
      store_pint16(lc, item, index, pass);
      break;
    case CFG_TYPE_INT32:
      store_int32(lc, item, index, pass);
      break;
    case CFG_TYPE_PINT32:
      store_pint32(lc, item, index, pass);
      break;
    case CFG_TYPE_MSGS:
      StoreMsgs(lc, item, index, pass);
      break;
    case CFG_TYPE_INT64:
      store_int64(lc, item, index, pass);
      break;
    case CFG_TYPE_BIT:
      StoreBit(lc, item, index, pass);
      break;
    case CFG_TYPE_BOOL:
      StoreBool(lc, item, index, pass);
      break;
    case CFG_TYPE_TIME:
      StoreTime(lc, item, index, pass);
      break;
    case CFG_TYPE_SIZE64:
      store_size64(lc, item, index, pass);
      break;
    case CFG_TYPE_SIZE32:
      store_size32(lc, item, index, pass);
      break;
    case CFG_TYPE_SPEED:
      StoreSpeed(lc, item, index, pass);
      break;
    case CFG_TYPE_DEFS:
      StoreDefs(lc, item, index, pass);
      break;
    case CFG_TYPE_LABEL:
      StoreLabel(lc, item, index, pass);
      break;
    case CFG_TYPE_ADDRESSES:
      StoreAddresses(lc, item, index, pass);
      break;
    case CFG_TYPE_ADDRESSES_ADDRESS:
      StoreAddressesAddress(lc, item, index, pass);
      break;
    case CFG_TYPE_ADDRESSES_PORT:
      StoreAddressesPort(lc, item, index, pass);
      break;
    case CFG_TYPE_PLUGIN_NAMES:
      StorePluginNames(lc, item, index, pass);
      break;
    case CFG_TYPE_DIR_OR_CMD:
      StoreDir(lc, item, index, pass);
      break;
    default:
      return false;
  }

  return true;
}

void IndentConfigItem(PoolMem& cfg_str,
                      int level,
                      const char* config_item,
                      bool inherited)
{
  for (int i = 0; i < level; i++) { PmStrcat(cfg_str, DEFAULT_INDENT_STRING); }
  if (inherited) {
    PmStrcat(cfg_str, "#");
    PmStrcat(cfg_str, DEFAULT_INDENT_STRING);
  }
  PmStrcat(cfg_str, config_item);
}

std::string PrintNumberSiPrefixFormat(ResourceItem* item, uint64_t value_in)
{
  PoolMem temp;
  PoolMem volspec; /* vol specification string*/
  uint64_t value = value_in;
  int factor;

  /*
   * convert default value string to numeric value
   */
  static const char* modifier[] = {"g", "m", "k", "", NULL};
  const uint64_t multiplier[] = {
      1073741824, /* gibi */
      1048576,    /* mebi */
      1024,       /* kibi */
      1           /* byte */
  };

  if (value == 0) {
    PmStrcat(volspec, "0");
  } else {
    for (int t = 0; modifier[t]; t++) {
      Dmsg2(200, " %s bytes: %lld\n", item->name, value);
      factor = value / multiplier[t];
      value = value % multiplier[t];
      if (factor > 0) {
        Mmsg(temp, "%d %s ", factor, modifier[t]);
        PmStrcat(volspec, temp.c_str());
        Dmsg1(200, " volspec: %s\n", volspec.c_str());
      }
      if (value == 0) { break; }
    }
  }

  return std::string(volspec.c_str());
}

std::string Print32BitConfigNumberSiPrefixFormat(ResourceItem* item)
{
  uint32_t value_32_bit = GetItemVariable<uint32_t>(*item);
  return PrintNumberSiPrefixFormat(item, value_32_bit);
}

std::string Print64BitConfigNumberSiPrefixFormat(ResourceItem* item)
{
  uint64_t value_64_bit = GetItemVariable<uint64_t>(*item);
  return PrintNumberSiPrefixFormat(item, value_64_bit);
}

std::string PrintConfigTime(ResourceItem* item)
{
  PoolMem temp;
  PoolMem timespec;
  utime_t secs = GetItemVariable<utime_t>(*item);
  int factor;

  /*
   * Reverse time formatting: 1 Month, 1 Week, etc.
   *
   * convert default value string to numeric value
   */
  static const char* modifier[] = {"years", "months",  "weeks",   "days",
                                   "hours", "minutes", "seconds", NULL};
  static const int32_t multiplier[] = {60 * 60 * 24 * 365,
                                       60 * 60 * 24 * 30,
                                       60 * 60 * 24 * 7,
                                       60 * 60 * 24,
                                       60 * 60,
                                       60,
                                       1,
                                       0};

  if (secs == 0) {
    PmStrcat(timespec, "0");
  } else {
    for (int t = 0; modifier[t]; t++) {
      factor = secs / multiplier[t];
      secs = secs % multiplier[t];
      if (factor > 0) {
        Mmsg(temp, "%d %s ", factor, modifier[t]);
        PmStrcat(timespec, temp.c_str());
      }
      if (secs == 0) { break; }
    }
  }

  return std::string(timespec.c_str());
}

// message destinations
struct s_mdestination {
  const char* destination;
  bool where;
};

static std::map<MessageDestinationCode, s_mdestination> msg_destinations = {
    {MessageDestinationCode::kSyslog, {"syslog", false}},
    {MessageDestinationCode::kMail, {"mail", true}},
    {MessageDestinationCode::kFile, {"file", true}},
    {MessageDestinationCode::kAppend, {"append", true}},
    {MessageDestinationCode::kStdout, {"stdout", false}},
    {MessageDestinationCode::kStderr, {"stderr", false}},
    {MessageDestinationCode::kDirector, {"director", true}},
    {MessageDestinationCode::kOperator, {"operator", true}},
    {MessageDestinationCode::kConsole, {"console", false}},
    {MessageDestinationCode::KMailOnError, {"mailonerror", true}},
    {MessageDestinationCode::kMailOnSuccess, {"mailonsuccess", true}},
    {MessageDestinationCode::kCatalog, {"catalog", false}}};


std::string MessagesResource::GetMessageTypesAsSring(MessageDestinationInfo* d,
                                                     bool verbose)
{
  std::string cfg_str; /* configuration as string  */
  PoolMem temp;

  int nr_set = 0;
  int nr_unset = 0;
  PoolMem t; /* set   types */
  PoolMem u; /* unset types */

  for (int j = 0; j < M_MAX - 1; j++) {
    if (BitIsSet(msg_types[j].token, d->msg_types_)) {
      nr_set++;
      Mmsg(temp, ",%s", msg_types[j].name);
      PmStrcat(t, temp.c_str());
    } else {
      Mmsg(temp, ",!%s", msg_types[j].name);
      nr_unset++;
      PmStrcat(u, temp.c_str());
    }
  }

  if (verbose) {
    /* show all message types */
    cfg_str += t.c_str() + 1; /* skip first comma */
    cfg_str += u.c_str();
  } else {
    if (nr_set > nr_unset) { /* if more is set than is unset */
      cfg_str += "all";      /* all, but not ... */
      cfg_str += u.c_str();
    } else {                    /* only print set types */
      cfg_str += t.c_str() + 1; /* skip first comma */
    }
  }

  return cfg_str.c_str();
}

bool MessagesResource::PrintConfig(OutputFormatterResource& send,
                                   const ConfigurationParser& /* unused */,
                                   bool hide_sensitive_data,
                                   bool verbose)
{
  PoolMem cfg_str; /* configuration as string  */
  PoolMem temp;
  MessagesResource* msgres;
  OutputFormatter* of = send.GetOutputFormatter();

  msgres = this;

  send.ResourceTypeStart("Messages");
  send.ResourceStart(resource_name_);

  send.KeyQuotedString("Name", resource_name_);

  if (!msgres->mail_cmd_.empty()) {
    PoolMem esc;

    EscapeString(esc, msgres->mail_cmd_.c_str(), msgres->mail_cmd_.size());
    send.KeyQuotedString("MailCommand", esc.c_str());
  }

  if (!msgres->operator_cmd_.empty()) {
    PoolMem esc;

    EscapeString(esc, msgres->operator_cmd_.c_str(),
                 msgres->operator_cmd_.size());
    send.KeyQuotedString("OperatorCommand", esc.c_str());
  }

  if (!msgres->timestamp_format_.empty()) {
    PoolMem esc;

    EscapeString(esc, msgres->timestamp_format_.c_str(),
                 msgres->timestamp_format_.size());
    send.KeyQuotedString("TimestampFormat", esc.c_str());
  }

  for (MessageDestinationInfo* d : msgres->dest_chain_) {
    auto msg_dest_iter = msg_destinations.find(d->dest_code_);

    if (msg_dest_iter != msg_destinations.end()) {
      of->ObjectStart(msg_dest_iter->second.destination,
                      send.GetKeyFormatString(false, "%s").c_str());

      if (msg_dest_iter->second.where) {
        of->ObjectKeyValue("where", d->where_.c_str(), " = %s");
      }
      of->ObjectKeyValue("what", GetMessageTypesAsSring(d, verbose).c_str(),
                         " = %s");
      of->ObjectEnd(msg_dest_iter->second.destination, "\n");
    }
  }

  send.ResourceEnd(resource_name_);
  send.ResourceTypeEnd("Messages");

  return true;
}

const char* GetName(ResourceItem& item, s_kw* keywords)
{
  uint32_t value = GetItemVariable<uint32_t>(item);
  for (int j = 0; keywords[j].name; j++) {
    if (keywords[j].token == value) { return keywords[j].name; }
  }
  return nullptr;
}


bool HasDefaultValue(ResourceItem& item, s_kw* keywords)
{
  bool is_default = false;
  const char* name = GetName(item, keywords);
  if (item.flags & CFG_ITEM_DEFAULT) {
    is_default = Bstrcasecmp(name, item.default_value);
  } else {
    if (name == nullptr) { is_default = true; }
  }
  return is_default;
}

static bool HasDefaultValue(ResourceItem& item)
{
  bool is_default = false;

  if (item.flags & CFG_ITEM_DEFAULT) {
    /*
     * Check for default values.
     */
    switch (item.type) {
      case CFG_TYPE_STR:
      case CFG_TYPE_DIR:
      case CFG_TYPE_DIR_OR_CMD:
      case CFG_TYPE_NAME:
      case CFG_TYPE_STRNAME:
        is_default = bstrcmp(GetItemVariable<char*>(item), item.default_value);
        break;
      case CFG_TYPE_STDSTR:
      case CFG_TYPE_STDSTRDIR:
        is_default = bstrcmp(GetItemVariable<std::string&>(item).c_str(),
                             item.default_value);
        break;
      case CFG_TYPE_LABEL:
        is_default = HasDefaultValue(item, tapelabels);
        break;
      case CFG_TYPE_INT16:
        is_default = (GetItemVariable<int16_t>(item) ==
                      (int16_t)str_to_int32(item.default_value));
        break;
      case CFG_TYPE_PINT16:
        is_default = (GetItemVariable<uint16_t>(item) ==
                      (uint16_t)str_to_int32(item.default_value));
        break;
      case CFG_TYPE_INT32:
        is_default = (GetItemVariable<int32_t>(item) ==
                      str_to_int32(item.default_value));
        break;
      case CFG_TYPE_PINT32:
        is_default = (GetItemVariable<uint32_t>(item) ==
                      (uint32_t)str_to_int32(item.default_value));
        break;
      case CFG_TYPE_INT64:
        is_default = (GetItemVariable<int64_t>(item) ==
                      str_to_int64(item.default_value));
        break;
      case CFG_TYPE_SPEED:
        is_default = (GetItemVariable<uint64_t>(item) ==
                      (uint64_t)str_to_int64(item.default_value));
        break;
      case CFG_TYPE_SIZE64:
        is_default = (GetItemVariable<uint64_t>(item) ==
                      (uint64_t)str_to_int64(item.default_value));
        break;
      case CFG_TYPE_SIZE32:
        is_default = (GetItemVariable<uint32_t>(item) ==
                      (uint32_t)str_to_int32(item.default_value));
        break;
      case CFG_TYPE_TIME:
        is_default = (GetItemVariable<uint64_t>(item) ==
                      (uint64_t)str_to_int64(item.default_value));
        break;
      case CFG_TYPE_BOOL: {
        bool default_value = Bstrcasecmp(item.default_value, "true") ||
                             Bstrcasecmp(item.default_value, "yes");

        is_default = (GetItemVariable<bool>(item) == default_value);
        break;
      }
      default:
        break;
    }
  } else {
    switch (item.type) {
      case CFG_TYPE_STR:
      case CFG_TYPE_DIR:
      case CFG_TYPE_DIR_OR_CMD:
      case CFG_TYPE_NAME:
      case CFG_TYPE_STRNAME:
        is_default = (GetItemVariable<char*>(item) == nullptr);
        break;
      case CFG_TYPE_STDSTR:
      case CFG_TYPE_STDSTRDIR:
        is_default = GetItemVariable<std::string&>(item).empty();
        break;
      case CFG_TYPE_LABEL:
        is_default = HasDefaultValue(item, tapelabels);
        break;
      case CFG_TYPE_INT16:
        is_default = (GetItemVariable<int16_t>(item) == 0);
        break;
      case CFG_TYPE_PINT16:
        is_default = (GetItemVariable<uint16_t>(item) == 0);
        break;
      case CFG_TYPE_INT32:
        is_default = (GetItemVariable<int32_t>(item) == 0);
        break;
      case CFG_TYPE_PINT32:
        is_default = (GetItemVariable<uint32_t>(item) == 0);
        break;
      case CFG_TYPE_INT64:
        is_default = (GetItemVariable<int64_t>(item) == 0);
        break;
      case CFG_TYPE_SPEED:
        is_default = (GetItemVariable<uint64_t>(item) == 0);
        break;
      case CFG_TYPE_SIZE64:
        is_default = (GetItemVariable<uint64_t>(item) == 0);
        break;
      case CFG_TYPE_SIZE32:
        is_default = (GetItemVariable<uint32_t>(item) == 0);
        break;
      case CFG_TYPE_TIME:
        is_default = (GetItemVariable<uint64_t>(item) == 0);
        break;
      case CFG_TYPE_BOOL:
        is_default = (GetItemVariable<bool>(item) == false);
        break;
      default:
        break;
    }
  }

  return is_default;
}


void BareosResource::PrintResourceItem(ResourceItem& item,
                                       const ConfigurationParser& my_config,
                                       OutputFormatterResource& send,
                                       bool hide_sensitive_data,
                                       bool inherited,
                                       bool verbose)
{
  PoolMem value;
  PoolMem temp;
  bool print_item = false;

  Dmsg3(200, "%s (inherited: %d, verbose: %d):\n", item.name, inherited,
        verbose);

  /*
   * If this is an alias for another config keyword suppress it.
   */
  if (item.flags & CFG_ITEM_ALIAS) { return; }

  if (inherited && (!verbose)) {
    /*
     * If not in verbose mode, skip inherited directives.
     */
    return;
  }

  if ((item.flags & CFG_ITEM_REQUIRED) || !my_config.omit_defaults_) {
    /*
     * Always print required items or if my_config.omit_defaults_ is false
     */
    print_item = true;
  }

  if (HasDefaultValue(item)) {
    Dmsg1(200, "%s: default value\n", item.name);

    if ((verbose) && (!(item.flags & CFG_ITEM_DEPRECATED))) {
      /*
       * If value has a expliciet default value and verbose mode is on,
       * display directive as inherited.
       */
      print_item = true;
      inherited = true;
    }
  } else {
    print_item = true;
  }

  if (!print_item) {
    Dmsg1(200, "%s: not shown\n", item.name);
    return;
  }

  switch (item.type) {
    case CFG_TYPE_STR:
    case CFG_TYPE_DIR:
    case CFG_TYPE_NAME:
    case CFG_TYPE_STRNAME: {
      char* p = GetItemVariable<char*>(item);
      send.KeyQuotedString(item.name, p, inherited);
      break;
    }
    case CFG_TYPE_DIR_OR_CMD: {
      char* p = GetItemVariable<char*>(item);
      if (p == nullptr) {
        send.KeyQuotedString(item.name, nullptr, inherited);
      } else {
        EscapeString(temp, p, strlen(p));
        send.KeyQuotedString(item.name, temp.c_str(), inherited);
      }
      break;
    }
    case CFG_TYPE_STDSTR:
    case CFG_TYPE_STDSTRDIR: {
      const std::string& p = GetItemVariable<std::string&>(item);
      send.KeyQuotedString(item.name, p, inherited);
      break;
    }
    case CFG_TYPE_MD5PASSWORD:
    case CFG_TYPE_CLEARPASSWORD:
    case CFG_TYPE_AUTOPASSWORD: {
      s_password* password = GetItemVariablePointer<s_password*>(item);

      if (password && password->value != NULL) {
        if (hide_sensitive_data) {
          Mmsg(value, "****************");
        } else {
          switch (password->encoding) {
            case p_encoding_clear:
              Dmsg2(200, "%s = \"%s\"\n", item.name, password->value);
              Mmsg(value, "%s", password->value);
              break;
            case p_encoding_md5:
              Dmsg2(200, "%s = \"[md5]%s\"\n", item.name, password->value);
              Mmsg(value, "[md5]%s", password->value);
              break;
            default:
              break;
          }
        }
        send.KeyQuotedString(item.name, value.c_str(), inherited);
      }
      break;
    }
    case CFG_TYPE_LABEL:
      send.KeyQuotedString(item.name, GetName(item, tapelabels), inherited);
      break;
    case CFG_TYPE_INT16:
      send.KeySignedInt(item.name, GetItemVariable<int16_t>(item), inherited);
      break;
    case CFG_TYPE_PINT16:
      send.KeyUnsignedInt(item.name, GetItemVariable<uint16_t>(item),
                          inherited);
      break;
    case CFG_TYPE_INT32:
      send.KeySignedInt(item.name, GetItemVariable<int32_t>(item), inherited);
      break;
    case CFG_TYPE_PINT32:
      send.KeyUnsignedInt(item.name, GetItemVariable<uint32_t>(item),
                          inherited);
      break;
    case CFG_TYPE_INT64:
      send.KeySignedInt(item.name, GetItemVariable<int64_t>(item), inherited);
      break;
    case CFG_TYPE_SPEED:
      send.KeyUnsignedInt(item.name, GetItemVariable<uint64_t>(item),
                          inherited);
      break;
    case CFG_TYPE_SIZE64: {
      const std::string& value = Print64BitConfigNumberSiPrefixFormat(&item);
      send.KeyString(item.name, value, inherited);
      break;
    }
    case CFG_TYPE_SIZE32: {
      const std::string& value = Print32BitConfigNumberSiPrefixFormat(&item);
      send.KeyString(item.name, value, inherited);
      break;
    }
    case CFG_TYPE_TIME: {
      const std::string& value = PrintConfigTime(&item);
      send.KeyString(item.name, value, inherited);
      break;
    }
    case CFG_TYPE_BOOL: {
      send.KeyBool(item.name, GetItemVariable<bool>(item), inherited);
      break;
    }
    case CFG_TYPE_STR_VECTOR:
    case CFG_TYPE_STR_VECTOR_OF_DIRS: {
      /*
       * One line for each member of the list
       */
      const std::vector<std::string>& list =
          GetItemVariable<std::vector<std::string>&>(item);
      send.KeyMultipleStringsOnePerLine(item.name, list, inherited);
      break;
    }
    case CFG_TYPE_ALIST_STR:
    case CFG_TYPE_ALIST_DIR:
    case CFG_TYPE_PLUGIN_NAMES: {
      /*
       * One line for each member of the list
       */
      send.KeyMultipleStringsOnePerLine(
          item.name, GetItemVariable<alist*>(item), inherited);
      break;
    }
    case CFG_TYPE_ALIST_RES: {
      /*
       * Each member of the list is comma-separated
       */
      send.KeyMultipleStringsOnePerLine(item.name,
                                        GetItemVariable<alist*>(item),
                                        GetResourceName, inherited, true);
      break;
    }
    case CFG_TYPE_RES: {
      BareosResource* res;

      res = GetItemVariable<BareosResource*>(item);
      if (res != NULL && res->resource_name_ != NULL) {
        send.KeyQuotedString(item.name, res->resource_name_, inherited);
      } else {
        send.KeyQuotedString(item.name, "", inherited);
      }
      break;
    }
    case CFG_TYPE_BIT: {
      send.KeyBool(item.name,
                   BitIsSet(item.code, GetItemVariablePointer<char*>(item)),
                   inherited);
      break;
    }
    case CFG_TYPE_MSGS:
      /*
       * We ignore these items as they are printed in a special way in
       * MessagesResource::PrintConfig()
       */
      break;
    case CFG_TYPE_ADDRESSES: {
      dlist* addrs = GetItemVariable<dlist*>(item);
      IPADDR* adr;
      send.ArrayStart(item.name, inherited, "%s = {\n");
      foreach_dlist (adr, addrs) {
        send.SubResourceStart(NULL, inherited, "");
        adr->BuildConfigString(send, inherited);
        send.SubResourceEnd(NULL, inherited, "");
      }
      send.ArrayEnd(item.name, inherited, "}\n");
      break;
    }
    case CFG_TYPE_ADDRESSES_PORT:
      /*
       * Is stored in CFG_TYPE_ADDRESSES and printed there.
       */
      break;
    case CFG_TYPE_ADDRESSES_ADDRESS:
      /*
       * Is stored in CFG_TYPE_ADDRESSES and printed there.
       */
      break;
    default:
      /*
       * This is a non-generic type call back to the daemon to get things
       * printed.
       */
      if (my_config.print_res_) {
        my_config.print_res_(item, send, hide_sensitive_data, inherited,
                             verbose);
      }
      break;
  }
}


bool BareosResource::PrintConfig(OutputFormatterResource& send,
                                 const ConfigurationParser& my_config,
                                 bool hide_sensitive_data,
                                 bool verbose)
{
  ResourceItem* items;
  int rindex;

  /*
   * If entry is not used, then there is nothing to print.
   */
  if (rcode_ < (uint32_t)my_config.r_first_ || refcnt_ <= 0) { return true; }

  rindex = rcode_ - my_config.r_first_;

  /*
   * don't dump internal resources.
   */
  if ((internal_) && (!verbose)) { return true; }
  /*
   * Make sure the resource class has any items.
   */
  if (!my_config.resources_[rindex].items) { return true; }
  items = my_config.resources_[rindex].items;

  *my_config.resources_[rindex].allocated_resource_ = this;

  send.ResourceTypeStart(my_config.ResGroupToStr(rcode_), internal_);
  send.ResourceStart(resource_name_);

  for (int i = 0; items[i].name; i++) {
    bool inherited = BitIsSet(i, inherit_content_);
    if (internal_) { inherited = true; }
    PrintResourceItem(items[i], my_config, send, hide_sensitive_data, inherited,
                      verbose);
  }

  send.ResourceEnd(resource_name_);
  send.ResourceTypeEnd(my_config.ResToStr(rcode_), internal_);

  return true;
}


#ifdef HAVE_JANSSON
/*
 * resource item schema description in JSON format.
 * Example output:
 *
 *   "filesetacl": {
 *     "datatype": "BOOLEAN",
 *     "datatype_number": 51,
 *     "code": int
 *     [ "defaultvalue": "xyz", ]
 *     [ "required": true, ]
 *     [ "alias": true, ]
 *     [ "deprecated": true, ]
 *     [ "equals": true, ]
 *     ...
 *     "type": "ResourceItem"
 *   }
 */
json_t* json_item(ResourceItem* item)
{
  json_t* json = json_object();

  json_object_set_new(json, "datatype",
                      json_string(DatatypeToString(item->type)));
  json_object_set_new(json, "code", json_integer(item->code));

  if (item->flags & CFG_ITEM_ALIAS) {
    json_object_set_new(json, "alias", json_true());
  }
  if (item->flags & CFG_ITEM_DEFAULT) {
    /* FIXME? would it be better to convert it to the right type before
     * returning? */
    json_object_set_new(json, "default_value",
                        json_string(item->default_value));
  }
  if (item->flags & CFG_ITEM_PLATFORM_SPECIFIC) {
    json_object_set_new(json, "platform_specific", json_true());
  }
  if (item->flags & CFG_ITEM_DEPRECATED) {
    json_object_set_new(json, "deprecated", json_true());
  }
  if (item->flags & CFG_ITEM_NO_EQUALS) {
    json_object_set_new(json, "equals", json_false());
  } else {
    json_object_set_new(json, "equals", json_true());
  }
  if (item->flags & CFG_ITEM_REQUIRED) {
    json_object_set_new(json, "required", json_true());
  }
  if (item->versions) {
    json_object_set_new(json, "versions", json_string(item->versions));
  }
  if (item->description) {
    json_object_set_new(json, "description", json_string(item->description));
  }

  return json;
}

json_t* json_item(s_kw* item)
{
  json_t* json = json_object();

  json_object_set_new(json, "token", json_integer(item->token));

  return json;
}

json_t* json_items(ResourceItem items[])
{
  json_t* json = json_object();

  if (items) {
    for (int i = 0; items[i].name; i++) {
      json_object_set_new(json, items[i].name, json_item(&items[i]));
    }
  }

  return json;
}
#endif

static DatatypeName datatype_names[] = {
    /*
     * Standard resource types. handlers in res.c
     */
    {CFG_TYPE_STR, "STRING", "String"},
    {CFG_TYPE_DIR, "DIRECTORY", "directory"},
    {CFG_TYPE_STDSTR, "STRING", "String"},
    {CFG_TYPE_STDSTRDIR, "DIRECTORY", "directory"},
    {CFG_TYPE_MD5PASSWORD, "MD5PASSWORD", "Password in MD5 format"},
    {CFG_TYPE_CLEARPASSWORD, "CLEARPASSWORD", "Password as cleartext"},
    {CFG_TYPE_AUTOPASSWORD, "AUTOPASSWORD",
     "Password stored in clear when needed otherwise hashed"},
    {CFG_TYPE_NAME, "NAME", "Name"},
    {CFG_TYPE_STRNAME, "STRNAME", "String name"},
    {CFG_TYPE_RES, "RES", "Resource"},
    {CFG_TYPE_ALIST_RES, "RESOURCE_LIST", "Resource list"},
    {CFG_TYPE_ALIST_STR, "STRING_LIST", "string list"},
    {CFG_TYPE_ALIST_DIR, "DIRECTORY_LIST", "directory list"},
    {CFG_TYPE_INT16, "INT16", "Integer 16 bits"},
    {CFG_TYPE_PINT16, "PINT16", "Positive 16 bits Integer (unsigned)"},
    {CFG_TYPE_INT32, "INT32", "Integer 32 bits"},
    {CFG_TYPE_PINT32, "PINT32", "Positive 32 bits Integer (unsigned)"},
    {CFG_TYPE_MSGS, "MESSAGES", "Message resource"},
    {CFG_TYPE_INT64, "INT64", "Integer 64 bits"},
    {CFG_TYPE_BIT, "BIT", "Bitfield"},
    {CFG_TYPE_BOOL, "BOOLEAN", "boolean"},
    {CFG_TYPE_TIME, "TIME", "time"},
    {CFG_TYPE_SIZE64, "SIZE64", "64 bits file size"},
    {CFG_TYPE_SIZE32, "SIZE32", "32 bits file size"},
    {CFG_TYPE_SPEED, "SPEED", "speed"},
    {CFG_TYPE_DEFS, "DEFS", "definition"},
    {CFG_TYPE_LABEL, "LABEL", "label"},
    {CFG_TYPE_ADDRESSES, "ADDRESSES", "ip addresses list"},
    {CFG_TYPE_ADDRESSES_ADDRESS, "ADDRESS", "ip address"},
    {CFG_TYPE_ADDRESSES_PORT, "PORT", "network port"},
    {CFG_TYPE_PLUGIN_NAMES, "PLUGIN_NAMES", "Plugin Name(s)"},
    {CFG_TYPE_STR_VECTOR, "STRING_LIST", "string list"},
    {CFG_TYPE_STR_VECTOR_OF_DIRS, "DIRECTORY_LIST", "directory list"},
    {CFG_TYPE_DIR_OR_CMD, "DIRECTORY_OR_COMMAND", "Directory or command"},

    /*
     * Director resource types. handlers in dird_conf.
     */
    {CFG_TYPE_ACL, "ACL", "User Access Control List"},
    {CFG_TYPE_AUDIT, "AUDIT_COMMAND_LIST", "Auditing Command List"},
    {CFG_TYPE_AUTHPROTOCOLTYPE, "AUTH_PROTOCOL_TYPE",
     "Authentication Protocol"},
    {CFG_TYPE_AUTHTYPE, "AUTH_TYPE", "Authentication Type"},
    {CFG_TYPE_DEVICE, "DEVICE", "Device resource"},
    {CFG_TYPE_JOBTYPE, "JOB_TYPE", "Type of Job"},
    {CFG_TYPE_PROTOCOLTYPE, "PROTOCOL_TYPE", "Protocol"},
    {CFG_TYPE_LEVEL, "BACKUP_LEVEL", "Backup Level"},
    {CFG_TYPE_REPLACE, "REPLACE_OPTION", "Replace option"},
    {CFG_TYPE_SHRTRUNSCRIPT, "RUNSCRIPT_SHORT", "Short Runscript definition"},
    {CFG_TYPE_RUNSCRIPT, "RUNSCRIPT", "Runscript"},
    {CFG_TYPE_RUNSCRIPT_CMD, "RUNSCRIPT_COMMAND", "Runscript Command"},
    {CFG_TYPE_RUNSCRIPT_TARGET, "RUNSCRIPT_TARGET", "Runscript Target (Host)"},
    {CFG_TYPE_RUNSCRIPT_BOOL, "RUNSCRIPT_BOOLEAN", "Runscript Boolean"},
    {CFG_TYPE_RUNSCRIPT_WHEN, "RUNSCRIPT_WHEN", "Runscript When expression"},
    {CFG_TYPE_MIGTYPE, "MIGRATION_TYPE", "Migration Type"},
    {CFG_TYPE_INCEXC, "INCLUDE_EXCLUDE_ITEM", "Include/Exclude item"},
    {CFG_TYPE_RUN, "SCHEDULE_RUN_COMMAND", "Schedule Run Command"},
    {CFG_TYPE_ACTIONONPURGE, "ACTION_ON_PURGE", "Action to perform on Purge"},
    {CFG_TYPE_POOLTYPE, "POOLTYPE", "Pool Type"},

    /*
     * Director fileset options. handlers in dird_conf.
     */
    {CFG_TYPE_FNAME, "FILENAME", "Filename"},
    {CFG_TYPE_PLUGINNAME, "PLUGIN_NAME", "Pluginname"},
    {CFG_TYPE_EXCLUDEDIR, "EXCLUDE_DIRECTORY", "Exclude directory"},
    {CFG_TYPE_OPTIONS, "OPTIONS", "Options block"},
    {CFG_TYPE_OPTION, "OPTION", "Option of Options block"},
    {CFG_TYPE_REGEX, "REGEX", "Regular Expression"},
    {CFG_TYPE_BASE, "BASEJOB", "Basejob Expression"},
    {CFG_TYPE_WILD, "WILDCARD", "Wildcard Expression"},
    {CFG_TYPE_PLUGIN, "PLUGIN", "Plugin definition"},
    {CFG_TYPE_FSTYPE, "FILESYSTEM_TYPE", "FileSystem match criterium (UNIX)"},
    {CFG_TYPE_DRIVETYPE, "DRIVE_TYPE", "DriveType match criterium (Windows)"},
    {CFG_TYPE_META, "META_TAG", "Meta tag"},

    /*
     * Storage daemon resource types
     */
    {CFG_TYPE_DEVTYPE, "DEVICE_TYPE", "Device Type"},
    {CFG_TYPE_MAXBLOCKSIZE, "MAX_BLOCKSIZE", "Maximum Blocksize"},
    {CFG_TYPE_IODIRECTION, "IO_DIRECTION", "IO Direction"},
    {CFG_TYPE_CMPRSALGO, "COMPRESSION_ALGORITHM", "Compression Algorithm"},

    /*
     * File daemon resource types
     */
    {CFG_TYPE_CIPHER, "ENCRYPTION_CIPHER", "Encryption Cipher"},

    {0, NULL, NULL}};

DatatypeName* GetDatatype(int number)
{
  int size = sizeof(datatype_names) / sizeof(datatype_names[0]);

  if (number >= size) {
    /*
     * Last entry of array is a dummy entry
     */
    number = size - 1;
  }

  return &(datatype_names[number]);
}

const char* DatatypeToString(int type)
{
  for (int i = 0; datatype_names[i].name; i++) {
    if (datatype_names[i].number == type) { return datatype_names[i].name; }
  }

  return "unknown";
}

const char* DatatypeToDescription(int type)
{
  for (int i = 0; datatype_names[i].name; i++) {
    if (datatype_names[i].number == type) {
      return datatype_names[i].description;
    }
  }

  return NULL;
}
