// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2025, 2026 South Australia Medical Imaging

#include "spect.h"

#include <charconv> // std::from_chars
#include <chrono>
#include <cmath>   // std::exp, std::log
#include <cstddef> // std::size_t
#include <cstdlib> // std::strtod
#include <expected>
#include <iomanip> // std::quoted
#include <istream>
#include <optional>
#include <ostream>
#include <string>
#include <string_view>
#include <system_error> // std::errc
#include <variant>
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

std::ostream&
operator<<(std::ostream& os, const Spect& s)
{
  std::ostringstream tmp;
  if (s.patient_name.has_value())
    tmp << "patient_name=" << std::quoted(s.patient_name.value()) << ", ";
  if (s.radiopharmaceutical_start_date_time.has_value())
    tmp << "radiopharmaceutical_start_date_time="
        << std::quoted(s.radiopharmaceutical_start_date_time.value()) << ", ";
  if (s.acquisition_date.has_value())
    tmp << "acquisition_date=" << std::quoted(s.acquisition_date.value())
        << ", ";
  if (s.acquisition_time.has_value())
    tmp << "acquisition_time=" << std::quoted(s.acquisition_time.value())
        << ", ";
  if (s.series_date.has_value())
    tmp << "series_date=" << std::quoted(s.series_date.value()) << ", ";
  if (s.series_time.has_value())
    tmp << "series_time=" << std::quoted(s.series_time.value()) << ", ";
  if (s.frame_reference_time.has_value())
    tmp << "frame_reference_time=" << s.frame_reference_time.value()
        << " ms, ";
  if (s.timezone_offset_from_utc.has_value())
    tmp << "timezone_offset_from_utc="
        << std::quoted(s.timezone_offset_from_utc.value()) << ", ";
  if (s.decay_correction.has_value())
    tmp << "decay_correction=" << std::quoted(s.decay_correction.value())
        << ", ";
  if (s.radionuclide_half_life.has_value())
    tmp << "radionuclide_half_life=" << s.radionuclide_half_life.value()
        << " s, ";

  std::string body = tmp.str();
  if (!body.empty())
    // Remove the last ", ".
    body.erase(body.size() - 2);

  os << "Spect{" << body << "}";
  return os;
}

std::optional<std::string>
GetPatientName(const gdcm::DataSet& ds)
{
  if (!ds.FindDataElement(gdcm::Tag(0x0010, 0x0010)))
    {
      Warning() << "missing DICOM attribute: PatientName\n";
      return {};
    }
  gdcm::Attribute<0x0010, 0x0010> a;
  a.SetFromDataSet(ds);
  return std::make_optional<std::string>(a.GetValue());
}

std::optional<std::string>
GetRadiopharmaceuticalStartDateTime(const gdcm::DataSet& ds)
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
  const gdcm::Tag tag(0x0018, 0x1078);
  if (!nds.FindDataElement(tag))
    {
      Warning()
          << "missing DICOM attribute: RadiopharmaceuticalStartDateTime\n";
      return {};
    }
  gdcm::Attribute<0x0018, 0x1078> a;
  a.SetFromDataElement(nds.GetDataElement(tag));
  return std::make_optional<std::string>(a.GetValue());
}

std::optional<std::string>
GetAcquisitionDate(const gdcm::DataSet& ds)
{
  if (!ds.FindDataElement(gdcm::Tag(0x0008, 0x0022)))
    {
      Warning() << "missing DICOM attribute: AcquisitionDate\n";
      return {};
    }
  gdcm::Attribute<0x0008, 0x0022> at;
  at.SetFromDataSet(ds);
  return std::make_optional<std::string>(at.GetValue());
}

std::optional<std::string>
GetAcquisitionTime(const gdcm::DataSet& ds)
{
  if (!ds.FindDataElement(gdcm::Tag(0x0008, 0x0032)))
    {
      Warning() << "missing DICOM attribute: AcquisitionTime\n";
      return {};
    }
  gdcm::Attribute<0x0008, 0x0032> at;
  at.SetFromDataSet(ds);
  return std::make_optional<std::string>(at.GetValue());
}

std::optional<std::string>
GetSeriesDate(const gdcm::DataSet& ds)
{
  if (!ds.FindDataElement(gdcm::Tag(0x0008, 0x0021)))
    {
      Warning() << "missing DICOM attribute: SeriesDate\n";
      return {};
    }
  gdcm::Attribute<0x0008, 0x0021> at;
  at.SetFromDataSet(ds);
  return std::make_optional<std::string>(at.GetValue());
}

