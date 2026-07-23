/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2025-2026 Bareos GmbH & Co. KG

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

#include <variant>
#include "include/job_types.h"
#include "lib/util.h"
#if !defined(PLUGIN_NAME)
#  error PLUGIN_NAME not set
#endif

#include "include/bareos.h"
#include "filed/fd_plugins.h"
#include "bareos_api.h"
#include "include/filetypes.h"
#include "include/job_level.h"
#include "parser.h"
#include "dump.h"
#include "plugin.h"
#include "restore.h"
#include "lib/bool_string.h"
#include <comdef.h>
#include <time.h>

#include "util.h"

#include <charconv>
#include <memory>

#include <tl/expected.hpp>

#if defined(MSVC_JOINED_THE_MODERN_WORLD)
#  define warn_msg(ctx, fmt, ...) \
    JobLog((ctx), M_WARN, (fmt)__VA_OPT__(, ) __VA_ARGS__)
#  define err_msg(ctx, fmt, ...) \
    JobLog((ctx), M_ERROR, (fmt)__VA_OPT__(, ) __VA_ARGS__)
#  define fatal_msg(ctx, fmt, ...) \
    JobLog((ctx), M_FATAL, (fmt)__VA_OPT__(, ) __VA_ARGS__)
#  define info_msg(ctx, fmt, ...) \
    JobLog((ctx), M_INFO, (fmt)__VA_OPT__(, ) __VA_ARGS__)
#  define debug_msg(level, fmt, ...) \
    DebugLog((level), (fmt)__VA_OPT__(, ) __VA_ARGS__)
#else
#  define warn_msg(ctx, ...) JobLog((ctx), M_WARNING, __VA_ARGS__)
#  define err_msg(ctx, ...) JobLog((ctx), M_ERROR, __VA_ARGS__)
#  define fatal_msg(ctx, ...) JobLog((ctx), M_FATAL, __VA_ARGS__)
#  define info_msg(ctx, ...) JobLog((ctx), M_INFO, __VA_ARGS__)
#  define debug_msg(level, ...) DebugLog((level), __VA_ARGS__)
#endif

bool AmICompatibleWith(filedaemon::PluginApiDefinition* core_info)
{
  if (core_info->size != sizeof(*core_info)
      || core_info->version != FD_PLUGIN_INTERFACE_VERSION) {
    return false;
  }

  return true;
}

const PluginInformation my_info = {
    .size = sizeof(my_info),
    .version = FD_PLUGIN_INTERFACE_VERSION,
    .plugin_magic = FD_PLUGIN_MAGIC,
    .plugin_license = "Bareos AGPLv3",
    .plugin_author = "Sebastian Sura",
    .plugin_date = "March 2026",
    .plugin_version = "26.0.0",
    .plugin_description
    = "This plugin allows you to backup your windows system for disaster "
      "recovery & restore it later.",
    .plugin_usage
    = "\non backup:\n---\n" PLUGIN_NAME
      R"(:save-unreferenced-disks=<yes|no>:save-unreferenced-partitions=<yes|no>:save-unreferenced-extents=<yes|no>:ignore-disks=<disks to ignore>

  save-unreferenced-disks=<yes|no>: try to save disks that contain no snapshotted data, default=yes
  save-unreferenced-partitions=<yes|no>: try to save partitions that contain no snapshotted data, default=yes
  save-unreferenced-extents=<yes|no>: try to save even unsnapshotted parts of partitions, default=yes
  disks to ignore: a comma-separated list of disk ids (i.e. '1,2,5') of disks to not backup

on restore:
---
)" PLUGIN_NAME
      R"(:<target>,
  where <target> is one of the following:
    disks=file1,file2,file3 - restore the disks to the chosen disks
    vhdx-directory=dir      - restore the disks as vhdx files into the directory
    raw-directory=dir       - restore the disks as raw files into the directory
    copy=<yes|no>           - if yes, then the data gets restored as simple files

  The copy option allows you to restore the image via the barri cli tool instead
  of this filedaemon plugin.

  Keep in mind, that you specify full paths with a drive letter, say X:,
  by using /X/ at the start of the path.  I.e. C:\my\path, becomes
  /C/my/path.
)"


};

std::string_view trim_left(std::string_view input)
{
  std::size_t chars_to_trim = 0;

  for (auto& c : input) {
    if (!isspace(c)) { break; }
    chars_to_trim += 1;
  }

  return input.substr(chars_to_trim);
}

std::string_view trim_right(std::string_view input)
{
  std::size_t chars_to_keep = input.size();

  for (size_t i = 0; i < input.size(); ++i) {
    auto c = input[input.size() - i - 1];
    if (!isspace(c)) { break; }
    chars_to_keep -= 1;
  }

  return input.substr(0, chars_to_keep);
}

std::string_view trim(std::string_view input)
{
  return trim_left(trim_right(input));
}

std::string_view next_part(std::string_view& input, char sep)
{
  auto sep_pos = input.find_first_of(sep);

  if (sep_pos == input.npos) {
    auto temp = input;
    input = {};
    return temp;
  } else {
    auto temp = input.substr(0, sep_pos);
    input.remove_prefix(sep_pos + 1);
    return temp;
  }
}

