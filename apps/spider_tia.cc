// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 South Australia Medical Imaging

#include <algorithm> // std::all_of, std::transform
#include <cctype>    // std::tolower
#include <chrono>
#include <cstdlib> // EXIT_FAILURE, EXIT_SUCCESS
#include <filesystem>
#include <fstream> // std::ifstream
#include <iostream>
#include <stdexcept> // std::runtime_error
#include <string>
#include <string_view>
#include <system_error> // std::error_code
#include <vector>

#include <gdcmDataSet.h>
#include <gdcmReader.h>
#include <itkImage.h>
#include <itkImageFileWriter.h>
#include <itkMacro.h> // itk::ExceptionObject

#include "logging.h"
#include "spect.h"
#include "tia/tia_pipeline.h"
#include "tz_compat.h"

namespace
{

// The name of this program.  Print this instead of argv[0], which
// might be a symlink to a long path.
constexpr char kProgramName[] = "spider_tia";

void
Usage()
{
  std::cerr << "usage: " << kProgramName << " [-fV] [-o output_file]\n"
            << "       " << "{ [-z time_zone] -d directory -i image } ...\n";
}

struct ParsedArguments
{
  bool overwrite = false;
  std::string out_filename;
  std::vector<std::string> tz_names;
  std::vector<std::string> dicom_dirs;
  std::vector<std::string> image_filenames;
};

// Parse program arguments: options (-f, -V) and option-arguments (-o
// output_file, -z time_zone, -d directory, -i image).
ParsedArguments
ParseArguments(int argc, char* argv[])
{
  ParsedArguments out;
  out.out_filename = "tia.nii";
  for (int i = 1; i < argc; ++i)
    {
      const char* arg = argv[i];

      // Reject positional arguments, including a lone "-".
      if (arg[0] != '-' || arg[1] == '\0')
        {
          std::cerr << kProgramName << ": unexpected argument -- " << arg
                    << "\n";
          Usage();
          std::exit(EXIT_FAILURE);
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

          if (opt == 'd')
            {
              const char* zarg = nullptr;
              if (arg[j + 1] != '\0')
                {
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
                  zarg = argv[++i];
                }
              out.dicom_dirs.emplace_back(zarg);
              break;
            }

          if (opt == 'i')
            {
              const char* zarg = nullptr;
              if (arg[j + 1] != '\0')
                {
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
                  zarg = argv[++i];
                }
              out.image_filenames.emplace_back(zarg);
              break;
            }

          if (opt == 'o')
            {
              const char* zarg = nullptr;
              if (arg[j + 1] != '\0')
                {
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
                  zarg = argv[++i];
                }
              out.out_filename = zarg;
              break;
            }

          std::cerr << kProgramName << ": unknown option -- " << opt << "\n";
          Usage();
          std::exit(EXIT_FAILURE);
        }
    }

  return out;
}

std::vector<std::filesystem::path>
OutputFilenames(const std::string& filename)
{
  // itk::ImageFileWriter may write file(s) other than the name passed
  // to SetFileName.  Return the file names that are actually written
  // when FILENAME is passed to SetFileName.
  const std::filesystem::path out_path{ filename };
  const auto ext = out_path.extension();
  if (ext == ".mhd" || ext == ".nhdr")
    {
      // For MetaImage and NRRD header files, a .raw file is written
      // too.
      auto raw = out_path;
      raw.replace_extension(".raw");
      return { out_path, raw };
    }
  auto lower_ext_str = ext.string();
  std::transform(lower_ext_str.begin(), lower_ext_str.end(),
                 lower_ext_str.begin(),
                 [](unsigned char c) { return std::tolower(c); });
  if (lower_ext_str == ".mhd" || (ext != ".mha" && lower_ext_str == ".mha"))
    {
      // A MetaImage extension containing an upper case character
      // causes .mhd and .raw files to be written.
      auto out = out_path;
      out.replace_extension(".mhd");
      auto raw = out_path;
      raw.replace_extension(".raw");
      return { out, raw };
    }
  return { out_path };
}

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
  if (argc == 1)
    {
      Usage();
      std::exit(EXIT_FAILURE);
    }

