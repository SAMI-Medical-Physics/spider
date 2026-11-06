// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 South Australia Medical Imaging

// Compare two images.

#include <math.h>   // round
#include <stdio.h>  // fputc, fputs, puts, stderr, stdout,
#include <stdlib.h> // exit, EXIT_FAILURE, EXIT_SUCCESS, strtof
#include <string.h> // strcmp

#include <filesystem>
#include <optional>
#include <string>

#include <itkBinaryFunctorImageFilter.h>
#include <itkClampImageFilter.h>
#include <itkFlipImageFilter.h>
#include <itkImage.h>
#include <itkImageFileReader.h>
#include <itkImageSeriesWriter.h>
#include <itkNumericSeriesFileNames.h>
#include <itkResampleImageFilter.h>
#include <itkRescaleIntensityImageFilter.h>
#include <itkTernaryFunctorImageFilter.h>

namespace
{

void
Usage()
{
  fputs(
"usage: spider_image_compare [-frVv] [-b background] [-l window_level]\n"
"                            [-o output_directory] [-w window_width] image1\n"
"                            image2\n",
      stderr);
}

struct ParsedArguments
{
  // We don't use spider::LogLevel from src/logging.h because this
  // program doesn't use any Spider libraries.
  bool verbose;
  bool overwrite;
  bool reverse_row_order;
  std::optional<float> window_level;
  std::optional<float> window_width;
  char const* background_filename;
  char const* output_directory;
  char const* image1_filename;
  char const* image2_filename;
};

// Parse program arguments: options (-f, -r, -V, -v), option-arguments
// (-b background, -l window_level, -o output_directory, -w
// window_width), and operands (image1, image2).  background_filename
// and output_directory are null or set to argv members;
// image1_filename and image2_filename are set to argv members.
ParsedArguments
ParseArguments(int argc, char const* const argv[])
{
  ParsedArguments out{};
  int i = 1;
  for (; i < argc; ++i)
    {
      char const* arg = argv[i];

      // Detect start of operands.
      if (strcmp(arg, "--") == 0)
        {
          ++i;
          break;
        }
      if (arg[0] != '-' || arg[1] == '\0')
        {
          break;
        }

      // Parse a cluster of options.
      for (int j = 1; arg[j] != '\0'; ++j)
        {
          const char opt = arg[j];

          if (opt == 'b')
            {
              const char* barg = nullptr;
              if (arg[j + 1] != '\0')
                {
                  // The background argument is the remainder of arg
                  // after "-b".
                  barg = arg + j + 1;
                }
              else
                {
                  if (i + 1 == argc)
                    {
                      fputs("spider_image_compare: option requires an "
                            "argument -- b\n",
                            stderr);
                      Usage();
                      exit(EXIT_FAILURE);
                    }
                  // The background argument is the next argv element.
                  barg = argv[++i];
                }
              out.background_filename = barg;
              break; // finished with argv[i]
            }

          if (opt == 'f')
            {
              out.overwrite = true;
              continue;
            }

          if (opt == 'l')
            {
              const char* larg = nullptr;
              if (arg[j + 1] != '\0')
                {
                  larg = arg + j + 1;
                }
              else
                {
                  if (i + 1 == argc)
                    {
                      fputs("spider_image_compare: option requires an "
                            "argument -- l\n",
                            stderr);
                      Usage();
                      exit(EXIT_FAILURE);
                    }
                  larg = argv[++i];
                }
              out.window_level = strtof(larg, nullptr);
              break;
            }

          if (opt == 'o')
            {
              const char* oarg = nullptr;
              if (arg[j + 1] != '\0')
                {
                  oarg = arg + j + 1;
                }
              else
                {
                  if (i + 1 == argc)
                    {
                      fputs("spider_image_compare: option requires an "
                            "argument -- o\n",
                            stderr);
                      Usage();
                      exit(EXIT_FAILURE);
                    }
                  oarg = argv[++i];
                }
              out.output_directory = oarg;
              break;
            }

          if (opt == 'r')
            {
              out.reverse_row_order = true;
              continue;
            }

          if (opt == 'V')
            {
              fputs("Spider ", stdout);
              puts(SPIDER_VERSION);
              exit(EXIT_SUCCESS);
            }

          if (opt == 'v')
            {
              out.verbose = true;
              continue;
            }

          if (opt == 'w')
            {
              const char* warg = nullptr;
              if (arg[j + 1] != '\0')
                {
                  warg = arg + j + 1;
                }
              else
                {
                  if (i + 1 == argc)
                    {
                      fputs("spider_image_compare: option requires an "
                            "argument -- w\n",
                            stderr);
                      Usage();
                      exit(EXIT_FAILURE);
                    }
                  warg = argv[++i];
                }
              out.window_width = strtof(warg, nullptr);
              break;
            }

          fputs("spider_image_compare: unknown option -- ", stderr);
          fputc(opt, stderr);
          fputc('\n', stderr);
          Usage();
          exit(EXIT_FAILURE);
        }
    }

  if (i != argc - 2)
    {
      if (argc != 1)
        {
          fprintf(stderr, "spider_image_compare requires 2 operands\n");
        }
      Usage();
      exit(EXIT_FAILURE);
    }
  out.image1_filename = argv[argc - 2];
  out.image2_filename = argv[argc - 1];

  return out;
}

// Background image pre-processing.  The background image is expected
// to be co-registered with image1 but may have different size,
// spacing, origin, and direction.  Resample it onto image1, apply a
// window if requested, then rescale it to 0-255.0f in preparation for
// compositing using TernaryComposeFunctor.
struct ReaderResampleClampRescaleFilters
{
  using Image3DFloatType = itk::Image<float, 3>;
  using ImageFileReaderType = itk::ImageFileReader<Image3DFloatType>;
  using ResampleFilterType
      = itk::ResampleImageFilter<Image3DFloatType, Image3DFloatType>;
  using ClampFilterType
      = itk::ClampImageFilter<Image3DFloatType, Image3DFloatType>;
  using RescaleFilterType
      = itk::RescaleIntensityImageFilter<Image3DFloatType, Image3DFloatType>;

