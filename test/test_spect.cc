// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2025, 2026 South Australia Medical Imaging

#include "spect.h"

#include <chrono>
#include <optional>
#include <sstream>
#include <string>
#include <vector>

#include <gdcmAttribute.h>
#include <gdcmDataSet.h>
#include <gdcmItem.h>
#include <gdcmReader.h>
#include <gdcmSequenceOfItems.h>
#include <gdcmSmartPointer.h>
#include <gdcmTag.h>
#include <gdcmVR.h>
#include <gtest/gtest.h>

#include "tz_compat.h"

namespace
{

void
SetRadionuclideHalfLife(double half_life, gdcm::DataSet& ds)
{
  // Create or replace the DICOM attribute
  // RadiopharmaceuticalInformationSequence in dataset DS with a
  // single-item sequence containing only the attribute
  // RadionuclideHalfLife set to HALF_LIFE.
  gdcm::Attribute<0x0018, 0x1075> a; // RadionuclideHalfLife
  a.SetValue(half_life);

  gdcm::Item item;
  gdcm::DataSet& nds = item.GetNestedDataSet();
  nds.Insert(a.GetAsDataElement());

  gdcm::SmartPointer<gdcm::SequenceOfItems> seq = new gdcm::SequenceOfItems;
  seq->AddItem(item);

  gdcm::Tag seq_tag(0x0054, 0x0016); // RadiopharmaceuticalInformationSequence
  gdcm::DataElement seq_de(seq_tag);
  seq_de.SetVR(gdcm::VR::SQ);
  seq_de.SetValue(*seq);
  // seq_de.SetVLToUndefined();
  ds.Insert(seq_de);
}

constexpr char kTestFilename[] = SPIDER_TEST_DATA_DIR
    "/PETAC/"
    "C11PHANTOM.PT.PET-MR_QA_MULTIPLE_PET_PHANTOM.30003.0077.2018.11.07.17.59."
    "46.799308.153692236.IMA";

} // namespace

TEST(GetPatientNameTest, Example)
{
  std::string patient_name = "DOE^JOHN";
  gdcm::Attribute<0x0010, 0x0010> at; // PatientName
  at.SetValue(patient_name);
  gdcm::DataSet ds;
  ds.Insert(at.GetAsDataElement());
  EXPECT_EQ(spider::GetPatientName(ds), patient_name);
}

TEST(GetPatientNameTest, RealDataset)
{
  gdcm::Reader r;
  r.SetFileName(kTestFilename);
  ASSERT_TRUE(r.Read());
  const gdcm::DataSet& ds = r.GetFile().GetDataSet();
  EXPECT_EQ(spider::GetPatientName(ds), "C11Phantom");
}

TEST(GetAcquisitionDateTest, Example)
{
  std::string date = "19930822";
  gdcm::Attribute<0x0008, 0x0022> at; // AcquisitionDate
  at.SetValue(date);
  gdcm::DataSet ds;
  ds.Insert(at.GetAsDataElement());
  EXPECT_EQ(spider::GetAcquisitionDate(ds), date);
}

TEST(GetAcquisitionDateTest, RealDataset)
{
  gdcm::Reader r;
  r.SetFileName(kTestFilename);
  ASSERT_TRUE(r.Read());
  const gdcm::DataSet& ds = r.GetFile().GetDataSet();
  EXPECT_EQ(spider::GetAcquisitionDate(ds), "20181105");
}

TEST(GetAcquisitionTimeTest, Example)
{
  std::string time = "070907.0705 ";
  gdcm::Attribute<0x0008, 0x0032> at; // AcquisitionTime
  at.SetValue(time);
  gdcm::DataSet ds;
  ds.Insert(at.GetAsDataElement());
  EXPECT_EQ(spider::GetAcquisitionTime(ds), time);
}

TEST(GetAcquisitionTimeTest, RealDataset)
{
  gdcm::Reader r;
  r.SetFileName(kTestFilename);
  ASSERT_TRUE(r.Read());
  const gdcm::DataSet& ds = r.GetFile().GetDataSet();
  EXPECT_EQ(spider::GetAcquisitionTime(ds), "122601.000000 ");
}

