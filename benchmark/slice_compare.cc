// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 South Australia Medical Imaging

// Usage: ./slice_compare image1 image2 background z contour alpha
//
// For each of the 3D scalar images IMAGE1 and IMAGE2, output a PNG
// image of the axial slice at index Z overlaid in colour on the
// registered BACKGROUND (also a 3D scalar image) shown in greyscale.
// The same mapping from intensity to RGB is used for both IMAGE1 and
// IMAGE2, with each intensity assigned a unique colour.  A cyan
// contour is drawn on the colour-mapped overlay for IMAGE1 where
// IMAGE1 intensity is CONTOUR, and likewise for IMAGE2.  ALPHA (0-1)
// is the opacity of the colour-mapped overlay used for alpha
// compositing.

#include <algorithm> // std::min, std::max
#include <cmath>     // std::round
#include <cstdlib>   // EXIT_FAILURE, EXIT_SUCCESS
#include <iostream>  // std::cerr
#include <string>    // std::stoi, std::stof, std::to_string

#include <itkBinaryContourImageFilter.h>
#include <itkBinaryFunctorImageFilter.h>
#include <itkBinaryThresholdImageFilter.h>
#include <itkExtractImageFilter.h>
#include <itkFlipImageFilter.h>
#include <itkImage.h>
#include <itkImageFileReader.h>
#include <itkImageFileWriter.h>
#include <itkMinimumMaximumImageFilter.h>
#include <itkResampleImageFilter.h>
#include <itkRescaleIntensityImageFilter.h>
#include <itkScalarToRGBColormapImageFilter.h>

namespace
{

struct ImageMinMax
{
  itk::Image<float, 3>::Pointer image;
  float min;
  float max;
};

ImageMinMax
ReadImageMinMax(const char* filename)
{
  // The image file reader must be executed ('Update' called) to
  // determine the minimum and maximum pixel values in the image.
  using Image3DType = itk::Image<float, 3>;
  using ImageFileReaderType = itk::ImageFileReader<Image3DType>;
  auto file_reader = ImageFileReaderType::New();
  file_reader->SetFileName(filename);

  using MinMaxFilterType = itk::MinimumMaximumImageFilter<Image3DType>;
  auto min_max_filter = MinMaxFilterType::New();
  min_max_filter->SetInput(file_reader->GetOutput());
  min_max_filter->Update();

  return ImageMinMax{ .image = file_reader->GetOutput(),
                      .min = min_max_filter->GetMinimum(),
                      .max = min_max_filter->GetMaximum() };
}

struct ExtractColormapFilters
{
  using Image3DType = itk::Image<float, 3>;
  using Image2DFloatType = itk::Image<float, 2>;
  using ExtractFilterType
      = itk::ExtractImageFilter<Image3DType, Image2DFloatType>;
  ExtractFilterType::Pointer extract_filter;

