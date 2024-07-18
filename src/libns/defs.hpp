#pragma once

#include <system_error>
#include <tl/expected.hpp>

template <class T>

using expected = tl::expected<T, std::error_code>;
using tl::unexpected;
