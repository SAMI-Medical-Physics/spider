// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2025, 2026 South Australia Medical Imaging

#include "tia/exp_fit_functor.h"

#include <chrono>
#include <cmath>  // std::log
#include <limits> // for std::numeric_limits<float>::epsilon()
#include <vector>

#include <gtest/gtest.h>
#include <itkComposeImageFilter.h>
#include <itkImage.h>
#include <itkTestingComparisonImageFilter.h>
#include <itkUnaryFunctorImageFilter.h>
#include <itkVectorImage.h>

#include "test_utils.h"

TEST(ExpFitFunctorTest, Pixel)
{
  const std::vector<std::chrono::seconds> time_points{
    std::chrono::hours{ 6 }, std::chrono::hours{ 12 },
    std::chrono::hours{ 18 }, std::chrono::hours{ 24 }
  };
  spider::ExpFitFunctor func;
  func.SetTimePoints(time_points);

  itk::VariableLengthVector<float> pixel_in;
  pixel_in.SetSize(4);
  pixel_in[0] = 10.0f;
  pixel_in[1] = 5.0f;
  pixel_in[2] = 2.5f;
  pixel_in[3] = 1.25f;
  const float pixel_out = func(pixel_in);

  // The data is a perfect fit to: 20 * exp(-log(2) * t / (6 h)).  TIA
  // is in units of pixel units * seconds.
  const float tia = 20.0 * 6.0 * 60.0 * 60.0 / std::log(2);
  EXPECT_EQ(pixel_out, tia);
}

TEST(ExpFitFunctorTest, Image)
{
  using PixelType = float;
  // A unary functor acts on pixels of type
  // itk::VariableLengthVector<PixelType>, where PixelType is float in
  // the case of spider::ExpFitFunctor.  It does not know about the
  // image dimension, so it has no effect in this test.
  constexpr unsigned int ImageDimension = 3;
  using ScalarImageType = itk::Image<PixelType, ImageDimension>;

  auto image_1 = spider::test::CreateImage<ScalarImageType>();
  image_1->FillBuffer(10.0f);
  auto image_2 = spider::test::CreateImage<ScalarImageType>();
  image_2->FillBuffer(5.0f);
  auto image_3 = spider::test::CreateImage<ScalarImageType>();
  image_3->FillBuffer(2.5f);
  auto image_4 = spider::test::CreateImage<ScalarImageType>();
  image_4->FillBuffer(1.25f);

  using ComposeImageFilterType = itk::ComposeImageFilter<ScalarImageType>;
  auto compose_image_filter = ComposeImageFilterType::New();
  compose_image_filter->SetInput(0, image_1);
  compose_image_filter->SetInput(1, image_2);
  compose_image_filter->SetInput(2, image_3);
  compose_image_filter->SetInput(3, image_4);

  using VectorImageType = itk::VectorImage<PixelType, ImageDimension>;
  using UnaryFunctorImageFilterType
      = itk::UnaryFunctorImageFilter<VectorImageType, ScalarImageType,
                                     spider::ExpFitFunctor>;
  auto unary_functor_image_filter = UnaryFunctorImageFilterType::New();
  const std::vector<std::chrono::seconds> time_points{
    std::chrono::hours{ 6 }, std::chrono::hours{ 12 },
    std::chrono::hours{ 18 }, std::chrono::hours{ 24 }
  };
  unary_functor_image_filter->GetFunctor().SetTimePoints(time_points);
  unary_functor_image_filter->SetInput(compose_image_filter->GetOutput());

  // The data is a perfect fit to: 20 * exp(-log(2) * t / (6 h)).  TIA
  // is in units of pixel units * seconds.
  const float tia = 20.0 * 6.0 * 60.0 * 60.0 / std::log(2);
  auto tia_image = spider::test::CreateImage<ScalarImageType>();
  tia_image->FillBuffer(tia);

  auto diff = itk::Testing::ComparisonImageFilter<ScalarImageType,
                                                  ScalarImageType>::New();
  diff->SetValidInput(tia_image);
  diff->SetTestInput(unary_functor_image_filter->GetOutput());
  // For checking equality between floats.
  diff->SetDifferenceThreshold(std::numeric_limits<float>::epsilon());
  diff->Update();
  EXPECT_EQ(diff->GetNumberOfPixelsWithDifferences(), 0);

  // Ensure this appproach identifies different pixel values.
  const float tia_wrong = tia + 0.1;
  auto tia_wrong_image = spider::test::CreateImage<ScalarImageType>();
  tia_wrong_image->FillBuffer(tia_wrong);
  diff->SetValidInput(tia_wrong_image);
  diff->Update();
  EXPECT_NE(diff->GetNumberOfPixelsWithDifferences(), 0);
}