std::optional<std::string>
GetSeriesTime(const gdcm::DataSet& ds)
{
  if (!ds.FindDataElement(gdcm::Tag(0x0008, 0x0031)))
    {
      Warning() << "missing DICOM attribute: SeriesTime\n";
      return {};
    }
  gdcm::Attribute<0x0008, 0x0031> at;
  at.SetFromDataSet(ds);
  return std::make_optional<std::string>(at.GetValue());
}

std::optional<double>
GetFrameReferenceTime(const gdcm::DataSet& ds)
{
  if (!ds.FindDataElement(gdcm::Tag(0x0054, 0x1300)))
    {
      Warning() << "missing DICOM attribute: FrameReferenceTime\n";
      return {};
    }
  gdcm::Attribute<0x0054, 0x1300> at;
  at.SetFromDataSet(ds);
  return at.GetValue();
}

std::optional<std::string>
GetTimezoneOffsetFromUtc(const gdcm::DataSet& ds)
{
  if (!ds.FindDataElement(gdcm::Tag(0x0008, 0x0201)))
    {
      Warning() << "missing DICOM attribute: TimezoneOffsetFromUTC\n";
      return {};
    }
  gdcm::Attribute<0x0008, 0x0201> at;
  at.SetFromDataSet(ds);
  return std::make_optional<std::string>(at.GetValue());
}

std::optional<std::string>
GetDecayCorrection(const gdcm::DataSet& ds)
{
  if (!ds.FindDataElement(gdcm::Tag(0x0054, 0x1102)))
    {
      Warning() << "missing DICOM attribute: DecayCorrection\n";
      return {};
    }
  gdcm::Attribute<0x0054, 0x1102> a;
  a.SetFromDataSet(ds);
  return std::make_optional<std::string>(a.GetValue());
}

std::optional<double>
GetRadionuclideHalfLife(const gdcm::DataSet& ds)
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

Spect
ReadDicomSpect(const gdcm::DataSet& ds)
{
  return Spect{ .patient_name = GetPatientName(ds),
                .radiopharmaceutical_start_date_time
                = GetRadiopharmaceuticalStartDateTime(ds),
                .acquisition_date = GetAcquisitionDate(ds),
                .acquisition_time = GetAcquisitionTime(ds),
                .series_date = GetSeriesDate(ds),
                .series_time = GetSeriesTime(ds),
                .frame_reference_time = GetFrameReferenceTime(ds),
                .timezone_offset_from_utc = GetTimezoneOffsetFromUtc(ds),
                .decay_correction = GetDecayCorrection(ds),
                .radionuclide_half_life = GetRadionuclideHalfLife(ds) };
}

std::optional<DateComplete>
ParseDicomDate(std::string_view v)
{
  // For DICOM DA values, all components are required.
  if (v.size() != 8)
    return {};
  int y = 0;
  if (!ParseYear(v, y))
    return {};
  v.remove_prefix(4);
  int m = 0;
  if (!ParseMonthOrDay(v, m))
    return {};
  v.remove_prefix(2);
  int d = 0;
  if (!ParseMonthOrDay(v, d))
    return {};
  return std::make_optional<DateComplete>({ .year = y, .month = m, .day = d });
}

std::optional<DicomTime>
ParseDicomTime(std::string_view v)
{
  // For DICOM TM values, only the hour component is required.
  int hour = 0;
  if (!ParseHour(v, hour))
    return {};
  v.remove_prefix(2);
  if (v.size() == 1)
    return {};
  std::optional<int> minute;
  if (v.size() >= 2)
    {
      int m = 0;
      if (!ParseMinute(v, m))
        return {};
      minute = m;
      v.remove_prefix(2);
    }
  if (v.size() == 1)
    return {};
  std::optional<int> second;
  if (v.size() >= 2)
    {
      int s = 0;
      // XXX: We ignore any fractional second component '.FFFFFF'.
      if (!ParseSecond(v, s))
        return {};
      second = s;
    }
  return std::make_optional<DicomTime>(
      { .hour = hour, .minute = minute, .second = second });
}

