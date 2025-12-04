// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2025 South Australia Medical Imaging

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
      std::cerr << "Usage: " << argv[0] << " new old [threshold]\n";
      return EXIT_FAILURE;
    }

  std::optional<float> threshold;
  if (argc > 3)
    {
      threshold = std::stof(argv[3]);
    }

  // XXX: This is the pixel type AFTER applying rescale slope and
  // intercept.
  using PixelType = float;
  constexpr unsigned int ImageDimension = 3;
  using ImageType = itk::Image<PixelType, ImageDimension>;

  using ImageFileReaderType = itk::ImageFileReader<ImageType>;
  auto image_file_reader_new = ImageFileReaderType::New();
  image_file_reader_new->SetFileName(argv[1]);
  auto image_file_reader_old = ImageFileReaderType::New();
  image_file_reader_old->SetFileName(argv[2]);

  using ThresholdImageFilterType = itk::ThresholdImageFilter<ImageType>;
  auto threshold_image_filter = ThresholdImageFilterType::New();
  if (threshold)
    {
      // Set pixels in NEW image to 0 if below THRESHOLD.
      threshold_image_filter->SetInput(image_file_reader_new->GetOutput());
      threshold_image_filter->ThresholdBelow(threshold.value());
      threshold_image_filter->SetOutsideValue(0.0);
    }

  // XXX: If the denominator (pixel in OLD image) is zero, the result
  // of division is 3.40282e+38 for float.
  using DivideImageFilterType
      = itk::DivideImageFilter<ImageType, ImageType, ImageType>;
  auto divide_image_filter = DivideImageFilterType::New();
  // FIXME: Why is this required?  I.e. why does elastix output have
  // slightly different coordinates?
  divide_image_filter->SetCoordinateTolerance(1e-3);
  divide_image_filter->SetInput1(threshold
                                     ? threshold_image_filter->GetOutput()
                                     : image_file_reader_new->GetOutput());
  divide_image_filter->SetInput2(image_file_reader_old->GetOutput());

  // Take the log base 10.
  using Log10ImageFilterType = itk::Log10ImageFilter<ImageType, ImageType>;
  auto log10_image_filter = Log10ImageFilterType::New();
  log10_image_filter->SetInput(divide_image_filter->GetOutput());

  // Histogram of log base 10 of new / old images.
  using ImageToHistogramFilterType
      = itk::Statistics::ImageToHistogramFilter<ImageType>;
  constexpr int measurement_vector_size = 1; // grayscale
  // Bin width of 0.5, centred on 0.
  constexpr int bins_per_dimension = 40;
  ImageToHistogramFilterType::HistogramType::MeasurementVectorType lower_bound(
      bins_per_dimension);
  lower_bound.Fill(-10.25);
  ImageToHistogramFilterType::HistogramType::MeasurementVectorType upper_bound(
      bins_per_dimension);
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