std::size_t next_option(std::string_view& to_parse,
                        std::span<const std::string_view> keywords,
                        std::string_view* keyword,
                        std::string_view* value)
{
  // we dont want to change to_parse, if we cannot find a keyword
  auto working_copy = to_parse;
  debug_msg(300, "to_parse = {}", working_copy);

  auto found = next_part(working_copy, ':');

  auto input = trim_left(found);
  debug_msg(300, " => input = {}", input);

  auto key = trim_right(next_part(input, '='));

  debug_msg(300, " => key = {}, value = {}", key, input);
  *value = trim(input);

  for (size_t i = 0; i < keywords.size(); ++i) {
    if (key == keywords[i]) {
      *keyword = key;
      debug_msg(300, " => keyword #{}", i);
      // if we found a keyword, then we update to_parse
      to_parse = working_copy;
      return i;
    }
  }

  debug_msg(300, " => keyword not found");
  return keywords.size();
}

std::optional<std::string> insert_numbers(std::vector<std::size_t>& nums,
                                          std::string_view to_parse)
{
  auto current = to_parse;
  debug_msg(300, "converting '{}' into a list of numbers", to_parse);
  for (;;) {
    current = trim_left(current);
    if (current.size() == 0) { break; }


    auto found = trim_right(next_part(current, ','));

    debug_msg(300, " => converting '{}'", found);

    std::size_t number = 0;
    auto conversion_result = std::from_chars(
        found.data(), found.data() + found.size(), number, 10);

    auto left_over = found.substr(conversion_result.ptr - found.data());

    if (conversion_result.ec != std::errc{}) {
      switch (conversion_result.ec) {
        default:
          [[fallthrough]];
        case std::errc::invalid_argument: {
          return libbareos::format("could not parse {} as a number", found);
        } break;
        case std::errc::result_out_of_range: {
          return libbareos::format("'{}' is out of the acceptable range",
                                   found);
        } break;
      }
    }

    debug_msg(300, " => left_over = '{}'", left_over);

    bool ignore_left_over = true;

    for (auto c : left_over) {
      if (!isspace(c)) {
        ignore_left_over = false;
        break;
      }
    }

    if (!ignore_left_over) {
      return libbareos::format("'{}' contains unparsable junk ({})", found,
                               left_over);
    }

    debug_msg(300, " => parsed {}", number);

    nums.push_back(number);
  }
  return std::nullopt;
}

constexpr std::size_t index_of(std::span<const std::string_view> keywords,
                               std::string_view keyword)
{
  for (std::size_t i = 0; i < keywords.size(); ++i) {
    if (keywords[i] == keyword) { return i; }
  }

  throw std::logic_error{"no such keyword"};
}

struct context {
  virtual bool parse(PluginContext* ctx,
                     const char* text,
                     std::string_view overrides)
      = 0;
  virtual bRC pluginIO(PluginContext* ctx, filedaemon::io_pkt* pkt) = 0;
  virtual ~context() = default;
};

struct plugin_ctx {
  struct CoUninitializer {
    ~CoUninitializer() { CoUninitialize(); }
  };

  CoUninitializer unititializer{};
  std::unique_ptr<context> context;

  std::string option_overrides;
};

plugin_ctx* get_private_context(PluginContext* ctx)
{
  return reinterpret_cast<plugin_ctx*>(ctx->plugin_private_context);
}

void set_private_context(PluginContext* ctx, plugin_ctx* priv_ctx)
{
  ctx->plugin_private_context = priv_ctx;
}

enum class file : std::size_t
{
  Dump,
  Log,
  Count,
};

using copy_location = std::string;

using barri::restore::disk_ids;
using barri::restore::raw_directory;
using barri::restore::vhdx_directory;

using restore_target = std::variant<disk_ids, raw_directory, vhdx_directory>;

struct plugin_arguments {
  using Error = tl::unexpected<std::string>;

