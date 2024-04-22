#include "crud_storage.h"
#include "fmt/format.h"
#include "lib/bpipe.h"

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
  bool timed_out() { return bpipe->timer_id && bpipe->timer_id->killed; }
  int close()
  {
    ASSERT(bpipe);
    int ret = CloseBpipe(bpipe);
    bpipe = nullptr;
    return ret;
  }
};

const std::string prog_name{
    "/home/arogge/workspace/bareos/core/scripts/s3cmd-wrapper.sh"};

}  // namespace

bool CrudStorage::test_connection()
{
  Dmsg0(200, "test_connection called\n");
  return true;
}

auto CrudStorage::stat(std::string_view obj_name, std::string_view obj_part)
    -> std::optional<Stat>
{
  Dmsg1(200, "stat %s called\n", obj_name.data());
  std::string cmdline
      = fmt::format("'{}' stat '{}' '{}'", prog_name, obj_name, obj_part);
  auto bph{BPipeHandle(cmdline.c_str(), 30, "r")};
  auto rfh = bph.getReadFd();
  Stat stat;
  if (int n = fscanf(rfh, "%zu\n", &stat.size); n != 1) {
    Dmsg1(200, "fscanf() returned %u\n", n);
    return std::nullopt;
  }
  if (auto ret = bph.close(); ret != 0) {
    Dmsg1(200, "stat returned %u\n", ret);
    return std::nullopt;
  }
  Dmsg1(200, "stat returns %zu\n", stat.size);
  return stat;
}

auto CrudStorage::list(std::string_view obj_spec) -> std::map<std::string, Stat>
{
  Dmsg1(200, "list %s called\n", obj_spec.data());
  // get list of volume files
  return {};
}

bool CrudStorage::upload(std::string_view obj_name,
                         std::string_view obj_part,
                         gsl::span<char> /* obj_data */)
{
  Dmsg1(200, "upload %s/%s called\n", obj_name.data(), obj_part.data());
  // upload data to somewhere
  return true;
}

std::optional<gsl::span<char>> CrudStorage::download(std::string_view obj_name,
                                                     std::string_view obj_part,
                                                     gsl::span<char> buffer)
{
  Dmsg1(200, "download %s/%s called\n", obj_name.data(), obj_part.data());
  // download data from somewhere
  std::string cmdline
      = fmt::format("'{}' download '{}' '{}'", prog_name, obj_name, obj_part);

  auto bph{BPipeHandle(cmdline.c_str(), 30, "r")};
  auto rfh = bph.getReadFd();
  size_t total_read{0};
  constexpr size_t max_read_size{256*1024};
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
  if(fgetc(rfh) != EOF) {
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
  Dmsg1(200, "remove %s/%s called\n", obj_name.data(), obj_part.data());
  return true;
}