TEST(GetDecayCorrectionTest, Example)
{
  std::string decay_correction = "ADMIN ";
  gdcm::Attribute<0x0054, 0x1102> at; // DecayCorrection
  at.SetValue(decay_correction);
  gdcm::DataSet ds;
  ds.Insert(at.GetAsDataElement());
  EXPECT_EQ(spider::GetDecayCorrection(ds), decay_correction);
}

TEST(GetDecayCorrectionTest, RealDataset)
{
  gdcm::Reader r;
  r.SetFileName(kTestFilename);
  ASSERT_TRUE(r.Read());
  const gdcm::DataSet& ds = r.GetFile().GetDataSet();
  EXPECT_EQ(spider::GetDecayCorrection(ds), "START ");
}

TEST(GetRadionuclideHalfLifeTest, AttributePresent)
{
  double half_life = 574300.0;
  gdcm::DataSet ds;
  SetRadionuclideHalfLife(half_life, ds);
  EXPECT_EQ(spider::GetRadionuclideHalfLife(ds), half_life);
}

TEST(GetRadionuclideHalfLifeTest, AttributeAbsent)
{
  gdcm::DataSet ds;
  EXPECT_EQ(spider::GetRadionuclideHalfLife(ds), std::nullopt);
}

TEST(GetRadionuclideHalfLifeTest, RealDataset)
{
  gdcm::Reader r;
  r.SetFileName(kTestFilename);
  ASSERT_TRUE(r.Read());
  const gdcm::DataSet& ds = r.GetFile().GetDataSet();
  EXPECT_EQ(spider::GetRadionuclideHalfLife(ds), 1223.0);
}

TEST(ReadDicomSpectTest, Example)
{
  std::string date = "19930822";
  gdcm::Attribute<0x0008, 0x0022> at_date; // AcquisitionDate
  at_date.SetValue(date);

  std::string time = "070907.0705 ";
  gdcm::Attribute<0x0008, 0x0032> at_time; // AcquisitionTime
  at_time.SetValue(time);

  std::string correct_str = "ADMIN ";
  gdcm::Attribute<0x0054, 0x1102> at_correct; // DecayCorrection
  at_correct.SetValue(correct_str);

  double half_life = 574300.0;

  gdcm::DataSet ds;
  ds.Insert(at_date.GetAsDataElement());
  ds.Insert(at_time.GetAsDataElement());
  ds.Insert(at_correct.GetAsDataElement());
  SetRadionuclideHalfLife(half_life, ds);

  spider::Spect spect = spider::ReadDicomSpect(ds);

  EXPECT_EQ(spect.patient_name, "");
  EXPECT_EQ(spect.acquisition_date, date);
  EXPECT_EQ(spect.acquisition_time, time);
  EXPECT_EQ(spect.decay_correction, correct_str);
  EXPECT_EQ(spect.radionuclide_half_life, half_life);
}

TEST(ReadDicomSpectTest, RealDataset)
{
  gdcm::Reader r;
  r.SetFileName(kTestFilename);
  ASSERT_TRUE(r.Read());
  const gdcm::DataSet& ds = r.GetFile().GetDataSet();
  spider::Spect spect = spider::ReadDicomSpect(ds);

  EXPECT_EQ(spect.patient_name, "C11Phantom");
  EXPECT_EQ(spect.acquisition_date, "20181105");
  EXPECT_EQ(spect.acquisition_time, "122601.000000 ");
  EXPECT_EQ(spect.decay_correction, "START ");
  EXPECT_EQ(spect.radionuclide_half_life, 1223.0);
}

TEST(GetFirstLineTest, WithNewline)
{
  EXPECT_EQ(spider::GetFirstLine("Hello,\nWorld"), "Hello,");
}

TEST(GetFirstLineTest, WithoutNewline)
{
  std::string s = "Hello, World";
  EXPECT_EQ(spider::GetFirstLine(s), s);
}