  static std::optional<plugin_arguments> parse(PluginContext* ctx,
                                               std::string_view str)
  {
    static constexpr std::string_view keywords[]
        = {"disks",
           "vhdx-directory",
           "raw-directory",
           "copy",
           "save-unreferenced-disks",
           "save-unreferenced-partitions",
           "save-unreferenced-extents",
           "ignore-disks"};

    auto name = next_part(str, ':');

    debug_msg(ctx, 300, "got name = '{}'", name);

    if (name != PLUGIN_NAME) {
      err_msg(ctx, "bad plugin options received, expected '{}', got '{}'",
              PLUGIN_NAME, name);
      return {};
    }

    plugin_arguments args{};
    for (;;) {
      str = trim_left(str);

      if (str.size() == 0) { break; }

      // we always choose the last option set, i.e. if
      //  drives=a,b,c dump=k drives=l,m,n
      // is given, we treat it the same as drives=l,m,n
      // This allows the user to overwrite the options via FdPluginOptions

      std::string_view value = {};
      std::string_view key = {};
      switch (next_option(str, keywords, &key, &value)) {
        case index_of(keywords, "disks"): {
          debug_msg(ctx, 300, "parsing disks value '{}'", value);

          auto ids = split_string_view(value, ',');

          // checking of whether the paths are valid are done later
          disk_ids list;
          list.reserve(ids.size());

          for (auto& id_str : ids) {
            std::size_t id{};
            auto result = std::from_chars(id_str.data(),
                                          id_str.data() + id_str.size(), id);

            if (result.ec != std::errc{}) {
              err_msg(ctx, "bad disk id '{}': {}", id_str,
                      std::make_error_code(result.ec).message());
              return {};
            }

            if (result.ptr != id_str.data() + id_str.size()) {
              err_msg(ctx, "bad disk id '{}': junk at end", id_str);
              return {};
            }

            list.push_back(id);
          }

          args.dump_to_disk = false;
          args.target = std::move(list);
        } break;

        case index_of(keywords, "vhdx-directory"): {
          // checking of whether the path is valid is done later
          if (value.empty()) {
            err_msg(ctx, "vhdx-directory cannot be empty");
            return std::nullopt;
          }

          if (value.size() >= 3 && IsPathSeparator(value[0])
              && IsPathSeparator(value[2])) {
            auto drive_letter = value[1];
            std::string path;
            path += drive_letter;
            path += ":\\";
            path += value.substr(3);

            debug_msg(ctx, 300, "Using vhdx dir {}", path);
            args.target = std::move(vhdx_directory{FromUtf8(path)});
          } else {
            debug_msg(ctx, 300, "Using vhdx dir {}", value);
            args.target = std::move(vhdx_directory{FromUtf8(value)});
          }

          args.dump_to_disk = false;
        } break;

        case index_of(keywords, "raw-directory"): {
          // checking of whether the path is valid is done later
          if (value.empty()) {
            err_msg(ctx, "raw-directory cannot be empty");
            return std::nullopt;
          }

          if (value.size() >= 3 && IsPathSeparator(value[0])
              && IsPathSeparator(value[2])) {
            auto drive_letter = value[1];
            std::string path;
            path += drive_letter;
            path += ":\\";
            path += value.substr(3);

            debug_msg(ctx, 300, "Using raw dir {}", path);
            args.target = std::move(raw_directory{FromUtf8(path)});
          } else {
            debug_msg(ctx, 300, "Using raw dir {}", value);
            args.target = std::move(raw_directory{FromUtf8(value)});
          }

          args.dump_to_disk = false;
        } break;


        case index_of(keywords, "copy"): {
          debug_msg(ctx, 300, "parsing copy value '{}'", value);
          switch (parse_user_bool(value)) {
            case parse_bool_result::True: {
              args.dump_to_disk = true;
            } break;
            case parse_bool_result::False: {
              args.dump_to_disk = false;
            } break;
            case parse_bool_result::Error: {
              fatal_msg(ctx, "unexpected value {} for {}", value, key);
              return std::nullopt;
            } break;
          }
        } break;

        case index_of(keywords, "save-unreferenced-disks"): {
          switch (parse_user_bool(value)) {
            case parse_bool_result::True: {
              args.save_unknown_disks = true;
            } break;
            case parse_bool_result::False: {
              args.save_unknown_disks = false;
            } break;
            case parse_bool_result::Error: {
              fatal_msg(ctx, "unexpected value {} for {}", value, key);
              return std::nullopt;
            } break;
          }
        } break;

        case index_of(keywords, "save-unreferenced-partitions"): {
          switch (parse_user_bool(value)) {
            case parse_bool_result::True: {
              args.save_unknown_partitions = true;
            } break;
            case parse_bool_result::False: {
              args.save_unknown_partitions = false;
            } break;
            case parse_bool_result::Error: {
              fatal_msg(ctx, "unexpected value {} for {}", value, key);
              return std::nullopt;
            } break;
          }
        } break;

        case index_of(keywords, "save-unreferenced-extents"): {
          switch (parse_user_bool(value)) {
            case parse_bool_result::True: {
              args.save_unknown_extents = true;
            } break;
            case parse_bool_result::False: {
              args.save_unknown_extents = false;
            } break;
            case parse_bool_result::Error: {
              fatal_msg(ctx, "unexpected value {} for {}", value, key);
              return std::nullopt;
            } break;
          }
        } break;

        case index_of(keywords, "ignore-disks"): {
          if (value.empty()) {
            fatal_msg(ctx, "unexpected empty value for {}", key);
            return std::nullopt;
          }
          if (auto error = insert_numbers(args.ignored_disks, value)) {
            fatal_msg(ctx, "could not parse {} as a list of ints ({}): {}",
                      value, key, error.value());
            return std::nullopt;
          }
        } break;

        default: {
          fatal_msg(ctx, "could not parse plugin options string '{}'", str);

          return std::nullopt;
        } break;
      }
    }

    return args;
  }

  void apply_dump_context_settings(dump_context* ctx) const
  {
    dump_context_save_unknown_disks(ctx, save_unknown_disks);
    dump_context_save_unknown_partitions(ctx, save_unknown_partitions);
    dump_context_save_unknown_extents(ctx, save_unknown_extents);

    for (auto disk_id : ignored_disks) {
      dump_context_ignore_disk(ctx, disk_id);
    }
  }

  // restore options
  bool dump_to_disk{true};
  std::optional<restore_target> target;

  // backup options
  std::vector<size_t> ignored_disks;
  bool save_unknown_disks{true};
  bool save_unknown_partitions{true};
  bool save_unknown_extents{true};
};

struct plugin_logger : public GenericLogger {
  using Clock = std::chrono::steady_clock;

  void Begin(std::size_t FileSize) override
  {
    current_file_size = FileSize;
    current_file_offset = 0;
    last_file_offset = 0;
    last_time_stamp = Clock::now();

    data_milestone = FileSize / 10;  // around ~10 prints
  }
  void Progressed(std::size_t Amount) override
  {
    current_file_offset += Amount;

    auto current_ts = Clock::now();

    bool time_elapsed = current_ts - last_time_stamp > time_milestone;
    bool milestone_reached
        = current_file_offset - last_file_offset > data_milestone;
    if (time_elapsed || milestone_reached || last_file_offset == 0) {
      print_progress(current_ts, current_file_offset);
    }
  }
  void End() override { current_file_size = 0; }

