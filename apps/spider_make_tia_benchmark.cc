// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2025 South Australia Medical Imaging

#include <cstdlib> // EXIT_FAILURE, EXIT_SUCCESS
#include <iostream>
#include <string>
#include <vector>

#include <itkImage.h>
#include <itkImageFileWriter.h>
#include <itkMacro.h> // itk::ExceptionObject

#include "logging.h"
#include "tia/tia_pipeline.h"

int
main(int argc, char* argv[])
{
  if (argc < 3)
    {
      std::cerr << "Usage: " << argv[0]
                << " image1 image2 image3 image4 (in time order)\n";
      return EXIT_FAILURE;
    }

  spider::Log() << "Version " << SPIDER_VERSION << "\n";
  std::vector<std::string> input_filenames(argv + 1, argv + argc);
  std::vector<double> time_points{ 3.73, 27.72, 103.08, 123.98 }; // hours
  // The benchmark SPECTs are decay corrected to the administration
  // time.  Apply decay correction to the acquisiton time.
  constexpr double half_life = 574300.0 / (60.0 * 60.0); // hours
  std::vector<double> decay_factors;
  decay_factors.reserve(time_points.size());
  for (std::size_t i = 0; i < time_points.size(); ++i)
    {
      decay_factors.push_back(
          std::exp(-std::log(2.0) * time_points[i] / half_life));
    }

  spider::TiaFilters tia_filters = spider::PrepareTiaPipeline(
      time_points, input_filenames, decay_factors);

  // Write TIA image.
  std::string out_filename = "tia.nii";
  using PixelType = float;
  constexpr unsigned int ImageDimension = 3;
  using ImageType = itk::Image<PixelType, ImageDimension>;
  using ImageFileWriterType = itk::ImageFileWriter<ImageType>;
  auto image_file_writer = ImageFileWriterType::New();
  image_file_writer->SetFileName(out_filename);
  // image_file_writer->SetUseCompression(true);
  image_file_writer->SetInput(tia_filters.GetFinalFilter()->GetOutput());
  spider::Log() << "Executing TIA pipeline\n";
  try
    {
      image_file_writer->Update();
    }
  catch (const itk::ExceptionObject& e)
    {
      std::cerr << "Error: " << e << "\n";
      return EXIT_FAILURE;
    }
  spider::Log() << "Wrote " << out_filename << "\n";

  return EXIT_SUCCESS;
}
