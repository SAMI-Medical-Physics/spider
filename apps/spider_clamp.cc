// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 South Australia Medical Imaging

// Set negative pixel values to zero.

#include <cstdio>  // std::fputs, stderr, stdout, std::fputc, std::puts
#include <cstdlib> // EXIT_FAILURE, EXIT_SUCCESS, std::exit
#include <cstring> // std::strcmp
#include <filesystem>
#include <string>
#include <vector>

#include <itkImage.h>
#include <itkImageFileReader.h>
#include <itkImageFileWriter.h>
#include <itkMacro.h> // itk::ExceptionObject
#include <itkUnaryGeneratorImageFilter.h>

#include "logging.h"          // LogLevel, SetLogLevel, DebugF, ErrorF
#include "output_filenames.h" // OutputFilenames

namespace
{

void
Usage()
{
  std::fputs("usage: spider_clamp [-fVvZ] input_file output_file\n", stderr);
}

struct ParsedArguments
{
  spider::LogLevel log_level = spider::LogLevel::kWarn;
  bool overwrite = false;
  bool compress = false;
  std::string in_filename;
  std::string out_filename;
};

// Parse program arguments: options (-f, -V, -v, -Z) and operands
// (input_file, output_file).
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

          if (opt == 'f')
            {
              out.overwrite = true;
              continue;
            }

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

          if (opt == 'Z')
            {
              out.compress = true;
              continue;
            }

          std::fputs("spider_clamp: unknown option -- ", stderr);
          std::fputc(opt, stderr);
          std::fputc('\n', stderr);
          Usage();
          std::exit(EXIT_FAILURE);
        }
    }

  std::vector<const char*> pos_args(argv + i, argv + argc);
  if (pos_args.size() != 2)
    {
      Usage();
      std::exit(EXIT_FAILURE);
    }
  out.in_filename = pos_args[0];
  out.out_filename = pos_args[1];

  return out;
}

} // namespace

int
main(int argc, char* argv[])
{
  const ParsedArguments args = ParseArguments(argc, argv);
  spider::SetLogLevel(args.log_level);

  // Do not overwrite output files unless requested.
  std::vector<std::filesystem::path> out_filenames
      = spider::OutputFilenames(args.out_filename, args.compress);
  if (!args.overwrite)
    {
      for (const auto& p : out_filenames)
        {
          if (std::filesystem::exists(p))
            {
              spider::ErrorF("spider_clamp: file already exists: {}",
                             p.string());
              return EXIT_FAILURE;
            }
        }
    }

  using PixelType = float;
  constexpr unsigned int ImageDimension = 3;
  using ImageType = itk::Image<PixelType, ImageDimension>;

  auto file_reader = itk::ImageFileReader<ImageType>::New();
  file_reader->SetFileName(args.in_filename);

  auto generator_filter
      = itk::UnaryGeneratorImageFilter<ImageType, ImageType>::New();
  generator_filter->SetInput(file_reader->GetOutput());
  generator_filter->SetFunctor([](float y) { return y < 0.0f ? 0.0f : y; });

  auto image_file_writer = itk::ImageFileWriter<ImageType>::New();
  image_file_writer->SetInput(generator_filter->GetOutput());
  image_file_writer->SetFileName(args.out_filename);
  // 'SetUseCompression' has no effect if the output filename ends in
  // ".nii" or ".hdr".
  image_file_writer->SetUseCompression(args.compress);
  try
    {
      image_file_writer->Update();
    }
  catch (const itk::ExceptionObject& ex)
    {
      spider::ErrorF("spider_clamp: {}", ex.what());
      return EXIT_FAILURE;
    }
  for (const auto& p : out_filenames)
    {
      spider::DebugF("Wrote {}", p.string());
    }

  return EXIT_SUCCESS;
}
