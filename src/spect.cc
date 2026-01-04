// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2025, 2026 South Australia Medical Imaging

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
GetAcquisitionDate(const gdcm::DataSet& ds)
{
  if (!ds.FindDataElement(gdcm::Tag(0x0008, 0x0022)))
    {
      Warning() << "missing DICOM attribute: AcquisitionDate\n";
      return {};
    }
  gdcm::Attribute<0x0008, 0x0022> at;
  at.SetFromDataSet(ds);
  return at.GetValue();
}

std::string
GetAcquisitionTime(const gdcm::DataSet& ds)
{
  if (!ds.FindDataElement(gdcm::Tag(0x0008, 0x0032)))
    {
      Warning() << "missing DICOM attribute: AcquisitionTime\n";
      return {};
    }
  gdcm::Attribute<0x0008, 0x0032> at;
  at.SetFromDataSet(ds);
  return at.GetValue();
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
            << "patient_name=" << std::quoted(s.patient_name)
            << ", acquisition_date=" << std::quoted(s.acquisition_date)
            << ", acquisition_time=" << std::quoted(s.acquisition_time)
            << ", decay_correction_method="
            << std::quoted(s.decay_correction_method)
            << ", half_life=" << s.half_life << " s" << "}";
}

Spect
ReadDicomSpect(const gdcm::DataSet& ds)
{
  Spect spect;
  // Read patient name.
  if (ds.FindDataElement(gdcm::Tag(0x0010, 0x0010)))
    {
      gdcm::Attribute<0x0010, 0x0010> a;
      a.SetFromDataSet(ds);
      spect.patient_name = a.GetValue();
    }
  else
    Warning() << "missing DICOM attribute: PatientName\n";

  spect.acquisition_date = GetAcquisitionDate(ds);
  spect.acquisition_time = GetAcquisitionTime(ds);
  // Read decay correction method.
  if (ds.FindDataElement(gdcm::Tag(0x0054, 0x1102)))
    {
      gdcm::Attribute<0x0054, 0x1102> a;
      a.SetFromDataSet(ds);
      spect.decay_correction_method = a.GetValue();
    }
  else
    Warning() << "missing DICOM attribute: DecayCorrection\n";
  spect.half_life = GetHalfLife(ds);

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
      if (GetFirstLine(spects[i].acquisition_date)
          != spects[i].acquisition_date)
        Warning() << "SPECT " << i + 1 << ": acquisition date: " << msg;
      if (GetFirstLine(spects[i].acquisition_time)
          != spects[i].acquisition_time)
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
         << GetFirstLine(spect.acquisition_date) << "\n"
         << GetFirstLine(spect.acquisition_time) << "\n"
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
      if (!std::getline(in, s.acquisition_date))
        break;
      if (!std::getline(in, s.acquisition_time))
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

bool
ParseDicomDateTimeExcludingUtc(std::string_view v, DateParsed& date,
                               TimeParsed& time)
{
  // For DICOM Date Time (DT) values, only the year component is
  // required.
  int year = 0;
  if (!ParseYear(v, year))
    return false;
  v.remove_prefix(4);
  if (v.size() == 1)
    return false;
  std::optional<int> month;
  if (v.size() >= 2)
    {
      int m = 0;
      if (!ParseMonthOrDay(v, m))
        return false;
      month = m;
      v.remove_prefix(2);
    }
  if (v.size() == 1)
    return false;
  std::optional<int> day;
  if (v.size() >= 2)
    {
      int d = 0;
      if (!ParseMonthOrDay(v, d))
        return false;
      day = d;
      v.remove_prefix(2);
    }
  if (v.size() == 1)
    return false;
  std::optional<int> hour;
  if (v.size() >= 2)
    {
      int h = 0;
      if (!ParseHour(v, h))
        return false;
      hour = h;
      v.remove_prefix(2);
    }
  if (v.size() == 1)
    return false;
  std::optional<int> minute;
  if (v.size() >= 2)
    {
      int min = 0;
      if (!ParseMinute(v, min))
        return false;
      minute = min;
      v.remove_prefix(2);
    }
  if (v.size() == 1)
    return false;
  std::optional<int> second;
  if (v.size() >= 2)
    {
      int s = 0;
      if (!ParseSecond(v, s))
        return false;
      second = s;
    }
  date = DateParsed{ .year = year, .month = month, .day = day };
  time = TimeParsed{ .hour = hour, .minute = minute, .second = second };
  return true;
}

bool
ParseDicomUtcOffset(std::string_view v, std::chrono::minutes& offset)
{
  if (v.size() < 5)
    return false;
  // C.12.1.1.8: "Leading space characters shall not be present."
  const char sign = v[0];
  if (sign != '+' && sign != '-')
    return false;
  v.remove_prefix(1);
  int hour = 0;
  if (!ParseHour(v, hour))
    return false;
  v.remove_prefix(2);
  int minute = 0;
  if (!ParseMinute(v, minute))
    return false;
  std::chrono::minutes off{ hour * 60 + minute };
  if (sign == '-')
    off = -off;
  // C.12.1.1.8: "-0000 shall not be used."
  if (sign == '-' && off == std::chrono::minutes{ 0 })
    return false;
  // Table 6.2-1, Date Time VR, Note 1: "The range of the offset is
  // -1200 to +1400."
  if ((off < std::chrono::minutes{ -12 * 60 })
      || (off > std::chrono::minutes{ 14 * 60 }))
    return false;
  offset = off;
  return true;
}

std::expected<DateComplete, MakeTimePointError>
MakeDateComplete(const DateParsed& date_parsed)
{
  if (!date_parsed.month.has_value() || !date_parsed.day.has_value())
    return std::unexpected(MakeTimePointError::kIncompleteDate);
  return DateComplete{ .year = date_parsed.year,
                       .month = date_parsed.month.value(),
                       .day = date_parsed.day.value() };
}

std::expected<TimeComplete, MakeTimePointError>
MakeTimeComplete(const TimeParsed& time_parsed)
{
  if (!time_parsed.hour.has_value() || !time_parsed.minute.has_value()
      || !time_parsed.second.has_value())
    return std::unexpected(MakeTimePointError::kIncompleteTime);
  return TimeComplete{ .hour = time_parsed.hour.value(),
                       .minute = time_parsed.minute.value(),
                       .second = time_parsed.second.value() };
}

std::expected<tz::zoned_time<std::chrono::seconds>, MakeTimePointError>
MakeZonedTime(const DateComplete& d, const TimeComplete& t,
              const tz::time_zone& tz)
{
  const tz::year_month_day ymd{ tz::year{ d.year },
                                tz::month{ static_cast<unsigned>(d.month) },
                                tz::day{ static_cast<unsigned>(d.day) } };
  if (!ymd.ok())
    return std::unexpected(MakeTimePointError::kInvalidDate);
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
      return std::unexpected(MakeTimePointError::kNonexistentLocalTime);
    }
}

