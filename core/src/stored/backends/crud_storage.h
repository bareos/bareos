#include <map>
#include <optional>
#include <string_view>
#include <unordered_map>
#include <gsl/span>
#include "lib/bstringlist.h"
#include "tl/expected.hpp"

static constexpr int dlvl = 10;

class CrudStorage {
  struct Stat {
    size_t size{0};
  };
  std::string m_program{"/bin/false"};
  std::unordered_map<std::string, std::string> m_env_vars{};

 public:
  tl::expected<void, std::string> set_program(const std::string& program);
  tl::expected<BStringList, std::string> get_supported_options();
  tl::expected<void, std::string> set_option(const std::string& name,
                                             const std::string& value);
  tl::expected<void, std::string> test_connection();
  tl::expected<Stat, std::string> stat(std::string_view obj_name,
                                       std::string_view obj_part);
  tl::expected<std::map<std::string, Stat>, std::string> list(
      std::string_view obj_name);
  tl::expected<void, std::string> upload(std::string_view obj_name,
                                         std::string_view obj_part,
                                         gsl::span<char> obj_data);
  tl::expected<gsl::span<char>, std::string> download(std::string_view obj_name,
                                                      std::string_view obj_part,
                                                      gsl::span<char> buffer);
  tl::expected<void, std::string> remove(std::string_view obj_name,
                                         std::string_view obj_part);
};
