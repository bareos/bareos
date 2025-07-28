/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2002-2012 Free Software Foundation Europe e.V.
   Copyright (C) 2011-2012 Planets Communications B.V.
   Copyright (C) 2013-2025 Bareos GmbH & Co. KG

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
 * Parse Bootstrap Records (used for restores)
 *
 * Kern Sibbald, June MMII
 */

#include "include/bareos.h"
#include "jcr.h"
#include "stored/bsr.h"
#include "lib/berrno.h"
#include "lib/parse_bsr.h"
#include "lib/lex.h"

namespace libbareos {


struct bsr_parser {
  storagedaemon::BootStrapRecord* bsr;

  storagedaemon::bsr::volume prototype;
  std::vector<std::string> volume_names;

  void end_volume_group()
  {
    for (std::size_t i = 0; i < volume_names.size(); ++i) {
      auto& vol = bsr->volumes.emplace_back();
      if (i == volume_names.size() - 1) {
        // we have no need for the prototype anymore, so just
        // move it in the last iteration to save a bunch of copies
        vol = std::move(prototype);
      } else {
        // TODO: fix this
        // vol = prototype;
      }
      vol.volume_name = std::move(volume_names[i]);
    }

    prototype = {};
    volume_names.clear();
  }

  void begin_volume_group()
  {
    end_volume_group();
    prototype.root = bsr;
  }

  void push_volume(std::string_view name) { volume_names.emplace_back(name); }

  void push_media_type(std::string_view type)
  {
    if (!prototype.media_type) { prototype.media_type.emplace(); }
    prototype.media_type->assign(type);
  }

  void push_device(std::string_view device)
  {
    if (!prototype.device) { prototype.device.emplace(); }
    prototype.device->assign(device);
  }

  void push_storage(std::string_view storage)
  {
    // maybe this should be used as group separator, instead of volumes ...
    (void)storage;  // is ignored
  }


  void push_count(std::uint64_t count)
  {
    // this isnt really correct
    // when i say
    //  Volume = A | B | C
    //  Count = 5
    // then i mean 5 files _in total_, not 5 files each
    // but this isnt realy something that bareos uses anyways
    prototype.count = count;
  }

  void push_file_regex(std::string regex_str, std::unique_ptr<regex_t> regex_re)
  {
    prototype.fileregex_re = std::move(regex_re);
    prototype.fileregex = std::move(regex_str);
  }

  void push_slot(std::int32_t slot) { prototype.slot = slot; }

  void push_client(std::string_view client)
  {
    prototype.clients.emplace_back(client);
  }

  void push_job(std::string_view job) { prototype.jobs.emplace_back(job); }

  void push_file_indices(std::size_t start, std::size_t end)
  {
    prototype.file_indices.emplace_back(start, end);
  }

  void push_job_ids(std::size_t start, std::size_t end)
  {
    prototype.job_ids.emplace_back(start, end);
  }

  void push_vol_files(std::size_t start, std::size_t end)
  {
    prototype.files.emplace_back(start, end);
  }

  void push_blocks(std::size_t start, std::size_t end)
  {
    prototype.blocks.emplace_back(start, end);
  }

  void push_vol_addrs(std::size_t start, std::size_t end)
  {
    prototype.addresses.emplace_back(start, end);
  }

  void push_session_ids(std::size_t start, std::size_t end)
  {
    prototype.session_ids.emplace_back(start, end);
  }

  void push_session_time(std::size_t time)
  {
    prototype.session_times.emplace_back(time);
  }

  void push_stream(std::int32_t stream)
  {
    prototype.streams.emplace_back(stream);
  }

