// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2025 South Australia Medical Imaging

#ifndef SPIDER_TZ_COMPAT_H
#define SPIDER_TZ_COMPAT_H

#include <chrono>

// SPIDER_HAVE_STD_CHRONO_TZ is a CMake compile definition.
#if !SPIDER_HAVE_STD_CHRONO_TZ
#include <date/tz.h>
#endif

namespace spider
{

#if SPIDER_HAVE_STD_CHRONO_TZ
namespace tz = std::chrono;
#else
namespace tz = date;
#endif

} // namespace spider

#endif // SPIDER_TZ_COMPAT_H
