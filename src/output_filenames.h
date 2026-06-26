// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 South Australia Medical Imaging

// Predict output filenames, useful for overwrite detection.

#ifndef SPIDER_OUTPUT_FILENAMES_H
#define SPIDER_OUTPUT_FILENAMES_H

#include <filesystem>
#include <string_view>
#include <vector>

namespace spider
{

// itk::ImageFileWriter may write file(s) other than the name passed
// to SetFileName.  Return the file names that are actually written
// when FILENAME is passed to SetFileName and COMPRESS is passed to
// SetUseCompression.  This function only handles filenames that
// select the NIfTI, NRRD, and MetaImage IO modules.
std::vector<std::filesystem::path>
OutputFilenames(std::string_view filename, bool compress);

} // namespace spider

#endif // SPIDER_OUTPUT_FILENAMES_H