TEST(WriteSpectsTest, Example)
{
  spider::Spect s1 = { .patient_name = "DOE^JOHN",
                       .acquisition_date = "20241013",
                       .acquisition_time = "123250.1123 ",
                       .decay_correction = "START ",
                       .radionuclide_half_life = 1234567.89 };
  spider::Spect s2 = { .patient_name = "DOE^JANE",
                       .acquisition_date = "20220311",
                       .acquisition_time = "02",
                       .decay_correction = "NONE",
                       .radionuclide_half_life = std::nullopt };
  std::ostringstream oss;
  spider::WriteSpects({ s1, s2 }, oss);

  // WriteSpects writes doubles with a precision of 6 significant
  // figures.
  std::string ans{ "This file was created by Spider.\n"
                   "If you edit it by hand, you could mess it up.\n"
                   "\nDOE^JOHN\n20241013\n123250.1123 \nSTART \n"
                   // A double of 1234567.89 is written as 1.23457e+06
                   // due to the precision of 6 significant figures.
                   "1.23457e+06\n"
                   "\nDOE^JANE\n20220311\n02\nNONE\n"
                   // If a std::optional<double> does not contain a
                   // value (is disengaged), WriteSpects writes an
                   // empty string for it.
                   "\n" };

  EXPECT_EQ(oss.str(), ans);
}

TEST(ReadDicomAttributesTest, RealDataset)
{
  gdcm::Reader r;
  r.SetFileName(kTestFilename);
  ASSERT_TRUE(r.Read());
  const gdcm::DataSet& ds = r.GetFile().GetDataSet();
  spider::Spect s1 = spider::ReadDicomSpect(ds);
  spider::Spect s2 = spider::ReadDicomSpect(ds);
  std::ostringstream oss;
  spider::WriteSpects({ s1, s2 }, oss);

  std::string ans{ "This file was created by Spider.\n"
                   "If you edit it by hand, you could mess it up.\n"
                   "\nC11Phantom\n20181105\n122601.000000 \nSTART \n1223\n"
                   "\nC11Phantom\n20181105\n122601.000000 \nSTART \n1223\n" };

  EXPECT_EQ(oss.str(), ans);
}

TEST(ReadSpectsTest, Example)
{
  std::istringstream input(
      "This file was created by Spider.\n"
      "If you edit it by hand, you could mess it up.\n"
      "\nDOE^JOHN\n20241013\n123250.1123 \nSTART \n23.11\n"
      "\nDOE^JANE\n20220311\n02\nNONE\n\n");
  std::vector<spider::Spect> spects = spider::ReadSpects(input);

  ASSERT_EQ(spects.size(), 2);
  EXPECT_EQ(spects[0].patient_name, "DOE^JOHN");
  EXPECT_EQ(spects[0].acquisition_date, "20241013");
  EXPECT_EQ(spects[0].acquisition_time, "123250.1123 ");
  EXPECT_EQ(spects[0].decay_correction, "START ");
  EXPECT_EQ(spects[0].radionuclide_half_life, 23.11);
  EXPECT_EQ(spects[1].patient_name, "DOE^JANE");
  EXPECT_EQ(spects[1].acquisition_date, "20220311");
  EXPECT_EQ(spects[1].acquisition_time, "02");
  EXPECT_EQ(spects[1].decay_correction, "NONE");
  // If there is an empty string in the position of a
  // std::optional<double>, the optional is disengaged.
  EXPECT_EQ(spects[1].radionuclide_half_life, std::nullopt);
}

TEST(ParseDicomDateTest, Example)
{
  spider::DateComplete date;
  spider::ParseDicomDate("19930822", date);
  EXPECT_EQ(date.year, 1993);
  EXPECT_EQ(date.month, 8);
  EXPECT_EQ(date.day, 22);
}

TEST(ParseDicomDateTest, NonFourDigitYear)
{
  spider::DateComplete date;
  spider::ParseDicomDate("00310822", date);
  EXPECT_EQ(date.year, 31);
  EXPECT_EQ(date.month, 8);
  EXPECT_EQ(date.day, 22);
}