  ImageFileReaderType::Pointer reader;
  ResampleFilterType::Pointer resample_filter;
  ClampFilterType::Pointer clamp_filter;
  RescaleFilterType::Pointer rescale_filter;
};

ReaderResampleClampRescaleFilters
ReaderResampleClampRescale(const char* image_filename,
                           const itk::Image<float, 3>* reference_image,
                           std::optional<float> window_level,
                           std::optional<float> window_width)
{
  using Image3DFloatType = itk::Image<float, 3>;
  using ImageFileReaderType = itk::ImageFileReader<Image3DFloatType>;
  auto reader = ImageFileReaderType::New();
  reader->SetFileName(image_filename);

  using ResampleFilterType
      = itk::ResampleImageFilter<Image3DFloatType, Image3DFloatType>;
  auto resample_filter = ResampleFilterType::New();
  resample_filter->SetInput(reader->GetOutput());
  resample_filter->SetReferenceImage(reference_image);
  resample_filter->UseReferenceImageOn();
  resample_filter->UpdateOutputInformation();

  using ClampFilterType
      = itk::ClampImageFilter<Image3DFloatType, Image3DFloatType>;
  ClampFilterType::Pointer clamp_filter;
  if (window_level.has_value() && window_width.has_value())
    {
      clamp_filter = ClampFilterType::New();
      clamp_filter->SetInput(resample_filter->GetOutput());
      clamp_filter->SetBounds(*window_level - *window_width / 2.0,
                              *window_level + *window_width / 2.0);
    }

  using RescaleFilterType
      = itk::RescaleIntensityImageFilter<Image3DFloatType, Image3DFloatType>;
  auto rescale_filter = RescaleFilterType::New();
  rescale_filter->SetInput(
      (window_level.has_value() && window_width.has_value())
          ? clamp_filter->GetOutput()
          : resample_filter->GetOutput());
  rescale_filter->SetOutputMinimum(0.0f);
  rescale_filter->SetOutputMaximum(255.0f);

  return ReaderResampleClampRescaleFilters{ .reader = reader,
                                            .resample_filter = resample_filter,
                                            .clamp_filter = clamp_filter,
                                            .rescale_filter = rescale_filter };
}

// The 4 corner colours used to construct a RGB image from image1 and
// image2 using bilinear interpolation:
// <https://en.wikipedia.org/wiki/Bilinear_interpolation>.
constexpr unsigned char kColourLowLow[] = { 0, 0, 0 };       // black
constexpr unsigned char kColourHighLow[] = { 255, 255, 0 };  // yellow
constexpr unsigned char kColourLowHigh[] = { 0, 0, 255 };    // blue
constexpr unsigned char kColourHighHigh[] = { 255, 0, 255 }; // magenta

class TernaryComposeFunctor
{
public:
  using InPixelType1 = float; // image1
  using InPixelType2 = float; // image2
  using InPixelType3 = float; // background image
  using OutPixelType = itk::RGBPixel<unsigned char>;

