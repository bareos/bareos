#include <type_traits>

struct should_be_trivially_copyable
{
  int i;
  int j;
};

int main()
{
  static_assert(std::is_trivially_copyable<should_be_trivially_copyable>::value);
  return 0;
}