TEST(ParseDicomTimeTest, HourOnly)
{
  spider::TimeParsed time;
  spider::ParseDicomTime("07", time);
  ASSERT_TRUE(time.hour.has_value());
  EXPECT_EQ(time.hour.value(), 7);
  EXPECT_FALSE(time.minute.has_value());
  EXPECT_FALSE(time.second.has_value());
}

TEST(ParseDicomTimeTest, HourAndMinuteOnly)
{
  spider::TimeParsed time;
  spider::ParseDicomTime("1009", time);
  ASSERT_TRUE(time.hour.has_value());
  EXPECT_EQ(time.hour.value(), 10);
  ASSERT_TRUE(time.minute.has_value());
  EXPECT_EQ(time.minute.value(), 9);
  EXPECT_FALSE(time.second.has_value());
}

TEST(ParseDicomTimeTest, HourAndMinuteAndSecondOnly)
{
  spider::TimeParsed time;
  spider::ParseDicomTime("151305", time);
  ASSERT_TRUE(time.hour.has_value());
  EXPECT_EQ(time.hour.value(), 15);
  ASSERT_TRUE(time.minute.has_value());
  EXPECT_EQ(time.minute.value(), 13);
  ASSERT_TRUE(time.second.has_value());
  EXPECT_EQ(time.second.value(), 5);
}

TEST(ParseDicomTimeTest, HourAndMinuteAndSecondAndFraction)
{
  // If DICOM TM value contains a fractional part of a second, that
  // component is ignored.
  spider::TimeParsed time;
  EXPECT_TRUE(spider::ParseDicomTime("030914.0103", time));
  ASSERT_TRUE(time.hour.has_value());
  EXPECT_EQ(time.hour.value(), 3);
  ASSERT_TRUE(time.minute.has_value());
  EXPECT_EQ(time.minute.value(), 9);
  ASSERT_TRUE(time.second.has_value());
  EXPECT_EQ(time.second.value(), 14);
}

TEST(ParseDicomDateTimeExcludingUtcTest, YearOnly)
{
  spider::DateParsed date;
  spider::TimeParsed time;
  EXPECT_TRUE(spider::ParseDicomDateTimeExcludingUtc("2023", date, time));
  EXPECT_EQ(date.year, 2023);
  EXPECT_FALSE(date.month.has_value());
  EXPECT_FALSE(date.day.has_value());
  EXPECT_FALSE(time.hour.has_value());
  EXPECT_FALSE(time.minute.has_value());
  EXPECT_FALSE(time.second.has_value());
}

TEST(ParseDicomDateTimeExcludingUtcTest, YearMonthOnly)
{
  spider::DateParsed date;
  spider::TimeParsed time;
  EXPECT_TRUE(spider::ParseDicomDateTimeExcludingUtc("202303", date, time));
  EXPECT_EQ(date.year, 2023);
  ASSERT_TRUE(date.month.has_value());
  EXPECT_EQ(date.month.value(), 3);
  EXPECT_FALSE(date.day.has_value());
  EXPECT_FALSE(time.hour.has_value());
  EXPECT_FALSE(time.minute.has_value());
  EXPECT_FALSE(time.second.has_value());
}

TEST(ParseDicomDateTimeExcludingUtcTest, YearMonthDayOnly)
{
  spider::DateParsed date;
  spider::TimeParsed time;
  EXPECT_TRUE(spider::ParseDicomDateTimeExcludingUtc("20230314", date, time));
  EXPECT_EQ(date.year, 2023);
  ASSERT_TRUE(date.month.has_value());
  EXPECT_EQ(date.month.value(), 3);
  ASSERT_TRUE(date.day.has_value());
  EXPECT_EQ(date.day.value(), 14);
  EXPECT_FALSE(time.hour.has_value());
  EXPECT_FALSE(time.minute.has_value());
  EXPECT_FALSE(time.second.has_value());
}

