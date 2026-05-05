#include <cassert>
#include <string_view>

#include "aetheris/infrastructure/uuid_generator.hpp"
#include "aetheris/interface/c_api.h"

int main() {
  assert(!std::string_view{aetheris_version()}.empty());
  assert(aetheris_abi_version() == 1);

  aetheris_context* context = nullptr;
  const auto status = aetheris_create_context(&context);
  assert(status.code == AETHERIS_STATUS_OK);
  assert(context != nullptr);
  aetheris_destroy_context(context);

  aetheris::infrastructure::UuidGenerator ids{42};
  const auto session_id = ids.next_session_id();
  assert(session_id.has_value());
  assert(!session_id->value().empty());

  return 0;
}
