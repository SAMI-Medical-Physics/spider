// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2025, 2026 South Australia Medical Imaging

#include "tia/tia_pipeline.h"

#include <cassert>
#include <chrono>
#include <cstddef> // std::size_t
#include <string>
#include <vector>

#include <itkComposeImageFilter.h>
#include <itkImage.h>
#include <itkImageFileReader.h>
#include <itkShiftScaleImageFilter.h>
#include <itkUnaryFunctorImageFilter.h>
#include <itkVectorImage.h>

#include "tia/exp_fit_functor.h"

namespace spider
{
TiaFilters
PrepareTiaPipeline(const std::vector<std::string>& input_filenames,
                   const std::vector<std::chrono::seconds>& time_points,
                   const std::vector<double>& decay_factors)
{
  const std::size_t num_images = input_filenames.size();
  TiaFilters filters;

  // Insert file reader filters.
  filters.file_readers.reserve(num_images);
  using ImageFileReaderType = itk::ImageFileReader<itk::Image<float, 3>>;
  for (auto& fname : input_filenames)
    {
      auto file_reader = ImageFileReaderType::New();
      file_reader->SetFileName(fname);
      filters.file_readers.push_back(file_reader);
    }

  // Insert scale filters.
  assert(decay_factors.size() == num_images);
  filters.scale_filters.reserve(num_images);
  using ShiftScaleImageFilterType
      = itk::ShiftScaleImageFilter<itk::Image<float, 3>, itk::Image<float, 3>>;
  for (std::size_t i = 0; i < num_images; ++i)
    {
      auto scale_filter = ShiftScaleImageFilterType::New();
      scale_filter->SetInput(filters.file_readers[i]->GetOutput());
      scale_filter->SetScale(decay_factors[i]);
      filters.scale_filters.push_back(scale_filter);
    }

  // Set compose filter.
  using ComposeImageFilterType = itk::ComposeImageFilter<itk::Image<float, 3>>;
  filters.compose_filter = ComposeImageFilterType::New();
  for (std::size_t i = 0; i < num_images; ++i)
    {
      filters.compose_filter->SetInput(
          i, (decay_factors[i] == 1.0)
                 ? filters.file_readers[i]->GetOutput()
                 : filters.scale_filters[i]->GetOutput());
    }

  // Set functor filter.
  using UnaryFunctorImageFilterType
      = itk::UnaryFunctorImageFilter<itk::VectorImage<float, 3>,
                                     itk::Image<float, 3>, ExpFitFunctor>;
  filters.functor_filter = UnaryFunctorImageFilterType::New();
  filters.functor_filter->GetFunctor().SetTimePoints(time_points);
  filters.functor_filter->SetInput(filters.compose_filter->GetOutput());
  return filters;
}
} // namespace spider