  bool started() const { return !volume_names.empty(); }
};

#define ITEM_HANDLER(Name) bool(Name)(lexer * lc, bsr_parser & parser);

using item_handler = ITEM_HANDLER(*);

static ITEM_HANDLER(store_vol);

static ITEM_HANDLER(store_mediatype);
static ITEM_HANDLER(StoreDevice);
static ITEM_HANDLER(store_client);
static ITEM_HANDLER(store_job);
static ITEM_HANDLER(store_jobid);
static ITEM_HANDLER(store_count);
static ITEM_HANDLER(StoreJobtype);
static ITEM_HANDLER(store_joblevel);
static ITEM_HANDLER(store_findex);
static ITEM_HANDLER(store_sessid);
static ITEM_HANDLER(store_volfile);
static ITEM_HANDLER(store_volblock);
static ITEM_HANDLER(store_voladdr);
static ITEM_HANDLER(store_sesstime);
static ITEM_HANDLER(store_include);
static ITEM_HANDLER(store_exclude);
static ITEM_HANDLER(store_stream);
static ITEM_HANDLER(store_slot);
static ITEM_HANDLER(store_fileregex);
static ITEM_HANDLER(store_storage);

struct kw_items {
  const char* name;
  item_handler handler;
};

// List of all keywords permitted in bsr files and their handlers
struct kw_items items[] = {{"volume", store_vol},
                           {"mediatype", store_mediatype},
                           {"client", store_client},
                           {"job", store_job},
                           {"jobid", store_jobid},
                           {"count", store_count},
                           {"fileindex", store_findex},
                           {"jobtype", StoreJobtype},
                           {"joblevel", store_joblevel},
                           {"volsessionid", store_sessid},
                           {"volsessiontime", store_sesstime},
                           {"include", store_include},
                           {"exclude", store_exclude},
                           {"volfile", store_volfile},
                           {"volblock", store_volblock},
                           {"voladdr", store_voladdr},
                           {"stream", store_stream},
                           {"slot", store_slot},
                           {"device", StoreDevice},
                           {"fileregex", store_fileregex},
                           {"storage", store_storage},
                           {NULL, NULL}};

// Format a scanner error message
PRINTF_LIKE(4, 5)
static void s_err(const char* file, int line, lexer* lc, const char* msg, ...)
{
  va_list ap;
  PoolMem buf(PM_NAME);
  JobControlRecord* jcr = (JobControlRecord*)(lc->caller_ctx);

  va_start(ap, msg);
  buf.Bvsprintf(msg, ap);
  va_end(ap);

  if (jcr) {
    Jmsg(jcr, M_FATAL, 0,
         T_("Bootstrap file error: %s\n"
            "            : Line %d, col %d of file %s\n%s\n"),
         buf.c_str(), lc->line_no, lc->col_no, lc->fname, lc->line);
  } else {
    e_msg(file, line, M_FATAL, 0,
          T_("Bootstrap file error: %s\n"
             "            : Line %d, col %d of file %s\n%s\n"),
          buf.c_str(), lc->line_no, lc->col_no, lc->fname, lc->line);
  }
}

// Format a scanner warning message
PRINTF_LIKE(4, 5)
static void s_warn(const char* file, int line, lexer* lc, const char* msg, ...)
{
  va_list ap;
  PoolMem buf(PM_NAME);
  JobControlRecord* jcr = (JobControlRecord*)(lc->caller_ctx);

  va_start(ap, msg);
  buf.Bvsprintf(msg, ap);
  va_end(ap);

  if (jcr) {
    Jmsg(jcr, M_WARNING, 0,
         T_("Bootstrap file warning: %s\n"
            "            : Line %d, col %d of file %s\n%s\n"),
         buf.c_str(), lc->line_no, lc->col_no, lc->fname, lc->line);
  } else {
    p_msg(file, line, 0,
          T_("Bootstrap file warning: %s\n"
             "            : Line %d, col %d of file %s\n%s\n"),
          buf.c_str(), lc->line_no, lc->col_no, lc->fname, lc->line);
  }
}

static inline bool IsFastRejectionOk(storagedaemon::BootStrapRecord* bsr)
{
  /* Although, this can be optimized, for the moment, require
   *  all bsrs to have both sesstime and sessid set before
   *  we do fast rejection. */

  for (auto& volume : bsr->volumes) {
    if (volume.session_times.empty() || volume.session_ids.empty()) {
      return false;
    }
  }

  return true;
}

static inline bool IsPositioningOk(storagedaemon::BootStrapRecord* bsr)
{
  /* Every bsr should have a volfile entry and a volblock entry
   * or a VolAddr
   *   if we are going to use positioning */
  for (auto& volume : bsr->volumes) {
    if ((volume.files.empty() || volume.blocks.empty())
        && volume.addresses.empty()) {
      return false;
    }
  }
  return true;
}

// Parse Bootstrap file
storagedaemon::BootStrapRecord* parse_bsr(JobControlRecord* jcr, char* fname)
{
  lexer* lc = NULL;
  storagedaemon::BootStrapRecord* bsr = new storagedaemon::BootStrapRecord;

  bsr_parser parser{};
  parser.bsr = bsr;

  Dmsg1(300, "Enter parse_bsf %s\n", fname);
  if ((lc = lex_open_file(lc, fname, s_err, s_warn)) == NULL) {
    BErrNo be;
    Emsg2(M_ERROR_TERM, 0, T_("Cannot open bootstrap file %s: %s\n"), fname,
          be.bstrerror());
  }
  lc->caller_ctx = (void*)jcr;

  bool error = false;

  while (!error) {
    int token = LexGetToken(lc, BCT_ALL);
    if (token == BCT_EOF) { break; }

    Dmsg1(300, "parse got token=%s\n", lex_tok_to_str(token));
    if (token == BCT_EOL) { continue; }

    int i;
    for (i = 0; items[i].name; i++) {
      if (Bstrcasecmp(items[i].name, lc->str)) {
        token = LexGetToken(lc, BCT_ALL);
        Dmsg1(300, "in BCT_IDENT got token=%s\n", lex_tok_to_str(token));
        if (token != BCT_EQUALS) {
          scan_err1(lc, "expected an equals, got: %s", lc->str);
          error = true;
          break;
        }
        Dmsg1(300, "calling handler for %s\n", items[i].name);
        // Call item handler
        if (!items[i].handler(lc, parser)) { error = true; }
        i = -1;
        break;
      }
    }
    if (i >= 0) {
      Dmsg1(300, "Keyword = %s\n", lc->str);
      scan_err1(lc, "Keyword %s not found", lc->str);
      error = true;
      break;
    }
  }
  parser.end_volume_group();

  lc = LexCloseFile(lc);
  Dmsg0(300, "Leave parse_bsf()\n");

  if (error) {
    FreeBsr(bsr);
    bsr = NULL;
  }

  if (bsr) {
    bsr->use_fast_rejection = IsFastRejectionOk(bsr);
    bsr->use_positioning = IsPositioningOk(bsr);
  }

  return bsr;
}

static bool store_vol(lexer* lc, bsr_parser& parser)
{
  char *p, *n;

  storagedaemon::bsr::volume volume = {};

  int token = LexGetToken(lc, BCT_STRING);
  if (token == BCT_ERROR) { return false; }

  /* This may actually be more than one volume separated by a |
   * If so, separate them.
   */

  parser.begin_volume_group();
  for (p = lc->str; p && *p;) {
    n = strchr(p, '|');
    if (n) { *n++ = 0; }
    parser.push_volume(p);

    p = n;
  }

  return true;
}

// Shove the MediaType in each Volume in the current bsr
static bool store_mediatype(lexer* lc, bsr_parser& parser)
{
  int token;

  token = LexGetToken(lc, BCT_STRING);
  if (token == BCT_ERROR) { return false; }
  if (!parser.started()) {
    Emsg1(M_ERROR, 0, T_("MediaType %s in bsr at inappropriate place.\n"),
          lc->str);
    return false;
  }

  parser.push_media_type(lc->str);

  return true;
}

// Shove the Device name in each Volume in the current bsr
static bool StoreDevice(lexer* lc, bsr_parser& parser)
{
  int token;

  token = LexGetToken(lc, BCT_STRING);
  if (token == BCT_ERROR) { return false; }
  if (!parser.started()) {
    Emsg1(M_ERROR, 0, T_("Device \"%s\" in bsr at inappropriate place.\n"),
          lc->str);
    return false;
  }

  parser.push_device(lc->str);
  return true;
}

static bool store_storage(lexer* lc, bsr_parser& parser)
{
  int token = LexGetToken(lc, BCT_STRING);

  if (token == BCT_ERROR) { return false; }
  parser.push_storage(lc->str);

  return true;
}

static bool store_client(lexer* lc, bsr_parser& parser)
{
  if (!parser.started()) { return false; }

  for (;;) {
    int token = LexGetToken(lc, BCT_NAME);
    if (token == BCT_ERROR) { return false; }

    parser.push_client(lc->str);

    token = LexGetToken(lc, BCT_ALL);
    if (token != BCT_COMMA) { break; }
  }
  return true;
}

static bool store_job(lexer* lc, bsr_parser& parser)
{
  if (!parser.started()) { return false; }

  for (;;) {
    int token = LexGetToken(lc, BCT_NAME);
    if (token == BCT_ERROR) { return false; }

    parser.push_job(lc->str);

    token = LexGetToken(lc, BCT_ALL);
    if (token != BCT_COMMA) { break; }
  }
  return true;
}

static bool store_findex(lexer* lc, bsr_parser& parser)
{
  if (!parser.started()) { return false; }

  for (;;) {
    int token = LexGetToken(lc, BCT_PINT32_RANGE);
    if (token == BCT_ERROR) { return false; }

    parser.push_file_indices(lc->u.pint32_val, lc->u2.pint32_val);

    token = LexGetToken(lc, BCT_ALL);
    if (token != BCT_COMMA) { break; }
  }
  return true;
}

static bool store_jobid(lexer* lc, bsr_parser& parser)
{
  if (!parser.started()) { return false; }

  for (;;) {
    int token = LexGetToken(lc, BCT_PINT32_RANGE);
    if (token == BCT_ERROR) { return false; }

    parser.push_job_ids(lc->u.pint32_val, lc->u2.pint32_val);

    token = LexGetToken(lc, BCT_ALL);
    if (token != BCT_COMMA) { break; }
  }
  return true;
}

static bool store_count(lexer* lc, bsr_parser& parser)
{
  if (!parser.started()) { return false; }

  int token = LexGetToken(lc, BCT_PINT32);
  if (token == BCT_ERROR) { return false; }
  parser.push_count(lc->u.pint32_val);
  ScanToEol(lc);
  return true;
}

static bool store_fileregex(lexer* lc, bsr_parser& parser)
{
  if (!parser.started()) { return false; }

  int token = LexGetToken(lc, BCT_STRING);
  if (token == BCT_ERROR) { return false; }

  std::string regex_str{lc->str};

  auto regex_ptr = std::make_unique<regex_t>();

  int rc
      = regcomp(regex_ptr.get(), regex_str.c_str(), REG_EXTENDED | REG_NOSUB);
  if (rc != 0) {
    char prbuf[500];
    regerror(rc, regex_ptr.get(), prbuf, sizeof(prbuf));
    Emsg2(M_ERROR, 0, T_("REGEX '%s' compile error. ERR=%s\n"),
          regex_str.c_str(), prbuf);
    return false;
  }

  parser.push_file_regex(std::move(regex_str), std::move(regex_ptr));

  return true;
}

static bool StoreJobtype(lexer*, bsr_parser& parser)
{
  (void)parser;
  /* *****FIXME****** */
  Pmsg0(-1, T_("JobType not yet implemented\n"));
  return false;
}

static bool store_joblevel(lexer*, bsr_parser& parser)
{
  (void)parser;
  /* *****FIXME****** */
  Pmsg0(-1, T_("JobLevel not yet implemented\n"));
  return false;
}

// Routine to handle Volume start/end file
static bool store_volfile(lexer* lc, bsr_parser& parser)
{
  if (!parser.started()) { return false; }

  for (;;) {
    int token = LexGetToken(lc, BCT_PINT32_RANGE);
    if (token == BCT_ERROR) { return false; }

    parser.push_vol_files(lc->u.pint32_val, lc->u2.pint32_val);

    token = LexGetToken(lc, BCT_ALL);
    if (token != BCT_COMMA) { break; }
  }
  return true;
}

// Routine to handle Volume start/end Block
static bool store_volblock(lexer* lc, bsr_parser& parser)
{
  if (!parser.started()) { return false; }

  for (;;) {
    int token = LexGetToken(lc, BCT_PINT32_RANGE);
    if (token == BCT_ERROR) { return false; }

    parser.push_blocks(lc->u.pint32_val, lc->u2.pint32_val);

    token = LexGetToken(lc, BCT_ALL);
    if (token != BCT_COMMA) { break; }
  }
  return true;
}

// Routine to handle Volume start/end address
static bool store_voladdr(lexer* lc, bsr_parser& parser)
{
  if (!parser.started()) { return false; }

  for (;;) {
    int token = LexGetToken(lc, BCT_PINT32_RANGE);
    if (token == BCT_ERROR) { return false; }

    parser.push_vol_addrs(lc->u.pint32_val, lc->u2.pint32_val);

    token = LexGetToken(lc, BCT_ALL);
    if (token != BCT_COMMA) { break; }
  }
  return true;
}

static bool store_sessid(lexer* lc, bsr_parser& parser)
{
  if (!parser.started()) { return false; }

  for (;;) {
    int token = LexGetToken(lc, BCT_PINT32_RANGE);
    if (token == BCT_ERROR) { return false; }

    parser.push_session_ids(lc->u.pint32_val, lc->u2.pint32_val);

    token = LexGetToken(lc, BCT_ALL);
    if (token != BCT_COMMA) { break; }
  }
  return true;
}

static bool store_sesstime(lexer* lc, bsr_parser& parser)
{
  if (!parser.started()) { return false; }

  for (;;) {
    int token = LexGetToken(lc, BCT_PINT32);
    if (token == BCT_ERROR) { return false; }

    parser.push_session_time(lc->u.pint32_val);

    token = LexGetToken(lc, BCT_ALL);
    if (token != BCT_COMMA) { break; }
  }
  return true;
}

static bool store_stream(lexer* lc, bsr_parser& parser)
{
  if (!parser.started()) { return false; }

  for (;;) {
    int token = LexGetToken(lc, BCT_INT32);
    if (token == BCT_ERROR) { return false; }

    parser.push_stream(lc->u.int32_val);

    token = LexGetToken(lc, BCT_ALL);
    if (token != BCT_COMMA) { break; }
  }
  return true;
}

static bool store_slot(lexer* lc, bsr_parser& parser)
{
  int token = LexGetToken(lc, BCT_PINT32);
  if (token == BCT_ERROR) { return false; }
  if (!parser.started()) {
    Emsg1(M_ERROR, 0, T_("Slot %d in bsr at inappropriate place.\n"),
          lc->u.pint32_val);
    return false;
  }
  parser.push_slot(lc->u.pint32_val);
  ScanToEol(lc);
  return true;
}

static bool store_include(lexer* lc, bsr_parser& parser)
{
  (void)parser;
  ScanToEol(lc);
  return false;
}

static bool store_exclude(lexer* lc, bsr_parser& parser)
{
  (void)parser;
  ScanToEol(lc);
  return false;
}

void DumpBsr(storagedaemon::BootStrapRecord* bsr)
{
  int save_debug = debug_level;
  debug_level = 1;
  if (!bsr) {
    Pmsg0(-1, T_("storagedaemon::BootStrapRecord is NULL\n"));
    debug_level = save_debug;
    return;
  }

  static auto print_interval = +[](const char* name, bool short_print,
                                   storagedaemon::bsr::interval iv) {
    if (short_print && (iv.start == iv.end)) {
      Pmsg2(-1, T_("%s     : %" PRIu64 "\n"), name, iv.start);
    } else {
      Pmsg2(-1, T_("%s     : %" PRIu64 "-%" PRIu64 "\n"), name, iv.start,
            iv.end);
    }
  };

  static auto print_intervals
      = [](const char* name, bool short_print,
           const std::vector<storagedaemon::bsr::interval>& ivs) {
          for (auto& iv : ivs) { print_interval(name, short_print, iv); }
        };

  for (auto& volume : bsr->volumes) {
    Pmsg1(-1, T_("VolumeName  : %s\n"), volume.volume_name.c_str());
    Pmsg1(-1, T_("  MediaType : %s\n"),
          volume.media_type ? volume.media_type->c_str() : "(unset)");
    Pmsg1(-1, T_("  Device    : %s\n"),
          volume.device ? volume.device->c_str() : "(unset)");
    Pmsg1(-1, T_("  Slot      : %d\n"), volume.slot ? volume.slot.value() : -1);

    print_intervals("SessId", true, volume.session_ids);

    for (auto& sesstime : volume.session_times) {
      Pmsg1(-1, T_("SessTime    : %u\n"), sesstime);
    }

    print_intervals("VolFile", false, volume.files);
    print_intervals("VolBlock", false, volume.blocks);
    print_intervals("VolAddr", false, volume.addresses);

    for (auto& client : volume.clients) {
      Pmsg1(-1, T_("Client      : %s\n"), client.c_str());
    }

    print_intervals("JobId", true, volume.job_ids);

    for (auto& job : volume.jobs) {
      Pmsg1(-1, T_("Job          : %s\n"), job.c_str());
    }

    print_intervals("FileIndex", true, volume.file_indices);

    Pmsg1(-1, T_("count       : %" PRIu64 "\n"), volume.count);
    Pmsg1(-1, T_("found       : %" PRIu64 "\n"), volume.found);
    Pmsg1(-1, T_("done        : %s\n"), volume.done ? T_("yes") : T_("no"));
  }

  Pmsg1(-1, T_("positioning : %d\n"), bsr->use_positioning);
  Pmsg1(-1, T_("fast_reject : %d\n"), bsr->use_fast_rejection);

  debug_level = save_debug;
}

// Free all bsrs in chain
void FreeBsr(storagedaemon::BootStrapRecord* bsr)
{
  if (bsr->attr) { FreeAttr(bsr->attr); }
  delete bsr;
}

} /* namespace libbareos */