  using RGBPixelType = itk::RGBPixel<unsigned char>;
  using RGBImageType = itk::Image<RGBPixelType, 2>;
  using RGBFilterType
      = itk::ScalarToRGBColormapImageFilter<Image2DFloatType, RGBImageType>;
  RGBFilterType::Pointer colormap_filter;
};

ExtractColormapFilters
ExtractColormap(const itk::Image<float, 3>* img, int z, float min, float max)
{
  // Get the axial slice at index Z of IMG.
  using Image3DType = itk::Image<float, 3>;
  using Image2DFloatType = itk::Image<float, 2>;
  using ExtractFilterType
      = itk::ExtractImageFilter<Image3DType, Image2DFloatType>;
  auto extract_filter = ExtractFilterType::New();
  extract_filter->SetInput(img);
  const auto region = img->GetLargestPossibleRegion();
  Image3DType::IndexType extract_index = region.GetIndex();
  extract_index[2] = z;
  Image3DType::SizeType extract_size = region.GetSize();
  extract_size[2] = 0;
  Image3DType::RegionType extract_region;
  extract_region.SetIndex(extract_index);
  extract_region.SetSize(extract_size);
  extract_filter->SetExtractionRegion(extract_region);
  extract_filter->SetDirectionCollapseToSubmatrix();

  // Convert slice to RGB.
  using RGBPixelType = itk::RGBPixel<unsigned char>;
  using RGBImageType = itk::Image<RGBPixelType, 2>;
  using RGBFilterType
      = itk::ScalarToRGBColormapImageFilter<Image2DFloatType, RGBImageType>;
  auto colormap_filter = RGBFilterType::New();
  colormap_filter->SetInput(extract_filter->GetOutput());
  colormap_filter->SetColormap(
      itk::ScalarToRGBColormapImageFilterEnums::RGBColormapFilter::Hot);
  // Do not map slice's min-max to 0-255; instead map MIN-MAX to
  // 0-255.
  colormap_filter->SetUseInputImageExtremaForScaling(false);
  colormap_filter->GetModifiableColormap()->SetMinimumInputValue(min);
  colormap_filter->GetModifiableColormap()->SetMaximumInputValue(max);

  return ExtractColormapFilters{ .extract_filter = extract_filter,
                                 .colormap_filter = colormap_filter };
}

struct ThresholdContourFilters
{
  using Image2DFloatType = itk::Image<float, 2>;
  using MaskImageType = itk::Image<unsigned char, 2>;
  using ThresholdFilterType
      = itk::BinaryThresholdImageFilter<Image2DFloatType, MaskImageType>;
  ThresholdFilterType::Pointer threshold_filter;

  using ContourFilterType
      = itk::BinaryContourImageFilter<MaskImageType, MaskImageType>;
  ContourFilterType::Pointer contour_filter;
};

ThresholdContourFilters
ThresholdContour(const itk::Image<float, 2>* slice, float y)
{
  using Image2DFloatType = itk::Image<float, 2>;
  using MaskImageType = itk::Image<unsigned char, 2>;
  using ThresholdFilterType
      = itk::BinaryThresholdImageFilter<Image2DFloatType, MaskImageType>;
  auto threshold_filter = ThresholdFilterType::New();
  threshold_filter->SetInput(slice);
  threshold_filter->SetLowerThreshold(y);
  threshold_filter->SetInsideValue(1);
  threshold_filter->SetOutsideValue(0);

  using ContourFilterType
      = itk::BinaryContourImageFilter<MaskImageType, MaskImageType>;
  auto contour_filter = ContourFilterType::New();
  contour_filter->SetInput(threshold_filter->GetOutput());
  contour_filter->SetForegroundValue(1);
  contour_filter->SetBackgroundValue(0);

  return ThresholdContourFilters{ .threshold_filter = threshold_filter,
                                  .contour_filter = contour_filter };
}

struct ResampleExtractRescaleFilters
{
  using Image3DType = itk::Image<float, 3>;
  using ResampleFilterType
      = itk::ResampleImageFilter<Image3DType, Image3DType>;
  ResampleFilterType::Pointer resample_filter;

  using Image2DFloatType = itk::Image<float, 2>;
  using ExtractFilterType
      = itk::ExtractImageFilter<Image3DType, Image2DFloatType>;
  ExtractFilterType::Pointer extract_filter;

