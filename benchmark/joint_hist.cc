// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 South Australia Medical Imaging

// Compute a 2D/joint histogram of IMAGE1 and IMAGE2.  Print to stdout
// lines in the format 'xcentre ycentre xlow xhigh ylow yhigh counts'.
// x refers to IMAGE1 intensity.

#include <cstdio>   // std::fputs, std::puts, stderr, stdout
#include <cstdlib>  // EXIT_FAILURE, EXIT_SUCCESS
#include <iostream> // std::cerr, std::cout

#include <itkComposeImageFilter.h>
#include <itkFixedArray.h>
#include <itkImage.h>
#include <itkImageFileReader.h>
#include <itkImageToHistogramFilter.h>
#include <itkMacro.h> // itk::ExceptionObject

int
main(int argc, char* argv[])
{
  if (argc < 3)
    {
      std::fputs("usage: joint_hist image1 image2\n", stderr);
      return EXIT_FAILURE;
    }

  using ImageFileReaderType = itk::ImageFileReader<itk::Image<float, 3>>;
  auto file_reader1 = ImageFileReaderType::New();
  file_reader1->SetFileName(argv[1]);
  auto file_reader2 = ImageFileReaderType::New();
  file_reader2->SetFileName(argv[2]);

  using ArrayImageType = itk::Image<itk::FixedArray<float, 2>, 3>;
  using ComposeImageFilterType
      = itk::ComposeImageFilter<itk::Image<float, 3>, ArrayImageType>;
  auto compose_filter = ComposeImageFilterType::New();
  compose_filter->SetCoordinateTolerance(1e-3);
  compose_filter->SetInput(0, file_reader1->GetOutput());
  compose_filter->SetInput(1, file_reader2->GetOutput());

  using ImageToHistogramFilterType
      = itk::Statistics::ImageToHistogramFilter<ArrayImageType>;
  auto image_to_histogram_filter = ImageToHistogramFilterType::New();
  image_to_histogram_filter->SetInput(compose_filter->GetOutput());

  try
    {
      // Execute the pipeline.
      image_to_histogram_filter->Update();
    }
  catch (const itk::ExceptionObject& ex)
    {
      std::cerr << "Error: " << ex << "\n";
      return EXIT_FAILURE;
    }

  auto histogram = image_to_histogram_filter->GetOutput();
  ImageToHistogramFilterType::HistogramType::IndexType index(2);
  std::puts("# xcentre ycentre xlow xhigh ylow yhigh counts");
  for (long unsigned int i = 0; i < histogram->GetSize()[0]; ++i)
    {
      index[0] = i;
      for (long unsigned int j = 0; j < histogram->GetSize()[1]; ++j)
        {
          index[1] = j;
          if (histogram->GetFrequency(index) > 0)
            std::cout << histogram->GetMeasurementVector(index)[0] << " "
                      << histogram->GetMeasurementVector(index)[1] << " "
                      << histogram->GetHistogramMinFromIndex(index)[0] << " "
                      << histogram->GetHistogramMaxFromIndex(index)[0] << " "
                      << histogram->GetHistogramMinFromIndex(index)[1] << " "
                      << histogram->GetHistogramMaxFromIndex(index)[1] << " "
                      << histogram->GetFrequency(index) << "\n";
        }
    }

  return EXIT_SUCCESS;
}