TEST(ParseDicomDateTimeExcludingUtcTest, YearMonthDayHourOnly)
{
  spider::DateParsed date;
  spider::TimeParsed time;
  EXPECT_TRUE(
      spider::ParseDicomDateTimeExcludingUtc("2023031409", date, time));
  EXPECT_EQ(date.year, 2023);
  ASSERT_TRUE(date.month.has_value());
  EXPECT_EQ(date.month.value(), 3);
  ASSERT_TRUE(date.day.has_value());
  EXPECT_EQ(date.day.value(), 14);
  ASSERT_TRUE(time.hour.has_value());
  EXPECT_EQ(time.hour.value(), 9);
  EXPECT_FALSE(time.minute.has_value());
  EXPECT_FALSE(time.second.has_value());
}

TEST(ParseDicomDateTimeExcludingUtcTest, YearMonthDayHourMinuteOnly)
{
  spider::DateParsed date;
  spider::TimeParsed time;
  EXPECT_TRUE(
      spider::ParseDicomDateTimeExcludingUtc("202303140945", date, time));
  EXPECT_EQ(date.year, 2023);
  ASSERT_TRUE(date.month.has_value());
  EXPECT_EQ(date.month.value(), 3);
  ASSERT_TRUE(date.day.has_value());
  EXPECT_EQ(date.day.value(), 14);
  ASSERT_TRUE(time.hour.has_value());
  EXPECT_EQ(time.hour.value(), 9);
  ASSERT_TRUE(time.minute.has_value());
  EXPECT_EQ(time.minute.value(), 45);
  EXPECT_FALSE(time.second.has_value());
}

TEST(ParseDicomDateTimeExcludingUtcTest, YearMonthDayHourMinuteSecondOnly)
{
  spider::DateParsed date;
  spider::TimeParsed time;
  EXPECT_TRUE(
      spider::ParseDicomDateTimeExcludingUtc("20230314094556", date, time));
  EXPECT_EQ(date.year, 2023);
  ASSERT_TRUE(date.month.has_value());
  EXPECT_EQ(date.month.value(), 3);
  ASSERT_TRUE(date.day.has_value());
  EXPECT_EQ(date.day.value(), 14);
  ASSERT_TRUE(time.hour.has_value());
  EXPECT_EQ(time.hour.value(), 9);
  ASSERT_TRUE(time.minute.has_value());
  EXPECT_EQ(time.minute.value(), 45);
  ASSERT_TRUE(time.second.has_value());
  EXPECT_EQ(time.second.value(), 56);
}

TEST(ParseDicomDateTimeExcludingUtcTest, YearMonthDayHourMinuteSecondFraction)
{
  // If DICOM DT value contains a fractional part of a second, that
  // component is ignored.
  spider::DateParsed date;
  spider::TimeParsed time;
  EXPECT_TRUE(
      spider::ParseDicomDateTimeExcludingUtc("20230314094556.30", date, time));
  EXPECT_EQ(date.year, 2023);
  ASSERT_TRUE(date.month.has_value());
  EXPECT_EQ(date.month.value(), 3);
  ASSERT_TRUE(date.day.has_value());
  EXPECT_EQ(date.day.value(), 14);
  ASSERT_TRUE(time.hour.has_value());
  EXPECT_EQ(time.hour.value(), 9);
  ASSERT_TRUE(time.minute.has_value());
  EXPECT_EQ(time.minute.value(), 45);
  ASSERT_TRUE(time.second.has_value());
  EXPECT_EQ(time.second.value(), 56);
}

TEST(ParseDicomUtcOffsetTest, PositiveOffset)
{
  std::chrono::minutes offset;
  EXPECT_TRUE(spider::ParseDicomUtcOffset("+0930", offset));
  EXPECT_EQ(offset, std::chrono::minutes{ 9 * 60 + 30 });
}

TEST(ParseDicomUtcOffsetTest, NegativeOffset)
{
  std::chrono::minutes offset;
  EXPECT_TRUE(spider::ParseDicomUtcOffset("-0500", offset));
  EXPECT_EQ(offset, std::chrono::minutes{ -5 * 60 });
}

