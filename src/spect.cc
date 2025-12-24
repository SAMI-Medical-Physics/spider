// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2025 South Australia Medical Imaging

#include "spect.h"

#include <charconv> // std::from_chars
#include <chrono>
#include <cstddef> // std::size_t
#include <expected>
#include <iomanip> // std::quoted
#include <istream>
#include <ostream>
#include <string>
#include <string_view>
#include <system_error> // std::errc
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
  constexpr const char msg[] = "contains a newline; discarding characters "
                               "after and including the first newline\n";
  for (std::size_t i = 0; i < spects.size(); ++i)
    {
      if (GetFirstLine(spects[i].patient_name) != spects[i].patient_name)
        Warning() << "SPECT " << i + 1 << ": patient name: " << msg;
      if (GetFirstLine(spects[i].acquisition_timestamp)
          != spects[i].acquisition_timestamp)
        Warning() << "SPECT " << i + 1 << ": acquisition time: " << msg;
      if (GetFirstLine(spects[i].decay_correction_method)
          != spects[i].decay_correction_method)
        Warning() << "SPECT " << i + 1 << ": decay correction: " << msg;
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

std::vector<Spect>
ReadSpects(std::istream& in)
{
  std::string line;
  // Skip the first two lines containing commentary.
  std::getline(in, line);
  std::getline(in, line);
  if (!in)
    return {};
  std::vector<Spect> spects;
  for (;;)
    {
      // Skip leading empty line.
      if (!std::getline(in, line))
        break;
      // Keep an incomplete SPECT.
      spects.emplace_back();
      Spect& s = spects.back();
      if (!std::getline(in, s.patient_name))
        break;
      if (!std::getline(in, s.acquisition_timestamp))
        break;
      if (!std::getline(in, s.decay_correction_method))
        break;
      if (!std::getline(in, line))
        break;
      auto [ptr, ec] = std::from_chars(line.data(), line.data() + line.size(),
                                       s.half_life);
      if (ec != std::errc())
        {
          Warning() << "SPECT " << spects.size()
                    << ": failed to set radionuclide half-life\n";
        }
    }
  return spects;
}

bool
ParseDicomDate(const std::string_view v, Date& date)
{
  if (v.size() != 8)
    return false;
  int y{}, m{}, d{};
  auto [ptr_y, ec_y] = std::from_chars(v.data(), v.data() + 4, y);
  auto [ptr_m, ec_m] = std::from_chars(ptr_y, ptr_y + 2, m);
  auto [ptr_d, ec_d] = std::from_chars(ptr_m, ptr_m + 2, d);
  if (ec_y != std::errc() || ec_m != std::errc() || ec_d != std::errc()
      || ptr_d != v.data() + 8 || y < 0 || m < 0 || d < 0)
    return false;
  date.year = y;
  date.month = m;
  date.day = d;
  return true;
}

bool
ParseDicomTime(std::string_view v, Time& time)
{
  if (v.size() < 2)
    return false;
  // Hours as HH.
  int h{};
  auto [ptr_h, ec_h] = std::from_chars(v.data(), v.data() + 2, h);
  if (ec_h != std::errc() || ptr_h != v.data() + 2 || h < 0 || h > 23)
    return false;
  // Minutes as MM.
  v.remove_prefix(2);
  if (v.size() == 1)
    return false;
  int m = 0;
  if (v.size() >= 2)
    {
      auto [ptr_m, ec_m] = std::from_chars(v.data(), v.data() + 2, m);
      if (ec_m != std::errc() || ptr_m != v.data() + 2 || m < 0 || m > 59)
        return false;
      v.remove_prefix(2);
    }
  // Seconds as SS.
  if (v.size() == 1)
    return false;
  int s = 0;
  if (v.size() >= 2)
    {
      // Discard fractional seconds.
      auto [ptr_s, ec_s] = std::from_chars(v.data(), v.data() + 2, s);
      if (ec_s != std::errc() || ptr_s != v.data() + 2 || s < 0 || s > 60)
        return false;
    }
  time.hour = h;
  time.minute = m;
  time.second = s;
  return true;
}

std::expected<std::chrono::zoned_time<std::chrono::seconds>,
              DatetimeParseError>
MakeZonedTime(const Date& d, const Time& t, const std::chrono::time_zone* tz)
{
  using namespace std::chrono;
  const year_month_day ymd{ year{ d.year },
                            month{ static_cast<unsigned>(d.month) },
                            day{ static_cast<unsigned>(d.day) } };
  if (!ymd.ok())
    return std::unexpected(DatetimeParseError::kInvalidDate);
  const local_seconds lt = local_days{ ymd } + hours{ t.hour }
                           + minutes{ t.minute } + seconds{ t.second };
  try
    {
      return zoned_time<seconds>{ tz, lt };
    }
  catch (const std::chrono::ambiguous_local_time& e)
    {
      // DST fall-back.
      Warning() << e.what() << "\nChoosing the earlier time point\n";
      return zoned_time<seconds>{ tz, lt, choose::earliest };
    }
  catch (const std::chrono::nonexistent_local_time&)
    {
      // DST spring-forward.
      return std::unexpected(DatetimeParseError::kNonexistentLocalTime);
    }
}

std::expected<std::chrono::zoned_time<std::chrono::seconds>,
              DatetimeParseError>
ParseTimestamp(std::string_view v, const std::chrono::time_zone* tz)
{
  if (v.size() < 10)
    return std::unexpected(DatetimeParseError::kTooShort);
  Date date;
  if (!ParseDicomDate(v.substr(0, 8), date))
    return std::unexpected(DatetimeParseError::kFailedDate);
  v.remove_prefix(8);
  Time time;
  if (!ParseDicomTime(v, time))
    return std::unexpected(DatetimeParseError::kFailedTime);
  return MakeZonedTime(date, time, tz);
}

std::chrono::seconds
DiffTime(const std::chrono::zoned_time<std::chrono::seconds>& time_end,
         const std::chrono::zoned_time<std::chrono::seconds>& time_beg)
{
  return time_end.get_sys_time() - time_beg.get_sys_time();
}

} // namespace spider
