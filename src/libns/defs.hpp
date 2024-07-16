#pragma once

#include <tl/expected.hpp>
#include <system_error>

template<class T>

using expected = tl::expected<T, std::error_code>;


