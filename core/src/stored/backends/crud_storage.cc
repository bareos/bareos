/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2024-2024 Bareos GmbH & Co. KG

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
#include "include/bareos.h"
#include <fmt/format.h>
#include "crud_storage.h"
#include "lib/berrno.h"
#include "lib/bpipe.h"
#include "lib/bstringlist.h"
#include "stored/stored_conf.h"
#include "stored/stored_globals.h"
#include <sys/stat.h>
#if defined(HAVE_WIN32)
#  include <shlwapi.h>  // for PathIsRelativeA()
#endif

namespace {
constexpr int debug_info = 110;
constexpr int debug_trace = 130;

bool path_is_relative(const std::string& path)
{
#if defined(HAVE_WIN32)
  return PathIsRelativeA(path.c_str());
#else
  return path[0] != '/';
#endif
}

class BPipeHandle {
  Bpipe* bpipe{nullptr};

 public:
  BPipeHandle(const char* prog,
              std::chrono::seconds wait,
              const char* mode,
              const std::unordered_map<std::string, std::string>& env_vars = {})
      : bpipe(OpenBpipe(prog, wait.count(), mode, true, env_vars))
  {
    if (!bpipe) { throw std::runtime_error("opening Bpipe"); }
  }
  BPipeHandle(const BPipeHandle&) = delete;
  BPipeHandle& operator=(const BPipeHandle&) = delete;

  BPipeHandle(BPipeHandle&& other) { std::swap(bpipe, other.bpipe); }

  BPipeHandle& operator=(BPipeHandle&& other)
  {
    std::swap(bpipe, other.bpipe);
    return *this;
  }

  ~BPipeHandle()
  {
    if (bpipe) { CloseBpipe(bpipe); }
  }

  static tl::expected<BPipeHandle, std::string> create(
      const char* prog,
      std::chrono::seconds wait,
      const char* mode,
      const std::unordered_map<std::string, std::string>& env_vars = {})
  {
    try {
      return BPipeHandle(prog, wait, mode, env_vars);
    } catch (const std::runtime_error& e) {
      return tl::unexpected(e.what());
    }
  }

  FILE* getReadFd() { return bpipe->rfd; }
  FILE* getWriteFd() { return bpipe->wfd; }
  std::string getOutput()
  {
    close_write();
    std::string output;
    char iobuf[1024];
    while (!feof(bpipe->rfd)) {
      size_t rsize = fread(iobuf, 1, 1024, bpipe->rfd);
      if (rsize > 0 && !ferror(bpipe->rfd)) { output.append(iobuf, rsize); }
    }
    return output;
  }
  void reset_timeout() { TimerKeepalive(*bpipe->timer_id); }
  bool timed_out() { return bpipe->timer_id && bpipe->timer_id->killed; }
  void close_write()
  {
    ASSERT(bpipe);
    CloseWpipe(bpipe);
  }
  int close()
  {
    ASSERT(bpipe);
    int ret = CloseBpipe(bpipe) & ~b_errno_exit;

    if (ret & b_errno_signal) {
      ret &= ~b_errno_signal;
      ret *= -1;
    }

    bpipe = nullptr;
    return ret;
  }
};

// std::isalnum is locale-sensitive but we need ASCII-only
bool is_ascii_alnum(char c)
{
  return std::isdigit(c) || ('a' <= c && c <= 'z') || ('A' <= c && c <= 'Z');
}

bool is_valid_env_name(const std::string& name)
{
  // According to IEEE Std 1003.1-2024 / POSIX.1-2024 the namespace used for
  // system environment variables is uppercase letters, digits and underscore.
  // Additionally variable names containing lowercase letters are reserved for
  // application use. As a result we allow upper- and lowercase letters, digits
  // and underscores.
  // As the standard also advises not to use variable names starting with a
  // digit, we prevent that, too.

  return !name.empty() && !std::isdigit(name[0])
         && std::all_of(name.cbegin(), name.cend(),
                        [](char c) { return is_ascii_alnum(c) || c == '_'; });
}

}  // namespace

tl::expected<void, std::string> CrudStorage::set_program(
    const std::string& program)
{
  if (path_is_relative(program)) {
    m_program
        = fmt::format("{}/{}", storagedaemon::me->scripts_directory, program);
  } else {
    m_program = program;
  }

  struct stat buffer;
  if (::stat(m_program.c_str(), &buffer) == -1) {
    Dmsg0(debug_info, "program path '%s' does not exist.\n", m_program.c_str());
    return tl::unexpected(
        fmt::format("program path {} does not exist.\n", m_program));
  }

  Dmsg0(debug_info, "using program path '%s'\n", m_program.c_str());
  return {};
}

void CrudStorage::set_program_timeout(std::chrono::seconds timeout)
{
  m_program_timeout = timeout;
}

