// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 South Australia Medical Imaging

// Escape MS-Windows command line arguments.

#include <string>
#include <string_view>

// Adapted from the Go implementation:
// <https://go.dev/src/syscall/exec_windows.go>.
//
// Copyright 2009 The Go Authors. All rights reserved.
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSES/go.txt file.
//
// Append to OUT the string that a MS-Windows command line parser will
// interpret as the argument ARG.  See also the Win32
// 'CommandLineToArgvW'.
void
AppendEscapeArg(std::string_view arg, std::string& out)
{
  if (arg.empty())
    {
      out += "\"\"";
      return;
    }

  bool needs_backslash = false;
  bool has_space = false;
  for (char c : arg)
    {
      switch (c)
        {
        case '"':
        case '\\':
          needs_backslash = true;
          break;

        case ' ':
        case '\t':
          has_space = true;
        }
    }

  if (!needs_backslash && !has_space)
    {
      out += arg;
      return;
    }
  if (!needs_backslash)
    {
      out += '"';
      out += arg;
      out += '"';
      return;
    }

  if (has_space)
    {
      out += '"';
    }
  // Replace NUM_BACKSLASHES >= 0 before a quote with (2 *
  // NUM_BACKSLASHES + 1) before the quote.
  int num_backslashes = 0;
  for (char c : arg)
    {
      switch (c)
        {
        case '\\':
          ++num_backslashes;
          break;
        case '"':
          while (num_backslashes > 0)
            {
              out += '\\';
              --num_backslashes;
            }
          out += '\\';
          break;
        default:
          num_backslashes = 0;
        }
      out += c;
    }
  // If it has space, we need to add a quote at the end.  In that
  // case, if it ended with NUM_BACKSLASHES, we need it to end with (2
  // * NUM_BACKSLASHES) before the closing quote.
  if (has_space)
    {
      while (num_backslashes > 0)
        {
          out += '\\';
          --num_backslashes;
        }
      out += '"';
    }
}

#ifdef TEST
#undef NDEBUG
#include <assert.h>
#include <stdlib.h>

int
main(void)
{
  std::string result;

  AppendEscapeArg("", result);
  assert(result == "\"\"");

  result.clear();
  AppendEscapeArg("hello", result);
  assert(result == "hello");

  result.clear();
  AppendEscapeArg("hello world", result);
  assert(result == "\"hello world\"");

  result.clear();
  AppendEscapeArg("hello\tworld", result);
  assert(result == "\"hello\tworld\"");

  result.clear();
  AppendEscapeArg("hello\\world", result);
  assert(result == "hello\\world");

  // N backslashes before a quote are replaced with (2N + 1)
  // backslashes before the quote.  E.g. 0 backslashes before a quote:
  result.clear();
  AppendEscapeArg("hello\"world", result);
  assert(result == "hello\\\"world");

  // 0 backslashes before a quote at the end.
  result.clear();
  AppendEscapeArg("hello\"", result);
  assert(result == "hello\\\"");

  // 0 backslashes before a quote at the end, with space.
  result.clear();
  AppendEscapeArg("hello world\"", result);
  assert(result == "\"hello world\\\"\"");

  // 1 backslash before a quote.
  result.clear();
  AppendEscapeArg("hello\\\"world", result);
  assert(result == "hello\\\\\\\"world");

  // 2 backslashes before a quote.
  result.clear();
  AppendEscapeArg("hello\\\\\"world", result);
  assert(result == "hello\\\\\\\\\\\"world");

  // 1 backslash before a quote, with space.
  result.clear();
  AppendEscapeArg("hi hello\\\"world", result);
  assert(result == "\"hi hello\\\\\\\"world\"");

  // If it ends with backslashes and there is no space, the end is
  // unchanged.
  result.clear();
  AppendEscapeArg("hello\\", result);
  assert(result == "hello\\");

  // If it ends with N backslashes and has space, it is changed to end
  // with 2N backlashes before the closing quote.  E.g. this ends with
  // 1 backslash and has space:
  result.clear();
  AppendEscapeArg("hello world\\", result);
  assert(result == "\"hello world\\\\\"");

  // Ends with 2 backslashes and has space.
  result.clear();
  AppendEscapeArg("hello world\\\\", result);
  assert(result == "\"hello world\\\\\\\\\"");

  // 1 backslash before a quote, ends with a backslash, no space.
  result.clear();
  AppendEscapeArg("hello\\\"world\\", result);
  assert(result == "hello\\\\\\\"world\\");

  // 1 backslash before a quote, ends with a backslash, has space.
  result.clear();
  AppendEscapeArg("he llo\\\"world\\", result);
  assert(result == "\"he llo\\\\\\\"world\\\\\"");

  // Check it appends.
  result = "prefix ";
  AppendEscapeArg("hello world", result);
  assert(result == "prefix \"hello world\"");

  return EXIT_SUCCESS;
}
#endif
