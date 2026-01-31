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

// Return an ITK data processing pipeline that computes a
// three-dimensional time-integrated activity image.  INPUT_FILENAMES
// are the file names of the three-dimensional SPECT images; see below
// for details.  TIME_POINTS are the elapsed times from
// radiopharmaceutical administration to the start of each SPECT
// acquisition.  DECAY_FACTORS are the factors required to
// decay-correct each SPECT image to its acquisition start time.  All
// arguments must have the same size, and that size must be at least
// 2.
//
// Image data is read from INPUT_FILENAMES using itk::ImageFileReader,
// which supports a variety of file formats, provided the
// corresponding ImageIO module is listed in the COMPONENTS argument
// in the CMake find_package(ITK ...) call.  The ImageIO type may be
// inferred from the file name suffix.
//
// We considered separating out the file reading, but
// itk::ImageFileReader cannot read an image from memory, so tests
// would still need to write to the file system.
//
// There are separate TIME_POINTS and DECAY_FACTORS arguments because
// the image files may not be in DICOM format.
TiaFilters
PrepareTiaPipeline(const std::vector<std::string>& input_filenames,
                   const std::vector<std::chrono::seconds>& time_points,
                   const std::vector<double>& decay_factors);
} // namespace spider

#endif // SPIDER_TIA_TIA_PIPELINE_H