tl::expected<BStringList, std::string> CrudStorage::get_supported_options()
{
  Dmsg0(debug_trace, "options called\n");
  std::string cmdline = fmt::format("\"{}\" options", m_program);
  auto bph{BPipeHandle::create(cmdline.c_str(), m_program_timeout, "r")};
  if (!bph) { return tl::unexpected(bph.error()); }
  auto output = bph->getOutput();
  auto ret = bph->close();
  Dmsg1(debug_trace,
        "options returned %d\n"
        "== Output ==\n"
        "%s"
        "============\n",
        ret, output.c_str());
  if (ret != 0) {
    return tl::unexpected(
        fmt::format("Running \"{}\" returned {}\n", cmdline, ret));
  }
  BStringList options{output, '\n'};
  if (!options.empty() && options.back().empty()) { options.pop_back(); }
  return options;
}

tl::expected<void, std::string> CrudStorage::set_option(
    const std::string& name,
    const std::string& value)
{
  if (!is_valid_env_name(name)) {
    return tl::unexpected(fmt::format(
        "Name \"{}\" is not usable as environment variable\n", name));
  }
  Dmsg0(debug_trace, "program environment variable '%s' set to '%s'\n",
        name.c_str(), value.c_str());
  m_env_vars[name] = value;
  return {};
}

tl::expected<void, std::string> CrudStorage::test_connection()
{
  Dmsg0(debug_trace, "test_connection called\n");
  std::string cmdline = fmt::format("\"{}\" testconnection", m_program);
  auto bph{
      BPipeHandle::create(cmdline.c_str(), m_program_timeout, "r", m_env_vars)};
  if (!bph) { return tl::unexpected(bph.error()); }
  auto output = bph->getOutput();
  auto ret = bph->close();
  Dmsg1(debug_trace,
        "testconnection returned %d\n"
        "== Output ==\n"
        "%s"
        "============\n",
        ret, output.c_str());
  if (ret != 0) {
    return tl::unexpected(
        fmt::format("Running \"{}\" returned {}\n", cmdline, ret));
  }
  return {};
}

auto CrudStorage::stat(std::string_view obj_name, std::string_view obj_part)
    -> tl::expected<Stat, std::string>
{
  Dmsg1(debug_trace, "stat %s called\n", obj_name.data());
  std::string cmdline
      = fmt::format("\"{}\" stat \"{}\" \"{}\"", m_program, obj_name, obj_part);
  auto bph{
      BPipeHandle::create(cmdline.c_str(), m_program_timeout, "r", m_env_vars)};
  if (!bph) { return tl::unexpected(bph.error()); }
  auto rfh = bph->getReadFd();
  Stat stat;
  if (int n = fscanf(rfh, "%zu\n", &stat.size); n != 1) {
    return tl::unexpected(
        fmt::format("could not parse data returned by {}\n", cmdline));
  }
  if (auto ret = bph->close(); ret != 0) {
    Dmsg1(debug_info, "stat returned %d\n", ret);
    return tl::unexpected(
        fmt::format("Running \"{}\" returned {}\n", cmdline, ret));
  }
  Dmsg1(debug_trace, "stat returns %zu\n", stat.size);
  return stat;
}

auto CrudStorage::list(std::string_view obj_name)
    -> tl::expected<std::map<std::string, Stat>, std::string>
{
  Dmsg1(debug_trace, "list %s called\n", obj_name.data());
  std::string cmdline = fmt::format("\"{}\" list \"{}\"", m_program, obj_name);
  auto bph{
      BPipeHandle::create(cmdline.c_str(), m_program_timeout, "r", m_env_vars)};
  if (!bph) { return tl::unexpected(bph.error()); }
  auto rfh = bph->getReadFd();

  std::map<std::string, Stat> result;
  while (!feof(rfh)) {
    Stat stat;
    auto obj_part = std::string(129, '\0');
    if (int n = fscanf(rfh, "%128s %zu\n", obj_part.data(), &stat.size);
        n != 2) {
      Dmsg1(debug_info, "fscanf() returned %d\n", n);
      return tl::unexpected(
          fmt::format("could not parse data returned by {}", cmdline));
    }
    obj_part.resize(std::strlen(obj_part.c_str()));
    result[obj_part] = stat;

    Dmsg1(debug_trace, "volume=%s part=%s size=%zu\n", obj_name.data(),
          obj_part.c_str(), stat.size);
  }

  if (auto ret = bph->close(); ret != 0) {
    Dmsg1(debug_info, "list returned %d\n", ret);
    return tl::unexpected(
        fmt::format("Running \"{}\" returned {}\n", cmdline, ret));
  }
  return result;
}

