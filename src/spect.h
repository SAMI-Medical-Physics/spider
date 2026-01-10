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
#include <vector>

#include <gdcmDataSet.h>

#include "tz_compat.h"

namespace spider
{

struct Spect
{
  std::string patient_name;
  std::string acquisition_date;
  std::string acquisition_time;
  std::string decay_correction;
  // Use std::optional for doubles to discern when the value is
  // absent.
  std::optional<double> radionuclide_half_life; // seconds
};

std::ostream&
operator<<(std::ostream& os, const Spect& s);

std::string
GetPatientName(const gdcm::DataSet& ds);

std::string
GetAcquisitionDate(const gdcm::DataSet& ds);

std::string
GetAcquisitionTime(const gdcm::DataSet& ds);

std::string
GetDecayCorrection(const gdcm::DataSet& ds);

std::optional<double>
GetRadionuclideHalfLife(const gdcm::DataSet& ds);

// Fill a Spect from the DICOM attributes in dataset DS.
Spect
ReadDicomSpect(const gdcm::DataSet& ds);

std::string_view
GetFirstLine(const std::string_view v);

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

// Parse a DICOM TimezoneOffsetFromUTC value from V into OFFSET.  For
// example, "-0200" is parsed as -120 minutes.  If parsing fails,
// return false and leave OFFSET unchanged.
bool
ParseDicomUtcOffset(std::string_view v, std::chrono::minutes& offset);

enum class MakeTimePointError
{
  // Missing required components.
  kMissingTimeZone,
  kIncompleteDate,
  kIncompleteTime,
  // Parsing failures.
  kFailedDate,
  kFailedTime,
  kFailedUtc,
  kFailedDateTimeExcludingUtc,
  kFailedUtcInDateTime,
  // Validation failures.
  kInvalidDate,
  kNonexistentLocalTime
};

constexpr std::string_view
ToString(MakeTimePointError e)
{
  switch (e)
    {
    case MakeTimePointError::kMissingTimeZone:
      return "missing required time zone";
    case MakeTimePointError::kIncompleteDate:
      return "incomplete date; missing month or day";
    case MakeTimePointError::kIncompleteTime:
      return "incomplete time; missing hour, minute or second";

    case MakeTimePointError::kFailedDate:
      return "failed to parse DICOM Date";
    case MakeTimePointError::kFailedTime:
      return "failed to parse DICOM Time";
    case MakeTimePointError::kFailedDateTimeExcludingUtc:
      return "failed to parse DICOM Date Time components (not including UTC "
             "offset)";
    case MakeTimePointError::kFailedUtc:
      return "failed to parse DICOM UTC offset";
    case MakeTimePointError::kFailedUtcInDateTime:
      return "failed to parse UTC offset suffix of DICOM Date Time";

    case MakeTimePointError::kInvalidDate:
      return "invalid date";
    case MakeTimePointError::kNonexistentLocalTime:
      return "nonexistent local time";
    }
  return "unknown error";
}

std::expected<DateComplete, MakeTimePointError>
MakeDateComplete(const DateParsed& date_parsed);

std::expected<TimeComplete, MakeTimePointError>
MakeTimeComplete(const TimeParsed& time_parsed);

// Return a zoned time constructed from the local calendar date D and
// clock time T in time zone TZ.  If D is not a valid calendar date,
// return MakeTimePointError::kInvalidDate.  If the local time is
// nonexistent (e.g. during a DST spring-forward), return
// MakeTimePointError::kNonexistentLocalTime.  If the local time is
// ambiguous (e.g. during a DST fall-back), the earlier time point is
// chosen.
std::expected<tz::zoned_time<std::chrono::seconds>, MakeTimePointError>
MakeZonedTime(const DateComplete& d, const TimeComplete& t,
              const tz::time_zone& tz);

// Return the Unix Time of the local calendar date D and clock time T
// at UTC OFFSET.  The offset is defined as (local time - UTC).
std::expected<std::chrono::sys_seconds, MakeTimePointError>
MakeSysTimeFromOffset(const DateComplete& d, const TimeComplete& t,
                      std::chrono::minutes offset);

// Return the Unix Time of the local calendar date DATE and clock time
// TIME at UTC offset VOFFSET, or if VOFFSET is empty, in time zone
// TZ.  If VOFFSET is not empty, it is formatted like the DICOM
// attribute TimezoneOffsetFromUTC.
std::expected<std::chrono::sys_seconds, MakeTimePointError>
MakeSysTimeFromOffsetOrTimeZone(const DateComplete& date,
                                const TimeComplete& time,
                                std::string_view voffset,
                                const tz::time_zone* tz);

// Return the Unix Time of the local calendar date VDATE and clock
// time VTIME at UTC offset VOFFSET, or if VOFFSET is empty, in time
// zone TZ.  VDATE is a DICOM DA value, VTIME is a DICOM TM value that
// includes at least the SS component, and if VOFFSET is not empty, it
// is formatted like the DICOM attribute TimezoneOffsetFromUTC.
std::expected<std::chrono::sys_seconds, MakeTimePointError>
MakeSysTimeFromDicomDateAndTime(std::string_view vdate, std::string_view vtime,
                                std::string_view voffset,
                                const tz::time_zone* tz = nullptr);

// Return the Unix Time of the DICOM DT value DATETIME.  If DATETIME
// does not contain a UTC offset, it is interpreted as the local
// calendar date and clock time at UTC offset VOFFSET.  If VOFFSET is
// empty, it is interpreted in the time zone TZ.  VDATE is a DICOM DA
// value and VTIME is a DICOM TM value that includes at least the SS
// component.  If VOFFSET is not empty, it is formatted like the DICOM
// attribute TimezoneOffsetFromUTC.
std::expected<std::chrono::sys_seconds, MakeTimePointError>
MakeSysTimeFromDicomDateTime(std::string_view datetime,
                             std::string_view voffset,
                             const tz::time_zone* tz = nullptr);

} // namespace spider

#endif // SPIDER_SPECT_H
