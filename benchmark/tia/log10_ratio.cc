// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2025, 2026 South Australia Medical Imaging

// Read files NUMERATOR and DENOM as three-dimensional images of pixel
// type float.  Output a histogram of the base-10 logarithm of the
// per-pixel ratio NUMERATOR / DENOM.  The output is formatted as:
// <lower edge of bin> <upper edge of bin> <bin counts>.  If the third
// argument THRESHOLD is provided, the histogram excludes pixels with
// value less than THRESHOLD in DENOM image.

#include <cstdlib> // EXIT_SUCCESS, EXIT_FAILURE
#include <iostream>
#include <optional>
#include <string> // std::stof

#include <itkDivideImageFilter.h>
#include <itkImage.h>
#include <itkImageFileReader.h>
#include <itkImageToHistogramFilter.h>
#include <itkLog10ImageFilter.h>
#include <itkMacro.h> // itk::ExceptionObject
#include <itkThresholdImageFilter.h>

int
main(int argc, char* argv[])
{
  if (argc < 3)
    {
      std::cerr << "Usage: " << argv[0] << " numerator denom [threshold]\n";
      return EXIT_FAILURE;
    }

  std::optional<float> threshold;
  if (argc > 3)
    {
      threshold = std::stof(argv[3]);
      std::cerr << "threshold = " << threshold.value() << "\n";
    }

  using PixelType = float;
  constexpr unsigned int ImageDimension = 3;
  using ImageType = itk::Image<PixelType, ImageDimension>;

  using ImageFileReaderType = itk::ImageFileReader<ImageType>;
  auto image_file_reader_numerator = ImageFileReaderType::New();
  image_file_reader_numerator->SetFileName(argv[1]);
  auto image_file_reader_denom = ImageFileReaderType::New();
  image_file_reader_denom->SetFileName(argv[2]);

  using ThresholdImageFilterType = itk::ThresholdImageFilter<ImageType>;
  auto threshold_image_filter = ThresholdImageFilterType::New();
  if (threshold)
    {
      // Set pixels in DENOM image to zero if below THRESHOLD.
      threshold_image_filter->SetInput(image_file_reader_denom->GetOutput());
      threshold_image_filter->ThresholdBelow(threshold.value());
      threshold_image_filter->SetOutsideValue(0.0f);
    }

  // XXX: If the pixel in DENOM image is zero, the result of division
  // is FLT_MAX (3.40282e+38).
  using DivideImageFilterType
      = itk::DivideImageFilter<ImageType, ImageType, ImageType>;
  auto divide_image_filter = DivideImageFilterType::New();
  // FIXME: Why is this required?  I.e. why does elastix output have
  // slightly different coordinates?
  divide_image_filter->SetCoordinateTolerance(1e-3);
  divide_image_filter->SetInput1(image_file_reader_numerator->GetOutput());
  divide_image_filter->SetInput2(threshold.has_value()
                                     ? threshold_image_filter->GetOutput()
                                     : image_file_reader_denom->GetOutput());

  // Take the logarithm with base 10.
  using Log10ImageFilterType = itk::Log10ImageFilter<ImageType, ImageType>;
  auto log10_image_filter = Log10ImageFilterType::New();
  log10_image_filter->SetInput(divide_image_filter->GetOutput());

  // Histogram of base-10 logarithm of NUMERATOR / DENOM.
  using ImageToHistogramFilterType
      = itk::Statistics::ImageToHistogramFilter<ImageType>;
  constexpr int measurement_vector_size = 1; // grayscale
  // Bin width of 0.5, including a bin from -0.25 to 0.25.
  constexpr int bins_per_dimension = 40;
  ImageToHistogramFilterType::HistogramType::MeasurementVectorType lower_bound(
      measurement_vector_size);
  lower_bound.Fill(-10.25);
  ImageToHistogramFilterType::HistogramType::MeasurementVectorType upper_bound(
      measurement_vector_size);
  // Choose an upper bound less than log10(FLT_MAX) ~ 38.5 to exclude
  // pixels with value zero in DENOM image.
  upper_bound.Fill(9.75);

  ImageToHistogramFilterType::HistogramType::SizeType size(
      measurement_vector_size);
  size.Fill(bins_per_dimension);

  auto image_to_histogram_filter = ImageToHistogramFilterType::New();
  image_to_histogram_filter->SetInput(log10_image_filter->GetOutput());
  image_to_histogram_filter->SetHistogramBinMinimum(lower_bound);
  image_to_histogram_filter->SetHistogramBinMaximum(upper_bound);
  image_to_histogram_filter->SetHistogramSize(size);
  image_to_histogram_filter->SetAutoMinimumMaximum(false);

  try
    {
      // Execute the pipeline.
      image_to_histogram_filter->Update();
    }
  catch (const itk::ExceptionObject& e)
    {
      std::cerr << "Error: " << e << "\n";
      return EXIT_FAILURE;
    }

  auto histogram = image_to_histogram_filter->GetOutput();
  for (long unsigned int i = 0; i < histogram->GetSize()[0]; ++i)
    {
      std::cout << histogram->GetBinMin(0, i) << " "
                << histogram->GetBinMax(0, i) << " "
                << histogram->GetFrequency(i) << "\n";
    }

  return EXIT_SUCCESS;
}