std::expected<std::chrono::sys_seconds, MakeTimePointError>
MakeSysTimeFromOffset(const DateComplete& d, const TimeComplete& t,
                      std::chrono::minutes offset)
{
  const tz::year_month_day ymd{ tz::year{ d.year },
                                tz::month{ static_cast<unsigned>(d.month) },
                                tz::day{ static_cast<unsigned>(d.day) } };
  if (!ymd.ok())
    return std::unexpected(MakeTimePointError::kInvalidDate);
  const tz::local_seconds lt
      = tz::local_days{ ymd } + std::chrono::hours{ t.hour }
        + std::chrono::minutes{ t.minute } + std::chrono::seconds{ t.second };
  return std::chrono::sys_seconds{ lt.time_since_epoch() - offset };
}

std::expected<std::chrono::sys_seconds, MakeTimePointError>
MakeSysTimeFromOffsetOrTimeZone(const DateComplete& date,
                                const TimeComplete& time,
                                std::string_view voffset,
                                const tz::time_zone* tz)
{
  if (voffset.size() != 0)
    {
      // Use offset in VOFFSET.
      std::chrono::minutes offset{};
      if (!ParseDicomUtcOffset(voffset, offset))
        return std::unexpected(MakeTimePointError::kFailedUtc);
      return MakeSysTimeFromOffset(date, time, offset);
    }
  // Interpret DATE and TIME in the time zone TZ.
  if (!tz)
    return std::unexpected(MakeTimePointError::kMissingTimeZone);
  const auto zt = MakeZonedTime(date, time, *tz);
  if (!zt.has_value())
    return std::unexpected(zt.error());
  return zt.value().get_sys_time();
}

std::expected<std::chrono::sys_seconds, MakeTimePointError>
MakeSysTimeFromDicomDateAndTime(std::string_view vdate, std::string_view vtime,
                                std::string_view voffset,
                                const tz::time_zone* tz)
{
  DateComplete date;
  if (!ParseDicomDate(vdate, date))
    return std::unexpected(MakeTimePointError::kFailedDate);
  TimeParsed time_parsed;
  if (!ParseDicomTime(vtime, time_parsed))
    return std::unexpected(MakeTimePointError::kFailedTime);
  const auto time_complete = MakeTimeComplete(time_parsed);
  if (!time_complete.has_value())
    return std::unexpected(time_complete.error());
  const TimeComplete time = time_complete.value();
  return MakeSysTimeFromOffsetOrTimeZone(date, time, voffset, tz);
}

std::expected<std::chrono::sys_seconds, MakeTimePointError>
MakeSysTimeFromDicomDateTime(std::string_view datetime,
                             std::string_view voffset, const tz::time_zone* tz)
{
  DateParsed date_parsed;
  TimeParsed time_parsed;
  if (!ParseDicomDateTimeExcludingUtc(datetime, date_parsed, time_parsed))
    return std::unexpected(MakeTimePointError::kFailedDateTimeExcludingUtc);
  const auto date_complete = MakeDateComplete(date_parsed);
  if (!date_complete.has_value())
    return std::unexpected(date_complete.error());
  const auto time_complete = MakeTimeComplete(time_parsed);
  if (!time_complete.has_value())
    return std::unexpected(time_complete.error());

  const DateComplete date = date_complete.value();
  const TimeComplete time = time_complete.value();
  if (const std::size_t pos = datetime.find_first_of("+-");
      pos != std::string_view::npos)
    {
      // DT includes an offset; use it.
      std::chrono::minutes offset{};
      if (!ParseDicomUtcOffset(datetime.substr(pos), offset))
        return std::unexpected(MakeTimePointError::kFailedUtcInDateTime);
      return MakeSysTimeFromOffset(date, time, offset);
    }
  // DT does not include an offset.
  return MakeSysTimeFromOffsetOrTimeZone(date, time, voffset, tz);
}

} // namespace spider
