#include <map>
#include <optional>
#include <string_view>
#include <gsl/span>

class CrudStorage {
  struct Stat {
    size_t size{0};
  };
  std::string m_program{"/bin/false"};

 public:
  bool set_program(const std::string& program);
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
