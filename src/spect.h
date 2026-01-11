// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2025, 2026 South Australia Medical Imaging

#ifndef SPIDER_SPECT_H
#define SPIDER_SPECT_H

#include <chrono>
#include <expected>
#include <istream>
#include <optional>
#include <ostream>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

#include <gdcmDataSet.h>

#include "tz_compat.h"

namespace spider
{

// Selected DICOM attributes from a SPECT DICOM series.
struct Spect
{
  // For DICOM attributes represented as strings by GDCM, this
  // abstraction does not distinguish between an empty string value
  // and an absent attribute.
  std::string patient_name;
  std::string radiopharmaceutical_start_date_time;
  std::string acquisition_date;
  std::string acquisition_time;
  std::string series_date;
  std::string series_time;
  // std::optional is used for numeric types because there is no
  // initialised value semantically equivalent to an absent attribute.
  std::optional<double> frame_reference_time; // milliseconds
  std::string timezone_offset_from_utc;
  std::string decay_correction;
  std::optional<double> radionuclide_half_life; // seconds
};

std::ostream&
operator<<(std::ostream& os, const Spect& s);

std::string
GetPatientName(const gdcm::DataSet& ds);

std::string
GetRadiopharmaceuticalStartDateTime(const gdcm::DataSet& ds);

std::string
GetAcquisitionDate(const gdcm::DataSet& ds);

std::string
GetAcquisitionTime(const gdcm::DataSet& ds);

std::string
GetSeriesDate(const gdcm::DataSet& ds);

std::string
GetSeriesTime(const gdcm::DataSet& ds);

std::optional<double>
GetFrameReferenceTime(const gdcm::DataSet& ds);

std::string
GetTimezoneOffsetFromUtc(const gdcm::DataSet& ds);

std::string
GetDecayCorrection(const gdcm::DataSet& ds);

std::optional<double>
GetRadionuclideHalfLife(const gdcm::DataSet& ds);

// Fill a Spect from the DICOM attributes in dataset DS.
Spect
ReadDicomSpect(const gdcm::DataSet& ds);

std::string_view
GetFirstLine(std::string_view v);

void
WriteSpects(const std::vector<Spect>& spects, std::ostream& os);

std::vector<Spect>
ReadSpects(std::istream& in);

struct DateParsed
{
  int year;
  // DICOM Date Time (DT) values may be missing month and day.
  std::optional<int> month; // [1, 12]
  std::optional<int> day;   // [1, 31]
};

struct TimeParsed
{
  // DICOM Time (TM) values may be missing minute and second.  DICOM
  // Date Time (DT) values may be missing hour, minute, and second.
  std::optional<int> hour = 0;   // [0, 23]
  std::optional<int> minute = 0; // [0, 59]
  std::optional<int> second = 0; // [0, 60], 60 for leap second
};

struct DateComplete
{
  int year;
  int month; // [1, 12]
  int day;   // [1, 31]
};

struct TimeComplete
{
  int hour = 0;   // [0, 23]
  int minute = 0; // [0, 59]
  int second = 0; // [0, 60], 60 for leap second
};

// Parse a DICOM Date (DA) value from V into DATE.  If parsing fails,
// return false and leave DATE unchanged.
bool
ParseDicomDate(std::string_view v, DateComplete& date);

// Parse a DICOM Time (TM) value from V into TIME.  If parsing fails,
// return false and leave TIME unchanged.  Characters after the SS
// component are ignored, so non-DICOM-conformant strings may be
// parsed successfully.
bool
ParseDicomTime(std::string_view v, TimeParsed& time);

// Parse the date and time components of a DICOM Date Time (DT) value
// from V into D and T, ignoring any fractional second component and
// any UTC offset suffix.  If parsing fails, return false and leave D
// and T unchanged.  Characters after the SS component are ignored, so
// non-DICOM-conformant strings may be parsed successfully.
bool
ParseDicomDateTimeExcludingUtc(std::string_view v, DateParsed& d,
                               TimeParsed& t);

// Parse a UTC offset string formatted like "{+,-}HHMM" from V into
// OFFSET.  For example, "-0200" is parsed as -120 minutes.  V must be
// a valid value of the DICOM attribute TimezoneOffsetFromUtc.  If
// parsing fails, return false and leave OFFSET unchanged.
bool
ParseDicomUtcOffset(std::string_view v, std::chrono::minutes& offset);

enum class TimePointError
{
  // Parsing failures.
  kFailedDate,
  kFailedTime,
  kFailedUtcOffset,
  // The next one is like kFailedUtcOffset but with additional context
  // provided by Spect: the offset is the DICOM attribute
  // TimezoneOffsetFromUtc.
  kFailedTimezoneOffsetFromUtc,
  kFailedDateTimeExcludingUtcOffset,
  kFailedUtcOffsetInDateTime,
  // Missing a required component.
  kIncompleteDate,
  kIncompleteTime,
  kMissingTimeZone,
  // Validation failures.
  kInvalidDate,
  kNonexistentLocalTime,
};

enum class TimePointId
{
  kRadiopharmaceuticalStartDateTime,
  kAcquisitionDateAndTime,
  kSeriesDateAndTime,
};

struct TimePointErrorWithId
{
  TimePointId id;
  TimePointError error;
};

constexpr std::string_view
ToString(TimePointError e)
{
  switch (e)
    {
    case TimePointError::kFailedDate:
      return "failed to parse DICOM Date";
    case TimePointError::kFailedTime:
      return "failed to parse DICOM Time";
    case TimePointError::kFailedUtcOffset:
      return "failed to parse UTC offset (expected \"{+,-}HHMM\")";
    case TimePointError::kFailedTimezoneOffsetFromUtc:
      return "failed to parse DICOM attribute: TimezoneOffsetFromUtc";
    case TimePointError::kFailedDateTimeExcludingUtcOffset:
      return "failed to parse DICOM Date Time components (not including UTC "
             "offset)";
    case TimePointError::kFailedUtcOffsetInDateTime:
      return "failed to parse UTC offset suffix of DICOM Date Time";

    case TimePointError::kIncompleteDate:
      return "incomplete date; missing month or day";
    case TimePointError::kIncompleteTime:
      return "incomplete time; missing hour, minute or second";
    case TimePointError::kMissingTimeZone:
      return "missing required time zone";

    case TimePointError::kInvalidDate:
      return "invalid date";
    case TimePointError::kNonexistentLocalTime:
      return "nonexistent local time";
    }
  return "unknown error";
}

constexpr std::string_view
ToString(TimePointId id)
{
  switch (id)
    {
    case TimePointId::kRadiopharmaceuticalStartDateTime:
      return "radiopharmaceutical start date time";
    case TimePointId::kAcquisitionDateAndTime:
      return "acquisition date and time";
    case TimePointId::kSeriesDateAndTime:
      return "series date and time";
    }
  return "unknown";
}

inline std::string
ToString(const TimePointErrorWithId& e)
{
  return "failed to make time point for " + std::string(ToString(e.id)) + ": "
         + std::string(ToString(e.error));
}

std::expected<DateComplete, TimePointError>
MakeDateComplete(const DateParsed& date_parsed);

std::expected<TimeComplete, TimePointError>
MakeTimeComplete(const TimeParsed& time_parsed);

// Return a zoned time constructed from the local calendar date D and
// clock time T in time zone TZ.  If D is not a valid calendar date,
// return TimePointError::kInvalidDate.  If the local time is
// nonexistent (e.g. during a DST spring-forward), return
// TimePointError::kNonexistentLocalTime.  If the local time is
// ambiguous (e.g. during a DST fall-back), the earlier time point is
// chosen.
std::expected<tz::zoned_time<std::chrono::seconds>, TimePointError>
MakeZonedTime(const DateComplete& d, const TimeComplete& t,
              const tz::time_zone& tz);

// Return the Unix Time of the local calendar date D and clock time T
// at UTC OFFSET.  The offset is defined as (local time - UTC).
std::expected<std::chrono::sys_seconds, TimePointError>
MakeSysTimeFromOffset(const DateComplete& d, const TimeComplete& t,
                      std::chrono::minutes offset);

// Return the Unix Time of the local calendar date DATE and clock time
// TIME at UTC offset VOFFSET, or if VOFFSET is empty, in time zone
// TZ.  If VOFFSET is not empty, it is a valid value of the DICOM
// attribute TimezoneOffsetFromUTC.
std::expected<std::chrono::sys_seconds, TimePointError>
MakeSysTimeFromOffsetOrTimeZone(const DateComplete& date,
                                const TimeComplete& time,
                                std::string_view voffset,
                                const tz::time_zone* tz = nullptr);

// Return the Unix Time of the local calendar date VDATE and clock
// time VTIME at UTC offset VOFFSET, or if VOFFSET is empty, in time
// zone TZ.  VDATE is a DICOM DA value, VTIME is a DICOM TM value that
// includes at least the SS component, and if VOFFSET is not empty, it
// is a valid value of the DICOM attribute TimezoneOffsetFromUTC.
std::expected<std::chrono::sys_seconds, TimePointError>
MakeSysTimeFromDicomDateAndTime(std::string_view vdate, std::string_view vtime,
                                std::string_view voffset,
                                const tz::time_zone* tz = nullptr);

// Return the Unix Time of the DICOM DT value DATETIME.  If DATETIME
// does not contain a UTC offset, it is interpreted as the local
// calendar date and clock time at UTC offset VOFFSET.  If VOFFSET is
// empty, DATETIME is interpreted in the time zone TZ.  If VOFFSET is
// not empty, it is a valid value of the DICOM attribute
// TimezoneOffsetFromUTC.
std::expected<std::chrono::sys_seconds, TimePointError>
MakeSysTimeFromDicomDateTime(std::string_view datetime,
                             std::string_view voffset,
                             const tz::time_zone* tz = nullptr);

// Convenience wrapper around MakeSysTimeFromDicomDateAndTime for
// Spect::acquisition_date, Spect::acquisition_time, and
// Spect::timezone_offset_from_utc.  On
// TimePointError::kFailedUtcOffset, remaps to
// TimePointError::kFailedTimezoneOffsetFromUtc so the error refers to
// Spect::timezone_offset_from_utc.
inline std::expected<std::chrono::sys_seconds, TimePointErrorWithId>
MakeAcquisitionSysTime(const Spect& s, const tz::time_zone* tz = nullptr)
{
  return MakeSysTimeFromDicomDateAndTime(s.acquisition_date,
                                         s.acquisition_time,
                                         s.timezone_offset_from_utc, tz)
      .transform_error(
          [](TimePointError e)
            {
              return TimePointErrorWithId{
                .id = TimePointId::kAcquisitionDateAndTime,
                // Provide more context for the UTC offset parsing
                // failure.
                .error = (e == TimePointError::kFailedUtcOffset)
                             ? TimePointError::kFailedTimezoneOffsetFromUtc
                             : e
              };
            });
}

// Convenience wrapper around MakeSysTimeFromDicomDateTime for
// Spect::radiopharmaceutical_start_date_time and
// Spect::timezone_offset_from_utc.  On
// TimePointError::kFailedUtcOffset, remaps to
// TimePointError::kFailedTimezoneOffsetFromUtc so the error refers to
// Spect::timezone_offset_from_utc.
inline std::expected<std::chrono::sys_seconds, TimePointErrorWithId>
MakeRadiopharmaceuticalStartSysTime(const Spect& s,
                                    const tz::time_zone* tz = nullptr)
{
  return MakeSysTimeFromDicomDateTime(s.radiopharmaceutical_start_date_time,
                                      s.timezone_offset_from_utc, tz)
      .transform_error(
          [&](TimePointError e)
            {
              return TimePointErrorWithId{
                .id = TimePointId::kRadiopharmaceuticalStartDateTime,
                // Provide more context for UTC offset parsing
                // failure.
                .error = (e == TimePointError::kFailedUtcOffset)
                             ? TimePointError::kFailedTimezoneOffsetFromUtc
                             : e
              };
            });
}

// Values of the DICOM attribute DecayCorrection.
enum class DecayCorrection
{
  kNone,
  kStart,
  kAdmin
};

constexpr std::optional<DecayCorrection>
ParseDicomDecayCorrection(const std::string_view v)
{
  if (v == "NONE")
    return DecayCorrection::kNone;
  if (v == "START ")
    return DecayCorrection::kStart;
  if (v == "ADMIN ")
    return DecayCorrection::kAdmin;
  return {};
}

enum class DecayCorrectionError
{
  // Missing a required component.
  kMissingFrameReferenceTime,
  kMissingHalfLife,
  // Validation failures.
  kInvalidDecayCorrection,
  kHalfLifeLessThanOrEqualToZero,
  // Other.
  kUnreachable
};

constexpr std::string_view
ToString(DecayCorrectionError e)
{
  switch (e)
    {
    case DecayCorrectionError::kMissingFrameReferenceTime:
      return "missing DICOM attribute: FrameReferenceTime";
    case DecayCorrectionError::kMissingHalfLife:
      return "missing DICOM attribute: RadionuclideHalfLife";

    case DecayCorrectionError::kInvalidDecayCorrection:
      return "invalid value for DICOM attribute: DecayCorrection";
    case DecayCorrectionError::kHalfLifeLessThanOrEqualToZero:
      return "value <= 0 for DICOM attribute: RadionuclideHalfLife";

    case DecayCorrectionError::kUnreachable:
      return "you found a bug";
    }
  return "unknown error";
}

// Compute the decay factor for the SPECT series as if the DICOM
// attribute DecayCorrection were NONE.  See ComputeDecayFactor for
// the meaning of decay factor.  Note that Spect::decay_correction
// does not need to be "NONE"; its value is ignored.
std::expected<double, std::variant<TimePointErrorWithId, DecayCorrectionError>>
ComputeDecayFactorNone(const Spect& s, const tz::time_zone* tz = nullptr);

// Compute the decay factor for the SPECT series as if the DICOM
// attribute DecayCorrection were ADMIN.  See ComputeDecayFactor for
// the meaning of decay factor.  Note that Spect::decay_correction
// does not need to be "ADMIN "; its value is ignored.
std::expected<double, std::variant<TimePointErrorWithId, DecayCorrectionError>>
ComputeDecayFactorAdmin(const Spect& s, const tz::time_zone* tz = nullptr);

// Compute the decay factor for the SPECT series.  The decay factor is
// defined as the factor to decay-correct images of the SPECT series
// to acquisition start.
std::expected<double, std::variant<TimePointErrorWithId, DecayCorrectionError>>
ComputeDecayFactor(const Spect& s, const tz::time_zone* tz = nullptr);

inline std::string
ToString(const std::variant<TimePointErrorWithId, DecayCorrectionError>& e)
{
  return std::visit([](const auto& x) { return std::string(ToString(x)); }, e);
}

} // namespace spider

#endif // SPIDER_SPECT_H