  void SetStatus(std::string_view Status) override { (void)Status; }
  void Output(Message message) override
  {
    if (message.is_trace) {
      DebugLog(ctx, 500, "{}", message.text);
    } else {
      JobLog(ctx, M_INFO, "{}", message.text);
    }

    if (messages.size() < max_messages_size) {
      // lets make sure that this does not grow too big.
      messages.insert(messages.end(), message.text.begin(), message.text.end());
      messages.push_back('\n');
    }
  }

  plugin_logger(PluginContext* ctx_) : GenericLogger{true}, ctx{ctx_} {}

  std::span<const char> log() const { return messages; }

  PluginContext* context() { return ctx; }

 private:
  void print_progress(Clock::time_point current_ts, std::size_t current_offset)
  {
    if (!current_file_size) { return; }

    auto data_written = current_offset - last_file_offset;

    auto progress = static_cast<double>(current_offset)
                    / static_cast<double>(current_file_size);

    auto seconds = std::chrono::duration_cast<std::chrono::seconds>(
        current_ts - last_time_stamp);

    if (seconds.count() == 0) { return; }

    auto approx_speed = data_written / seconds.count();

    std::string log_msg = format_progress_bar(progress, approx_speed, 20);

    Message msg = {log_msg, false};

    Output(msg);

    last_file_offset = current_offset;
    last_time_stamp = current_ts;
  }

  PluginContext* ctx;

  Clock::time_point last_time_stamp;
  std::size_t last_file_offset;
  std::size_t current_file_offset = 0;
  std::size_t current_file_size = 0;
  std::size_t data_milestone = 0;
  static constexpr Clock::duration time_milestone = std::chrono::minutes(5);

  static constexpr std::size_t max_messages_size = 1 << 30;  // 1GB
  std::vector<char> messages;
};

namespace backup {
struct session_ctx {
  plugin_logger logger;
  std::unique_ptr<dump_context,
                  decltype([](dump_context* ctx) { destroy_context(ctx); })>
      dctx{};
  std::unique_ptr<data_dumper,
                  decltype([](data_dumper* ptr) { dumper_stop(ptr); })>
      dumper{};

  session_ctx(PluginContext* ctx, const plugin_arguments& args)
      : logger{ctx}, dctx{make_context(&logger)}
  {
    args.apply_dump_context_settings(dctx.get());
    dumper.reset(dumper_setup(&logger, dump_context_create_plan(dctx.get())));
  }
};

struct context : ::context {
  virtual ~context() = default;

  file current_file = file::Dump;

  static std::unique_ptr<context> make(PluginContext* ctx)
  {
    std::optional client_name = bVar::Get<bVar::Client>(ctx);
    if (!client_name) {
      fatal_msg(ctx, "could not retrieve client name from the bareos api!");
      return nullptr;
    }

    auto now = time(NULL);

    auto bctx = std::make_unique<context>();

    std::string_view hostname{client_name.value()};
    bctx->dump_file_name
        = libbareos::format("@BARRI/{}{}", hostname, dump_ending);
    bctx->log_file_name
        = libbareos::format("@BARRI/{}{}", hostname, log_ending);

    bctx->timestamp = now;

    return bctx;
  }


  bool parse(PluginContext* ctx,
             const char* text,
             std::string_view overrides) override
  {
    std::string computed_value{text};
    if (!overrides.empty()) {
      computed_value += ":";
      computed_value += overrides;
    }

    std::optional parsed = plugin_arguments::parse(ctx, computed_value);
    if (!parsed) { return false; }

    args = std::move(*parsed);
    return true;
  }

  std::optional<session_ctx> current_session;
  plugin_arguments args;

  void begin_session(PluginContext* ctx) { current_session.emplace(ctx, args); }

  std::size_t session_read(std::span<char> data)
  {
    if (!current_session) { return 0; }
    return dumper_write(current_session->dumper.get(), data);
  }

  bool has_session() const { return current_session.has_value(); }

  std::string dump_file_name;
  std::string log_file_name;
  time_t timestamp;
  std::size_t log_bytes_sent = 0;