  OutPixelType
  operator()(const InPixelType1& y1, const InPixelType2& y2,
             const InPixelType3& y3) const
  {
    // Compose y1 in 0-1.0f (from image1) and y2 in 0-1.0f (from
    // image2) to RGB using bilinear interpolation on the unit square
    // (<https://en.wikipedia.org/wiki/Bilinear_interpolation>) and
    // alpha blend with y3 in 0-255.0f (from the background image).
    OutPixelType out;
    constexpr double kAlpha = 0.7; // opacity (0-1) of colour overlay
    out.SetRed(round((1.0 - kAlpha) * y3
                     + kAlpha
                           * (kColourLowLow[0] * (1 - y1) * (1 - y2)
                              + kColourLowHigh[0] * (1 - y1) * y2
                              + kColourHighLow[0] * y1 * (1 - y2)
                              + kColourHighHigh[0] * y1 * y2)));
    out.SetGreen(round((1.0 - kAlpha) * y3
                       + kAlpha
                             * (kColourLowLow[1] * (1 - y1) * (1 - y2)
                                + kColourLowHigh[1] * (1 - y1) * y2
                                + kColourHighLow[1] * y1 * (1 - y2)
                                + kColourHighHigh[1] * y1 * y2)));
    out.SetBlue(round((1.0 - kAlpha) * y3
                      + kAlpha
                            * (kColourLowLow[2] * (1 - y1) * (1 - y2)
                               + kColourLowHigh[2] * (1 - y1) * y2
                               + kColourHighLow[2] * y1 * (1 - y2)
                               + kColourHighHigh[2] * y1 * y2)));
    return out;
  }
};

class BinaryComposeFunctor
{
public:
  using InPixelType1 = float; // image1
  using InPixelType2 = float; // image2
  using OutPixelType = itk::RGBPixel<unsigned char>;

  OutPixelType
  operator()(const InPixelType1& y1, const InPixelType2& y2) const
  {
    // Compose y1 in 0-1.0f (from image1) and y2 in 0-1.0f (from
    // image2) to RGB using bilinear interpolation on the unit square
    // (<https://en.wikipedia.org/wiki/Bilinear_interpolation>).
    OutPixelType out;
    out.SetRed(round(kColourLowLow[0] * (1 - y1) * (1 - y2)
                     + kColourLowHigh[0] * (1 - y1) * y2
                     + kColourHighLow[0] * y1 * (1 - y2)
                     + kColourHighHigh[0] * y1 * y2));
    out.SetGreen(round(kColourLowLow[1] * (1 - y1) * (1 - y2)
                       + kColourLowHigh[1] * (1 - y1) * y2
                       + kColourHighLow[1] * y1 * (1 - y2)
                       + kColourHighHigh[1] * y1 * y2));
    out.SetBlue(round(kColourLowLow[2] * (1 - y1) * (1 - y2)
                      + kColourLowHigh[2] * (1 - y1) * y2
                      + kColourHighLow[2] * y1 * (1 - y2)
                      + kColourHighHigh[2] * y1 * y2));
    return out;
  }
};

} // namespace