TEST(MakeDateCompleteTest, Example)
{
  spider::DateParsed date_parsed{ .year = 2023, .month = 2, .day = 14 };
  auto date_complete = spider::MakeDateComplete(date_parsed);
  ASSERT_TRUE(date_complete.has_value());
  EXPECT_EQ(date_complete.value().year, 2023);
  EXPECT_EQ(date_complete.value().month, 2);
  EXPECT_EQ(date_complete.value().day, 14);
}

TEST(MakeTimeCompleteTest, Example)
{
  spider::TimeParsed time_parsed{ .hour = 14, .minute = 34, .second = 1 };
  auto time_complete = spider::MakeTimeComplete(time_parsed);
  ASSERT_TRUE(time_complete.has_value());
  EXPECT_EQ(time_complete.value().hour, 14);
  EXPECT_EQ(time_complete.value().minute, 34);
  EXPECT_EQ(time_complete.value().second, 1);
}

TEST(MakeZonedTimeTest, Example)
{
  spider::DateComplete date = { .year = 1997, .month = 7, .day = 15 };
  spider::TimeComplete time = { .hour = 16, .minute = 43, .second = 9 };
  const spider::tz::time_zone* tz = spider::tz::locate_zone("Asia/Tokyo");
  auto zt = spider::MakeZonedTime(date, time, *tz);
  ASSERT_TRUE(zt.has_value()) << spider::ToString(zt.error());

  spider::tz::local_seconds lt_valid
      = spider::tz::local_days{ spider::tz::year{ 1997 }
                                / spider::tz::month{ 7 }
                                / spider::tz::day{ 15 } }
        + std::chrono::hours{ 16 } + std::chrono::minutes{ 43 }
        + std::chrono::seconds{ 9 };
  auto zt_valid = spider::tz::zoned_time<std::chrono::seconds>{ tz, lt_valid };
  EXPECT_EQ(zt.value(), zt_valid);
}

TEST(MakeZonedTimeTest, NonexistentLocalTime)
{
  // MakeZonedTime does not return a nonexistent local time, e.g. one
  // during a DST spring-forward.
  spider::DateComplete date = { .year = 2024, .month = 10, .day = 6 };
  spider::TimeComplete time = { .hour = 2, .minute = 45, .second = 0 };
  const spider::tz::time_zone* tz
      = spider::tz::locate_zone("Australia/Adelaide");
  auto zt_test = spider::MakeZonedTime(date, time, *tz);
  EXPECT_FALSE(zt_test.has_value());
}

TEST(MakeZonedTimeTest, NonexistentLocalTimeDifferentTimeZone)
{
  spider::DateComplete date = { .year = 2025, .month = 3, .day = 30 };
  spider::TimeComplete time = { .hour = 2, .minute = 20, .second = 0 };
  const spider::tz::time_zone* tz = spider::tz::locate_zone("Europe/Berlin");
  auto zt_test = spider::MakeZonedTime(date, time, *tz);
  EXPECT_FALSE(zt_test.has_value());
}

TEST(MakeZonedTimeTest, AmbiguousLocalTime)
{
  // If the local time is ambiguous, e.g. due to a DST fall-back,
  // MakeZonedTime returns the earlier time point.
  spider::DateComplete date = { .year = 2024, .month = 4, .day = 7 };
  spider::TimeComplete time = { .hour = 2, .minute = 45, .second = 0 };
  const spider::tz::time_zone* tz
      = spider::tz::locate_zone("Australia/Adelaide");
  auto zt_beg = spider::MakeZonedTime(date, time, *tz);
  ASSERT_TRUE(zt_beg.has_value()) << spider::ToString(zt_beg.error());

  spider::tz::local_seconds lt_end
      = spider::tz::local_days{ spider::tz::year{ 2024 }
                                / spider::tz::month{ 4 }
                                / spider::tz::day{ 7 } }
        + std::chrono::hours{ 4 } + std::chrono::minutes{ 0 }
        + std::chrono::seconds{ 0 };
  auto zt_end = spider::tz::zoned_time<std::chrono::seconds>{ tz, lt_end };
  std::chrono::seconds secs
      = zt_end.get_sys_time() - zt_beg.value().get_sys_time();
  // If the later time point was chosen, the difference would be 1
  // hour and 15 mins.
  EXPECT_EQ(secs, std::chrono::seconds{ (2 * 60 * 60) + (15 * 60) });
}

