// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2025, 2026 South Australia Medical Imaging

#ifndef SPIDER_LOGGING_H
#define SPIDER_LOGGING_H

#include <cstdio> // stderr
#include <format> // std::format_string
#include <print>  // std::println
#include <string_view>
#include <utility> // std::forward

namespace spider
{

enum class LogLevel
{
  kQuiet,
  kError,
  kWarn,
  kInfo,
  kDebug
};

// Set the log level for the Spider library.  XXX: Not thread-safe;
// call before multithreaded work starts.
void
SetLogLevel(LogLevel level);

bool
LogLevelEnabled(LogLevel level);

// The functions below print a message to standard error when enabled
// by the log level.  They all append a newline.

// Error messages are shown at log levels kError, kWarn, kInfo, and
// kDebug.
void
Error(std::string_view msg);

// "F" denotes a formatted variant; FMT is formatted using ARGS as
// with std::format and std::print.
template <typename... Args>
void
ErrorF(std::format_string<Args...> fmt, Args&&... args)
{
  if (LogLevelEnabled(LogLevel::kError))
    std::println(stderr, fmt, std::forward<Args>(args)...);
}

// Warning messages are shown at log levels kWarn, kInfo, and kDebug.
void
Warning(std::string_view msg);

template <typename... Args>
void
WarningF(std::format_string<Args...> fmt, Args&&... args)
{
  if (LogLevelEnabled(LogLevel::kWarn))
    std::println(stderr, fmt, std::forward<Args>(args)...);
}

// Debug messages are shown at log level kDebug.
void
Debug(std::string_view msg);

template <typename... Args>
void
DebugF(std::format_string<Args...> fmt, Args&&... args)
{
  if (LogLevelEnabled(LogLevel::kDebug))
    std::println(stderr, fmt, std::forward<Args>(args)...);
}
} // namespace spider

#endif // SPIDER_LOGGING_H