int
main(int argc, char* argv[])
{
  const ParsedArguments args = ParseArguments(argc, argv);

  if (args.verbose && args.background_filename && args.window_level.has_value()
      && args.window_width.has_value())
    {
      // Values returned by 'strtof'.
      fprintf(stderr,
              "Using window level %f and width %f for background image\n",
              *args.window_level, *args.window_width);
    }

  // Read image1 and image2.
  using Image3DFloatType = itk::Image<float, 3>;
  using ImageFileReaderType = itk::ImageFileReader<Image3DFloatType>;
  auto image1_reader = ImageFileReaderType::New();
  image1_reader->SetFileName(args.image1_filename);

  auto image2_reader = ImageFileReaderType::New();
  image2_reader->SetFileName(args.image2_filename);

  // Prepare the background image, if provided.
  ReaderResampleClampRescaleFilters background_filters;
  if (args.background_filename)
    {
      background_filters = ReaderResampleClampRescale(
          args.background_filename, image1_reader->GetOutput(),
          args.window_level, args.window_width);
    }

  // Rescale image1 and image2 to 0-1.0f.
  using RescaleFilterType
      = itk::RescaleIntensityImageFilter<Image3DFloatType, Image3DFloatType>;
  auto image1_norm = RescaleFilterType::New();
  image1_norm->SetInput(image1_reader->GetOutput());
  image1_norm->SetOutputMinimum(0.0f);
  image1_norm->SetOutputMaximum(1.0f);

  auto image2_norm = RescaleFilterType::New();
  image2_norm->SetInput(image2_reader->GetOutput());
  image2_norm->SetOutputMinimum(0.0f);
  image2_norm->SetOutputMaximum(1.0f);

  // Compose image1 and image2 and the background image, if provided,
  // into a 3D RGB image.
  using RGBPixelType = itk::RGBPixel<unsigned char>;
  using RGBImageType = itk::Image<RGBPixelType, 3>;
  using BinaryComposeFilterType
      = itk::BinaryFunctorImageFilter<Image3DFloatType, Image3DFloatType,
                                      RGBImageType, BinaryComposeFunctor>;
  using TernaryComposeFilterType
      = itk::TernaryFunctorImageFilter<Image3DFloatType, Image3DFloatType,
                                       Image3DFloatType, RGBImageType,
                                       TernaryComposeFunctor>;
  BinaryComposeFilterType::Pointer binary_compose_filter;
  TernaryComposeFilterType::Pointer ternary_compose_filter;
  if (args.background_filename)
    {
      ternary_compose_filter = TernaryComposeFilterType::New();
      ternary_compose_filter->SetInput1(image1_norm->GetOutput());
      ternary_compose_filter->SetInput2(image2_norm->GetOutput());
      ternary_compose_filter->SetInput3(
          background_filters.rescale_filter->GetOutput());
    }
  else
    {
      binary_compose_filter = BinaryComposeFilterType::New();
      binary_compose_filter->SetInput1(image1_norm->GetOutput());
      binary_compose_filter->SetInput2(image2_norm->GetOutput());
    }

  // Reverse the row order, if requested.
  using FlipFilterType = itk::FlipImageFilter<RGBImageType>;
  FlipFilterType::Pointer flip_filter;
  if (args.reverse_row_order)
    {
      flip_filter = FlipFilterType::New();
      flip_filter->SetInput(args.background_filename
                                ? ternary_compose_filter->GetOutput()
                                : binary_compose_filter->GetOutput());
      FlipFilterType::FlipAxesArrayType flip_array;
      flip_array[0] = false;
      flip_array[1] = true;
      flip_array[2] = false;
      flip_filter->SetFlipAxes(flip_array);
    }

  // Read image1 to get the number of slices.  Execute it now, since
  // it must be executed before the main ITK pipeline.
  try
    {
      image1_reader->Update();
    }
  catch (const itk::ExceptionObject& ex)
    {
      fprintf(stderr, "spider_image_compare: unable to read image1: %s\n",
              ex.what());
      return EXIT_FAILURE;
    }
  Image3DFloatType::SizeType size
      = image1_reader->GetOutput()->GetLargestPossibleRegion().GetSize();

  // Get output PNG filenames.
  using NameGeneratorType = itk::NumericSeriesFileNames;
  auto name_generator = NameGeneratorType::New();
  name_generator->SetStartIndex(1);
  name_generator->SetEndIndex(size[2]);

  std::filesystem::path output_filename_format;
  if (args.output_directory)
    {
      if (!std::filesystem::is_directory(args.output_directory))
        {
          fprintf(stderr, "spider_image_compare: not a directory: %s\n",
                  args.output_directory);
          return EXIT_FAILURE;
        }
      output_filename_format = args.output_directory;
    }
  output_filename_format /= "slice_%03d.png";
  name_generator->SetSeriesFormat(output_filename_format.string());

  // Do not overwrite output files unless requested.
  if (!args.overwrite)
    {
      for (const std::string& s : name_generator->GetFileNames())
        {
          if (std::filesystem::exists(s))
            {
              fprintf(stderr,
                      "spider_image_compare: file already exists: %s\n",
                      s.c_str());
              return EXIT_FAILURE;
            }
        }
    }

  // Write PNG files.
  using RGB2DImageType = itk::Image<RGBPixelType, 2>;
  using ImageSeriesWriterType
      = itk::ImageSeriesWriter<RGBImageType, RGB2DImageType>;
  auto image_series_writer = ImageSeriesWriterType::New();
  image_series_writer->SetFileNames(name_generator->GetFileNames());
  image_series_writer->SetInput(
      args.reverse_row_order     ? flip_filter->GetOutput()
      : args.background_filename ? ternary_compose_filter->GetOutput()
                                 : binary_compose_filter->GetOutput());
  try
    {
      image_series_writer->Update();
    }
  catch (const itk::ExceptionObject& ex)
    {
      fprintf(stderr, "spider_image_compare: %s\n", ex.what());
      return EXIT_FAILURE;
    }

  if (args.verbose)
    {
      for (const std::string& s : name_generator->GetFileNames())
        {
          fprintf(stderr, "Wrote %s\n", s.c_str());
        }
    }

  return EXIT_SUCCESS;
}