TEST(MakeSysTimeFromOffsetTest, PositiveOffset)
{
  spider::DateComplete d{ 2025, 1, 2 };
  spider::TimeComplete t{ 0, 0, 0 };
  std::chrono::minutes offset{ 9 * 60 + 30 };
  auto s = spider::MakeSysTimeFromOffset(d, t, offset);
  ASSERT_TRUE(s.has_value()) << spider::ToString(s.error());

  std::chrono::sys_seconds expected
      = std::chrono::sys_days{ spider::tz::year{ 2025 } / 1 / 1 }
        + std::chrono::hours{ 14 } + std::chrono::minutes{ 30 };
  EXPECT_EQ(s.value(), expected);
}

TEST(MakeSysTimeFromOffsetTest, NegativeOffset)
{
  spider::DateComplete d{ 2025, 1, 2 };
  spider::TimeComplete t{ 0, 0, 0 };
  std::chrono::minutes offset{ -(9 * 60 + 30) };
  auto s = spider::MakeSysTimeFromOffset(d, t, offset);
  ASSERT_TRUE(s.has_value()) << spider::ToString(s.error());

  std::chrono::sys_seconds expected
      = std::chrono::sys_days{ spider::tz::year{ 2025 } / 1 / 2 }
        + std::chrono::hours{ 9 } + std::chrono::minutes{ 30 };
  EXPECT_EQ(s.value(), expected);
}

TEST(MakeSysTimeFromOffsetOrTimeZoneTest, WithOffset)
{
  // If a UTC offset is provided, it is used and the time zone
  // argument has no effect.
  spider::DateComplete d{ 2025, 1, 2 };
  spider::TimeComplete t{ 0, 0, 0 };
  const spider::tz::time_zone* tz
      = spider::tz::locate_zone("America/Vancouver"); // not used
  auto s = spider::MakeSysTimeFromOffsetOrTimeZone(d, t, "-0930", tz);
  ASSERT_TRUE(s.has_value()) << spider::ToString(s.error());

  std::chrono::sys_seconds expected
      = std::chrono::sys_days{ spider::tz::year{ 2025 } / 1 / 2 }
        + std::chrono::hours{ 9 } + std::chrono::minutes{ 30 };
  EXPECT_EQ(s.value(), expected);
}

TEST(MakeSysTimeFromOffsetOrTimeZoneTest, NoOffset)
{
  // If the UTC offset is empty, the time zone is used.
  spider::DateComplete d{ 2025, 1, 2 };
  spider::TimeComplete t{ 15, 23, 12 };
  const spider::tz::time_zone* tz
      = spider::tz::locate_zone("America/Vancouver");
  auto s = spider::MakeSysTimeFromOffsetOrTimeZone(d, t, "", tz);
  ASSERT_TRUE(s.has_value()) << spider::ToString(s.error());

  spider::tz::local_seconds lt
      = spider::tz::local_days{ spider::tz::year{ 2025 }
                                / spider::tz::month{ 1 }
                                / spider::tz::day{ 2 } }
        + std::chrono::hours{ 15 } + std::chrono::minutes{ 23 }
        + std::chrono::seconds{ 12 };
  auto zt = spider::tz::zoned_time<std::chrono::seconds>{ tz, lt };
  EXPECT_EQ(s.value(), zt.get_sys_time());
}

