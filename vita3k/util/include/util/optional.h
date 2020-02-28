#pragma once

#if VITA3K_CPP17
#include <optional>
using std::optional;
#elif VITA3K_CPP14
#include <experimental/optional>
using std::experimental::optional;
#else
#include <boost/optional.hpp>
using boost::optional;
#endif