  bRC pluginIO(PluginContext* ctx, filedaemon::io_pkt* pkt) override;
};

bRC pluginIO_Dump(context* bctx, PluginContext* ctx, filedaemon::io_pkt* pkt)
{
  switch (pkt->func) {
    case filedaemon::IO_OPEN: {
      if (bctx->has_session()) {
        err_msg(ctx, "context can only be created once");
        pkt->status = -1;
        return bRC_Error;
      }
      try {
        bctx->begin_session(ctx);
        pkt->status = 0;
        return bRC_OK;
      } catch (const std::exception& ex) {
        err_msg(ctx, "could not start: {}", ex.what());
        pkt->status = -1;
        return bRC_Error;
      } catch (...) {
        err_msg(ctx, "could not start: unknown error occurred");
        pkt->status = -1;
        return bRC_Error;
      }
    } break;
    case filedaemon::IO_READ: {
      if (pkt->count < 0) {
        err_msg(ctx, "its impossible to read {} bytes...", pkt->count);
        pkt->status = -1;
        return bRC_Error;
      }
      pkt->status = bctx->session_read(
          std::span{pkt->buf, static_cast<std::size_t>(pkt->count)});
      return bRC_OK;
    } break;
    case filedaemon::IO_CLOSE: {
      if (!bctx->has_session()) {
        err_msg(ctx, "context can only be closed, if its open");
        pkt->status = -1;
        return bRC_Error;
      }
      pkt->status = 0;
      return bRC_OK;
    } break;
  }

  fatal_msg(ctx, "unhandled code path: {}", pkt->func);
  return bRC_Error;
}

bRC pluginIO_Log(context* bctx, PluginContext* ctx, filedaemon::io_pkt* pkt)
{
  switch (pkt->func) {
    case filedaemon::IO_OPEN: {
      if (!bctx->has_session()) {
        err_msg(ctx, "cannot open log: no session open");
        pkt->status = -1;
        return bRC_Error;
      }
      return bRC_OK;
    } break;
    case filedaemon::IO_READ: {
      if (pkt->count < 0) {
        err_msg(ctx, "it is impossible to read {} bytes...", pkt->count);
        pkt->status = -1;
        return bRC_Error;
      }

      auto log = bctx->current_session->logger.log();

      std::size_t& log_bytes_sent = bctx->log_bytes_sent;
      auto bytes_to_read = std::min(log.size() - log_bytes_sent,
                                    static_cast<std::size_t>(pkt->count));

      memcpy(pkt->buf, log.data() + log_bytes_sent, bytes_to_read);
      pkt->status = bytes_to_read;
      log_bytes_sent += bytes_to_read;
      return bRC_OK;
    } break;
    case filedaemon::IO_CLOSE: {
      pkt->status = 0;
      return bRC_OK;
    } break;
  }
  fatal_msg(ctx, "unhandled code path: {}", pkt->func);
  return bRC_Error;
}

bRC context::pluginIO(PluginContext* ctx, filedaemon::io_pkt* pkt)
{
  auto handler = [&] {
    switch (current_file) {
      case file::Dump: {
        return &pluginIO_Dump;
      } break;
      case file::Log: {
        return &pluginIO_Log;
      } break;
      default: {
        return static_cast<decltype(&pluginIO_Dump)>(nullptr);
      } break;
    }
  }();

  if (!handler) {
    fatal_msg(ctx, "could not find handler for file {}",
              static_cast<std::size_t>(current_file));
    return bRC_Error;
  }


  switch (pkt->func) {
    case filedaemon::IO_OPEN:
      [[fallthrough]];
    case filedaemon::IO_READ:
      [[fallthrough]];
    case filedaemon::IO_CLOSE: {
      return handler(this, ctx, pkt);
    } break;

    case filedaemon::IO_WRITE: {
      err_msg(ctx, "restores are not supported during a backup");
      pkt->status = -1;
      return bRC_Error;
    } break;
  }
  return bRC_Error;
}
};  // namespace backup

namespace restore {
struct session {
  plugin_logger logger;

  restartable_parser* parser{};
  std::unique_ptr<GenericHandler> handler{};

  session(PluginContext* ctx) : logger{ctx} {}

  template <typename T>
    requires std::same_as<T, vhdx_directory> || std::same_as<T, raw_directory>
             || std::same_as<T, disk_ids>
  bool begin(T arg)
  {
    try {
      handler = barri::restore::GetHandler(&logger, std::move(arg));
      if (!handler) { return false; }
      parser = parse_begin(handler.get(), &logger);
      if (!parser) { return false; }
    } catch (const std::exception& ex) {
      err_msg(logger.context(), "Exception occured during creation: {}",
              ex.what());
      return false;
    }
    return true;
  }

  ssize_t write(std::span<const char> data)
  {
    try {
      parse_data(parser, data);
      return data.size();
    } catch (const std::exception& ex) {
      err_msg(logger.context(), "Exception occured during write: {}",
              ex.what());
      return -1;
    }
  }

  ~session()
  {
    if (parser) { parse_end(parser); }
  }
};

struct context : ::context {
  virtual ~context() = default;

  static std::unique_ptr<context> make(PluginContext*)
  {
    auto rctx = std::make_unique<context>();

    return rctx;
  }

  bool parse(PluginContext* ctx,
             const char* text,
             std::string_view overrides) override
  {
    std::string computed_value{text};
    if (!overrides.empty()) {
      computed_value += ":";
      computed_value += overrides;
    }

    std::optional opt = plugin_arguments::parse(ctx, computed_value);
    if (!opt) { return false; }
    args = std::move(*opt);
    return true;
  }

  bRC start_file(PluginContext* ctx, const char* name)
  {
    (void)ctx;
    (void)name;
    return bRC_OK;
  }

  bRC end_current_file(PluginContext* ctx)
  {
    (void)ctx;
    current_file_type.reset();
    return bRC_OK;
  }