TEST(MakeSysTimeFromDicomDateAndTimeTest, WithOffset)
{
  // If a UTC offset is provided, it is used and the time zone
  // argument has no effect.
  const spider::tz::time_zone* tz
      = spider::tz::locate_zone("America/Vancouver"); // not used
  auto s = spider::MakeSysTimeFromDicomDateAndTime("20250102", "000000",
                                                   "+1030", tz);
  ASSERT_TRUE(s.has_value()) << spider::ToString(s.error());

  std::chrono::sys_seconds expected
      = std::chrono::sys_days{ spider::tz::year{ 2025 } / 1 / 1 }
        + std::chrono::hours{ 13 } + std::chrono::minutes{ 30 };
  EXPECT_EQ(s.value(), expected);
}

TEST(MakeSysTimeFromDicomDateAndTimeTest, NoOffset)
{
  // If the UTC offset is empty, the time zone is used.
  const spider::tz::time_zone* tz
      = spider::tz::locate_zone("America/Vancouver");
  auto s
      = spider::MakeSysTimeFromDicomDateAndTime("20230314", "123301", "", tz);
  ASSERT_TRUE(s.has_value()) << spider::ToString(s.error());

  spider::tz::local_seconds lt
      = spider::tz::local_days{ spider::tz::year{ 2023 }
                                / spider::tz::month{ 3 }
                                / spider::tz::day{ 14 } }
        + std::chrono::hours{ 12 } + std::chrono::minutes{ 33 }
        + std::chrono::seconds{ 1 };
  auto zt = spider::tz::zoned_time<std::chrono::seconds>{ tz, lt };
  EXPECT_EQ(s.value(), zt.get_sys_time());
}

TEST(MakeSysTimeFromDicomDateTimeTest, OffsetInDateTime)
{
  // If the DT value contains a UTC offset suffix, it is used and the
  // separate offset argument and time zone argument have no effect.
  const spider::tz::time_zone* tz
      = spider::tz::locate_zone("America/Vancouver");
  auto s = spider::MakeSysTimeFromDicomDateTime("20250102000000+0330",
                                                "-0500", // not used
                                                tz);     // not used
  ASSERT_TRUE(s.has_value()) << spider::ToString(s.error());

  std::chrono::sys_seconds expected
      = std::chrono::sys_days{ spider::tz::year{ 2025 } / 1 / 1 }
        + std::chrono::hours{ 20 } + std::chrono::minutes{ 30 };
  EXPECT_EQ(s.value(), expected);
}

TEST(MakeSysTimeFromDicomDateTimeTest, SeparateOffset)
{
  // If the DT value does not contain a UTC offset suffix, the
  // separate offset argument is used.  If the separate offset
  // argument is not empty, the time zone argument has no effect.
  const spider::tz::time_zone* tz
      = spider::tz::locate_zone("America/Vancouver");
  auto s = spider::MakeSysTimeFromDicomDateTime("20250102000000",
                                                "-0500", // used
                                                tz);     // not used
  ASSERT_TRUE(s.has_value()) << spider::ToString(s.error());

  std::chrono::sys_seconds expected
      = std::chrono::sys_days{ spider::tz::year{ 2025 } / 1 / 2 }
        + std::chrono::hours{ 5 };
  EXPECT_EQ(s.value(), expected);
}

TEST(MakeSysTimeFromDicomDateTimeTest, NoOffset)
{
  // If the DT value does not include a UTC offset suffix, and the
  // separate UTC offset argument is empty, the time zone is used.
  const spider::tz::time_zone* tz
      = spider::tz::locate_zone("America/Vancouver");
  auto s = spider::MakeSysTimeFromDicomDateTime("20230314123301", "", tz);
  ASSERT_TRUE(s.has_value()) << spider::ToString(s.error());

  spider::tz::local_seconds lt
      = spider::tz::local_days{ spider::tz::year{ 2023 }
                                / spider::tz::month{ 3 }
                                / spider::tz::day{ 14 } }
        + std::chrono::hours{ 12 } + std::chrono::minutes{ 33 }
        + std::chrono::seconds{ 1 };
  auto zt = spider::tz::zoned_time<std::chrono::seconds>{ tz, lt };
  EXPECT_EQ(s.value(), zt.get_sys_time());
}
