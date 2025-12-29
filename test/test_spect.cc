// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2025 South Australia Medical Imaging

#include "spect.h"

#include <chrono>
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
SetHalfLife(double half_life, gdcm::DataSet& ds)
{
  // Create or replace the RadiopharmaceuticalInformationSequence in
  // dataset DS with a single-item sequence containing only the DICOM
  // attribute RadionuclideHalfLife set to HALF_LIFE.
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

TEST(GetAcquisitionTimestampTest, Example)
{
  std::string date = "19930822";
  gdcm::Attribute<0x0008, 0x0022> ad; // AcquisitionDate
  ad.SetValue(date);

  std::string time = "070907.0705 ";
  gdcm::Attribute<0x0008, 0x0032> at; // AcquisitionTime
  at.SetValue(time);

  gdcm::DataSet ds;
  ds.Insert(ad.GetAsDataElement());
  ds.Insert(at.GetAsDataElement());

  EXPECT_EQ(spider::GetAcquisitionTimestamp(ds), date + time);
}

TEST(GetAcquisitionTimestampTest, RealDataset)
{
  gdcm::Reader r;
  r.SetFileName(kTestFilename);
  ASSERT_EQ(r.Read(), true);
  const gdcm::DataSet& ds = r.GetFile().GetDataSet();
  EXPECT_EQ(spider::GetAcquisitionTimestamp(ds), "20181105122601.000000 ");
}

TEST(GetHalfLifeTest, Example)
{
  double half_life = 574300.0;
  gdcm::DataSet ds;
  SetHalfLife(half_life, ds);
  EXPECT_EQ(spider::GetHalfLife(ds), half_life);
}

TEST(GetHalfLifeTest, RealDataset)
{
  gdcm::Reader r;
  r.SetFileName(kTestFilename);
  ASSERT_EQ(r.Read(), true);
  const gdcm::DataSet& ds = r.GetFile().GetDataSet();
  EXPECT_EQ(spider::GetHalfLife(ds), 1223.0);
}

TEST(ReadDicomSpectTest, Example)
{
  std::string date = "19930822";
  gdcm::Attribute<0x0008, 0x0022> at_date; // AcquisitionDate
  at_date.SetValue(date);

  std::string time = "070907.0705 ";
  gdcm::Attribute<0x0008, 0x0032> at_time; // AcquisitionTime
  at_time.SetValue(time);

  gdcm::DataSet ds;
  ds.Insert(at_date.GetAsDataElement());
  ds.Insert(at_time.GetAsDataElement());

  double half_life = 574300.0;
  SetHalfLife(half_life, ds);

  std::string correct_str = "ADMIN ";
  gdcm::Attribute<0x0054, 0x1102> at_correct; // DecayCorrection
  at_correct.SetValue(correct_str);
  ds.Insert(at_correct.GetAsDataElement());

  spider::Spect spect = spider::ReadDicomSpect(ds);

  EXPECT_EQ(spect.acquisition_timestamp, date + time);
  EXPECT_EQ(spect.half_life, half_life);
  EXPECT_EQ(spect.decay_correction_method, correct_str);
  EXPECT_EQ(spect.patient_name, "");
}

TEST(ReadDicomSpectTest, RealDataset)
{
  gdcm::Reader r;
  r.SetFileName(kTestFilename);
  ASSERT_EQ(r.Read(), true);
  const gdcm::DataSet& ds = r.GetFile().GetDataSet();
  spider::Spect spect = spider::ReadDicomSpect(ds);

  EXPECT_EQ(spect.acquisition_timestamp, "20181105122601.000000 ");
  EXPECT_EQ(spect.half_life, 1223.0);
  EXPECT_EQ(spect.decay_correction_method, "START ");
  EXPECT_EQ(spect.patient_name, "C11Phantom");
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
  spider::Spect s1;
  s1.patient_name = "DOE^JOHN";
  s1.acquisition_timestamp = "20241013123250.1123 ";
  s1.decay_correction_method = "START ";
  s1.half_life = 23.11;

  spider::Spect s2;
  s2.patient_name = "DOE^JANE";
  s2.acquisition_timestamp = "2022031102";
  s2.decay_correction_method = "NONE";
  s2.half_life = 23121;

  std::ostringstream oss;
  spider::WriteSpects({ s1, s2 }, oss);

  std::string ans{ "This file was created by Spider.\n"
                   "If you edit it by hand, you could mess it up.\n"
                   "\nDOE^JOHN\n20241013123250.1123 \nSTART \n23.11\n"
                   "\nDOE^JANE\n2022031102\nNONE\n23121\n" };

  EXPECT_EQ(oss.str(), ans);
}

TEST(ReadDicomAttributesTest, RealDataset)
{
  gdcm::Reader r;
  r.SetFileName(kTestFilename);
  ASSERT_EQ(r.Read(), true);
  const gdcm::DataSet& ds = r.GetFile().GetDataSet();
  spider::Spect s1 = spider::ReadDicomSpect(ds);
  spider::Spect s2 = spider::ReadDicomSpect(ds);
  std::ostringstream oss;
  spider::WriteSpects({ s1, s2 }, oss);

  std::string ans{ "This file was created by Spider.\n"
                   "If you edit it by hand, you could mess it up.\n"
                   "\nC11Phantom\n20181105122601.000000 \nSTART \n1223\n"
                   "\nC11Phantom\n20181105122601.000000 \nSTART \n1223\n" };

  EXPECT_EQ(oss.str(), ans);
}

TEST(ReadSpectsTest, Example)
{
  std::istringstream input("This file was created by Spider.\n"
                           "If you edit it by hand, you could mess it up.\n"
                           "\nDOE^JOHN\n20241013123250.1123 \nSTART \n23.11\n"
                           "\nDOE^JANE\n2022031102\nNONE\n23121\n");
  std::vector<spider::Spect> spects = spider::ReadSpects(input);

  ASSERT_EQ(spects.size(), 2);
  EXPECT_EQ(spects[0].patient_name, "DOE^JOHN");
  EXPECT_EQ(spects[0].acquisition_timestamp, "20241013123250.1123 ");
  EXPECT_EQ(spects[0].decay_correction_method, "START ");
  EXPECT_EQ(spects[0].half_life, 23.11);
  EXPECT_EQ(spects[1].patient_name, "DOE^JANE");
  EXPECT_EQ(spects[1].acquisition_timestamp, "2022031102");
  EXPECT_EQ(spects[1].decay_correction_method, "NONE");
  EXPECT_EQ(spects[1].half_life, 23121);
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
  // Any fraction of a second is ignored.
  spider::TimeParsed time;
  spider::ParseDicomTime("030914.0103", time);
  ASSERT_TRUE(time.hour.has_value());
  EXPECT_EQ(time.hour.value(), 3);
  ASSERT_TRUE(time.minute.has_value());
  EXPECT_EQ(time.minute.value(), 9);
  ASSERT_TRUE(time.second.has_value());
  EXPECT_EQ(time.second.value(), 14);
}

TEST(MakeZonedTimeTest, Example)
{
  spider::DateComplete date = { .year = 1997, .month = 7, .day = 15 };
  spider::TimeComplete time = { .hour = 16, .minute = 43, .second = 9 };
  const spider::tz::time_zone* tz = spider::tz::locate_zone("Asia/Tokyo");
  auto zt = spider::MakeZonedTime(date, time, tz);
  ASSERT_EQ(zt.has_value(), true);

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
  auto zt_test = spider::MakeZonedTime(date, time, tz);
  EXPECT_EQ(zt_test.has_value(), false);
}

TEST(MakeZonedTimeTest, NonexistentLocalTimeDifferentTimeZone)
{
  spider::DateComplete date = { .year = 2025, .month = 3, .day = 30 };
  spider::TimeComplete time = { .hour = 2, .minute = 20, .second = 0 };
  const spider::tz::time_zone* tz = spider::tz::locate_zone("Europe/Berlin");
  auto zt_test = spider::MakeZonedTime(date, time, tz);
  EXPECT_EQ(zt_test.has_value(), false);
}

TEST(MakeZonedTimeTest, AmbiguousLocalTime)
{
  // If the local time is ambiguous, e.g. due to a DST fall-back,
  // MakeZonedTime returns the earlier time point.
  spider::DateComplete date = { .year = 2024, .month = 4, .day = 7 };
  spider::TimeComplete time = { .hour = 2, .minute = 45, .second = 0 };
  const spider::tz::time_zone* tz
      = spider::tz::locate_zone("Australia/Adelaide");
  auto zt_beg = spider::MakeZonedTime(date, time, tz);
  ASSERT_EQ(zt_beg.has_value(), true);

  spider::tz::local_seconds lt_end
      = spider::tz::local_days{ spider::tz::year{ 2024 }
                                / spider::tz::month{ 4 }
                                / spider::tz::day{ 7 } }
        + std::chrono::hours{ 4 } + std::chrono::minutes{ 0 }
        + std::chrono::seconds{ 0 };
  auto zt_end = spider::tz::zoned_time<std::chrono::seconds>{ tz, lt_end };
  std::chrono::seconds secs = spider::DiffTime(zt_end, zt_beg.value());
  // If the later time point was chosen, the difference would be 1
  // hour and 15 mins.
  EXPECT_EQ(secs, std::chrono::seconds{ (2 * 60 * 60) + (15 * 60) });
}

TEST(ToStringTest, Example)
{
  spider::DatetimeParseError e{};
  EXPECT_NE(spider::ToString(e), "");
}

TEST(DiffTimeTest, Example)
{
  spider::tz::local_seconds lt_beg
      = spider::tz::local_days{ spider::tz::year{ 1997 }
                                / spider::tz::month{ 7 }
                                / spider::tz::day{ 15 } }
        + std::chrono::hours{ 16 } + std::chrono::minutes{ 43 }
        + std::chrono::seconds{ 9 };
  const spider::tz::time_zone* tz
      = spider::tz::locate_zone("America/New_York");
  auto zt_beg = spider::tz::zoned_time<std::chrono::seconds>{ tz, lt_beg };

  spider::tz::local_seconds lt_end
      = spider::tz::local_days{ spider::tz::year{ 1997 }
                                / spider::tz::month{ 7 }
                                / spider::tz::day{ 15 } }
        + std::chrono::hours{ 16 } + std::chrono::minutes{ 45 }
        + std::chrono::seconds{ 13 };
  auto zt_end = spider::tz::zoned_time<std::chrono::seconds>{ tz, lt_end };

  std::chrono::seconds secs = spider::DiffTime(zt_end, zt_beg);
  EXPECT_EQ(secs, std::chrono::seconds{ 124 });
}

TEST(DiffTimeTest, SpringForward)
{
  spider::tz::year_month_day ymd{ spider::tz::year{ 2024 },
                                  spider::tz::month{ 10 },
                                  spider::tz::day{ 6 } };
  spider::tz::local_seconds lt_beg
      = spider::tz::local_days{ ymd } + std::chrono::hours{ 1 }
        + std::chrono::minutes{ 30 } + std::chrono::seconds{ 0 };
  // In Adelaide, Australia, this time point is 1 hour later, not 2
  // hours, due to a DST spring-forward.
  spider::tz::local_seconds lt_end
      = spider::tz::local_days{ ymd } + std::chrono::hours{ 3 }
        + std::chrono::minutes{ 30 } + std::chrono::seconds{ 0 };
  const spider::tz::time_zone* tz
      = spider::tz::locate_zone("Australia/Adelaide");
  auto zt_beg = spider::tz::zoned_time<std::chrono::seconds>{ tz, lt_beg };
  auto zt_end = spider::tz::zoned_time<std::chrono::seconds>{ tz, lt_end };

  std::chrono::seconds secs = spider::DiffTime(zt_end, zt_beg);
  EXPECT_EQ(secs, std::chrono::seconds{ 60 * 60 });
}