  bRC pluginIO_Session(PluginContext* ctx,
                       restore_target& target,
                       filedaemon::io_pkt* pkt)
  {
    // this function should only b
    if (!args.target.has_value()) { return bRC_Error; }

    switch (pkt->func) {
      case filedaemon::IO_OPEN: {
        if (current_session) {
          err_msg(ctx, "context can only be created once");
          pkt->status = -1;
          return bRC_Error;
        }

        try {
          current_session.emplace(ctx);

          auto start_ok = std::visit(
              [&](auto& val) { return current_session->begin(val); }, target);

          if (!start_ok) {
            err_msg(ctx, "could not begin session");
            pkt->status = -1;
            return bRC_Error;
          }

          pkt->status = 0;
          return bRC_OK;
        } catch (const std::exception& ex) {
          err_msg(ctx, "could not start: {}", ex.what());
          pkt->status = -1;
          return bRC_Error;
        } catch (...) {
          err_msg(ctx, "could not start: unknown error occurred");
          pkt->status = -1;
          return bRC_Error;
        }
      } break;
      case filedaemon::IO_WRITE: {
        if (pkt->count < 0) {
          err_msg(ctx, "its impossible to write {} bytes...", pkt->count);
          pkt->status = -1;
          return bRC_Error;
        }
        if (!current_session) {
          err_msg(ctx, "its impossible to write with no session", pkt->count);
          pkt->status = -1;
          return bRC_Error;
        }

        pkt->status = current_session->write(
            std::span{pkt->buf, static_cast<std::size_t>(pkt->count)});
        if (pkt->status < 0) { return bRC_Error; }
        return bRC_OK;
      } break;
      case filedaemon::IO_CLOSE: {
        if (!current_session) {
          err_msg(ctx, "context can only be closed, if its open");
          pkt->status = -1;
          return bRC_Error;
        }
        current_session.reset();
        pkt->status = 0;
        return bRC_OK;
      } break;
    }

    fatal_msg(ctx, "unhandled code path: {}", pkt->func);
    return bRC_Error;
  }

  std::optional<session> current_session;

  bRC pluginIO_Dump(PluginContext* ctx, filedaemon::io_pkt* pkt)
  {
    switch (pkt->func) {
      case filedaemon::IO_OPEN: {
        auto hndl = CreateFileA(pkt->fname, GENERIC_WRITE, 0, NULL,
                                CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);

        if (hndl == INVALID_HANDLE_VALUE) {
          warn_msg(ctx, "Could not open file {} for writing", pkt->fname);
          pkt->status = -1;
          return bRC_Error;
        } else {
          pkt->hndl = hndl;
          pkt->status = IoStatus::do_io_in_core;

          return bRC_OK;
        }
      } break;
      case filedaemon::IO_CLOSE: {
        if (CloseHandle(pkt->hndl) == 0) { return bRC_Error; }
        return bRC_OK;
      } break;
      default: {
        fatal_msg(ctx,
                  "Internal error occured: received unexpected instruction: {}",
                  pkt->func);
        return bRC_Error;
      } break;
    }
  }

  bRC pluginIO(PluginContext* ctx, filedaemon::io_pkt* pkt) override
  {
    if (args.dump_to_disk) { return pluginIO_Dump(ctx, pkt); }

    if (!args.target.has_value()) {
      fatal_msg(ctx, "Cannot do IO as it was not set up yet!");
      return bRC_Error;
    }

    return pluginIO_Session(ctx, *args.target, pkt);
  }

  bRC create_file(PluginContext* ctx, filedaemon::restore_pkt* pkt)
  {
    if (args.dump_to_disk) {
      // TODO: we should delete @BARRI/ from the pkt->ofname
      // and restore to there

      debug_msg(ctx, 300, "duming {} to disk at {}", pkt->original_file_name,
                pkt->ofname);

      pkt->create_status = CF_CORE;
      return bRC_OK;
    }

    if (!args.target.has_value()) {
      fatal_msg(ctx, "Cannot create {} as context was not set up yet!",
                pkt->original_file_name);
      return bRC_Error;
    }

    auto fname = std::string_view{pkt->original_file_name};
    if (fname.ends_with(log_ending)) {
      debug_msg(ctx, 300, "skipping {}", pkt->original_file_name, pkt->ofname);
      pkt->create_status = CF_SKIP;
      return bRC_OK;
    } else if (fname.ends_with(dump_ending)) {
      // we do not actually create files here, so we just ignore this

      debug_msg(ctx, 300, "extracting {}", pkt->original_file_name,
                pkt->ofname);
      pkt->create_status = CF_EXTRACT;
      return bRC_OK;
    }

    fatal_msg(ctx, "Cannot restore {} of unknown type!", fname);
    return bRC_Error;
  }

  std::optional<file> current_file_type{};
  plugin_arguments args;
};
};  // namespace restore

backup::context* get_backup_context(PluginContext* ctx)
{
  auto* pctx = get_private_context(ctx);

  return dynamic_cast<backup::context*>(pctx->context.get());
}

restore::context* get_restore_context(PluginContext* ctx)
{
  auto* pctx = get_private_context(ctx);
  (void)pctx;

  return dynamic_cast<restore::context*>(pctx->context.get());
}


bRC newPlugin(PluginContext* ctx)
{
  auto hr = CoInitializeEx(NULL, COINIT_MULTITHREADED);

  if (FAILED(hr)) {
    _com_error err(hr);
    err_msg(ctx, "could not initialize com: {}", err.ErrorMessage());
  }

  set_private_context(ctx, new plugin_ctx);
  RegisterBareosEvent(ctx, filedaemon::bEventNewPluginOptions);
  RegisterBareosEvent(ctx, filedaemon::bEventPluginCommand);
  RegisterBareosEvent(ctx, filedaemon::bEventJobStart);
  RegisterBareosEvent(ctx, filedaemon::bEventRestoreCommand);
  RegisterBareosEvent(ctx, filedaemon::bEventEstimateCommand);
  RegisterBareosEvent(ctx, filedaemon::bEventBackupCommand);
  RegisterBareosEvent(ctx, filedaemon::bEventRestoreObject);

  return bRC_OK;
}

