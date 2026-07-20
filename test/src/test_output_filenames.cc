// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 South Australia Medical Imaging

#include "output_filenames.h"

#include <algorithm> // std::find_if
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

#include <gtest/gtest.h>
#include <itkImage.h>
#include <itkImageFileWriter.h>
#include <itkMacro.h> // itk::ExceptionObject

#include "test_utils.h" // test::CreateImage

namespace
{

class ChangeDirGuard
{
public:
  ChangeDirGuard(const std::filesystem::path& p)
      : old_{ std::filesystem::current_path() }
  {
    std::filesystem::current_path(p);
  }

  ~ChangeDirGuard() { std::filesystem::current_path(old_); }

private:
  std::filesystem::path old_;
};

// Test spider::OutputFilenames.  REL_FNAME must be a relative file
// name.  If ABSOLUTE is false, call spider::OutputFilenames like
// 'spider::OutputFilenames(REL_FNAME, COMPRESS)'.  Otherwise,
// construct an absolute path ending in REL_FNAME and use that as the
// first arg to spider::OutputFilenames.
void
TestOutputFilenames(std::string_view rel_fname, bool compress,
                    bool absolute = false)
{
  // Assume the 'filename' component of REL_FNAME is not the same in
  // more than 1 GoogleTest TEST, so that we can use it to form a
  // write location that other TESTs won't use, allowing parallel test
  // execution with 'ctest -j N'.
  const auto rel_fname_p = std::filesystem::path{ rel_fname };
  const auto this_test_dir
      = std::filesystem::path{ "spider-tests-tmp/OutputFilenamesTest" }
        / rel_fname_p.filename();
  std::filesystem::remove_all(this_test_dir);
  std::filesystem::create_directories(this_test_dir);
  ChangeDirGuard cd{ this_test_dir };
  const std::filesystem::path parent_path = rel_fname_p.parent_path();
  if (!parent_path.empty())
    {
      // For itk::ImageFileWriter, the parent directory must already
      // exist.
      std::filesystem::create_directories(parent_path);
    }
  const std::string input_fname
      = (absolute ? std::filesystem::absolute(rel_fname_p) : rel_fname_p)
            .string();

  // Create an image.
  using PixelType = float;
  constexpr unsigned int ImageDimension = 3;
  using ImageType = itk::Image<PixelType, ImageDimension>;
  auto image = spider::test::CreateImage<ImageType>();
  image->FillBuffer(10.0f);

  // Use itk::ImageFileWriter.
  auto writer = itk::ImageFileWriter<ImageType>::New();
  writer->SetInput(image);
  writer->SetFileName(input_fname);
  writer->SetUseCompression(compress);
  try
    {
      writer->Update();
    }
  catch (const itk::ExceptionObject&)
    {
      // For ".nhdr", a file is written before an exception is thrown.
    }

  // Check that the expected filenames were written.
  std::vector<std::filesystem::path> expected_fnames
      = spider::OutputFilenames(input_fname, compress);
  // Make "test.nii" compare equal to "./test.nii".
  auto path_equal
      = [](const std::filesystem::path& p1, const std::filesystem::path& p2)
    { return p1.lexically_normal() == p2.lexically_normal(); };
  const std::filesystem::path output_dir
      = absolute ? std::filesystem::absolute(parent_path.empty() ? "."
                                                                 : parent_path)
                 : (parent_path.empty() ? "." : parent_path);
  int num_files = 0;
  for (const auto& entry : std::filesystem::directory_iterator(output_dir))
    {
      EXPECT_NE(std::find_if(expected_fnames.begin(), expected_fnames.end(),
                             [&](const auto& f)
                               { return path_equal(f, entry.path()); }),
                expected_fnames.end())
          << "(test arg " << rel_fname << ", input filename " << input_fname
          << ", compress " << compress
          << "): " << entry.path().lexically_normal()
          << " was written but was not returned by OutputFilenames";
      ++num_files;
    }
  EXPECT_EQ(expected_fnames.size(), num_files);
}

} // namespace

// Test filenames that select the NIfTI IO module.
TEST(OutputFilenamesTest, Nifti)
{
  TestOutputFilenames("test.nii", false);
  TestOutputFilenames("test.nii.gz", false);
  TestOutputFilenames("test.hdr", false);
  TestOutputFilenames("test.img", false);
  TestOutputFilenames("test.hdr.gz", false);
  TestOutputFilenames("test.img.gz", false);

  TestOutputFilenames("test.nii", true);
  TestOutputFilenames("test.nii.gz", true);
  TestOutputFilenames("test.hdr", true);
  TestOutputFilenames("test.img", true);
  TestOutputFilenames("test.hdr.gz", true);
  TestOutputFilenames("test.img.gz", true);

  TestOutputFilenames("path/to/test.nii", false);

  TestOutputFilenames("test.nii", false, true);

  TestOutputFilenames("./.nii", false);
}

// Test filenames that select the NRRD IO module.
TEST(OutputFilenamesTest, Nrrd)
{
  TestOutputFilenames("test.nrrd", false);
  TestOutputFilenames("test.nhdr", false);

  TestOutputFilenames("test.nrrd", true);
  TestOutputFilenames("test.nhdr", true);

  TestOutputFilenames("path/to/test.nrrd", false);

  TestOutputFilenames("test.nrrd", false, true);

  TestOutputFilenames("test.Nhdr", false);

  TestOutputFilenames(".nhdr", false);
}

// Test filenames that select the MetaImage IO module.
TEST(OutputFilenamesTest, Metaimage)
{
  TestOutputFilenames("test.mha", false);
  TestOutputFilenames("test.mhd", false);

  TestOutputFilenames("test.mha", true);
  TestOutputFilenames("test.mhd", true);

  TestOutputFilenames("test.mha", false, true);

  TestOutputFilenames("test.Mha", false);

  TestOutputFilenames(".mhd", false);
}