std::optional<DicomDateTime>
ParseDicomDateTime(std::string_view v)
{
  // For DICOM Date Time (DT) values, only the year component is
  // required.
  int year = 0;
  if (!ParseYear(v, year))
    return {};
  v.remove_prefix(4);
  // Table 6.2-1, Date Time VR, Note 5: "The offset may be included
  // regardless of null components; e.g., 2007-0500 is a legal value."
  std::optional<std::chrono::minutes> utc_offset;
  if (const std::size_t pos = v.find_first_of("+-");
      pos != std::string_view::npos)
    {
      utc_offset = ParseDicomUtcOffset(v.substr(pos));
      if (!utc_offset.has_value())
        return {};
      v.remove_suffix(v.size() - pos);
    }
  if (v.size() == 1)
    return {};
  std::optional<int> month;
  if (v.size() >= 2)
    {
      int m = 0;
      if (!ParseMonthOrDay(v, m))
        return {};
      month = m;
      v.remove_prefix(2);
    }
  if (v.size() == 1)
    return {};
  std::optional<int> day;
  if (v.size() >= 2)
    {
      int d = 0;
      if (!ParseMonthOrDay(v, d))
        return {};
      day = d;
      v.remove_prefix(2);
    }
  if (v.size() == 1)
    return {};
  std::optional<int> hour;
  if (v.size() >= 2)
    {
      int h = 0;
      if (!ParseHour(v, h))
        return {};
      hour = h;
      v.remove_prefix(2);
    }
  if (v.size() == 1)
    return {};
  std::optional<int> minute;
  if (v.size() >= 2)
    {
      int min = 0;
      if (!ParseMinute(v, min))
        return {};
      minute = min;
      v.remove_prefix(2);
    }
  if (v.size() == 1)
    return {};
  std::optional<int> second;
  if (v.size() >= 2)
    {
      int s = 0;
      if (!ParseSecond(v, s))
        return {};
      second = s;
    }
  return std::make_optional<DicomDateTime>({ .year = year,
                                             .month = month,
                                             .day = day,
                                             .hour = hour,
                                             .minute = minute,
                                             .second = second,
                                             .utc_offset = utc_offset });
}

std::optional<std::chrono::minutes>
ParseDicomUtcOffset(std::string_view v)
{
  if (v.size() < 5)
    return {};
  // C.12.1.1.8: "Leading space characters shall not be present."
  const char sign = v[0];
  if (sign != '+' && sign != '-')
    return {};
  v.remove_prefix(1);
  int hour = 0;
  if (!ParseHour(v, hour))
    return {};
  v.remove_prefix(2);
  int minute = 0;
  if (!ParseMinute(v, minute))
    return {};
  std::chrono::minutes offset{ hour * 60 + minute };
  if (sign == '-')
    offset = -offset;
  // C.12.1.1.8: "-0000 shall not be used."
  if (sign == '-' && offset == std::chrono::minutes{ 0 })
    return {};
  // Table 6.2-1, Date Time VR, Note 1: "The range of the offset is
  // -1200 to +1400."
  if ((offset < std::chrono::minutes{ -12 * 60 })
      || (offset > std::chrono::minutes{ 14 * 60 }))
    return {};
  return offset;
}

std::expected<DateComplete, TimePointError>
MakeDateComplete(const DicomDateTime& date_time)
{
  if (!date_time.month.has_value() || !date_time.day.has_value())
    return std::unexpected(TimePointError::kIncompleteDate);
  return DateComplete{ .year = date_time.year,
                       .month = date_time.month.value(),
                       .day = date_time.day.value() };
}

std::expected<TimeComplete, TimePointError>
MakeTimeComplete(const DicomTime& dicom_time)
{
  if (!dicom_time.minute.has_value() || !dicom_time.second.has_value())
    return std::unexpected(TimePointError::kIncompleteTimeInDicomTime);
  return TimeComplete{ .hour = dicom_time.hour,
                       .minute = dicom_time.minute.value(),
                       .second = dicom_time.second.value() };
}

std::expected<TimeComplete, TimePointError>
MakeTimeComplete(const DicomDateTime& date_time)
{
  if (!date_time.hour.has_value() || !date_time.minute.has_value()
      || !date_time.second.has_value())
    return std::unexpected(TimePointError::kIncompleteTimeInDicomDateTime);
  return TimeComplete{ .hour = date_time.hour.value(),
                       .minute = date_time.minute.value(),
                       .second = date_time.second.value() };
}