tl::expected<void, std::string> CrudStorage::upload(std::string_view obj_name,
                                                    std::string_view obj_part,
                                                    gsl::span<char> obj_data)
{
  Dmsg1(debug_trace, "upload %s/%s called\n", obj_name.data(), obj_part.data());
  std::string cmdline = fmt::format("\"{}\" upload \"{}\" \"{}\"", m_program,
                                    obj_name, obj_part);

  auto bph{BPipeHandle::create(cmdline.c_str(), m_program_timeout, "rw",
                               m_env_vars)};
  if (!bph) { return tl::unexpected(bph.error()); }
  auto wfh = bph->getWriteFd();

  constexpr size_t max_write_size{256 * 1024};
  size_t remaining_bytes{obj_data.size()};

  while (remaining_bytes > 0) {
    const size_t write_size = std::min(max_write_size, remaining_bytes);
    const size_t offset = obj_data.size() - remaining_bytes;
    if (auto has_written = fwrite(obj_data.data() + offset, 1, write_size, wfh);
        has_written != write_size) {
      if (errno == EINTR) {
        ASSERT(has_written == 0);
        clearerr(wfh);
        continue;
      } else if (errno == EPIPE) {
        return tl::unexpected(
            fmt::format("Broken pipe after writing {} of {} bytes at offset {} "
                        "into {}/{}\n",
                        has_written, write_size, offset, obj_name, obj_part));
      } else {
        return tl::unexpected(fmt::format(
            "Got errno={} after writing {} of {} bytes at offset {} "
            "into {}/{}\n",
            errno, has_written, write_size, offset, obj_name, obj_part));
      }
    }
    bph->reset_timeout();
    remaining_bytes -= write_size;
  }
  auto output = bph->getOutput();
  auto ret = bph->close();
  Dmsg1(debug_trace,
        "upload returned %d\n"
        "== Output ==\n"
        "%s"
        "============\n",
        ret, output.c_str());
  if (ret != 0) {
    return tl::unexpected(fmt::format(
        "Upload failed with returncode={} after data was sent\n", ret));
  }
  return {};
}

tl::expected<gsl::span<char>, std::string> CrudStorage::download(
    std::string_view obj_name,
    std::string_view obj_part,
    gsl::span<char> buffer)
{
  Dmsg1(debug_trace, "download %s/%s called\n", obj_name.data(),
        obj_part.data());
  // download data from somewhere
  std::string cmdline = fmt::format("\"{}\" download \"{}\" \"{}\"", m_program,
                                    obj_name, obj_part);

  auto bph{
      BPipeHandle::create(cmdline.c_str(), m_program_timeout, "r", m_env_vars)};
  if (!bph) { return tl::unexpected(bph.error()); }
  auto rfh = bph->getReadFd();
  size_t total_read{0};
  constexpr size_t max_read_size{256 * 1024};
  do {
    const size_t read_size
        = std::min(buffer.size_bytes() - total_read, max_read_size);
    const size_t bytes_read
        = fread(buffer.data() + total_read, 1, read_size, rfh);
    bph->reset_timeout();
    total_read += bytes_read;
    if (bytes_read < read_size) {
      if (feof(rfh)) {
        return tl::unexpected(
            fmt::format("unexpected EOF after reading {} of {} bytes while "
                        "downloading {}/{}",
                        total_read, buffer.size_bytes(), obj_name, obj_part));
      } else if (ferror(rfh)) {
        if (errno == EINTR) {
          ASSERT(bytes_read == 0);
          clearerr(rfh);
          continue;
        }
        return tl::unexpected(fmt::format(
            "stream error after reading {} of {} bytes while downloading {}/{}",
            total_read, buffer.size_bytes(), obj_name, obj_part));
      }
    }
  } while (total_read < buffer.size_bytes());
  if (fgetc(rfh) != EOF) {
    return tl::unexpected(fmt::format(
        "additional data after expected end of stream while downloading {}/{}",
        obj_name, obj_part));
  }
  if (auto ret = bph->close(); ret != 0) {
    return tl::unexpected(fmt::format(
        "Download failed with returncode={} after data was received\n", ret));
  }
  Dmsg1(debug_trace, "read %zu bytes\n", total_read);
  return buffer;
}

tl::expected<void, std::string> CrudStorage::remove(std::string_view obj_name,
                                                    std::string_view obj_part)
{
  Dmsg1(debug_trace, "remove %s/%s called\n", obj_name.data(), obj_part.data());
  std::string cmdline = fmt::format("\"{}\" remove \"{}\" \"{}\"", m_program,
                                    obj_name, obj_part);
  auto bph{
      BPipeHandle::create(cmdline.c_str(), m_program_timeout, "r", m_env_vars)};
  if (!bph) { return tl::unexpected(bph.error()); }
  auto output = bph->getOutput();
  auto ret = bph->close();

  Dmsg1(debug_trace,
        "remove returned %d\n"
        "== Output ==\n"
        "%s"
        "============\n",
        ret, output.c_str());
  if (ret != 0) {
    return tl::unexpected(
        fmt::format("Running \"{}\" returned {}\n", cmdline, ret));
  }
  return {};
}
