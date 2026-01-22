// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2025, 2026 South Australia Medical Imaging

#include <cstdio>  // stdout
#include <cstdlib> // EXIT_FAILURE, EXIT_SUCCESS, std::exit
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

#include <gdcmDataSet.h>
#include <gdcmReader.h>

#include "logging.h"
#include "spect.h"

#ifdef _WIN32
#include <io.h> // _isatty, _fileno
#else
#include <unistd.h> // isatty, STDOUT_FILENO
#endif              // _WIN32

namespace
{

bool
StdoutIsTty()
{
#ifdef _WIN32
  return _isatty(_fileno(stdout)) != 0;
#else
  return isatty(STDOUT_FILENO) == 1;
#endif //_WIN32
}

// The name of this program.  Print this instead of argv[0], which
// might be a symlink to a long path.
constexpr char kProgramName[] = "spider_dicom_dump";

bool
ReadDicomFileInDir(const std::string_view dir,
                   std::filesystem::path& path_found, gdcm::Reader& r)
{
  std::error_code ec;
  std::filesystem::directory_iterator it(dir, ec);
  if (ec)
    {
      std::cerr << kProgramName << ": Cannot open directory '" << dir
                << "': " << ec.message() << '\n';
      std::exit(EXIT_FAILURE);
    }
  const std::filesystem::directory_iterator end{};
  for (; it != end; ++it)
    {
      const std::filesystem::directory_entry& e = *it;
      if (!e.is_regular_file())
        continue;
      // Use SetStream instead of SetFileName because filesystem::path
      // is wchar_t on Windows.
      std::ifstream is(e.path(), std::ios::binary);
      if (!is)
        continue;
      r.SetStream(is);
      if (r.Read())
        {
          path_found = e.path();
          return true;
        }
    }
  return false;
}
} // namespace

int
main(int argc, char* argv[])
{
  if (argc < 2)
    {
      std::cerr << "usage: " << kProgramName << " directory ...\n";
      return EXIT_FAILURE;
    }
  if (StdoutIsTty())
    {
      spider::Log()
          << "try piping this program's stdout to another Spider program\n";
    }

  spider::Log() << "Version " << SPIDER_VERSION << "\n";
  std::vector<spider::Spect> spects;
  for (int i = 1; i < argc; ++i)
    {
      std::filesystem::path p;
      gdcm::Reader r;
      if (!ReadDicomFileInDir(argv[i], p, r))
        {
          std::cerr << kProgramName
                    << ": Failed to read a DICOM file in directory '"
                    << argv[i] << "'\n";
          return EXIT_FAILURE;
        }
      const gdcm::DataSet& ds = r.GetFile().GetDataSet();
      spider::Log() << "SPECT " << i << ": reading DICOM attributes in " << p
                    << "...\n";
      spects.emplace_back(spider::ReadDicomSpect(ds));
      spider::Log() << "SPECT " << i << ": " << spects.back() << "\n";
    }

  spider::WriteSpects(spects, std::cout);
  return EXIT_SUCCESS;
}