  const ParsedArguments args = ParseArguments(argc, argv);

  if (args.image_filenames.size() < 2)
    {
      // To fit an exponential.
      std::cerr << kProgramName
                << ": you must specify at least 2 image arguments\n";
      return EXIT_FAILURE;
    }

  if (args.tz_names.size() > 1
      && args.tz_names.size() != args.image_filenames.size())
    {
      std::cerr
          << kProgramName
          << ": when specifying more than one time zone, you must specify the "
             "same number of time zones as image arguments\n";
      return EXIT_FAILURE;
    }

  // Read DICOM attributes for each SPECT.
  if (args.dicom_dirs.size() != args.image_filenames.size())
    {
      std::cerr << kProgramName
                << ": number of image arguments does not match number of "
                   "directory arguments\n";
      return EXIT_FAILURE;
    }

  std::vector<spider::Spect> spects;
  for (std::size_t i = 0; i < args.dicom_dirs.size(); ++i)
    {
      std::filesystem::path p;
      gdcm::Reader r;
      if (!ReadDicomFileInDir(args.dicom_dirs[i], p, r))
        {
          std::cerr << kProgramName
                    << ": Failed to read a DICOM file in directory '"
                    << args.dicom_dirs[i] << "'\n";
          return EXIT_FAILURE;
        }
      const gdcm::DataSet& ds = r.GetFile().GetDataSet();
      spider::Log() << "SPECT " << i + 1 << ": reading DICOM attributes in "
                    << p << "...\n";
      spects.emplace_back(spider::ReadDicomSpect(ds));
      spider::Log() << "SPECT " << i + 1 << ": " << spects.back() << "\n";
    }

  // Make a time zone for each SPECT using the specified time zone
  // names or the current time zone.
  std::vector<const spider::tz::time_zone*> time_zones;
  const auto tz_current = spider::tz::current_zone();
  time_zones.reserve(args.image_filenames.size());
  try
    {
      if (args.tz_names.empty())
        {
          time_zones.assign(args.image_filenames.size(), tz_current);
        }
      else if (args.tz_names.size() == 1)
        {
          time_zones.assign(args.image_filenames.size(),
                            spider::tz::locate_zone(args.tz_names[0]));
        }
      else
        {
          // We already checked that args.tz_names has the same size as
          // args.image_filenames.
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

  std::vector<std::filesystem::path> out_filenames
      = OutputFilenames(args.out_filename);
  if (!args.overwrite)
    {
      for (const auto& p : out_filenames)
        {
          if (std::filesystem::exists(p))
            {
              std::cerr << kProgramName << ": file already exists: " << p
                        << "\n";
              return EXIT_FAILURE;
            }
        }
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

  // Calculate the time delay from administration to the start of each
  // SPECT acquisition.
  std::vector<std::chrono::seconds> elapsed_since_administration(
      acquisition_times.size());
  std::transform(acquisition_times.begin(), acquisition_times.end(),
                 elapsed_since_administration.begin(),
                 [&](const std::chrono::sys_seconds& acquisition_time)
                   { return acquisition_time - administration_time; });

  for (std::size_t i = 0; i < elapsed_since_administration.size(); ++i)
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
                    << ", delay: "
                    << std::chrono::duration<double>(
                           elapsed_since_administration[i])
                               .count()
                           / 3600.0
                    << " h"
                    << ", decay factor: " << decay_factors[i] << "\n";
    }

  // Compute TIA image.
  spider::TiaFilters tia_filters = spider::PrepareTiaPipeline(
      args.image_filenames, elapsed_since_administration, decay_factors);
  using PixelType = float;
  constexpr unsigned int ImageDimension = 3;
  using ImageType = itk::Image<PixelType, ImageDimension>;
  using ImageFileWriterType = itk::ImageFileWriter<ImageType>;
  auto image_file_writer = ImageFileWriterType::New();
  image_file_writer->SetFileName(args.out_filename);
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

  for (const auto& p : out_filenames)
    {
      spider::Log() << "Wrote " << p << "\n";
    }

  return EXIT_SUCCESS;
}
