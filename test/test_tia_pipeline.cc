// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2025 South Australia Medical Imaging

#include "tia_pipeline.h"

#include <cmath>   // std::log
#include <cstddef> // std::size_t
#include <filesystem>
#include <limits> // for std::numeric_limits<float>::epsilon()
#include <string> // std::string, std::to_string
#include <vector>

#include <gtest/gtest.h>
#include <itkImage.h>
#include <itkImageFileWriter.h>
#include <itkTestingComparisonImageFilter.h>

#include "exp_fit_functor.h"
#include "test_utils.h"

TEST(TiaPipelineTest, NoDecay)
{
  const std::vector<double> time_points{ 6.0, 12.0, 18.0, 24.0 }; // hours
  const std::vector<double> decay_factors(time_points.size(), 1.0);

  // Write to a location owned by this test so this test does not
  // interfere with another test and vice versa.
  const std::filesystem::path this_test_dir
      = "spider-tests-tmp/TiaPipelineTest/NoDecay";
  std::filesystem::create_directories(this_test_dir);

  using PixelType = float;
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
  std::vector<itk::Image<float, 3>::Pointer> images{ image_1, image_2, image_3,
                                                     image_4 };

  std::vector<std::string> image_filenames;
  image_filenames.reserve(time_points.size());
  std::filesystem::path image_filename;
  for (std::size_t i = 0; i < time_points.size(); ++i)
    {
      image_filename = this_test_dir / ("image_" + std::to_string(i) + ".nii");
      EXPECT_EQ(std::filesystem::exists(image_filename), false);
      itk::WriteImage(images[i], image_filename.string(), true); // compression
      image_filenames.push_back(image_filename.string());
    }

  const auto tia_filters = spider::PrepareTiaPipeline(
      time_points, image_filenames, decay_factors);

  // The data is a perfect fit to: 20 * exp(-log(2) * t / (6 h)).  TIA
  // is in units of pixel units * seconds.
  const float tia = 20.0 * 6.0 * 60.0 * 60.0 / std::log(2);
  auto tia_image = spider::test::CreateImage<ScalarImageType>();
  tia_image->FillBuffer(tia);

  auto diff = itk::Testing::ComparisonImageFilter<ScalarImageType,
                                                  ScalarImageType>::New();
  diff->SetValidInput(tia_image);
  diff->SetTestInput(tia_filters.GetFinalFilter()->GetOutput());
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

  // Clean up.
  std::filesystem::remove_all(this_test_dir);
}

TEST(TiaPipelineTest, Decay)
{
  const std::vector<double> time_points{ 6.0, 12.0, 18.0, 24.0 }; // hours
  // Lu-177 (hours).
  constexpr double half_life = 574300.0 / (60.0 * 60.0);
  std::vector<double> decay_factors;
  decay_factors.reserve(time_points.size());
  for (std::size_t i = 0; i < time_points.size(); ++i)
    {
      decay_factors.push_back(
          std::exp(-std::log(2) * time_points[i] / half_life));
    }

  // Write to a location owned by this test so this test does not
  // interfere with another test and vice versa.
  const std::filesystem::path this_test_dir
      = "spider-tests-tmp/TiaPipelineTest/Decay";
  std::filesystem::create_directories(this_test_dir);

  using PixelType = float;
  constexpr unsigned int ImageDimension = 3;
  using ScalarImageType = itk::Image<PixelType, ImageDimension>;

  // Since 'PrepareTiaPipeline' will apply a decay correction, to make
  // the numbers nice, start with the inverse of the decay correction.
  auto image_1 = spider::test::CreateImage<ScalarImageType>();
  image_1->FillBuffer(10.0 / decay_factors[0]);
  auto image_2 = spider::test::CreateImage<ScalarImageType>();
  image_2->FillBuffer(5.0 / decay_factors[1]);
  auto image_3 = spider::test::CreateImage<ScalarImageType>();
  image_3->FillBuffer(2.5 / decay_factors[2]);
  auto image_4 = spider::test::CreateImage<ScalarImageType>();
  image_4->FillBuffer(1.25 / decay_factors[3]);
  std::vector<itk::Image<float, 3>::Pointer> images{ image_1, image_2, image_3,
                                                     image_4 };

  std::vector<std::string> image_filenames;
  image_filenames.reserve(time_points.size());
  std::filesystem::path image_filename;
  for (std::size_t i = 0; i < time_points.size(); ++i)
    {
      image_filename = this_test_dir / ("image" + std::to_string(i) + ".nii");
      EXPECT_EQ(std::filesystem::exists(image_filename), false);
      itk::WriteImage(images[i], image_filename.string(), true); // compression
      image_filenames.push_back(image_filename.string());
    }

  const auto tia_filters = spider::PrepareTiaPipeline(
      time_points, image_filenames, decay_factors);

  // The data is a perfect fit to: 20 * exp(-log(2) * t / (6 h)).  TIA
  // is in units of pixel units * seconds.
  const float tia = 20.0 * 6.0 * 60.0 * 60.0 / std::log(2);
  auto tia_image = spider::test::CreateImage<ScalarImageType>();
  tia_image->FillBuffer(tia);

  auto diff = itk::Testing::ComparisonImageFilter<ScalarImageType,
                                                  ScalarImageType>::New();
  diff->SetValidInput(tia_image);
  diff->SetTestInput(tia_filters.GetFinalFilter()->GetOutput());
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

  // Clean up.
  std::filesystem::remove_all(this_test_dir);
}
