// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2025 South Australia Medical Imaging

#ifndef SPIDER_LOGGING_H
#define SPIDER_LOGGING_H

#include <iostream>
#include <ostream>

namespace spider
{

inline std::ostream&
Log()
{
  std::cerr << "[Spider] ";
  return std::cerr;
}

inline std::ostream&
Warning()
{
  std::cerr << "[Spider Warning] ";
  return std::cerr;
}
} // namespace spider

#endif // SPIDER_LOGGING_H