std::expected<tz::zoned_time<std::chrono::seconds>, TimePointError>
MakeZonedTime(const DateComplete& d, const TimeComplete& t,
              const tz::time_zone& tz)
{
  const tz::year_month_day ymd{ tz::year{ d.year },
                                tz::month{ static_cast<unsigned>(d.month) },
                                tz::day{ static_cast<unsigned>(d.day) } };
  if (!ymd.ok())
    return std::unexpected(TimePointError::kInvalidDate);
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
      return std::unexpected(TimePointError::kNonexistentLocalTime);
    }
}

std::expected<std::chrono::sys_seconds, TimePointError>
MakeSysTimeFromOffset(const DateComplete& d, const TimeComplete& t,
                      std::chrono::minutes offset)
{
  const tz::year_month_day ymd{ tz::year{ d.year },
                                tz::month{ static_cast<unsigned>(d.month) },
                                tz::day{ static_cast<unsigned>(d.day) } };
  if (!ymd.ok())
    return std::unexpected(TimePointError::kInvalidDate);
  const tz::local_seconds lt
      = tz::local_days{ ymd } + std::chrono::hours{ t.hour }
        + std::chrono::minutes{ t.minute } + std::chrono::seconds{ t.second };
  return std::chrono::sys_seconds{ lt.time_since_epoch() - offset };
}

std::expected<std::chrono::sys_seconds, TimePointError>
MakeSysTimeFromOffsetOrTimeZone(const DateComplete& date,
                                const TimeComplete& time,
                                std::optional<std::string_view> voffset,
                                const tz::time_zone* tz)
{
  if (voffset.has_value())
    {
      // Use offset in VOFFSET.
      const std::optional<std::chrono::minutes> offset
          = ParseDicomUtcOffset(voffset.value());
      if (!offset.has_value())
        return std::unexpected(TimePointError::kInvalidUtcOffset);
      return MakeSysTimeFromOffset(date, time, offset.value());
    }
  // Interpret DATE and TIME in the time zone TZ.
  if (!tz)
    return std::unexpected(TimePointError::kMissingTimeZone);
  const auto zt = MakeZonedTime(date, time, *tz);
  if (!zt.has_value())
    return std::unexpected(zt.error());
  return zt.value().get_sys_time();
}

std::expected<std::chrono::sys_seconds, TimePointError>
MakeSysTimeFromDicomDateAndTime(std::string_view vdate, std::string_view vtime,
                                std::optional<std::string_view> voffset,
                                const tz::time_zone* tz)
{
  const std::optional<DateComplete> date_parsed = ParseDicomDate(vdate);
  if (!date_parsed.has_value())
    return std::unexpected(TimePointError::kInvalidDicomDate);
  const std::optional<DicomTime> time_parsed = ParseDicomTime(vtime);
  if (!time_parsed.has_value())
    return std::unexpected(TimePointError::kInvalidDicomTime);
  const auto time_complete = MakeTimeComplete(time_parsed.value());
  if (!time_complete.has_value())
    return std::unexpected(time_complete.error());

  const DateComplete date = date_parsed.value();
  const TimeComplete time = time_complete.value();
  return MakeSysTimeFromOffsetOrTimeZone(date, time, voffset, tz);
}

std::expected<std::chrono::sys_seconds, TimePointError>
MakeSysTimeFromDicomDateTime(std::string_view datetime,
                             std::optional<std::string_view> voffset,
                             const tz::time_zone* tz)
{
  const std::optional<DicomDateTime> date_time_parsed
      = ParseDicomDateTime(datetime);
  if (!date_time_parsed.has_value())
    return std::unexpected(TimePointError::kInvalidDicomDateTime);
  const auto date_complete = MakeDateComplete(date_time_parsed.value());
  if (!date_complete.has_value())
    return std::unexpected(date_complete.error());
  const auto time_complete = MakeTimeComplete(date_time_parsed.value());
  if (!time_complete.has_value())
    return std::unexpected(time_complete.error());

  const DateComplete date = date_complete.value();
  const TimeComplete time = time_complete.value();
  if (date_time_parsed.value().utc_offset.has_value())
    {
      // DT includes an offset; use it.
      return MakeSysTimeFromOffset(
          date, time, date_time_parsed.value().utc_offset.value());
    }
  // DT does not include an offset.
  return MakeSysTimeFromOffsetOrTimeZone(date, time, voffset, tz);
}

