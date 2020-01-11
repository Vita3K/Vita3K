#pragma once

#include <util/preprocessor.h>

#ifdef COMPILE_MODERN_CPP
#if VITA3K_CPP17
#include <optional>
using std::optional;
#elif VITA3K_CPP14
#include <experimental/optional>
using std::experimental::optional;
#endif
#else
#include <boost/optional.hpp>
using boost::optional;
#endif
