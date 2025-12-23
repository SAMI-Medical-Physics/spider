// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2025 South Australia Medical Imaging

#include "spect.h"

#include <iomanip>  // std::quoted
#include <ostream>
#include <string>
#include <string_view>
#include <vector>

#include <gdcmAttribute.h>
#include <gdcmDataSet.h>
#include <gdcmSequenceOfItems.h>
#include <gdcmSmartPointer.h>
#include <gdcmTag.h>

#include "logging.h"

namespace spider
{

std::string
GetAcquisitionTimestamp(const gdcm::DataSet& ds)
{
  if (!ds.FindDataElement(gdcm::Tag(0x0008, 0x0022))
      || !ds.FindDataElement(gdcm::Tag(0x0008, 0x0032)))
    {
      Warning()
          << "missing DICOM attribute: AcquisitionDate or AcquisitionTime\n";
      return {};
    }
  gdcm::Attribute<0x0008, 0x0022> at_date;
  at_date.SetFromDataSet(ds);
  gdcm::Attribute<0x0008, 0x0032> at_time;
  at_time.SetFromDataSet(ds);
  return at_date.GetValue() + at_time.GetValue();
}

double
GetHalfLife(const gdcm::DataSet& ds)
{
  const gdcm::Tag tag_sq(0x0054, 0x0016);
  if (!ds.FindDataElement(tag_sq))
    {
      Warning() << "missing DICOM attribute: "
                   "RadiopharmaceuticalInformationSequence\n";
      return {};
    }
  gdcm::SmartPointer<gdcm::SequenceOfItems> sq
      = ds.GetDataElement(tag_sq).GetValueAsSQ();
  if (!sq || !sq->GetNumberOfItems())
    {
      Warning() << "DICOM attribute RadiopharmaceuticalInformationSequence is "
                   "present but either empty or not encoded as SQ\n";
      return {};
    }
  const gdcm::DataSet& nds = sq->GetItem(1).GetNestedDataSet();
  const gdcm::Tag tag(0x0018, 0x1075);
  if (!nds.FindDataElement(tag))
    {
      Warning() << "missing DICOM attribute: RadionuclideHalfLife\n";
      return {};
    }
  gdcm::Attribute<0x0018, 0x1075> a;
  a.SetFromDataElement(nds.GetDataElement(tag));
  return a.GetValue();
}

std::ostream&
operator<<(std::ostream& os, const Spect& s)
{
  return os << "Spect{"
            << "half_life=" << s.half_life << " s, acquisition_timestamp="
            << std::quoted(s.acquisition_timestamp)
            << ", decay_correction_method="
            << std::quoted(s.decay_correction_method)
            << ", patient_name=" << std::quoted(s.patient_name) << "}";
}

Spect
ReadDicomSpect(const gdcm::DataSet& ds)
{
  Spect spect;
  // Read decay correction method.
  if (ds.FindDataElement(gdcm::Tag(0x0054, 0x1102)))
    {
      gdcm::Attribute<0x0054, 0x1102> a;
      a.SetFromDataSet(ds);
      spect.decay_correction_method = a.GetValue();
    }
  else
    Warning() << "missing DICOM attribute: DecayCorrection\n";

  spect.acquisition_timestamp = GetAcquisitionTimestamp(ds);
  spect.half_life = GetHalfLife(ds);
  // Read patient name.
  if (ds.FindDataElement(gdcm::Tag(0x0010, 0x0010)))
    {
      gdcm::Attribute<0x0010, 0x0010> a;
      a.SetFromDataSet(ds);
      spect.patient_name = a.GetValue();
    }
  else
    Warning() << "missing DICOM attribute: PatientName\n";

  return spect;
}

std::string_view
GetFirstLine(const std::string_view v)
{
  auto pos = v.find('\n');
  if (pos != std::string_view::npos)
    return v.substr(0, pos);
  return v;
}

void
WriteSpects(const std::vector<Spect>& spects, std::ostream& os)
{
  // Ensure each value is on a separate line and on contiguous lines.
  std::string warning_msg = "contains a newline; discarding characters "
                            "after and including the first newline\n";
  for (size_t i = 0; i < spects.size(); ++i)
    {
      if (GetFirstLine(spects[i].patient_name) != spects[i].patient_name)
        Warning() << "SPECT " << i + 1 << ": patient name: " << warning_msg;
      if (GetFirstLine(spects[i].acquisition_timestamp)
          != spects[i].acquisition_timestamp)
        Warning() << "SPECT " << i + 1
                  << ": acquisition time: " << warning_msg;
      if (GetFirstLine(spects[i].decay_correction_method)
          != spects[i].decay_correction_method)
        Warning() << "SPECT " << i + 1
                  << ": decay correction: " << warning_msg;
    }

  os << "This file was created by Spider.\n"
     << "If you edit it by hand, you could mess it up.\n";
  for (auto& spect : spects)
    {
      os << "\n"
         << GetFirstLine(spect.patient_name) << "\n"
         << GetFirstLine(spect.acquisition_timestamp) << "\n"
         << GetFirstLine(spect.decay_correction_method) << "\n"
         << spect.half_life << "\n";
    }
}

} // namespace spider