std::expected<double, std::variant<TimePointErrorWithId, DecayCorrectionError>>
ComputeDecayFactorNone(const Spect& s, const tz::time_zone* tz)
{
  // Compute the decay factor from DICOM FrameReferenceTime to
  // AcquisitionDate and AcquisitionTime.  FrameReferenceTime is an
  // offset from SeriesDate and SeriesTime.  See DICOM Positron
  // Emission Tomography Supplement, C.8.X.4.1.5.  If
  // FrameReferenceTime is after AcquisitionDate and AcquisitionTime,
  // the decay factor is greater than 1.
  if (!s.series_date.has_value())
    return std::unexpected(
        TimePointErrorWithId{ .id = TimePointId::kSeriesDateAndTime,
                              .error = TimePointError::kMissingSeriesDate });
  if (!s.series_time.has_value())
    return std::unexpected(
        TimePointErrorWithId{ .id = TimePointId::kSeriesDateAndTime,
                              .error = TimePointError::kMissingSeriesTime });
  const auto st_series = MakeSysTimeFromDicomDateAndTime(
      s.series_date.value(), s.series_time.value(),
      (s.timezone_offset_from_utc.has_value())
          ? std::optional<std::string_view>{ s.timezone_offset_from_utc
                                                 .value() }
          : std::nullopt,
      tz);
  if (!st_series.has_value())
    return std::unexpected(TimePointErrorWithId{
        .id = TimePointId::kSeriesDateAndTime, .error = st_series.error() });

  const auto st_acquisition = MakeAcquisitionSysTime(s, tz);
  if (!st_acquisition.has_value())
    return std::unexpected(st_acquisition.error());

  if (!s.frame_reference_time.has_value())
    return std::unexpected(DecayCorrectionError::kMissingFrameReferenceTime);
  // Time difference in seconds.
  const double delta_t = std::chrono::duration<double>(st_acquisition.value()
                                                       - st_series.value())
                             .count()
                         - s.frame_reference_time.value() / 1000.0;

  if (!s.radionuclide_half_life.has_value())
    return std::unexpected(DecayCorrectionError::kMissingHalfLife);
  if (s.radionuclide_half_life.value() <= 0.0)
    return std::unexpected(
        DecayCorrectionError::kHalfLifeLessThanOrEqualToZero);
  return std::exp(-std::log(2) * delta_t / s.radionuclide_half_life.value());
}

std::expected<double, std::variant<TimePointErrorWithId, DecayCorrectionError>>
ComputeDecayFactorAdmin(const Spect& s, const tz::time_zone* tz)
{
  // Compute the decay factor from DICOM
  // RadiopharmaceuticalStartDateTime to AcquisitionDate and
  // AcquisitionTime.
  const auto st_admin = MakeRadiopharmaceuticalStartSysTime(s, tz);
  if (!st_admin.has_value())
    return std::unexpected(st_admin.error());

  const auto st_acquisition = MakeAcquisitionSysTime(s, tz);
  if (!st_acquisition.has_value())
    return std::unexpected(st_acquisition.error());
  // Time difference in seconds.
  const double delta_t = std::chrono::duration<double>(st_acquisition.value()
                                                       - st_admin.value())
                             .count();

  if (!s.radionuclide_half_life.has_value())
    return std::unexpected(DecayCorrectionError::kMissingHalfLife);
  if (s.radionuclide_half_life.value() <= 0.0)
    return std::unexpected(
        DecayCorrectionError::kHalfLifeLessThanOrEqualToZero);
  return std::exp(-std::log(2) * delta_t / s.radionuclide_half_life.value());
}

std::expected<double, std::variant<TimePointErrorWithId, DecayCorrectionError>>
ComputeDecayFactor(const Spect& s, const tz::time_zone* tz)
{
  if (!s.decay_correction.has_value())
    return std::unexpected(DecayCorrectionError::kMissingDecayCorrection);
  const auto dc = ParseDicomDecayCorrection(s.decay_correction.value());
  if (!dc.has_value())
    return std::unexpected(DecayCorrectionError::kInvalidDecayCorrection);
  switch (dc.value())
    {
    case DecayCorrection::kNone:
      return ComputeDecayFactorNone(s, tz);
    case DecayCorrection::kStart:
      return 1.0;
    case DecayCorrection::kAdmin:
      return ComputeDecayFactorAdmin(s, tz);
    }
  return std::unexpected(DecayCorrectionError::kUnreachable);
}

bool
UsesTimeZone(const Spect& s)
{
  return !s.timezone_offset_from_utc.has_value();
}

} // namespace spider
