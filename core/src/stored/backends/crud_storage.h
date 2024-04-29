#include <map>
#include <optional>
#include <string_view>
#include <unordered_map>
#include <gsl/span>
#include "lib/bstringlist.h"

class CrudStorage {
  struct Stat {
    size_t size{0};
  };
  std::string m_program{"/bin/false"};
  std::unordered_map<std::string, std::string> m_env_vars{};

 public:
  bool set_program(const std::string& program);
  BStringList get_supported_options();
  bool set_option(const std::string& name, const std::string& value);
  bool test_connection();
  std::optional<Stat> stat(std::string_view obj_name,
                           std::string_view obj_part);
  std::map<std::string, Stat> list(std::string_view /* obj_name */);
  bool upload(std::string_view obj_name,
              std::string_view obj_part,
              gsl::span<char> obj_data);
  std::optional<gsl::span<char>> download(std::string_view obj_name,
                                          std::string_view obj_part,
                                          gsl::span<char> buffer);
  bool remove(std::string_view obj_name, std::string_view obj_part);
};
