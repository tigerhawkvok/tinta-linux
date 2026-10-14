// Force-included into every Linux C++ translation unit (platform/linux/linux.cmake).
// MSVC's standard headers pull in <cstdint>/<cstring> transitively and its CRT
// provides _stricmp, swprintf_s and friends, so shared code uses them without
// naming a header. Providing them here keeps shared files identical to upstream.
#ifndef TINTA_LINUX_PRELUDE_H
#define TINTA_LINUX_PRELUDE_H

#include <climits>
#include <cstdint>
#include <cstring>
#include <cwctype>

#include "msvc_crt.h"

#endif  // TINTA_LINUX_PRELUDE_H
