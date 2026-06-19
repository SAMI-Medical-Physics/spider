// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 South Australia Medical Imaging

// Usage: ./set_modality_pt FILE
//
// Edit the DICOM file FILE in place, setting the attribute Modality
// (0008,0060) to 'PT'.

#include <cstdio>  // std::fputs, stderr
#include <cstdlib> // EXIT_FAILURE, EXIT_SUCCESS

#include <gdcmAttribute.h>
#include <gdcmDataSet.h>
#include <gdcmReader.h>
#include <gdcmWriter.h>

int
main(int argc, char* argv[])
{
  if (argc < 2)
    {
      std::fputs("usage: ./set_modality_pt file\n", stderr);
      return EXIT_FAILURE;
    }
  gdcm::Reader r;
  r.SetFileName(argv[1]);
  if (!r.Read())
    {
      std::fputs("Failed to read file\n", stderr);
      return EXIT_FAILURE;
    }
  gdcm::DataSet& ds = r.GetFile().GetDataSet();
  gdcm::Attribute<0x0008, 0x0060> a; // Modality
  a.SetValue("PT");
  ds.Replace(a.GetAsDataElement());
  gdcm::Writer w;
  w.SetFileName(argv[1]);
  w.SetFile(r.GetFile());
  if (!w.Write())
    {
      std::fputs("Failed to write file\n", stderr);
      return EXIT_FAILURE;
    }
  return EXIT_SUCCESS;
}
