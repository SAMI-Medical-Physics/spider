// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 South Australia Medical Imaging

#include <algorithm> // std::all_of, std::transform
#include <chrono>
#include <cstdio>  // stdin
#include <cstdlib> // EXIT_FAILURE, EXIT_SUCCESS
#include <cstring> // std::strcmp
#include <filesystem>
#include <iostream>
#include <stdexcept> // std::runtime_error
#include <string>
#include <vector>

#include <itkImage.h>
#include <itkImageFileWriter.h>
#include <itkMacro.h> // itk::ExceptionObject

#include "logging.h"
#include "spect.h"
#include "tia/tia_pipeline.h"
#include "tz_compat.h"

#ifdef _WIN32
#include <io.h> // _isatty, _fileno
#else
#include <unistd.h> // isatty
#endif              // _WIN32

namespace
{

bool
StdinIsTty()
{
#ifdef _WIN32
  return _isatty(_fileno(stdin)) != 0;
#else
  return isatty(STDIN_FILENO) == 1;
#endif //_WIN32
}

// The name of this program.  Print this instead of argv[0], which
// might be a symlink to a long path.
constexpr char kProgramName[] = "spider_tia";

void
Usage()
{
  std::cerr << "usage: " << kProgramName
            << " [-fV] [-z time_zone] image1 image2 ... < file\n"
            << "       " << "spider_dicom_dump directory1 directory2 ... |\n"
            << "           " << kProgramName << " image1 image2 ...\n";
}

struct ParsedArguments
{
  bool overwrite = false;
  std::vector<std::string> tz_names;
  std::vector<std::string> input_filenames;
};

// Parse program arguments in the style of POSIX shell getopts:
// options (-f, -V) and option-arguments (-z time_zone) first, then
// positional arguments (image filenames).
ParsedArguments
ParseArguments(int argc, char* argv[])
{
  ParsedArguments out;
  int i = 1;
  for (; i < argc; ++i)
    {
      const char* arg = argv[i];

      if (std::strcmp(arg, "--") == 0)
        {
          ++i; // consume "--"
          break;
        }

      // Stop at the first non-option.  A lone "-" is treated as a
      // positional argument.
      if (arg[0] != '-' || arg[1] == '\0')
        {
          break;
        }

      // Parse a cluster of options.
      for (int j = 1; arg[j] != '\0'; ++j)
        {
          const char opt = arg[j];

          if (opt == 'f')
            {
              out.overwrite = true;
              continue;
            }

          if (opt == 'V')
            {
              std::cout << "Spider " << SPIDER_VERSION << "\n";
              std::exit(EXIT_SUCCESS);
            }

          if (opt == 'z')
            {
              // Support both "-zUTC" and "-z UTC", for example.
              const char* zarg = nullptr;
              if (arg[j + 1] != '\0')
                {
                  // The time zone argument is the remainder of arg
                  // after "-z".
                  zarg = arg + j + 1;
                }
              else
                {
                  if (i + 1 == argc)
                    {
                      std::cerr << kProgramName
                                << ": option requires an argument -- " << opt
                                << "\n";
                      Usage();
                      std::exit(EXIT_FAILURE);
                    }
                  // The time zone argument is the next argv element.
                  zarg = argv[++i]; // consume time zone argument
                }
              out.tz_names.emplace_back(zarg);
              break; // finished with argv[i]
            }

          std::cerr << kProgramName << ": unknown option -- " << opt << "\n";
          Usage();
          std::exit(EXIT_FAILURE);
        }
    }

  out.input_filenames.assign(argv + i, argv + argc);
  return out;
}

} // namespace

