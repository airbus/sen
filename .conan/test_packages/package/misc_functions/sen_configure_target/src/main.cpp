#include <cstdlib>
#include <optional>

std::optional<int> configuredValue();

int main()
{
  const auto value = configuredValue();
  return value && *value == 17 ? EXIT_SUCCESS : EXIT_FAILURE;
}
