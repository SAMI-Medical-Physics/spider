// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 South Australia Medical Imaging

#include "output_filenames.h"

#include <algorithm> // std::transform
#include <cctype>    // std::tolower
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

namespace spider
{

namespace
{

std::filesystem::path
WithExtension(std::filesystem::path p, std::string_view ext)
{
  p.replace_extension(ext);
  return p;
}

std::filesystem::path
SiblingFile(const std::filesystem::path& p, std::string_view name)
{
  return p.parent_path() / name;
}

std::string
Lower(std::string s)
{
  std::transform(s.begin(), s.end(), s.begin(),
                 [](unsigned char c) { return std::tolower(c); });
  return s;
}

} // namespace

std::vector<std::filesystem::path>
OutputFilenames(std::string_view filename, bool compress)
{
  const std::filesystem::path out{ filename };
  const auto ext = out.extension();
  const auto stem_ext = out.stem().extension();

  // Do the most common ones first.
  if (ext == ".nii" || (ext == ".gz" && stem_ext == ".nii") || ext == ".nrrd"
      || ext == ".mha")
    return { out };

  // Other NIfTI.
  if (ext == ".hdr")
    return { out, WithExtension(out, ".img") };
  if (ext == ".img")
    return { WithExtension(out, ".hdr"), out };
  if (ext == ".gz" && stem_ext == ".hdr") // path/to/x.hdr.gz
    {
      auto img = out;
      img.replace_extension().replace_extension(".img.gz");
      return { out, img };
    }
  if (ext == ".gz" && stem_ext == ".img")
    {
      auto hdr = out;
      hdr.replace_extension().replace_extension(".hdr.gz");
      return { hdr, out };
    }
  // NIfTI only accepts dotfiles when a directory is included in the
  // name.
  const auto fname = out.filename();
  if (!out.parent_path().empty())
    {
      if (fname == ".nii" || fname == ".nii.gz")
        return { out };
      if (fname == ".hdr")
        return { out, SiblingFile(out, ".img") };
      if (fname == ".img")
        return { SiblingFile(out, ".hdr"), out };
      if (fname == ".hdr.gz")
        return { out, SiblingFile(out, ".img.gz") };
      if (fname == ".img.gz")
        return { SiblingFile(out, ".hdr.gz"), out };
    }

  // Other NRRD.
  if (ext == ".nhdr")
    return { out, WithExtension(out, compress ? ".raw.gz" : ".raw") };
  // The dotfile ".nhdr" gives an error but still writes an empty file
  // with that name.  "x.Nhdr" and ".Nhdr" give attached headers.
  const auto ext_lower = Lower(ext.string());
  const auto fname_lower = Lower(fname.string());
  if (ext_lower == ".nrrd" || fname == ".nrrd" || ext_lower == ".nhdr"
      || fname_lower == ".nhdr")
    return { out };

  // Other MetaImage.
  if (fname == ".mha")
    return { out };
  const auto meta_data_ext = compress ? ".zraw" : ".raw";
  if (ext == ".mhd")
    return { out, WithExtension(out, meta_data_ext) };
  if (fname == ".mhd")
    return { out, SiblingFile(out, meta_data_ext) };
  // MetaImage .mha or .mhd containing an upper case character causes
  // .mhd and .raw or .zraw files to be written.
  const bool mixed_case_meta_ext
      = ext_lower == ".mhd" || (ext != ".mha" && ext_lower == ".mha");
  if (mixed_case_meta_ext)
    return { WithExtension(out, ".mhd"), WithExtension(out, meta_data_ext) };
  const bool mixed_case_meta_dotfile
      = fname_lower == ".mhd" || (fname != ".mha" && fname_lower == ".mha");
  if (mixed_case_meta_dotfile)
    return { SiblingFile(out, ".mhd"), SiblingFile(out, meta_data_ext) };

  return {};
}

} // namespace spider
