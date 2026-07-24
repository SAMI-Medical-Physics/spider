// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 South Australia Medical Imaging

// Compute radiopharmaceutical administration identifier.

#include <cstdio>  // std::fputs, stderr, stdout, std::fputc,
                   // std::puts
#include <cstdlib> // EXIT_FAILURE, EXIT_SUCCESS, std::exit
#include <cstring> // std::strcmp
#include <string>

#include "logging.h"      // LogLevel, SetLogLevel, DebugF, ErrorF
#include "spect.h"        // ReadSpectFromDirectory
#include "spect_format.h" // DebugF with Spect argument

namespace
{

void
Usage()
{
  std::fputs("usage: spider_id [-Vv] directory\n", stderr);
}

struct ParsedArguments
{
  spider::LogLevel log_level = spider::LogLevel::kWarn;
  std::string dicom_dir;
};

// Parse program arguments: options (-V, -v) and operand (directory).
ParsedArguments
ParseArguments(int argc, char* argv[])
{
  ParsedArguments out;
  int i = 1;
  for (; i < argc; ++i)
    {
      const char* arg = argv[i];

      // Detect start of operands.
      if (std::strcmp(arg, "--") == 0)
        {
          ++i;
          break;
        }
      if (arg[0] != '-' || arg[1] == '\0')
        break;

      // Parse a cluster of options.
      for (int j = 1; arg[j] != '\0'; ++j)
        {
          const char opt = arg[j];

          if (opt == 'V')
            {
              std::fputs("Spider ", stdout);
              std::puts(SPIDER_VERSION);
              std::exit(EXIT_SUCCESS);
            }

          if (opt == 'v')
            {
              out.log_level = spider::LogLevel::kDebug;
              continue;
            }

          std::fputs("spider_id: unknown option -- ", stderr);
          std::fputc(opt, stderr);
          std::fputc('\n', stderr);
          Usage();
          std::exit(EXIT_FAILURE);
        }
    }

  // Require exactly 1 operand.
  if (i != argc - 1)
    {
      Usage();
      std::exit(EXIT_FAILURE);
    }
  out.dicom_dir = argv[i];

  return out;
}

void
PrintSanitised(const std::string& s)
{
  for (char c : s)
    {
      // Omit spaces and characters that cannot appear in file names
      // on one or more platforms.
      if (c != ' ' && c != '/' && c != '<' && c != '>' && c != ':' && c != '"'
          && c != '\\' && c != '|' && c != '?' && c != '*')
        std::fputc(c, stdout);
    }
}

} // namespace

int
main(int argc, char* argv[])
{
  const ParsedArguments args = ParseArguments(argc, argv);
  spider::SetLogLevel(args.log_level);

  const auto spect = spider::ReadSpectFromDirectory(args.dicom_dir);
  if (!spect.has_value())
    {
      spider::ErrorF("spider_id: failed to read SPECT "
                     "information from directory '{}': {}",
                     args.dicom_dir, spider::ToString(spect.error()));
      return EXIT_FAILURE;
    }
  spider::DebugF("{}", spect.value());

  // Print the radiopharmaceutical administration identifier.
  if (spect.value().patient_name.has_value())
    PrintSanitised(spect.value().patient_name.value());
  std::fputs("--", stdout);

  if (spect.value().patient_id.has_value())
    PrintSanitised(spect.value().patient_id.value());
  std::fputs("__", stdout);

  if (spect.value().radionuclide.has_value())
    PrintSanitised(spect.value().radionuclide.value());
  std::fputs("++", stdout);

  if (spect.value().radiopharmaceutical_start_date_time.has_value())
    {
      for (char c : spect.value().radiopharmaceutical_start_date_time.value())
        {
          if (c == '.')
            break;
          // Omit spaces and characters that cannot appear in file
          // names on one or more platforms.
          if (c != ' ' && c != '/' && c != '<' && c != '>' && c != ':'
              && c != '"' && c != '\\' && c != '|' && c != '?' && c != '*')
            std::fputc(c, stdout);
        }
    }
  std::fputc('\n', stdout);

  return EXIT_SUCCESS;
}
