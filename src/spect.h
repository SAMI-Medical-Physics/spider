// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2025, 2026 South Australia Medical Imaging

#ifndef SPIDER_SPECT_H
#define SPIDER_SPECT_H

#include <cassert>
#include <chrono>
#include <expected>
#include <istream>
#include <optional>
#include <ostream>
#include <string>
#include <string_view>
#include <vector>

#include <gdcmDataSet.h>

#include "tz_compat.h"

namespace spider
{

// Selected DICOM attributes from a SPECT DICOM series.
struct Spect
{
  std::optional<std::string> patient_name;
  std::optional<std::string> radiopharmaceutical_start_date_time;
  std::optional<std::string> acquisition_date;
  std::optional<std::string> acquisition_time;
  std::optional<std::string> series_date;
  std::optional<std::string> series_time;
  std::optional<double> frame_reference_time; // milliseconds
  std::optional<std::string> timezone_offset_from_utc;
  std::optional<std::string> decay_correction;
  std::optional<double> radionuclide_half_life; // seconds
};

std::ostream&
operator<<(std::ostream& os, const Spect& s);

std::optional<std::string>
GetPatientName(const gdcm::DataSet& ds);

std::optional<std::string>
GetRadiopharmaceuticalStartDateTime(const gdcm::DataSet& ds);

std::optional<std::string>
GetAcquisitionDate(const gdcm::DataSet& ds);

std::optional<std::string>
GetAcquisitionTime(const gdcm::DataSet& ds);

std::optional<std::string>
GetSeriesDate(const gdcm::DataSet& ds);

std::optional<std::string>
GetSeriesTime(const gdcm::DataSet& ds);

std::optional<double>
GetFrameReferenceTime(const gdcm::DataSet& ds);

std::optional<std::string>
GetTimezoneOffsetFromUtc(const gdcm::DataSet& ds);

std::optional<std::string>
GetDecayCorrection(const gdcm::DataSet& ds);

std::optional<double>
GetRadionuclideHalfLife(const gdcm::DataSet& ds);

// Fill a Spect from the DICOM attributes in dataset DS.
Spect
ReadDicomSpect(const gdcm::DataSet& ds);

struct DateComplete
{
  // A date with all components present.  DICOM Date (DA) values map
  // directly to this type.
  int year;
  int month; // [1, 12]
  int day;   // [1, 31]
};

struct DicomTime
{
  // DICOM Time (TM) value; only the hour component is required.
  int hour;                  // [0, 23]
  std::optional<int> minute; // [0, 59]
  std::optional<int> second; // [0, 60], 60 for leap second
};

struct DicomDateTime
{
  // DICOM Date Time (DT) value; only the year component is required.
  int year;
  std::optional<int> month;  // [1, 12]
  std::optional<int> day;    // [1, 31]
  std::optional<int> hour;   // [0, 23]
  std::optional<int> minute; // [0, 59]
  std::optional<int> second; // [0, 60], 60 for leap second
  std::optional<std::chrono::minutes> utc_offset; // [-12h, 14h]
};

struct TimeComplete
{
  int hour;   // [0, 23]
  int minute; // [0, 59]
  int second; // [0, 60], 60 for leap second
};

// Extract the date from the provided string containing a DICOM Date
// (DA) value.  Returns std::nullopt if the input is invalid.
std::optional<DateComplete>
ParseDicomDate(std::string_view v);

// Extract the DICOM Time (TM) value from the provided string,
// ignoring any fractional second component.  Returns std::nullopt if
// the input is invalid.  Characters after the SS component are
// ignored, so non-DICOM-conformant strings may be parsed
// successfully.
std::optional<DicomTime>
ParseDicomTime(std::string_view v);

// Extract the DICOM Date Time (DT) value from the provided string,
// ignoring any fractional second component.  Returns std::nullopt if
// the input is invalid.  Characters after the SS component and before
// the UTC offset suffix are ignored, so non-DICOM-conformant strings
// may be parsed successfully.
std::optional<DicomDateTime>
ParseDicomDateTime(std::string_view v);

// Extract the UTC offset from the provided string formatted like
// "{+,-}HHMM".  Returns std::nullopt if the input is invalid.  For
// example, "-0200" is parsed as -120 minutes.  The string must be a
// valid value of the DICOM attribute TimezoneOffsetFromUtc.
std::optional<std::chrono::minutes>
ParseDicomUtcOffset(std::string_view v);

enum class TimePointError
{
  // Parsing failures.
  kInvalidDicomDate,
  kInvalidDicomTime,
  kInvalidUtcOffset,
  kInvalidDicomDateTime,
  // Missing a required component.
  kIncompleteDate,
  kIncompleteTimeInDicomTime,
  kIncompleteTimeInDicomDateTime,
  kMissingTimeZone,
  // Validation failures.
  kInvalidDate,
  kNonexistentLocalTime,
};

constexpr std::string_view
ToString(TimePointError e)
{
  switch (e)
    {
    case TimePointError::kInvalidDicomDate:
      return "invalid DICOM Date";
    case TimePointError::kInvalidDicomTime:
      return "invalid DICOM Time";
    case TimePointError::kInvalidUtcOffset:
      return "invalid UTC offset";
    case TimePointError::kInvalidDicomDateTime:
      return "invalid DICOM Date Time";

    case TimePointError::kIncompleteDate:
      return "incomplete date in DICOM Date Time; missing month or day";
    case TimePointError::kIncompleteTimeInDicomTime:
      return "incomplete time in DICOM Time; missing minute or second";
    case TimePointError::kIncompleteTimeInDicomDateTime:
      return "incomplete time in DICOM Date Time; missing hour, minute or "
             "second";
    case TimePointError::kMissingTimeZone:
      return "missing required time zone";

    case TimePointError::kInvalidDate:
      return "invalid date";
    case TimePointError::kNonexistentLocalTime:
      return "nonexistent local time";
    }
  return "unknown error";
}

std::expected<DateComplete, TimePointError>
MakeDateComplete(const DicomDateTime& date_time);

std::expected<TimeComplete, TimePointError>
MakeTimeComplete(const DicomTime& time);

std::expected<TimeComplete, TimePointError>
MakeTimeComplete(const DicomDateTime& date_time);

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
// TIME at UTC offset VOFFSET, or if VOFFSET is absent, in time zone
// TZ.  If VOFFSET is present, it is a valid value of the DICOM
// attribute TimezoneOffsetFromUTC.
std::expected<std::chrono::sys_seconds, TimePointError>
MakeSysTimeFromOffsetOrTimeZone(const DateComplete& date,
                                const TimeComplete& time,
                                std::optional<std::string_view> voffset,
                                const tz::time_zone* tz = nullptr);

// Return the Unix Time of the local calendar date VDATE and clock
// time VTIME at UTC offset VOFFSET, or if VOFFSET is absent, in time
// zone TZ.  VDATE is a DICOM DA value, VTIME is a DICOM TM value that
// includes at least the SS component, and if VOFFSET is present, it
// is a valid value of the DICOM attribute TimezoneOffsetFromUTC.
std::expected<std::chrono::sys_seconds, TimePointError>
MakeSysTimeFromDicomDateAndTime(std::string_view vdate, std::string_view vtime,
                                std::optional<std::string_view> voffset,
                                const tz::time_zone* tz = nullptr);

// Return the Unix Time of the DICOM DT value DATETIME.  If DATETIME
// does not contain a UTC offset, it is interpreted as the local
// calendar date and clock time at UTC offset VOFFSET.  If VOFFSET is
// absent, DATETIME is interpreted in the time zone TZ.  If VOFFSET is
// present, it is a valid value of the DICOM attribute
// TimezoneOffsetFromUTC.
std::expected<std::chrono::sys_seconds, TimePointError>
MakeSysTimeFromDicomDateTime(std::string_view datetime,
                             std::optional<std::string_view> voffset,
                             const tz::time_zone* tz = nullptr);

enum class SpectErrorCode
{
  // Missing a required DICOM attribute.
  kMissingRadiopharmaceuticalStartDateTime,
  kMissingAcquisitionDate,
  kMissingAcquisitionTime,
  kMissingSeriesDate,
  kMissingSeriesTime,
  kMissingFrameReferenceTime,
  kMissingHalfLife,
  kMissingDecayCorrection,
  // Validation failures.
  kInvalidDecayCorrection,
  kHalfLifeLessThanOrEqualToZero,
  // This is TimePointError::kInvalidUtcOffset with additional context
  // provided by Spect: the offset is the DICOM attribute
  // TimezoneOffsetFromUtc; not the offset in a DICOM DT value.
  kInvalidTimezoneOffsetFromUtc,
  // Failure to make a time point due to a TimePointError.
  kRadiopharmaceuticalStartDateTimeError,
  kAcquisitionDateAndTimeError,
  kSeriesDateAndTimeError,
  // Other.
  kUnreachable,
};

constexpr std::string_view
ToString(SpectErrorCode e)
{
  switch (e)
    {
    case SpectErrorCode::kMissingRadiopharmaceuticalStartDateTime:
      return "missing DICOM attribute: RadiopharmaceuticalStartDateTime";
    case SpectErrorCode::kMissingAcquisitionDate:
      return "missing DICOM attribute: AcquisitionDate";
    case SpectErrorCode::kMissingAcquisitionTime:
      return "missing DICOM attribute: AcquisitionTime";
    case SpectErrorCode::kMissingSeriesDate:
      return "missing DICOM attribute: SeriesDate";
    case SpectErrorCode::kMissingSeriesTime:
      return "missing DICOM attribute: SeriesTime";
    case SpectErrorCode::kMissingFrameReferenceTime:
      return "missing DICOM attribute: FrameReferenceTime";
    case SpectErrorCode::kMissingHalfLife:
      return "missing DICOM attribute: RadionuclideHalfLife";
    case SpectErrorCode::kMissingDecayCorrection:
      return "missing DICOM attribute: DecayCorrection";

    case SpectErrorCode::kInvalidDecayCorrection:
      return "invalid value for DICOM attribute: DecayCorrection";
    case SpectErrorCode::kHalfLifeLessThanOrEqualToZero:
      return "value <= 0 for DICOM attribute: RadionuclideHalfLife";
    case SpectErrorCode::kInvalidTimezoneOffsetFromUtc:
      return "invalid value for DICOM attribute: TimezoneOffsetFromUtc";

    case SpectErrorCode::kRadiopharmaceuticalStartDateTimeError:
      return "failed to make time point for radiopharmaceutical start date "
             "time";
    case SpectErrorCode::kAcquisitionDateAndTimeError:
      return "failed to make time point for acquisition date and time";
    case SpectErrorCode::kSeriesDateAndTimeError:
      return "failed to make time point for series date and time";

    case SpectErrorCode::kUnreachable:
      return "you found a bug";
    }
  return "unknown error";
}

struct SpectError
{
  SpectErrorCode code;
  // A value should be provided for this field when CODE is
  // SpectErrorCode::kRadiopharmaceuticalStartDateTimeError,
  // kAcquisitionDateAndTimeError or kSeriesDateAndTimeError.
  std::optional<TimePointError> time_point_error;
};

inline std::string
ToString(const SpectError& e)
{
  switch (e.code)
    {
    case SpectErrorCode::kRadiopharmaceuticalStartDateTimeError:
    case SpectErrorCode::kAcquisitionDateAndTimeError:
    case SpectErrorCode::kSeriesDateAndTimeError:
      {
        assert(e.time_point_error.has_value());
        if (e.time_point_error.has_value())
          return std::string(ToString(e.code)) + ": "
                 + std::string(ToString(e.time_point_error.value()));
        return std::string(ToString(e.code));
      }
    default:
      return std::string(ToString(e.code));
    }
}

// Convenience wrapper around MakeSysTimeFromDicomDateAndTime for
// Spect::acquisition_date, Spect::acquisition_time, and, if present,
// Spect::timezone_offset_from_utc.
inline std::expected<std::chrono::sys_seconds, SpectError>
MakeAcquisitionSysTime(const Spect& s, const tz::time_zone* tz = nullptr)
{
  if (!s.acquisition_date.has_value())
    return std::unexpected(
        SpectError{ .code = SpectErrorCode::kMissingAcquisitionDate,
                    .time_point_error = std::nullopt });
  if (!s.acquisition_time.has_value())
    return std::unexpected(
        SpectError{ .code = SpectErrorCode::kMissingAcquisitionTime,
                    .time_point_error = std::nullopt });
  return MakeSysTimeFromDicomDateAndTime(
             s.acquisition_date.value(), s.acquisition_time.value(),
             (s.timezone_offset_from_utc.has_value())
                 ? std::optional<std::string_view>{ s.timezone_offset_from_utc
                                                        .value() }
                 : std::nullopt,
             tz)
      .transform_error(
          [](TimePointError e)
            {
              if (e == TimePointError::kInvalidUtcOffset)
                // Provide more context for the UTC offset parsing
                // failure.
                return SpectError{
                  .code = SpectErrorCode::kInvalidTimezoneOffsetFromUtc,
                  .time_point_error = std::nullopt
                };
              return SpectError{
                .code = SpectErrorCode::kAcquisitionDateAndTimeError,
                .time_point_error = e
              };
            });
}

// Convenience wrapper around MakeSysTimeFromDicomDateTime for
// Spect::radiopharmaceutical_start_date_time and, if present,
// Spect::timezone_offset_from_utc.
inline std::expected<std::chrono::sys_seconds, SpectError>
MakeRadiopharmaceuticalStartSysTime(const Spect& s,
                                    const tz::time_zone* tz = nullptr)
{
  if (!s.radiopharmaceutical_start_date_time.has_value())
    return std::unexpected(SpectError{
        .code = SpectErrorCode::kMissingRadiopharmaceuticalStartDateTime,
        .time_point_error = std::nullopt });
  return MakeSysTimeFromDicomDateTime(
             s.radiopharmaceutical_start_date_time.value(),
             (s.timezone_offset_from_utc.has_value())
                 ? std::optional<std::string_view>{ s.timezone_offset_from_utc
                                                        .value() }
                 : std::nullopt,
             tz)
      .transform_error(
          [&](TimePointError e)
            {
              if (e == TimePointError::kInvalidUtcOffset)
                // Provide more context for UTC offset parsing
                // failure.
                return SpectError{
                  .code = SpectErrorCode::kInvalidTimezoneOffsetFromUtc,
                  .time_point_error = std::nullopt
                };
              return SpectError{
                .code = SpectErrorCode::kRadiopharmaceuticalStartDateTimeError,
                .time_point_error = e
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

// Compute the decay factor for the SPECT series as if the DICOM
// attribute DecayCorrection were NONE.  See ComputeDecayFactor for
// the meaning of decay factor.  Note that Spect::decay_correction
// does not need to be "NONE"; its value is ignored.
std::expected<double, SpectError>
ComputeDecayFactorNone(const Spect& s, const tz::time_zone* tz = nullptr);

// Compute the decay factor for the SPECT series as if the DICOM
// attribute DecayCorrection were ADMIN.  See ComputeDecayFactor for
// the meaning of decay factor.  Note that Spect::decay_correction
// does not need to be "ADMIN "; its value is ignored.
std::expected<double, SpectError>
ComputeDecayFactorAdmin(const Spect& s, const tz::time_zone* tz = nullptr);

// Compute the decay factor for the SPECT series.  The decay factor is
// defined as the factor to decay-correct images of the SPECT series
// to acquisition start.
std::expected<double, SpectError>
ComputeDecayFactor(const Spect& s, const tz::time_zone* tz = nullptr);

// Returns true if forming time points from DICOM DA and TM values in
// Spect, or from a DICOM DT value in Spect that lacks a UTC offset
// suffix, uses the caller-supplied time zone.
bool
UsesTimeZone(const Spect& s);

} // namespace spider

#endif // SPIDER_SPECT_H
