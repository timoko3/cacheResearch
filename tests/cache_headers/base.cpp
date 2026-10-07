#include <type_traits>

#include "cache/cache.h"

static_assert(std::is_abstract_v<cache::Cache<int>>);
static_assert(std::has_virtual_destructor_v<cache::Cache<int>>);