bRC freePlugin(PluginContext* ctx)
{
  auto* pctx = get_private_context(ctx);
  set_private_context(ctx, nullptr);
  delete pctx;
  return bRC_OK;
}

bRC getPluginValue(PluginContext*, filedaemon::pVariable, void*)
{
  /* UNUSED */
  return bRC_Error;
}
bRC setPluginValue(PluginContext*, filedaemon::pVariable, void*)
{
  /* UNUSED */
  return bRC_Error;
}

bRC handlePluginEvent(PluginContext* ctx, filedaemon::bEvent* event, void* data)
{
  auto* pctx = get_private_context(ctx);
  switch (event->eventType) {
    case filedaemon::bEventJobStart: {
      return bRC_OK;
    }
    case filedaemon::bEventNewPluginOptions: {
      std::string_view s{static_cast<const char*>(data)};

      if (!s.starts_with("barri:")) {
        err_msg(
            ctx,
            "Could not parse plugin options: they do not start with barri:!");
        return bRC_Error;
      }

      s.remove_prefix(sizeof("barri:") - 1);

      if (pctx->option_overrides.empty()) {
        pctx->option_overrides = std::string{s};
        debug_msg(ctx, 300, "Setting option_overrides to {}",
                  pctx->option_overrides);
      } else {
        debug_msg(ctx, 300,
                  "Keeping option_overrides at {}, not changing to {}",
                  pctx->option_overrides, s);
      }

      return bRC_OK;
    } break;
    case filedaemon::bEventPluginCommand: {
      if (!pctx->context) {
        debug_msg(ctx, 300, "setting up a backup context");
        auto bctx = backup::context::make(ctx);
        if (!bctx) {
          fatal_msg(ctx, "Could not setup the backup context");
          return bRC_Error;
        }

        pctx->context = std::move(bctx);
      }

      auto* bctx = get_backup_context(ctx);

      if (!bctx) {
        // this can happen if we somehow setup a restore context before
        fatal_msg(ctx, "instructed to execute a backup command during restore");
        return bRC_Error;
      }

      if (!bctx->parse(ctx, static_cast<const char*>(data),
                       pctx->option_overrides)) {
        debug_msg(ctx, 300, "plugin option string could not be parsed");
        return bRC_Error;
      }
      return bRC_OK;
    } break;
    case filedaemon::bEventRestoreCommand: {
      if (!pctx->context) {
        debug_msg(ctx, 300, "setting up a restore context");
        auto rctx = restore::context::make(ctx);
        if (!rctx) {
          fatal_msg(ctx, "could not setup the restore context");
          return bRC_Error;
        }

        pctx->context = std::move(rctx);
      }

      auto* rctx = get_restore_context(ctx);

      if (!rctx) {
        // this can happen if we somehow setup a backup context before
        fatal_msg(ctx, "instructed to execute a restore command during backup");
        return bRC_Error;
      }

      if (!rctx->parse(ctx, static_cast<const char*>(data),
                       pctx->option_overrides)) {
        debug_msg(ctx, 300, "plugin option string could not be parsed");
        return bRC_Error;
      }
      return bRC_OK;
    } break;
  }

  warn_msg(ctx, "Unknown event {} passed", (int)event->eventType);
  return bRC_OK;
}

bRC startBackupFile(PluginContext* ctx, filedaemon::save_pkt* sp)
{
  auto* bctx = get_backup_context(ctx);

  if (!bctx) {
    fatal_msg(ctx, "Backup Context was not set before {}", __PRETTY_FUNCTION__);
    return bRC_Error;
  }

  // first we check if this is a full backup.  If not, we reject the backup
  {
    /* one would imagine it would make the most sense to do this check
     * in handlePluginEvent(bEventLevel), but that does not work:
     * - the check is not reliable: other plugins can swallow the event
     * - there is no good way to stop the backup: the return value is basically
     *   ignored; a fatal error message stops the job in a very bad place,
     *   leading to bad job logs and stuck jobs.
     * This is why this check was moved here */

    auto level = bVar::Get<bVar::Level>(ctx);
    if (!level) {
      fatal_msg(ctx,
                "could not determine the level that we are supposed to be "
                "running at");
      return bRC_Error;
    }

    if (level.value() != L_FULL) {
      fatal_msg(
          ctx,
          "this plugin only supports full backups, but level={} was requested",
          static_cast<char>(level.value()));
      return bRC_Stop;
    }
  }

  if (bctx->current_file == file::Dump) {
    // setting this does not do anything yet.  Maybe it will in the future
    sp->portable = true;  // we do not create windows backup data streams

    sp->fname = const_cast<char*>(bctx->dump_file_name.c_str());
    sp->type = FT_REG;
    sp->statp.st_mode = 0700 | S_IFREG;
    sp->statp.st_ctime = bctx->timestamp;
    sp->statp.st_mtime = bctx->timestamp;
    sp->statp.st_atime = bctx->timestamp;
    sp->statp.st_size = -1;
    sp->statp.st_blksize = 4096;
    sp->statp.st_blocks = 1;
  } else if (bctx->current_file == file::Log) {
    if (!bctx->current_session) {
      fatal_msg(ctx, "cannot backup log of a session that does not exist");
      return bRC_Error;
    }

    sp->portable = true;  // we do not create windows backup data streams

    sp->fname = const_cast<char*>(bctx->log_file_name.c_str());
    sp->type = FT_REG;
    sp->statp.st_mode = 0700 | S_IFREG;
    sp->statp.st_ctime = bctx->timestamp;
    sp->statp.st_mtime = bctx->timestamp;
    sp->statp.st_atime = bctx->timestamp;
    sp->statp.st_size = bctx->current_session->logger.log().size();
    sp->statp.st_blksize = 4096;
    sp->statp.st_blocks = (sp->statp.st_size + 4095) / 4096;
  } else {
    err_msg(ctx, "cannot create a third file.");
    return bRC_Error;
  }

  return bRC_OK;
}