int
main(int argc, char* argv[])
{
  if (argc == 1)
    {
      Usage();
      std::exit(EXIT_FAILURE);
    }

  const ParsedArguments args = ParseArguments(argc, argv);

  if (args.input_filenames.size() < 2)
    {
      // To fit an exponential.
      std::cerr << kProgramName
                << ": you must specify at least 2 image arguments\n";
      return EXIT_FAILURE;
    }

  if (args.tz_names.size() > 1 && args.tz_names.size() != args.input_filenames.size())
    {
      std::cerr
          << kProgramName
          << ": when specifying more than one time zone, you must specify the "
             "same number of time zones as image arguments\n";
      return EXIT_FAILURE;
    }

  // Parse DICOM attributes for each SPECT in stdin.
  if (StdinIsTty())
    {
      std::cerr << kProgramName << ": waiting for input on stdin...\n";
    }
  const std::vector<spider::Spect> spects = spider::ReadSpects(std::cin);
  if (spects.size() != args.input_filenames.size())
    {
      std::cerr
          << kProgramName
          << ": number of image arguments does not match input on stdin\n";
      return EXIT_FAILURE;
    }

  // Make a time zone for each SPECT using the specified time zone
  // names or the current time zone.
  std::vector<const spider::tz::time_zone*> time_zones;
  const auto tz_current = spider::tz::current_zone();
  time_zones.reserve(args.input_filenames.size());
  try
    {
      if (args.tz_names.empty())
        {
          time_zones.assign(args.input_filenames.size(), tz_current);
        }
      else if (args.tz_names.size() == 1)
        {
          time_zones.assign(args.input_filenames.size(),
                            spider::tz::locate_zone(args.tz_names[0]));
        }
      else
        {
          // We already checked that args.tz_names has the same size as
          // args.input_filenames.
          for (const auto& name : args.tz_names)
            time_zones.push_back(spider::tz::locate_zone(name));
        }
    }
  catch (const std::runtime_error& ex)
    {
      // Catch 'tzdb: cannot locate zone'.
      std::cerr << kProgramName << ": " << ex.what() << '\n';
      return EXIT_FAILURE;
    }

  const std::string out_filename = "tia.nii";
  if (!args.overwrite && std::filesystem::exists(out_filename))
    {
      std::cerr << kProgramName << ": file already exists: " << out_filename
                << "\n";
      return EXIT_FAILURE;
    }

  spider::Log() << "Version " << SPIDER_VERSION << "\n";

  std::vector<std::chrono::sys_seconds> administration_times;
  administration_times.reserve(spects.size());
  std::vector<std::chrono::sys_seconds> acquisition_times;
  acquisition_times.reserve(spects.size());
  std::vector<double> decay_factors;
  decay_factors.reserve(spects.size());
  for (std::size_t i = 0; i < spects.size(); ++i)
    {
      spider::Log() << "SPECT " << i + 1 << ": " << spects[i] << "\n";

      if (UsesTimeZone(spects[i]))
        spider::Warning()
            << "SPECT " << i + 1
            << ": assuming Dates (DA), Times (TM), and Date Times (DT) "
               "without a UTC offset suffix are in time zone "
            << time_zones[i]->name() << "\n";

      // Make administration time point.
      const auto administration_time
          = spider::MakeRadiopharmaceuticalStartSysTime(spects[i],
                                                        time_zones[i]);
      if (!administration_time.has_value())
        {
          std::cerr << kProgramName << ": SPECT " << i + 1 << ": "
                    << spider::ToString(administration_time.error()) << "\n";
          return EXIT_FAILURE;
        }
      administration_times.push_back(administration_time.value());

      // Make acquisition time point.
      const auto acquisition_time
          = spider::MakeAcquisitionSysTime(spects[i], time_zones[i]);
      if (!acquisition_time.has_value())
        {
          std::cerr << kProgramName << ": SPECT " << i + 1 << ": "
                    << spider::ToString(acquisition_time.error()) << "\n";
          return EXIT_FAILURE;
        }
      acquisition_times.push_back(acquisition_time.value());

      // Compute decay factor.
      const auto decay_factor
          = spider::ComputeDecayFactor(spects[i], time_zones[i]);
      if (!decay_factor.has_value())
        {
          std::cerr << kProgramName << ": SPECT " << i + 1 << ": "
                    << spider::ToString(decay_factor.error()) << "\n";
          return EXIT_FAILURE;
        }
      decay_factors.push_back(decay_factor.value());
    }

  if (!std::all_of(administration_times.begin() + 1,
                   administration_times.end(),
                   [&](const std::chrono::sys_seconds& st)
                     { return st == administration_times.front(); }))
    spider::Warning()
        << "administration date time differs for two or more SPECTs\n";
  const auto administration_time = administration_times.front();

  // Calculate the time delay (in hours) from administration to the
  // start of each SPECT acquisition.
  std::vector<double> hours_since_administration(acquisition_times.size());
  std::transform(acquisition_times.begin(), acquisition_times.end(),
                 hours_since_administration.begin(),
                 [&](const std::chrono::sys_seconds& acquisition_time)
                   {
                     const std::chrono::seconds duration
                         = acquisition_time - administration_time;
                     return std::chrono::duration<double>(duration).count()
                            / (60.0 * 60.0);
                   });

  for (std::size_t i = 0; i < hours_since_administration.size(); ++i)
    {
      spider::Log() << "SPECT " << i + 1 << ": administration: "
                    << spider::tz::zoned_time<
                           // The time can be presented in any time
                           // zone.
                           std::chrono::seconds>{ time_zones[i],
                                                  administration_times[i] }
                    << ", acquisition: "
                    << spider::tz::zoned_time<
                           std::chrono::seconds>{ time_zones[i],
                                                  acquisition_times[i] }
                    << ", delay: " << hours_since_administration[i] << " h"
                    << ", decay factor: " << decay_factors[i] << "\n";
    }

  // Compute TIA image.
  spider::TiaFilters tia_filters = spider::PrepareTiaPipeline(
      hours_since_administration, args.input_filenames, decay_factors);
  using PixelType = float;
  constexpr unsigned int ImageDimension = 3;
  using ImageType = itk::Image<PixelType, ImageDimension>;
  using ImageFileWriterType = itk::ImageFileWriter<ImageType>;
  auto image_file_writer = ImageFileWriterType::New();
  image_file_writer->SetFileName(out_filename);
  image_file_writer->SetInput(tia_filters.GetFinalFilter()->GetOutput());
  spider::Log() << "Executing TIA image pipeline\n";
  try
    {
      image_file_writer->Update();
    }
  catch (const itk::ExceptionObject& ex)
    {
      std::cerr << kProgramName << ": " << ex << "\n";
      return EXIT_FAILURE;
    }
  spider::Log() << "Wrote " << out_filename << "\n";

  return EXIT_SUCCESS;
}
