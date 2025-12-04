// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2025 South Australia Medical Imaging

#ifndef SPIDER_LOGGING_H
#define SPIDER_LOGGING_H

namespace spider
{
inline std::ostream&
Log()
{
  std::cerr << "\033[34m[Spider]\033[0m ";
  return std::cerr;
}
} // namespace spider

#endif // SPIDER_LOGGING_H
