// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2025 South Australia Medical Imaging

#ifndef SPIDER_TEST_TEST_UTILS_H
#define SPIDER_TEST_TEST_UTILS_H

namespace spider
{
namespace test
{
template <typename TImage>
// Inlined because the definition is included, as required for
// template.
inline typename TImage::Pointer
CreateImage(unsigned long extent = 2)
{
  using ImageType = TImage;
  auto image = ImageType::New();
  typename ImageType::IndexType start;
  start.Fill(0);
  typename ImageType::SizeType size;
  size.Fill(extent);
  typename ImageType::RegionType region(start, size);
  image->SetRegions(region);
  image->Allocate();
  return image;
}
} // namespace test
} // namespace spider

#endif // SPIDER_TEST_TEST_UTILS_H
