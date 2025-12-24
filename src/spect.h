// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2025 South Australia Medical Imaging

#ifndef SPIDER_SPECT_H
#define SPIDER_SPECT_H

#include <chrono>
#include <expected>
#include <istream>
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

std::ostream&
operator<<(std::ostream& os, const Spect& s);

// Fill a Spect from the DICOM attributes in dataset DS.
Spect
ReadDicomSpect(const gdcm::DataSet& ds);

std::string_view
GetFirstLine(const std::string_view v);

void
WriteSpects(const std::vector<Spect>& spects, std::ostream& os);

std::vector<Spect>
ReadSpects(std::istream& in);

struct Date
{
  int year;
  int month; // [1, 12]
  int day;   // [1, 31]
};

struct Time
{
  int hour = 0;   // [0, 23]
  int minute = 0; // [0, 59]
  int second = 0; // [0, 60], 60 for leap second
};

// Parse a DICOM Date (DA) value from V into DATE.  If parsing fails,
// return false and leave DATE unchanged.
bool
ParseDicomDate(const std::string_view v, Date& date);

// Parse a DICOM Time (TM) value from V into TIME.  If parsing fails,
// return false and leave TIME unchanged.  Characters after the SS
// component are ignored, so non-DICOM-conformant strings may be
// parsed successfully.
bool
ParseDicomTime(std::string_view v, Time& time);

enum class DatetimeParseError
{
  kTooShort,
  kFailedDate,
  kFailedTime,
  kInvalidDate,
  kNonexistentLocalTime,
};

// Return a zoned time constructed from the local calendar date D and
// clock time T in time zone TZ.  If D is not a valid calendar date,
// return DatetimeParseError::kInvalidDate.  If the local time is
// nonexistent (e.g. during a DST spring-forward), return
// DatetimeParseError::kNonexistentLocalTime.  If the local time is
// ambiguous (e.g. during a DST fall-back), the earlier time point is
// chosen.
std::expected<std::chrono::zoned_time<std::chrono::seconds>,
              DatetimeParseError>
MakeZonedTime(const Date& d, const Time& t, const std::chrono::time_zone* tz);

// Return a zoned time constructed from the timestamp V in the time
// zone TZ.  The timestamp should be of the form "YYYYMMDDHH[MMSS]",
// where the components in brackets are optional.  Any additional
// trailing characters are ignored.  In addition to the return values
// of MakeZonedTime, it also returns DateTimeParseError::kTooShort if
// the timestamp is less than 10 characters,
// DateTimeParseError::kFailedDate if it fails to parse the date
// component of the timestamp, and DateTimeParseError::kFailedTime if
// it fails to parse the time component of the timestamp.
std::expected<std::chrono::zoned_time<std::chrono::seconds>,
              DatetimeParseError>
ParseTimestamp(std::string_view v, const std::chrono::time_zone* tz);

constexpr std::string_view
ToString(DatetimeParseError e)
{
  switch (e)
    {
    case DatetimeParseError::kTooShort:
      return "timestamp is less than 10 characters";
    case DatetimeParseError::kFailedDate:
      return "failed to parse date component";
    case DatetimeParseError::kFailedTime:
      return "failed to parse time component";
    case DatetimeParseError::kInvalidDate:
      return "invalid date";
    case DatetimeParseError::kNonexistentLocalTime:
      return "nonexistent local time";
    }
  return "unknown parse error";
}

std::chrono::seconds
DiffTime(const std::chrono::zoned_time<std::chrono::seconds>& time_end,
         const std::chrono::zoned_time<std::chrono::seconds>& time_beg);

} // namespace spider

#endif // SPIDER_SPECT_H
