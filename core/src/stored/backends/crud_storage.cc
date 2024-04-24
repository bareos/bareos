#include "crud_storage.h"
#include "fmt/format.h"
#include "lib/berrno.h"
#include "lib/bpipe.h"
#include "stored/stored_conf.h"
#include "stored/stored_globals.h"
#include <sys/stat.h>

namespace {
class BPipeHandle {
  Bpipe* bpipe;

 public:
  BPipeHandle(const char* prog, int wait, const char* mode)
      : bpipe(OpenBpipe(prog, wait, mode))
  {
    if (!bpipe) { throw std::system_error(ENOENT, std::generic_category()); }
  }
  ~BPipeHandle()
  {
    if (bpipe) { CloseBpipe(bpipe); }
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
      if (rsize > 0 && !ferror(bpipe->rfd)) { output += std::string(iobuf, rsize); }
    }
    return output;
  }
  bool timed_out() { return bpipe->timer_id && bpipe->timer_id->killed; }
  void close_write() {
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

}  // namespace

bool CrudStorage::set_program(const std::string& program)
{
  if (program[0] != '/') {
    m_program
        = fmt::format("{}/{}", storagedaemon::me->scripts_directory, program);
  } else {
    m_program = program;
  }

  struct stat buffer;
  if (::stat(m_program.c_str(), &buffer) == -1) {
    Dmsg0(100, "program path '%s' does not exist.\n", m_program.c_str());
    return false;
  }

  Dmsg0(200, "using program path '%s'\n", m_program.c_str());
  return true;
}

bool CrudStorage::test_connection()
{
  Dmsg0(200, "test_connection called\n");
  std::string cmdline = fmt::format("'{}' testconnection", m_program);
  auto bph{BPipeHandle(cmdline.c_str(), 30, "r")};
  auto output = bph.getOutput();
  auto ret = bph.close();
  Dmsg1(200,
        "testconnection returned %d\n"
        "== Output ==\n"
        "%s"
        "============\n",
        ret, output.c_str());
  return ret == 0;
}

auto CrudStorage::stat(std::string_view obj_name, std::string_view obj_part)
    -> std::optional<Stat>
{
  Dmsg1(200, "stat %s called\n", obj_name.data());
  std::string cmdline
      = fmt::format("'{}' stat '{}' '{}'", m_program, obj_name, obj_part);
  auto bph{BPipeHandle(cmdline.c_str(), 30, "r")};
  auto rfh = bph.getReadFd();
  Stat stat;
  if (int n = fscanf(rfh, "%zu\n", &stat.size); n != 1) {
    Dmsg1(200, "fscanf() returned %d\n", n);
    return std::nullopt;
  }
  if (auto ret = bph.close(); ret != 0) {
    Dmsg1(200, "stat returned %d\n", ret);
    return std::nullopt;
  }
  Dmsg1(200, "stat returns %zu\n", stat.size);
  return stat;
}

auto CrudStorage::list(std::string_view obj_name) -> std::map<std::string, Stat>
{
  // TODO: better error handling, we should know if something actually went
  // wrong on the way or if the list is simply empty.
  Dmsg1(5, "list %s called\n", obj_name.data());
  std::string cmdline = fmt::format("'{}' list '{}'", m_program, obj_name);
  auto bph{BPipeHandle(cmdline.c_str(), 30, "r")};
  auto rfh = bph.getReadFd();

  std::map<std::string, Stat> result;
  while (!feof(rfh)) {
    Stat stat;
    auto obj_part = std::string(128, '\0');
    if (int n = fscanf(rfh, "%128s %zu\n", obj_part.data(), &stat.size);
        n != 2) {
      Dmsg1(200, "fscanf() returned %d\n", n);
      return {};
    }
    obj_part.resize(std::strlen(obj_part.c_str()));
    result[obj_part] = stat;

    Dmsg1(5, "volume=%s part=%s size=%zu\n", obj_name.data(), obj_part.c_str(),
          stat.size);
  }

  if (auto ret = bph.close(); ret != 0) {
    Dmsg1(200, "list returned %d\n", ret);
    return {};
  }
  return result;
}

bool CrudStorage::upload(std::string_view obj_name,
                         std::string_view obj_part,
                         gsl::span<char> obj_data)
{
  Dmsg1(5, "upload %s/%s called\n", obj_name.data(), obj_part.data());
  std::string cmdline
      = fmt::format("'{}' upload '{}' '{}'", m_program, obj_name, obj_part);

  auto bph{BPipeHandle(cmdline.c_str(), 30, "rw")};
  auto wfh = bph.getWriteFd();

  constexpr size_t max_write_size{256 * 1024};
  size_t remaining_bytes{obj_data.size()};

  while (remaining_bytes > 0) {
    size_t write_size = std::min(max_write_size, remaining_bytes);
    if (write_size
        != fwrite(obj_data.data() + (obj_data.size() - remaining_bytes), 1,
                  write_size, wfh)) {
      return false;
    }
    remaining_bytes -= write_size;
  }
  auto output = bph.getOutput();
  auto ret = bph.close();
  Dmsg1(200,
        "upload returned %d\n"
        "== Output ==\n"
        "%s"
        "============\n",
        ret, output.c_str());
  return ret == 0;
}

std::optional<gsl::span<char>> CrudStorage::download(std::string_view obj_name,
                                                     std::string_view obj_part,
                                                     gsl::span<char> buffer)
{
  Dmsg1(200, "download %s/%s called\n", obj_name.data(), obj_part.data());
  // download data from somewhere
  std::string cmdline
      = fmt::format("'{}' download '{}' '{}'", m_program, obj_name, obj_part);

  auto bph{BPipeHandle(cmdline.c_str(), 30, "r")};
  auto rfh = bph.getReadFd();
  size_t total_read{0};
  constexpr size_t max_read_size{256 * 1024};
  do {
    const size_t read_size
        = std::min(buffer.size_bytes() - total_read, max_read_size);
    const size_t bytes_read
        = fread(buffer.data() + total_read, 1, read_size, rfh);
    total_read += bytes_read;
    if (bytes_read < read_size) {
      if (feof(rfh)) {
        Dmsg1(200, "early EOF\n");
        // early EOF
        return std::nullopt;
      } else if (ferror(rfh)) {
        Dmsg1(200, "some ERROR\n");
        // some ERROR
        return std::nullopt;
      }
    }
  } while (total_read < buffer.size_bytes());
  if (fgetc(rfh) != EOF) {
    Dmsg1(200, "extra data after desired end\n");
    return std::nullopt;
  }
  if (bph.close() != 0) {
    Dmsg1(200, "script exited non-zero\n");
    return std::nullopt;
  }
  Dmsg1(200, "read %zu bytes\n", total_read);
  return buffer;
}

bool CrudStorage::remove(std::string_view obj_name, std::string_view obj_part)
{
  Dmsg1(5, "remove %s/%s called\n", obj_name.data(), obj_part.data());
  std::string cmdline = fmt::format("'{}' remove '{}' '{}'", m_program, obj_name, obj_part);
  auto bph{BPipeHandle(cmdline.c_str(), 30, "r")};
  auto output = bph.getOutput();
  auto ret = bph.close();

  Dmsg1(200,
        "remove returned %d\n"
        "== Output ==\n"
        "%s"
        "============\n",
        ret, output.c_str());
  return ret == 0;
}
