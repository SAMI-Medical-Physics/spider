// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2025, 2026 South Australia Medical Imaging

#ifndef SPIDER_TIA_TIA_PIPELINE_H
#define SPIDER_TIA_TIA_PIPELINE_H

#include <chrono>
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
struct TiaFilters
{
  using ImageFileReaderType = itk::ImageFileReader<itk::Image<float, 3>>;
  using ShiftScaleImageFilterType
      = itk::ShiftScaleImageFilter<itk::Image<float, 3>, itk::Image<float, 3>>;
  using ComposeImageFilterType = itk::ComposeImageFilter<itk::Image<float, 3>>;
  using UnaryFunctorImageFilterType
      = itk::UnaryFunctorImageFilter<itk::VectorImage<float, 3>,
                                     itk::Image<float, 3>, ExpFitFunctor>;

  std::vector<ImageFileReaderType::Pointer> file_readers;
  std::vector<ShiftScaleImageFilterType::Pointer> scale_filters;
  ComposeImageFilterType::Pointer compose_filter;
  UnaryFunctorImageFilterType::Pointer functor_filter;

  UnaryFunctorImageFilterType::Pointer
  GetFinalFilter() const
  {
    return functor_filter;
  }
};

// We expect INPUT_FILENAMES to be the names of 3D NIfTI files, so
// decay correction must be determined beforehand.  Hence the
// DECAY_FACTORS argument.
TiaFilters
PrepareTiaPipeline(const std::vector<std::chrono::seconds>& time_points,
                   const std::vector<std::string>& input_filenames,
                   const std::vector<double>& decay_factors);
} // namespace spider

#endif // SPIDER_TIA_TIA_PIPELINE_H
