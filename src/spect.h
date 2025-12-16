// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2025 South Australia Medical Imaging

#ifndef SPIDER_SPECT_H
#define SPIDER_SPECT_H

#include <ostream>
#include <string>
#include <string_view>
#include <vector>

#include <gdcmDataSet.h>

namespace spider
{

struct Spect
{
  double half_life = 0.0;              // (0x0018, 0x1075), seconds
  std::string acquisition_timestamp;   // (0x0008, 0x0022) + (0x0008, 0x0032)
  std::string decay_correction_method; // (0x0054, 0x1102)
  std::string patient_name;            // (0x0010, 0x0010)
};

// Concatenate the DICOM attributes AcquisitionDate and
// AcquisitionTime in dataset DS.
std::string
GetAcquisitionTimestamp(const gdcm::DataSet& ds);

double
GetHalfLife(const gdcm::DataSet& ds);

// Fill a Spect from the DICOM attributes in dataset DS.
Spect
ReadDicomSpect(const gdcm::DataSet& ds);

std::string_view
GetFirstLine(const std::string_view v);

void
WriteSpects(const std::vector<Spect>& spects, std::ostream& os);

} // namespace spider

#endif // SPIDER_SPECT_H