  using RescaleFilterType
      = itk::RescaleIntensityImageFilter<Image2DFloatType, Image2DFloatType>;
  RescaleFilterType::Pointer rescale_filter;
};

ResampleExtractRescaleFilters
ResampleExtractRescale(const itk::Image<float, 3>* background,

                       const itk::Image<float, 3>* img, int z)
{
  // Resample BACKGROUND on IMG.
  using Image3DType = itk::Image<float, 3>;
  using ResampleFilterType
      = itk::ResampleImageFilter<Image3DType, Image3DType>;
  auto resample_filter = ResampleFilterType::New();
  resample_filter->SetInput(background);
  resample_filter->SetReferenceImage(img);
  resample_filter->UseReferenceImageOn();
  resample_filter->UpdateOutputInformation();

  // Get the axial slice at index Z of the resampled background.
  using Image2DFloatType = itk::Image<float, 2>;
  using ExtractFilterType
      = itk::ExtractImageFilter<Image3DType, Image2DFloatType>;
  auto extract_filter = ExtractFilterType::New();
  extract_filter->SetInput(resample_filter->GetOutput());
  const auto region = resample_filter->GetOutput()->GetLargestPossibleRegion();
  Image3DType::IndexType extract_index = region.GetIndex();
  extract_index[2] = z;
  Image3DType::SizeType extract_size = region.GetSize();
  extract_size[2] = 0;
  Image3DType::RegionType extract_region;
  extract_region.SetIndex(extract_index);
  extract_region.SetSize(extract_size);
  extract_filter->SetExtractionRegion(extract_region);
  extract_filter->SetDirectionCollapseToSubmatrix();

  // Rescale slice to 0-255 in preparation for alpha blending.
  using RescaleFilterType
      = itk::RescaleIntensityImageFilter<Image2DFloatType, Image2DFloatType>;
  auto rescale_filter = RescaleFilterType::New();
  rescale_filter->SetInput(extract_filter->GetOutput());
  rescale_filter->SetOutputMinimum(0.0f);
  rescale_filter->SetOutputMaximum(255.0f);

  return ResampleExtractRescaleFilters{ .resample_filter = resample_filter,
                                        .extract_filter = extract_filter,
                                        .rescale_filter = rescale_filter };
}

class DrawContourFunctor
{
public:
  using InPixelType1 = itk::RGBPixel<unsigned char>;
  using InPixelType2 = unsigned char;
  using OutPixelType = itk::RGBPixel<unsigned char>;

  OutPixelType
  operator()(const InPixelType1& y1, const InPixelType2& y2) const
  {
    if (!y2)
      return y1;
    // Cyan.
    OutPixelType out;
    out.SetRed(0);
    out.SetGreen(255);
    out.SetBlue(255);
    return out;
  }
};

class AlphaBlendFunctor
{
public:
  using InPixelType1 = float;
  using InPixelType2 = itk::RGBPixel<unsigned char>;
  using OutPixelType = itk::RGBPixel<unsigned char>;

  void
  SetAlpha(float alpha)
  {
    alpha_ = alpha;
  }

  OutPixelType
  operator()(const InPixelType1& y1, const InPixelType2& y2) const
  {
    OutPixelType out;
    out.SetRed(std::round((1.0 - alpha_) * y1 + alpha_ * y2.GetRed()));
    out.SetGreen(std::round((1.0 - alpha_) * y1 + alpha_ * y2.GetGreen()));
    out.SetBlue(std::round((1.0 - alpha_) * y1 + alpha_ * y2.GetBlue()));
    return out;
  }

private:
  float alpha_ = 0.0f;
};

void
DrawBlendFlipWrite(const itk::Image<itk::RGBPixel<unsigned char>, 2>* img,
                   const itk::Image<unsigned char, 2>* contour,
                   const itk::Image<float, 2>* background, float alpha,
                   const std::string& out_filename)
{
  // Draw CONTOUR on IMG.
  using RGBPixelType = itk::RGBPixel<unsigned char>;
  using RGBImageType = itk::Image<RGBPixelType, 2>;
  using MaskImageType = itk::Image<unsigned char, 2>;
  using DrawContourFilterType
      = itk::BinaryFunctorImageFilter<RGBImageType, MaskImageType,
                                      RGBImageType, DrawContourFunctor>;
  auto draw_contour_filter = DrawContourFilterType::New();
  draw_contour_filter->SetInput1(img);
  draw_contour_filter->SetInput2(contour);

  // Alpha blend it over BACKGROUND.
  using Image2DFloatType = itk::Image<float, 2>;
  using BlendFilterType
      = itk::BinaryFunctorImageFilter<Image2DFloatType, RGBImageType,
                                      RGBImageType, AlphaBlendFunctor>;
  auto blend_filter = BlendFilterType::New();
  blend_filter->SetInput1(background);
  blend_filter->SetInput2(draw_contour_filter->GetOutput());
  blend_filter->GetFunctor().SetAlpha(alpha);

  // Flip the row order.  This is required because the input 3D images
  // were created using 'dcm2niix' which flips the row order by
  // default.  dcm2niix has an option argument "-y n" to preserve the
  // row order, but this is an "advanced feature" and it may not be
  // fully supported, so we do not use it.
  using FlipFilterType = itk::FlipImageFilter<RGBImageType>;
  auto flip_filter = FlipFilterType::New();
  flip_filter->SetInput(blend_filter->GetOutput());
  FlipFilterType::FlipAxesArrayType flip_array;
  flip_array[0] = false;
  flip_array[1] = true;
  flip_filter->SetFlipAxes(flip_array);

  // Write PNG file.
  using ImageFileWriterType = itk::ImageFileWriter<RGBImageType>;
  auto image_file_writer = ImageFileWriterType::New();
  image_file_writer->SetFileName(out_filename);
  image_file_writer->SetInput(flip_filter->GetOutput());
  image_file_writer->Update();
  std::cerr << "Wrote " << out_filename << "\n";
}

} // namespace