bRC endBackupFile(PluginContext* ctx)
{
  auto* bctx = get_backup_context(ctx);

  if (!bctx) {
    fatal_msg(ctx, "Backup Context was not set before {}", __PRETTY_FUNCTION__);
    return bRC_Error;
  }

  bctx->current_file
      = static_cast<file>(static_cast<std::size_t>(bctx->current_file) + 1);
  if (bctx->current_file != file::Count) { return bRC_More; }
  return bRC_OK;
}

bRC startRestoreFile(PluginContext* ctx, const char* file_name)
{
  DebugLog(ctx, 500, "starting to restore '{}'", file_name);

  auto* rctx = get_restore_context(ctx);

  if (!rctx) {
    fatal_msg(ctx, "Restore Context was not set before {}",
              __PRETTY_FUNCTION__);
    return bRC_Error;
  }

  return rctx->start_file(ctx, file_name);
}

bRC endRestoreFile(PluginContext* ctx)
{
  auto* rctx = get_restore_context(ctx);

  if (!rctx) {
    fatal_msg(ctx, "Restore Context was not set before {}",
              __PRETTY_FUNCTION__);
    return bRC_Error;
  }

  debug_msg(ctx, 500, "finished restoring file");

  return rctx->end_current_file(ctx);
}

bRC pluginIO(PluginContext* ctx, filedaemon::io_pkt* pkt)
{
  if (auto* bctx = get_backup_context(ctx)) {
    return bctx->pluginIO(ctx, pkt);
  } else if (auto* rctx = get_restore_context(ctx)) {
    return rctx->pluginIO(ctx, pkt);
  } else {
    fatal_msg(ctx, "context is not setup properly, but {} is called",
              __PRETTY_FUNCTION__);
    return bRC_Error;
  }
}

bRC createFile(PluginContext* ctx, filedaemon::restore_pkt* pkt)
{
  if (auto* rctx = get_restore_context(ctx)) {
    return rctx->create_file(ctx, pkt);
  } else {
    fatal_msg(ctx, "{} can only be called during restore", __PRETTY_FUNCTION__);
    return bRC_Error;
  }
}

bRC setFileAttributes(PluginContext* ctx, filedaemon::restore_pkt*)
{
  if (auto* rctx = get_restore_context(ctx)) {
    debug_msg(ctx, 300, "ignoring set_file_attributes");
    return bRC_OK;
  } else {
    fatal_msg(ctx, "{} can only be called during restore", __PRETTY_FUNCTION__);
    return bRC_Error;
  }
}

bRC checkFile(PluginContext* ctx, char* file_name)
{
  (void)ctx;
  (void)file_name;
  return bRC_Error;
}

bRC getAcl(PluginContext* ctx, filedaemon::acl_pkt* pkt)
{
  (void)ctx;
  (void)pkt;
  return bRC_Error;
}

bRC setAcl(PluginContext* ctx, filedaemon::acl_pkt* pkt)
{
  (void)ctx;
  (void)pkt;
  return bRC_Error;
}

bRC getXattr(PluginContext* ctx, filedaemon::xattr_pkt* pkt)
{
  (void)ctx;
  (void)pkt;
  return bRC_Error;
}

bRC setXattr(PluginContext* ctx, filedaemon::xattr_pkt* pkt)
{
  (void)ctx;
  (void)pkt;
  return bRC_Error;
}

const filedaemon::PluginFunctions my_functions = {
    .size = sizeof(my_functions),
    .version = FD_PLUGIN_INTERFACE_VERSION,
    .newPlugin = &newPlugin,
    .freePlugin = &freePlugin,
    .getPluginValue = &getPluginValue,
    .setPluginValue = &setPluginValue,
    .handlePluginEvent = &handlePluginEvent,
    .startBackupFile = &startBackupFile,
    .endBackupFile = &endBackupFile,
    .startRestoreFile = &startRestoreFile,
    .endRestoreFile = &endRestoreFile,
    .pluginIO = &pluginIO,
    .createFile = &createFile,
    .setFileAttributes = &setFileAttributes,
    .checkFile = &checkFile,
    .getAcl = &getAcl,
    .setAcl = &setAcl,
    .getXattr = &getXattr,
    .setXattr = &setXattr,
};

extern "C" __declspec(dllexport) int loadPlugin(
    filedaemon::PluginApiDefinition* core_info,
    filedaemon::CoreFunctions* core_funcs,
    PluginInformation** plugin_info,
    filedaemon::PluginFunctions** plugin_funcs)
{
  if (!SetupBareosApi(core_funcs)) { return -1; }

  if (!AmICompatibleWith(core_info)) { return -1; }

  *plugin_info = const_cast<PluginInformation*>(&my_info);
  *plugin_funcs = const_cast<filedaemon::PluginFunctions*>(&my_functions);

  return 0;
}

extern "C" __declspec(dllexport) int unloadPlugin() { return 0; }
