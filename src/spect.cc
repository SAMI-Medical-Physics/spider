// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2025 South Australia Medical Imaging

#include "spect.h"

#include <charconv> // std::from_chars
#include <chrono>
#include <cstddef> // std::size_t
#include <cstdlib> // std::strtod
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
#include "tz_compat.h"

namespace spider
{

namespace
{

bool
ParseYear(const std::string_view v, int& out)
{
  if (v.size() < 4)
    return false;
  auto [ptr, ec] = std::from_chars(v.data(), v.data() + 4, out);
  return (ec == std::errc() && (ptr == v.data() + 4) && out >= 0);
}

bool
ParseTwoDigits(const std::string_view v, int& out)
{
  if (v.size() < 2)
    return false;
  auto [ptr, ec] = std::from_chars(v.data(), v.data() + 2, out);
  return ec == std::errc() && (ptr == v.data() + 2);
}

bool
ParseMonthOrDay(const std::string_view v, int& out)
{
  return ParseTwoDigits(v, out) && out >= 0;
}

bool
ParseHour(const std::string_view v, int& out)
{
  return ParseTwoDigits(v, out) && out >= 0 && out <= 23;
}

bool
ParseMinute(const std::string_view v, int& out)
{
  return ParseTwoDigits(v, out) && out >= 0 && out <= 59;
}

bool
ParseSecond(const std::string_view v, int& out)
{
  return ParseTwoDigits(v, out) && out >= 0 && out <= 60;
}

} // namespace

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
      char* end{};
      s.half_life = std::strtod(line.c_str(), &end);
      if (end == line.c_str())
        {
          Warning() << "SPECT " << spects.size()
                    << ": failed to set radionuclide half-life\n";
        }
    }
  return spects;
}

bool
ParseDicomDate(std::string_view v, DateComplete& date)
{
  if (v.size() != 8)
    return false;
  int y{}, m{}, d{};
  if (!ParseYear(v, y))
    return false;
  v.remove_prefix(4);
  if (!ParseMonthOrDay(v, m))
    return false;
  v.remove_prefix(2);
  if (!ParseMonthOrDay(v, d))
    return false;
  date = DateComplete{ .year = y, .month = m, .day = d };
  return true;
}

bool
ParseDicomTime(std::string_view v, TimeParsed& time)
{
  // For DICOM TM values, only the hour component is required.
  int hour = 0;
  if (!ParseHour(v, hour))
    return false;
  v.remove_prefix(2);
  if (v.size() == 1)
    return false;
  std::optional<int> minute;
  if (v.size() >= 2)
    {
      int m = 0;
      if (!ParseMinute(v, m))
        return false;
      minute = m;
      v.remove_prefix(2);
    }
  if (v.size() == 1)
    return false;
  std::optional<int> second;
  if (v.size() >= 2)
    {
      int s = 0;
      // XXX: We ignore any fractional second component '.FFFFFF'.
      if (!ParseSecond(v, s))
        return false;
      second = s;
    }
  time = TimeParsed{ .hour = hour, .minute = minute, .second = second };
  return true;
}

std::expected<tz::zoned_time<std::chrono::seconds>, DatetimeParseError>
MakeZonedTime(const DateComplete& d, const TimeComplete& t,
              const tz::time_zone& tz)
{
  const tz::year_month_day ymd{ tz::year{ d.year },
                                tz::month{ static_cast<unsigned>(d.month) },
                                tz::day{ static_cast<unsigned>(d.day) } };
  if (!ymd.ok())
    return std::unexpected(DatetimeParseError::kInvalidDate);
  const tz::local_seconds lt
      = tz::local_days{ ymd } + std::chrono::hours{ t.hour }
        + std::chrono::minutes{ t.minute } + std::chrono::seconds{ t.second };
  try
    {
      return tz::zoned_time<std::chrono::seconds>{ &tz, lt };
    }
  catch (const tz::ambiguous_local_time& e)
    {
      // DST fall-back.
      Warning() << e.what() << "\nChoosing the earlier time point\n";
      return tz::zoned_time<std::chrono::seconds>{ &tz, lt,
                                                   tz::choose::earliest };
    }
  catch (const tz::nonexistent_local_time&)
    {
      // DST spring-forward.
      return std::unexpected(DatetimeParseError::kNonexistentLocalTime);
    }
}

std::chrono::seconds
DiffTime(const tz::zoned_time<std::chrono::seconds>& time_end,
         const tz::zoned_time<std::chrono::seconds>& time_beg)
{
  return time_end.get_sys_time() - time_beg.get_sys_time();
}

} // namespace spider