int
main(int argc, char* argv[])
{
  if (argc < 7)
    {
      std::cerr << "usage: " << argv[0]
                << " image1 image2 background z contour alpha\n";
      return EXIT_FAILURE;
    }

  const char* image1_filename = argv[1];
  const char* image2_filename = argv[2];
  const char* background_filename = argv[3];
  // Axial slice index of image1 and image2.
  const int z{ std::stoi(argv[4]) };
  // Value of image1 and image2 at which to draw contour.
  const float contour_value{ std::stof(argv[5]) };
  // Opacity of colour overlay (image1 or image2 slice + contour) for
  // alpha blending.
  const float alpha{ std::stof(argv[6]) };
  if (!(alpha <= 1.0f && alpha >= 0.0f))
    {
      std::cerr << "Invalid alpha: " << alpha << " (must be from 0 to 1)\n";
      return EXIT_FAILURE;
    }

  // Read image1 and image2.
  const ImageMinMax image1_input = ReadImageMinMax(image1_filename);
  const ImageMinMax image2_input = ReadImageMinMax(image2_filename);

  // Setup processing of image1 and image2.
  const float min = std::min(image1_input.min, image2_input.min);
  const float max = std::max(image1_input.max, image2_input.max);
  const ExtractColormapFilters image1_filters
      = ExtractColormap(image1_input.image, z, min, max);
  const ExtractColormapFilters image2_filters
      = ExtractColormap(image2_input.image, z, min, max);

  // Setup drawing of contours.
  const ThresholdContourFilters contour1_filters = ThresholdContour(
      image1_filters.extract_filter->GetOutput(), contour_value);
  const ThresholdContourFilters contour2_filters = ThresholdContour(
      image2_filters.extract_filter->GetOutput(), contour_value);

  // Setup processing of background image.
  using Image3DType = itk::Image<float, 3>;
  using ImageFileReaderType = itk::ImageFileReader<Image3DType>;
  auto background_file_reader = ImageFileReaderType::New();
  background_file_reader->SetFileName(background_filename);
  const ResampleExtractRescaleFilters background1_filters
      = ResampleExtractRescale(background_file_reader->GetOutput(),
                               image1_input.image, z);
  const ResampleExtractRescaleFilters background2_filters
      = ResampleExtractRescale(background_file_reader->GetOutput(),
                               image2_input.image, z);

  // Setup composition of image, contour, and background and execute
  // the pipeline.
  DrawBlendFlipWrite(image1_filters.colormap_filter->GetOutput(),
                     contour1_filters.contour_filter->GetOutput(),
                     background1_filters.rescale_filter->GetOutput(), alpha,
                     "image1_" + std::to_string(z) + ".png");
  DrawBlendFlipWrite(image2_filters.colormap_filter->GetOutput(),
                     contour2_filters.contour_filter->GetOutput(),
                     background2_filters.rescale_filter->GetOutput(), alpha,
                     "image2_" + std::to_string(z) + ".png");

  return EXIT_SUCCESS;
}
