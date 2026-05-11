// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 South Australia Medical Imaging

#include "logging.h"

#include <cstdio> // std::fwrite, std::fputc, stderr
#include <string_view>

namespace spider
{

namespace
{

// The log level for the Spider library.  Internal linkage; it can
// only be modified by SetLogLevel.
LogLevel log_level = LogLevel::kWarn;

void
DoLog(std::string_view msg)
{
  std::fwrite(msg.data(), sizeof(char), msg.size(), stderr);
  std::fputc('\n', stderr);
}
} // namespace

void
SetLogLevel(LogLevel level)
{
  log_level = level;
}

bool
LogLevelEnabled(LogLevel level)
{
  return log_level >= level;
}

void
Error(std::string_view msg)
{
  if (LogLevelEnabled(LogLevel::kError))
    DoLog(msg);
}

void
Warning(std::string_view msg)
{
  if (LogLevelEnabled(LogLevel::kWarn))
    DoLog(msg);
}

void
Debug(std::string_view msg)
{
  if (LogLevelEnabled(LogLevel::kDebug))
    DoLog(msg);
}
} // namespace spider
